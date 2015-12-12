/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "tpn/store.h"
#include "tpn/config.h"
#include "tpn/network.h"
#include "tpn/cache.h"

#include "pla/directory.h"
#include "pla/crypto.h"

namespace tpn
{

Store *Store::Instance = NULL;

BinaryString Store::Hash(const String &str)
{
	return Sha256().compute(str);
}

Store::Store(void)
{
	mDatabase = new Database("store.db");
	
	mDatabase->execute("CREATE TABLE IF NOT EXISTS blocks\
		(id INTEGER PRIMARY KEY AUTOINCREMENT,\
		digest BLOB,\
		file_id INTEGER,\
		offset INTEGER(8),\
		size INTEGER(8))");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS digest ON blocks (digest)");
	mDatabase->execute("CREATE UNIQUE INDEX IF NOT EXISTS location ON blocks (file_id, offset)");
	
	mDatabase->execute("CREATE TABLE IF NOT EXISTS files\
		(id INTEGER PRIMARY KEY AUTOINCREMENT,\
		name TEXT UNIQUE)");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS name ON files (name)");
	
	mDatabase->execute("CREATE TABLE IF NOT EXISTS map\
		(key BLOB,\
		value BLOB,\
		time INTEGER(8))");
	mDatabase->execute("CREATE UNIQUE INDEX IF NOT EXISTS pair ON map (key, value)");
	
	// Store is scheduled by Overlay on first connection
	Scheduler::Global->repeat(this, 600.);
}

Store::~Store(void)
{

}

bool Store::push(const BinaryString &digest, Fountain::Combination &input)
{
	Synchronize(this);
  
	if(hasBlock(digest)) return true;
  
	Fountain::Sink &sink = mSinks[digest];
	sink.solve(input);
	if(!sink.isDecoded()) return false;
	
	BinaryString sinkDigest;
	sink.hash(sinkDigest);
	
	LogDebug("Store::push", "Block is complete, digest is " + sinkDigest.toString());
	
	if(sinkDigest != digest)
	{
		LogWarn("Store::push", "Block digest is invalid (expected " + digest.toString() + ")");
		mSinks.erase(digest);
		return false;
	}
	
	String path = Cache::Instance->path(digest);
	
	File file(path, File::Write);
	int64_t size = sink.dump(file);
	file.close();
	
	notifyBlock(digest, path, 0, size);
	mSinks.erase(digest);
	return true;
}

bool Store::pull(const BinaryString &digest, Fountain::Combination &output, unsigned *tokens)
{
	Synchronize(this);
  
	int64_t size;
	File *file = getBlock(digest, size);
	if(!file) return false;
	
	Fountain::FileSource source(file, file->tellRead(), size);
	source.generate(output, tokens);
	return true;
}

bool Store::hasBlock(const BinaryString &digest)
{
	Synchronize(this);

	Database::Statement statement = mDatabase->prepare("SELECT 1 FROM blocks WHERE digest = ?1");
	statement.bind(1, digest);
	if(statement.step())
	{
		statement.finalize();
		return true;
	}
	
	statement.finalize();
	return false;
}

void Store::waitBlock(const BinaryString &digest)
{
	if(!waitBlock(digest, 60.))	// TODO
		throw Timeout();
}

bool Store::waitBlock(const BinaryString &digest, double &timeout)
{
	Synchronize(this);
	
	if(!hasBlock(digest))
	{
		Desynchronize(this);
		Network::Caller caller(digest);		// Block is missing locally, call it
		
		LogDebug("Store::waitBlock", "Waiting for block: " + digest.toString());
		
		{
			Synchronize(this);
			
			while(!hasBlock(digest))
				if(!wait(timeout))
					return false;
		}
		
		LogDebug("Store::waitBlock", "Block is now available: " + digest.toString());
	}
	
	return true;
}

bool Store::waitBlock(const BinaryString &digest, const double &timeout)
{
	double dummy = timeout;
	return waitBlock(digest, dummy);
}

File *Store::getBlock(const BinaryString &digest, int64_t &size)
{
	Synchronize(this);
  
	Database::Statement statement = mDatabase->prepare("SELECT f.name, b.offset, b.size FROM blocks b LEFT JOIN files f ON f.id = b.file_id WHERE b.digest = ?1 LIMIT 1");
	statement.bind(1, digest);
	if(statement.step())
	{
		String filename;
		int64_t offset;
		statement.value(0, filename);
		statement.value(1, offset);
		statement.value(2, size);
		statement.finalize();

		try {		
			File *file = new File(filename);
			file->seekRead(offset);
			return file;
		}
		catch(...)
		{
			notifyFileErasure(filename);
		}
		
		return NULL;
	}
	
	statement.finalize();
	return NULL;
}

void Store::notifyBlock(const BinaryString &digest, const String &filename, int64_t offset, int64_t size)
{
	Synchronize(this);
	
	//LogDebug("Store::notifyBlock", "Block notified: " + digest.toString());
	
	Database::Statement statement = mDatabase->prepare("INSERT OR IGNORE INTO files (name) VALUES (?1)");
	statement.bind(1, filename);
	statement.execute();
	
	statement = mDatabase->prepare("INSERT OR REPLACE INTO blocks (file_id, digest, offset, size) VALUES ((SELECT id FROM files WHERE name = ?1 LIMIT 1), ?2, ?3, ?4)");
	statement.bind(1, filename);
	statement.bind(2, digest);
	statement.bind(3, offset);
	statement.bind(4, size);
	statement.execute();
	
	notifyAll();
	
	// Publish into DHT
	DesynchronizeStatement(this, Network::Instance->storeValue(digest, Network::Instance->overlay()->localNode()));
}

void Store::notifyFileErasure(const String &filename)
{
	Synchronize(this);
	
	Database::Statement statement = mDatabase->prepare("DELETE FROM blocks WHERE file_id = (SELECT id FROM files WHERE name = ?1)");
	statement.bind(1, filename);
	statement.execute();
	
	statement = mDatabase->prepare("DELETE FROM files WHERE name = ?1");
	statement.bind(1, filename);
	statement.execute();
}

void Store::storeValue(const String &key, const BinaryString &value, bool permanent)
{
	Synchronize(this);
	
	if(permanent)
	{
		Database::Statement statement = mDatabase->prepare("DELETE FROM map WHERE key = ?1 AND time = 0");
		statement.bind(1, key);
		statement.execute();
	}
	
	Database::Statement statement = mDatabase->prepare("INSERT OR REPLACE INTO map (key, value, time) VALUES (?1, ?2, ?3)");
	statement.bind(1, key);
	statement.bind(2, value);
	statement.bind(3, (permanent ? Time(0) : Time::Now()));
	statement.execute();
}

bool Store::retrieveValue(const String &key, Set<BinaryString> &values)
{
	Synchronize(this);
	
	Database::Statement statement = mDatabase->prepare("SELECT key, value FROM map WHERE key = ?1");
	statement.bind(1, key);
	while(statement.step())
	{
		BinaryString v;
		statement.value(1, v);
		values.insert(v);
	}
	
	statement.finalize();
	return !values.empty();
}

void Store::run(void)
{	
	Synchronize(this);
 
	const double maxAge = 2*3600.;	// TODO
	const int batch = 10;		// TODO
	
	BinaryString node;
	DesynchronizeStatement(this, node = Network::Instance->overlay()->localNode());
	
	LogDebug("Store::run", "Started");

	// Delete old values
	Database::Statement statement = mDatabase->prepare("DELETE FROM map WHERE time > 0 AND time < ?1");
	statement.bind(1, Time::Now() - maxAge);
	statement.execute();
	
	try {
		// Publish everything into DHT periodically
		int offset = 0;
		while(true)
		{
			int n;
			DesynchronizeStatement(this, n = Network::Instance->overlay()->connectionsCount());
			if(n == 0)
			{
				LogDebug("Store::run", "Interrupted");
				return;
			}
			
			Database::Statement statement = mDatabase->prepare("SELECT digest FROM blocks WHERE digest IS NOT NULL ORDER BY digest LIMIT ?1 OFFSET ?2");
			statement.bind(1, batch);
			statement.bind(2, offset);
			
			List<BinaryString> result;
			statement.fetchColumn(0, result);
			statement.finalize();
			
			if(result.empty()) break;
			else {
				Desynchronize(this);
				
				for(List<BinaryString>::iterator it = result.begin(); it != result.end(); ++it)
					Network::Instance->storeValue(*it, node);
			}
			
			offset+= result.size();
			Thread::Sleep(1.);
		}
		
		LogDebug("Store::run", "Finished, " + String::number(offset) + " values published");
	}
	catch(const std::exception &e)
	{
		LogWarn("Store::run", e.what());
	}
}

}
