/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
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

#ifndef TPOT_DATABASE_H
#define TPOT_DATABASE_H

#include "include.h"
#include "string.h"
#include "bytestring.h"
#include "exception.h"

#include <sqlite3.h>

namespace tpot
{

class Database
{
public:
	Database(const String &filename);
	~Database(void);

	class Statement
	{
	public:
		Statement(sqlite3 *db, sqlite3_stmt *stmt);
		~Statement(void);
		
		bool next(void);
		void reset(void);
		void finalize(void);
		
		enum type_t { Null, Integer, Float, Text, Blob };
		
		int columnsCount(void) const;
		type_t type(int column) const;
		String name(int column) const;
		String value(int column) const;
		
		void value(int column, int &v) const;
		void value(int column, float &v) const;
		void value(int column, double &v) const;
		void value(int column, String &v) const;
		void value(int column, ByteString &v) const;
		
	private:
		sqlite3 *mDb;
		sqlite3_stmt *mStmt;
	};
	
	Statement prepare(const String &request);

private:
	sqlite3 *mDb;
};

class DatabaseException : public Exception
{
public:
	DatabaseException(sqlite3 *db, const String &message);
};

}

#endif
