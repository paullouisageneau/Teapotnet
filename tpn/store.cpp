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
	mDatabase->execute("CREATE INDEX IF NOT EXISTS type ON map (type, time)");
}

Store::~Store(void)
{

}

bool Store::push(const BinaryString &digest, Fountain::Combination &input)
{
	sptr<Sink> sink;
	{
		std::unique_lock<std::mutex> lock(mMutex);
	
		if(hasBlock(digest)) return true;
	
		mSinks.get(digest, sink);
		if(!sink)
		{
			sink = std::make_shared<Sink>(digest);
			mSinks.insert(digest, sink);
		}
	}
	
	//LogDebug("Store::push", "Pushing to " + digest.toString());
	
	if(sink->push(input))
	{
		// Block is decoded !
		std::unique_lock<std::mutex> lock(mMutex);
		mSinks.erase(digest);
		notifyBlock(digest, sink->path(), 0, sink->size());
		return true;
	}

	// We need more combinations
	return false;
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
	if(hasBlock(digest)) return 0;
	
	std::unique_lock<std::mutex> lock(mMutex);
	
	auto it = mSinks.find(digest);
	if(it != mSinks.end()) return it->second->missing();
	else return Block::MaxChunks;
}

bool Store::hasBlock(const BinaryString &digest)
{
	Database::Statement statement = mDatabase->prepare("SELECT f.name FROM blocks b LEFT JOIN files f ON f.id = b.file_id WHERE b.digest = ?1");
	statement.bind(1, digest);
	if(statement.step())
	{
		String filename;
		statement.value(0, filename);
		statement.finalize();
		
		if(File::Exist(filename))
			return true;
			
		notifyFileErasure(filename);
		return false;
	}
	
	statement.finalize();
	return false;
}

void Store::waitBlock(const BinaryString &digest)
{
	const duration timeout = milliseconds(Config::Get("request_timeout").toDouble())*2;
	if(!waitBlock(digest, timeout))
		throw Timeout();
}

bool Store::waitBlock(const BinaryString &digest, duration timeout)
{
	if(!hasBlock(digest))
	{
		Network::Caller caller(digest);		// Block is missing locally, call it
		
		LogDebug("Store::waitBlock", "Waiting for block: " + digest.toString());
		
		{
			std::unique_lock<std::mutex> lock(mMutex);
			
			if(!hasBlock(digest))
			{
				if(!mCondition.wait_for(lock, timeout, [this, digest]() { return hasBlock(digest); }))
					return false;
			}
		}
		
		LogDebug("Store::waitBlock", "Block is now available: " + digest.toString());
	}
	
	return true;
}

File *Store::getBlock(const BinaryString &digest, int64_t &size)
{
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
	
	Database::Statement statement = mDatabase->prepare("INSERT OR IGNORE INTO files (name) VALUES (?1)");
	statement.bind(1, filename);
	statement.execute();
		
	statement = mDatabase->prepare("INSERT OR REPLACE INTO blocks (file_id, digest, offset, size) VALUES ((SELECT id FROM files WHERE name = ?1 LIMIT 1), ?2, ?3, ?4)");
	statement.bind(1, filename);
	statement.bind(2, digest);
	statement.bind(3, offset);
	statement.bind(4, size);
	statement.execute();
	
	mCondition.notify_all();
	
	// Publish into DHT
	Network::Instance->storeValue(digest, Network::Instance->overlay()->localNode());
}

void Store::notifyFileErasure(const String &filename)
{
	Database::Statement statement = mDatabase->prepare("DELETE FROM blocks WHERE file_id = (SELECT id FROM files WHERE name = ?1)");
	statement.bind(1, filename);
	statement.execute();
	
	statement = mDatabase->prepare("DELETE FROM files WHERE name = ?1");
	statement.bind(1, filename);
	statement.execute();
}

void Store::hintBlock(const BinaryString &digest, const BinaryString &hint)
{
	// Values for digest are candidate nodes, re-hash to store hints
	storeValue(Store::Hash(digest), hint, Temporary);
}

bool Store::getBlockHints(const BinaryString &digest, Set<BinaryString> &result)
{
	retrieveValue(Store::Hash(digest), result);
	if(result.empty()) return false;
	
	// Add hints of order 2
	Set<BinaryString> tmp;
	for(const BinaryString &value : result)
		retrieveValue(Store::Hash(value), tmp);
	
	result.insertAll(tmp);
	return true;
}

void Store::storeValue(const BinaryString &key, const BinaryString &value, Store::ValueType type, Time time)
{
	const duration maxAge = seconds(Config::Get("store_max_age").toDouble());
	if(type != Permanent && Time::Now() - time >= maxAge) 
		return;
	
	Database::Statement statement = mDatabase->prepare("INSERT OR IGNORE INTO map (key, value, time, type) VALUES (?1, ?2, ?3, ?4)");
	statement.bind(1, key);
	statement.bind(2, value);
	statement.bind(3, time);
	statement.bind(4, static_cast<int>(type));
	statement.execute();
	
	statement = mDatabase->prepare("UPDATE map SET time = MAX(time, ?3) AND type = MIN(type, ?4) WHERE key = ?1 AND value = ?2");
	statement.bind(1, key);
	statement.bind(2, value);
	statement.bind(3, time);
	statement.bind(4, static_cast<int>(type));
	statement.execute();
}

