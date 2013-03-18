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

#include "tpn/database.h"

namespace tpn
{

Database::Database(const String &filename) :
	mDb(NULL)
{
	if(sqlite3_open(filename.c_str(), &mDb) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to open database file \"")+filename+"\"");	// TODO: close ?
}

Database::~Database(void)
{
	sqlite3_close(mDb);
}

Database::Statement Database::prepare(const String &request)
{
	sqlite3_stmt *stmt = NULL;
	if(sqlite3_prepare_v2(mDb, request.c_str(), -1, &stmt, NULL) != SQLITE_OK)
		 throw DatabaseException(mDb, String("Unable to prepare request \"")+request+"\"");
	
	return Statement(mDb, stmt);
}

void Database::execute(const String &request)
{
	Statement statement = prepare(request);
	statement.step();
	statement.finalize();
}

int64_t Database::insertId(void) const
{
	return sqlite3_last_insert_rowid(mDb); 
}

Database::Statement::Statement(void) :
	mDb(NULL),
	mStmt(NULL)
{

}

Database::Statement::Statement(sqlite3 *db, sqlite3_stmt *stmt) :
	mDb(db),
	mStmt(stmt)
{

}

Database::Statement::~Statement(void)
{
	// DO NOT call finalize, object is passed by copy
}

bool Database::Statement::step(void)
{
	int status = sqlite3_step(mStmt);
	if(status != SQLITE_DONE && status != SQLITE_ROW)
		throw DatabaseException(mDb, "Statement execution failed");
	  
	return (status == SQLITE_ROW);
}

void Database::Statement::reset(void)
{
	if(sqlite3_reset(mStmt) != SQLITE_OK)
		throw DatabaseException(mDb, "Unable to reset statement");
}

void Database::Statement::finalize(void)
{
	  sqlite3_finalize(mStmt);
	  mStmt = NULL;
}

void Database::Statement::execute(void)
{
	step();
	finalize();
}

int Database::Statement::parametersCount(void) const
{
	return sqlite3_bind_parameter_count(mStmt);
}

void Database::Statement::bind(int parameter, int value)
{
	if(sqlite3_bind_int(mStmt, parameter, value) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));  
}

void Database::Statement::bind(int parameter, int64_t value)
{
	if(sqlite3_bind_int64(mStmt, parameter, sqlite3_int64(value)) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));
}

void Database::Statement::bind(int parameter, unsigned value)
{
	if(sqlite3_bind_int(mStmt, parameter, int(value)) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));  
}

void Database::Statement::bind(int parameter, uint64_t value)
{
	if(sqlite3_bind_int64(mStmt, parameter, sqlite3_int64(value)) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));
}

void Database::Statement::bind(int parameter, float value)
{
	if(sqlite3_bind_double(mStmt, parameter, double(value)) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));
}

void Database::Statement::bind(int parameter, double value)
{
	if(sqlite3_bind_double(mStmt, parameter, value) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));
}

void Database::Statement::bind(int parameter, const String &value)
{
	if(sqlite3_bind_text(mStmt, parameter, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));  
}

void Database::Statement::bind(int parameter, const ByteString &value)
{
	// TODO
	std::vector<char> tmp;
	tmp.assign(value.begin(), value.end());
	if(sqlite3_bind_blob(mStmt, parameter, &tmp[0], tmp.size(), SQLITE_TRANSIENT) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));  
}

void Database::Statement::bindNull(int parameter)
{
	if(sqlite3_bind_null(mStmt, parameter) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));  
}

int Database::Statement::columnsCount(void) const
{
	return sqlite3_column_count(mStmt);  
}

Database::Statement::type_t Database::Statement::type(int column) const
{
	switch(sqlite3_column_type(mStmt, column))
	{
	  case SQLITE_INTEGER:  return Integer;
	  case SQLITE_FLOAT:	return Float;
	  case SQLITE_TEXT:	return Text;
	  case SQLITE_BLOB:	return Blob;
	  case SQLITE_NULL:	return Null;
	  default: throw DatabaseException(mDb, "Unable to retrieve column type");
	}
}

String Database::Statement::name(int column) const
{
	return sqlite3_column_name(mStmt, column);  
}

String Database::Statement::value(int column) const
{
	const char * text = reinterpret_cast<const char*>(sqlite3_column_text(mStmt, column));
	if(text) return String(text);
	else return String();
}
		
void Database::Statement::value(int column, int &v) const
{
	v = sqlite3_column_int(mStmt, column);
}

void Database::Statement::value(int column, int64_t &v) const
{
	v = sqlite3_column_int64(mStmt, column);
}

void Database::Statement::value(int column, unsigned &v) const
{
	v = unsigned(sqlite3_column_int(mStmt, column));
}

void Database::Statement::value(int column, uint64_t &v) const
{
	v = uint64_t(sqlite3_column_int64(mStmt, column));
}

void Database::Statement::value(int column, float &v) const
{
  	v = float(sqlite3_column_double(mStmt, column));
}

void Database::Statement::value(int column, double &v) const
{
  	v = sqlite3_column_double(mStmt, column);
}

void Database::Statement::value(int column, String &v) const
{
  	const char *text = reinterpret_cast<const char*>(sqlite3_column_text(mStmt, column));
	if(text) v = text;
	else v.clear();
}

void Database::Statement::value(int column, ByteString &v) const
{
	int size = sqlite3_column_bytes(mStmt, column);
	const char *data = reinterpret_cast<const char*>(sqlite3_column_text(mStmt, column));
	if(data) v.assign(data, data+size);
	else v.clear();
}

DatabaseException::DatabaseException(sqlite3 *db, const String &message) :
	Exception(String("Database error: ") + message + String(": ") + String(sqlite3_errmsg(db)))
{
  
}

}
