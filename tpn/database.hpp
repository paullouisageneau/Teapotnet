/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#ifndef TPN_DATABASE_H
#define TPN_DATABASE_H

#include "tpn/include.hpp"

#include "pla/string.hpp"
#include "pla/binarystring.hpp"
#include "pla/exception.hpp"
#include "pla/serializer.hpp"
#include "pla/time.hpp"
#include "pla/array.hpp"
#include "pla/list.hpp"

#ifdef USE_SYSTEM_SQLITE3
#include <sqlite3.h>
#else
#include "include/sqlite3.h"
#endif

namespace tpn
{

class Database
{
public:
	Database(const String &filename);
	~Database(void);

	class Statement : public Serializer
	{
	public:
		Statement(void);
		Statement(sqlite3 *db, sqlite3_stmt *stmt);
		~Statement(void);
		
		bool step(void);
		void reset(void);
		void finalize(void);		
		void execute(void);	// step + finalize

		template<typename T> bool fetch(List<T> &result);
		template<typename T> bool fetchColumn(int index, List<T> &result);
		template<typename T> bool fetch(Array<T> &result);
		template<typename T> bool fetchColumn(int index, Array<T> &result);
	
		enum type_t { Null, Integer, Float, Text, Blob };
		
		int parametersCount(void) const;
		String parameterName(int parameter) const;
		int parameterIndex(const String &name) const;
		void bind(int parameter, int value);
		void bind(int parameter, int64_t value);
		void bind(int parameter, unsigned value);
		void bind(int parameter, uint64_t value);
		void bind(int parameter, float value);
		void bind(int parameter, double value);
		void bind(int parameter, const String &value);
		void bind(int parameter, const BinaryString &value);
		void bind(int parameter, const Time &value);
		void bindNull(int parameter);
		
		int columnsCount(void) const;
		type_t type(int column) const;
		String name(int column) const;
		String value(int column) const;
		void value(int column, int &v) const;
		void value(int column, int64_t &v) const;
		void value(int column, unsigned &v) const;
		void value(int column, uint64_t &v) const;
		void value(int column, float &v) const;
		void value(int column, double &v) const;
		void value(int column, String &v) const;
		void value(int column, BinaryString &v) const;
		void value(int column, Time &v) const;
	
		inline bool retrieve(Serializable &s) { return read(s); }
	
		// --- Serializer interface
		virtual bool	read(Serializable &s);
		virtual void	write(const Serializable &s);
		virtual bool	read(String &s);
		virtual void	write(const String &s);
		virtual bool	read(BinaryString &s);
		virtual void	write(const BinaryString &s);
		
		virtual bool	read(std::string &str);
		virtual bool	read(uint8_t &i);
		virtual bool	read(uint16_t &i);
		virtual bool	read(uint32_t &i);
		virtual bool	read(uint64_t &i);
		virtual bool	read(int8_t &i);
		virtual bool	read(int16_t &i);
		virtual bool	read(int32_t &i);
		virtual bool	read(int64_t &i);
		virtual bool	read(float &f);
		virtual bool	read(double &f)	;
		virtual bool	read(bool &b);
		
		virtual void	write(const std::string &str);
		virtual void	write(uint8_t i);
		virtual void	write(uint16_t i);
		virtual void	write(uint32_t i);
		virtual void	write(uint64_t i);
		virtual void	write(int8_t i);
		virtual void	write(int16_t i);
		virtual void	write(int32_t i);
		virtual void	write(int64_t i);
		virtual void	write(float f);
		virtual void	write(double f);
		virtual void	write(bool b);
		
		virtual bool	skip(void);
		
		virtual bool	readArrayBegin(void)		{ return true; }
		virtual bool	readArrayNext(void)		{ return true; }
		virtual bool	readMapBegin(void)		{ return true; }
		virtual bool	readMapNext(void)		{ return true; }
		
		virtual void	writeArrayBegin(size_t size)	{}
		virtual void	writeArrayNext(size_t i)	{}
		virtual void	writeArrayEnd(void)		{}
		virtual void	writeMapBegin(size_t size)	{}
		virtual void	writeMapNext(size_t i)		{}
		virtual void	writeMapEnd(void)		{}
		virtual void	writeEnd(void)			{}
		// ---
		
	private:
		sqlite3 *mDb;
		sqlite3_stmt *mStmt;
		
		// For serializer
		int mInputColumn;
		int mOutputParameter;
		int mInputLevel, mOutputLevel;
	};
	
	Statement prepare(const String &request);
	void execute(const String &request);
	int64_t insertId(void) const;

	int64_t insert(const String &table, const Serializable &serializable);
	bool retrieve(const String &table, int64_t id, Serializable &serializable);

private:
	sqlite3 *mDb;
};

class DatabaseException : public Exception
{
public:
	DatabaseException(sqlite3 *db, const String &message);
};

template<typename T> bool Database::Statement::fetch(List<T> &result)
{
	result.clear();
	while(step())
	{
		T tmp;
		this->read(tmp);
		result.push_back(tmp);
	}

	// finalize is not called here
	return (!result.empty());
}

template<typename T> bool Database::Statement::fetchColumn(int index, List<T> &result)
{
	result.clear();
	while(step())
	{
		T tmp;
		value(index, tmp);
		result.push_back(tmp);
	}

	// finalize is not called here
	return (!result.empty());
}

template<typename T> bool Database::Statement::fetch(Array<T> &result)
{
	result.clear();
	while(step())
	{
		T tmp;
		this->read(tmp);
		result.push_back(tmp);
	}

	// finalize is not called here
	return (!result.empty());
}

template<typename T> bool Database::Statement::fetchColumn(int index, Array<T> &result)
{
	result.clear();
	while(step())
	{
		T tmp;
		value(index, tmp);
		result.push_back(tmp);
	}

	// finalize is not called here
	return (!result.empty());
}

}

#endif
