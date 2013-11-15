/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of TeapotNet.                                     *
 *                                                                       *
 *   TeapotNet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   TeapotNet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with TeapotNet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef TPN_DATABASE_H
#define TPN_DATABASE_H

#include "tpn/include.h"
#include "tpn/string.h"
#include "tpn/bytestring.h"
#include "tpn/exception.h"
#include "tpn/serializer.h"
#include "tpn/time.h"

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
		void bind(int parameter, const ByteString &value);
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
		void value(int column, ByteString &v) const;
		void value(int column, Time &v) const;
	
		inline bool retrieve(Serializable &s) { return input(s); }
	
		// --- Serializer interface
		virtual bool    input(Serializable &s);
		virtual bool	input(Element &element);
		virtual bool	input(Pair &pair);
	
		virtual bool    input(String &str);
		virtual bool    input(ByteString &str);
		virtual bool	input(int8_t &i);
		virtual bool	input(int16_t &i);
		virtual bool	input(int32_t &i);
		virtual bool	input(int64_t &i);
		virtual bool	input(uint8_t &i);
		virtual bool	input(uint16_t &i);
		virtual bool	input(uint32_t &i);
		virtual bool	input(uint64_t &i);
		virtual bool	input(bool &b);
		virtual bool	input(float &f);
		virtual bool	input(double &f);

		virtual void    output(const Serializable &s);
		virtual void	output(const Element &element);
		virtual void	output(const Pair &pair);
		
		virtual void    output(const String &str);
		virtual void    output(const ByteString &str);
		virtual void	output(int8_t i);
		virtual void	output(int16_t i);
		virtual void	output(int32_t i);
		virtual void	output(int64_t i);
		virtual void	output(uint8_t i);
		virtual void	output(uint16_t i);
		virtual void	output(uint32_t i);
		virtual void	output(uint64_t i);	
		virtual void	output(bool b);
		virtual void	output(float f);
		virtual void	output(double f);
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

template<typename T> bool Database::Statement::fetch(Array<T> &result)
{
	result.clear();
	while(step())
	{
		T tmp;
		tmp.deserialize(*this);
		result.append(tmp);
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
		result.append(tmp);
	}

	// finalize is not called here
	return (!result.empty());
}

}

#endif
