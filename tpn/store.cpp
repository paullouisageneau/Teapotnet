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

#include "tpn/store.hpp"
#include "tpn/config.hpp"
#include "tpn/network.hpp"
#include "tpn/cache.hpp"
#include "tpn/block.hpp"

#include "pla/directory.hpp"
#include "pla/crypto.hpp"

namespace tpn
{

Store *Store::Instance = NULL;

BinaryString Store::Hash(const String &str)
{
	return Sha256().compute(str);
}

Store::Store(void) :
	mRunning(false)
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
		time INTEGER(8),\
		type INTEGER(1))");
	mDatabase->execute("CREATE UNIQUE INDEX IF NOT EXISTS pair ON map (key, value)");
}

Store::~Store(void)
{

}

bool Store::push(const BinaryString &digest, Fountain::Combination &input)
{
	std::unique_lock<std::mutex> lock(mMutex);
  
	if(hasBlock(digest)) return true;
  
	Fountain::Sink &sink = mSinks[digest];
	sink.solve(input);
	if(!sink.isDecoded()) return false;
	
	BinaryString sinkDigest;
	sink.hash(sinkDigest);
	
	LogDebug("Store::pu.hpp", "Block is complete, digest is " + sinkDigest.toString());
	
	if(sinkDigest != digest)
	{
		LogWarn("Store::pu.hpp", "Block digest is invalid (expected " + digest.toString() + ")");
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

bool Store::pull(const BinaryString &digest, Fountain::Combination &output, unsigned *rank)
{
	std::unique_lock<std::mutex> lock(mMutex);
  
	int64_t size;
	File *file = getBlock(digest, size);
	if(!file) return false;
	
	Fountain::FileSource source(file, file->tellRead(), size);
	source.generate(output);
	if(rank) *rank = source.rank();
	return true;
}

unsigned Store::missing(const BinaryString &digest)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	Map<BinaryString,Fountain::Sink>::iterator it = mSinks.find(digest);
	if(it != mSinks.end()) return it->second.missing();
	else return uint16_t(-1);
}

bool Store::hasBlock(const BinaryString &digest)
{
	std::unique_lock<std::mutex> lock(mMutex);

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

void Store::waitBlock(const BinaryString &digest, const BinaryString &hint)
{
	const duration timeout = milliseconds(Config::Get("request_timeout").toDouble())*2;
	if(!waitBlock(digest, timeout, hint))
		throw Timeout();
}

bool Store::waitBlock(const BinaryString &digest, duration timeout, const BinaryString &hint)
{
	if(!hasBlock(digest))
	{
		Network::Caller caller(digest, hint);		// Block is missing locally, call it
		
		LogDebug("Store::waitBlock", "Waiting for block: " + digest.toString());
		
		{
			std::unique_lock<std::mutex> lock(mMutex);
			
			mCondition.wait_for(lock, timeout, [this, digest]() {
				return hasBlock(digest);
			});
			
			if(!hasBlock(digest))
				return false;
		}
		
		LogDebug("Store::waitBlock", "Block is now available: " + digest.toString());
	}
	
	return true;
}

File *Store::getBlock(const BinaryString &digest, int64_t &size)
{
	std::unique_lock<std::mutex> lock(mMutex);
  
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
	//LogDebug("Store::notifyBlock", "Block notified: " + digest.toString());
	
	{
		std::unique_lock<std::mutex> lock(mMutex);
		
		Database::Statement statement = mDatabase->prepare("INSERT OR IGNORE INTO files (name) VALUES (?1)");
		statement.bind(1, filename);
		statement.execute();
		
		statement = mDatabase->prepare("INSERT OR REPLACE INTO blocks (file_id, digest, offset, size) VALUES ((SELECT id FROM files WHERE name = ?1 LIMIT 1), ?2, ?3, ?4)");
		statement.bind(1, filename);
		statement.bind(2, digest);
		statement.bind(3, offset);
		statement.bind(4, size);
		statement.execute();
	}
	
	mCondition.notify_all();
	
	// Publish into DHT
	Network::Instance->storeValue(digest, Network::Instance->overlay()->localNode());
}

void Store::notifyFileErasure(const String &filename)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	Database::Statement statement = mDatabase->prepare("DELETE FROM blocks WHERE file_id = (SELECT id FROM files WHERE name = ?1)");
	statement.bind(1, filename);
	statement.execute();
	
	statement = mDatabase->prepare("DELETE FROM files WHERE name = ?1");
	statement.bind(1, filename);
	statement.execute();
}

void Store::storeValue(const BinaryString &key, const BinaryString &value, Store::ValueType type)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	if(type == Permanent)
	{
		Database::Statement statement = mDatabase->prepare("DELETE FROM map WHERE key = ?1 AND type = ?2");
		statement.bind(1, key);
		statement.bind(2, static_cast<int>(Permanent));
		statement.execute();
	}
	
	auto secs = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	
	Database::Statement statement = mDatabase->prepare("INSERT OR REPLACE INTO map (key, value, time, type) VALUES (?1, ?2, ?3, ?4)");
	statement.bind(1, key);
	statement.bind(2, value);
	statement.bind(3, secs);
	statement.bind(4, static_cast<int>(type));
	statement.execute();
}

