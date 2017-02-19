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

#ifndef TPN_STORE_H
#define TPN_STORE_H

#include "tpn/include.hpp"
#include "tpn/database.hpp"
#include "tpn/fountain.hpp"

#include "pla/file.hpp"
#include "pla/time.hpp"
#include "pla/map.hpp"
#include "pla/list.hpp"
#include "pla/set.hpp"
#include "pla/binarystring.hpp"

namespace tpn
{
  
class Store
{
public:
	static Store *Instance;
	static BinaryString Hash(const String &str);
	
	Store(void);
	~Store(void);
	
	bool push(const BinaryString &digest, Fountain::Combination &input);
	bool pull(const BinaryString &digest, Fountain::Combination &output, unsigned *rank = NULL);
	unsigned missing(const BinaryString &digest);
	
	bool hasBlock(const BinaryString &digest);
	void waitBlock(const BinaryString &digest);
	bool waitBlock(const BinaryString &digest, duration timeout);
	File *getBlock(const BinaryString &digest, int64_t &size);
	void notifyBlock(const BinaryString &digest, const String &filename, int64_t offset, int64_t size);
	void notifyFileErasure(const String &filename);
	
	void hintBlock(const BinaryString &digest, const BinaryString &hint);
	bool getBlockHints(const BinaryString &digest, Set<BinaryString> &result);
	
	enum ValueType
	{
		Permanent   = 0,	// Local and permanent
		Temporary   = 1,	// Local but temporary
		Distributed = 2		// DHT
	};
	
	void storeValue(const BinaryString &key, const BinaryString &value, ValueType type = Store::Temporary, Time time = Time::Now());
	void eraseValue(const BinaryString &key, const BinaryString &value);
	bool retrieveValue(const BinaryString &key, Set<BinaryString> &values);
	bool retrieveValue(const BinaryString &key, List<BinaryString> &values, List<Time> &times);
	bool hasValue(const BinaryString &key, const BinaryString &value) const;
	Time getValueTime(const BinaryString &key, const BinaryString &value) const;
	
	void start(void);
	
private:
	void run(void);

	// Sink wrapper with mutex
	class Sink
	{
	public:
		Sink(const BinaryString &digest = "");
		~Sink(void);
		
		bool push(Fountain::Combination &incoming);
		unsigned missing(void) const;
		
		String path(void) const;
		int64_t size(void) const;
		
	private:
		Fountain::Sink mSink;
		BinaryString mDigest;
		String mPath;
		int64_t mSize;
		
		mutable std::mutex mMutex;
	};
	
	Database *mDatabase;
	Map<BinaryString,sptr<Sink> > mSinks;
	bool mRunning;
	
	mutable std::mutex mMutex;
	mutable std::condition_variable mCondition;
};

}

#endif