void Store::eraseValue(const BinaryString &key, const BinaryString &value)
{
	Database::Statement statement = mDatabase->prepare("DELETE FROM map WHERE key = ?1 AND value = ?2");
	statement.bind(1, key);
	statement.bind(2, value);
	statement.execute();
}

bool Store::retrieveValue(const BinaryString &key, Set<BinaryString> &values)
{
	// Note: values is not cleared !
	
	const Identifier localNode = Network::Instance->overlay()->localNode();
	
	Database::Statement statement = mDatabase->prepare("SELECT value FROM map WHERE key = ?1");
	statement.bind(1, key);
	while(statement.step())
	{
		BinaryString v;
		statement.value(0, v);
		values.insert(v);
	}
	statement.finalize();
	
	if(!values.contains(localNode))
	{
		// Also look for digest in blocks in case map is not up-to-date
		statement = mDatabase->prepare("SELECT 1 FROM blocks WHERE digest = ?1 LIMIT 1");
		statement.bind(1, key);
		if(statement.step())
			values.insert(localNode);
		statement.finalize();
	}
	
	return !values.empty();
}

bool Store::retrieveValue(const BinaryString &key, List<BinaryString> &values, List<Time> &times)
{
	// Note: values is not cleared !
	
	const Identifier localNode = Network::Instance->overlay()->localNode();
	bool hasLocalNode = false; 
	
	// Look first for digest in blocks in case map is not up-to-date
	Database::Statement statement = mDatabase->prepare("SELECT 1 FROM blocks WHERE digest = ?1 LIMIT 1");
	statement.bind(1, key);
	if(statement.step())
	{
		values.push_back(localNode);
		times.push_back(Time::Now());
		hasLocalNode = true;
		
	}
	statement.finalize();
	
	statement = mDatabase->prepare("SELECT value, time FROM map WHERE key = ?1 ORDER BY time DESC");
	statement.bind(1, key);
	while(statement.step())
	{
		BinaryString v;
		statement.value(0, v);
		if(hasLocalNode && v == localNode)
			continue;
		
		Time t;
		statement.value(1, t);
		
		values.push_back(v);
		times.push_back(t);
	}
	statement.finalize();
	
	return !values.empty();
}

bool Store::hasValue(const BinaryString &key, const BinaryString &value) const
{
	Database::Statement statement = mDatabase->prepare("SELECT 1 FROM map WHERE key = ?1 AND value = ?2 LIMIT 1");
	statement.bind(1, key);
	statement.bind(2, value);
	
	bool found = statement.step();
	statement.finalize();
	return found;
}

Time Store::getValueTime(const BinaryString &key, const BinaryString &value) const
{
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
	const duration maxAge = seconds(Config::Get("store_max_age").toDouble());
	
	// TODO: delay and batch values
	const duration delay = seconds(1.);
	const int batch = 10;
	
	LogDebug("Store::run", "Started");

	try {
		const BinaryString node = Network::Instance->overlay()->localNode();
		
		// Publish everything into DHT periodically
		int offset = 0;
		while(true)
		{
			if(Network::Instance->overlay()->connectionsCount() == 0)
			{
				LogDebug("Store::run", "Interrupted");
				return;
			}

			Database::Statement statement;

			// Delete some old non-permanent values
			statement = mDatabase->prepare("DELETE FROM map WHERE rowid IN (SELECT rowid FROM map WHERE type != ?1 AND time <= ?2 LIMIT ?3)");
			statement.bind(1, static_cast<int>(Permanent));
			statement.bind(2, Time::Now() - maxAge);
			statement.bind(3, batch);
			statement.execute();	
	
			// Select DHT values
			statement = mDatabase->prepare("SELECT digest FROM blocks WHERE digest IS NOT NULL ORDER BY id DESC LIMIT ?1 OFFSET ?2");
			statement.bind(1, batch);
			statement.bind(2, offset);
			
			List<BinaryString> result;
			statement.fetchColumn(0, result);
			statement.finalize();
			
			if(result.empty()) break;
			offset+= result.size();

			for(List<BinaryString>::iterator it = result.begin(); it != result.end(); ++it)
				Network::Instance->storeValue(*it, node);
			
			std::this_thread::sleep_for(delay);
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
	}
}

Store::Sink::Sink(const BinaryString &digest) :
	mDigest(digest),
	mSize(0)
{

}

Store::Sink::~Sink(void)
{
	
}
		
bool Store::Sink::push(Fountain::Combination &incoming)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	mSink.solve(incoming);
	if(!mSink.isDecoded()) return false;
	
	BinaryString sinkDigest;
	mSink.hash(sinkDigest);
	
	LogDebug("Store::push", "Block is complete, digest is " + sinkDigest.toString());
	
	if(!mDigest.empty() && sinkDigest != mDigest)
	{
		LogWarn("Store::push", "Block digest is invalid (expected " + mDigest.toString() + ")");
		mSink.clear();
		return false;
	}
	
	mPath = Cache::Instance->path(mDigest);
	
	File file(mPath, File::Write);
	mSize = mSink.dump(file);
	file.close();
	return true;
}

unsigned Store::Sink::missing(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mSink.missing();
}

String Store::Sink::path(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mPath;
}

int64_t Store::Sink::size(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mSize;
}

}
