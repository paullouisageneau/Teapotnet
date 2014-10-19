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
#include "tpn/core.h"

#include "pla/directory.h"
#include "pla/crypto.h"

namespace tpn
{

Store *Store::Instance = NULL;

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
	
	mCacheDirectory = Config::Get("cache_dir");
}

Store::~Store(void)
{

}

bool Store::push(const BinaryString &digest, Stream &input)
{
	Synchronize(this);
  
	if(hasBlock(digest)) return true;
  
	Fountain::Sink &sink = mSinks[digest];
	if(!sink.solve(input)) return false;
	
	BinaryString sinkDigest;
	sink.hash(sinkDigest);
	
	LogDebug("Store::push", "Block is complete, digest is " + sinkDigest.toString());
	
	if(sinkDigest != digest)
	{
		LogWarn("Store::push", "Block digest is invalid (expected " + digest.toString() + ")");
		mSinks.erase(digest);
		return false;
	}
	
	const String filename = mCacheDirectory + Directory::Separator + digest.toString();
	File file(filename, File::Write);
	sink.dump(file);
	file.close();
	
	int64_t size = sink.size();
	notifyBlock(digest, filename, 0, size);
	
	mSinks.erase(digest);
	return true;
}

bool Store::pull(const BinaryString &digest, Stream &output, unsigned *tokens)
{
	Synchronize(this);
  
	int64_t size;
	File *file = getBlock(digest, size);
	if(!file) return false;
	
	Fountain::Source source(file, file->tellRead(), size);
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
	Synchronize(this);
	
	if(!hasBlock(digest))
	{
		Core::Caller caller(digest);		// Block is missing locally, call it
		
		LogDebug("Store::waitBlock", "Waiting for block: " + digest.toString());
		
		do wait();
		while(!hasBlock(digest));
		
		LogDebug("Store::waitBlock", "Block is now available: " + digest.toString());
	}
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

}