bool Store::retrieveValue(const BinaryString &key, Set<BinaryString> &values)
{
	Identifier localNode = Network::Instance->overlay()->localNode();
	
	std::unique_lock<std::mutex> lock(mMutex);
	
	Database::Statement statement = mDatabase->prepare("SELECT value FROM map WHERE key = ?1");
	statement.bind(1, key);
	while(statement.step())
	{
		BinaryString v;
		statement.value(0, v);
		values.insert(v);
	}
	statement.finalize();
	
	// Also look for digest in blocks in case map is not up-to-date
	statement = mDatabase->prepare("SELECT 1 FROM blocks WHERE digest = ?1 LIMIT 1");
	statement.bind(1, key);
	if(statement.step())
		values.insert(localNode);
	statement.finalize();
	
	return !values.empty();
}

bool Store::hasValue(const BinaryString &key, const BinaryString &value) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	Database::Statement statement = mDatabase->prepare("SELECT 1 FROM map WHERE key = ?1 AND value = ?2 LIMIT 1");
	statement.bind(1, key);
	statement.bind(2, value);
	
	bool found = statement.step();
	statement.finalize();
	return found;
}

Time Store::getValueTime(const BinaryString &key, const BinaryString &value) const
{
	std::unique_lock<std::mutex> lock(mMutex);

	Database::Statement statement = mDatabase->prepare("SELECT time FROM map WHERE key = ?1 AND value = ?2 LIMIT 1");
        statement.bind(1, key);
        statement.bind(2, value);

	Time time(0);
        if(statement.step())
		statement.value(0, time);
        statement.finalize();

        return time;
}

void Store::start(void)
{
	std::unique_lock<std::mutex> lock(mMutex);
	if(mRunning) return;
	mRunning = true;
	
	std::thread thread([this]()
	{
		run();
	});
	
	thread.detach();
}

void Store::run(void)
{	
	{
		std::unique_lock<std::mutex> lock(mMutex);
		if(mRunning) return;
		mRunning = true;
	}
	
	const duration maxAge = seconds(Config::Get("store_max_age").toDouble());
	const duration delay = seconds(1.);	// TODO
	const int batch = 10;			// TODO
	
	LogDebug("Store::run", "Started");

	try {
		BinaryString node = Network::Instance->overlay()->localNode();
		auto secs = std::chrono::duration_cast<std::chrono::seconds>((std::chrono::system_clock::now() - maxAge).time_since_epoch()).count();
		
		// Delete old non-permanent values
		Database::Statement statement = mDatabase->prepare("DELETE FROM map WHERE type != ?1 AND time < ?2");
		statement.bind(1, static_cast<int>(Permanent));
		statement.bind(2, secs);
		statement.execute();
		
		// Publish everything into DHT periodically
		int offset = 0;
		while(true)
		{
			if(Network::Instance->overlay()->connectionsCount() == 0)
			{
				LogDebug("Store::run", "Interrupted");
				return;
			}
			
			// Select DHT values
			Database::Statement statement = mDatabase->prepare("SELECT digest FROM blocks WHERE digest IS NOT NULL ORDER BY id DESC LIMIT ?1 OFFSET ?2");
			statement.bind(1, batch);
			statement.bind(2, offset);
			
			List<BinaryString> result;
			statement.fetchColumn(0, result);
			statement.finalize();
			
			if(result.empty()) break;
			else {
				for(List<BinaryString>::iterator it = result.begin(); it != result.end(); ++it)
					Network::Instance->storeValue(*it, node);
				
				std::this_thread::sleep_for(delay);
			}
			
			offset+= result.size();
		}
		
		LogDebug("Store::run", "Finished, " + String::number(offset) + " values published");
	}
	catch(const std::exception &e)
	{
		LogWarn("Store::run", e.what());
	}
	
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mRunning = false;
		
		// Store is scheduled by Overlay on first connection
		// TODO
		//Scheduler::Global->schedule(this, maxAge/2);
	}
}

}
