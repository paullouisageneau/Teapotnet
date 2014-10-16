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

#include "tpn/database.h"

#include "pla/lineserializer.h"
#include "pla/yamlserializer.h"

namespace tpn
{

Database::Database(const String &filename) :
	mDb(NULL)
{
	if(sqlite3_open(filename.c_str(), &mDb) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to open database file \"")+filename+"\"");	// TODO: close ?
	
	execute("PRAGMA synchronous = OFF");
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

int64_t Database::insert(const String &table, const Serializable &serializable)
{
	Statement dummy = prepare("SELECT * FROM `" + table + "` LIMIT 1");
	dummy.step();

	String columns;
	String values;

	int count = 0;
	for(int i=0; i<dummy.columnsCount(); ++i)
	{
		String name = dummy.name(i);
		if(name == "rowid" || name == "id")
			continue;		

		if(count) 
		{
			columns+= ',';
			values+= ',';
		}
		columns+= name;
		values+= "@" + name;
		
		++count;
	}

	dummy.finalize();

	String request = "INSERT OR REPLACE INTO `" + table + "` (" + columns + ") VALUES (" + values + ")";
	Statement statement = prepare(request);
	statement.output(serializable);
	statement.execute();	// unbound parameters will be interpreted as null

	return insertId();
}

bool Database::retrieve(const String &table, int64_t id, Serializable &serializable)
{
	Statement statement = prepare("SELECT * FROM `" + table + "` WHERE rowid=?1");
	statement.bind(1, id);

	bool success = false;
	if(statement.step())
		success = statement.input(serializable);	
	
	statement.finalize();
	return success;
}

Database::Statement::Statement(void) :
	mDb(NULL),
	mStmt(NULL)
{

}

Database::Statement::Statement(sqlite3 *db, sqlite3_stmt *stmt) :
	mDb(db),
	mStmt(stmt),
	mInputColumn(0),
	mOutputParameter(1),
	mInputLevel(0),
	mOutputLevel(0)
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
	  
	mInputColumn = 0;
	mOutputParameter = 1;
	mInputLevel = 0;
	mOutputLevel = 0;
	
	return (status == SQLITE_ROW);
}

void Database::Statement::reset(void)
{
	if(sqlite3_reset(mStmt) != SQLITE_OK)
		throw DatabaseException(mDb, "Unable to reset statement");
	
	mInputColumn = 0;
	mOutputParameter = 1;
	mInputLevel = 0;
	mOutputLevel = 0;
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

String Database::Statement::parameterName(int parameter) const
{
	String name = sqlite3_bind_parameter_name(mStmt, parameter);
	return name.substr(1);
}

int Database::Statement::parameterIndex(const String &name) const
{
	return sqlite3_bind_parameter_index(mStmt, (String("@")+name).c_str());
}

void Database::Statement::bind(int parameter, int value)
{
	if(!parameter) return;
	if(sqlite3_bind_int(mStmt, parameter, value) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));  
}

void Database::Statement::bind(int parameter, int64_t value)
{
	if(!parameter) return;
	if(sqlite3_bind_int64(mStmt, parameter, sqlite3_int64(value)) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));
}

void Database::Statement::bind(int parameter, unsigned value)
{
	if(!parameter) return;
	if(sqlite3_bind_int(mStmt, parameter, int(value)) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));  
}

void Database::Statement::bind(int parameter, uint64_t value)
{
	if(!parameter) return;
	if(sqlite3_bind_int64(mStmt, parameter, sqlite3_int64(value)) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));
}

void Database::Statement::bind(int parameter, float value)
{
	if(!parameter) return;
	if(sqlite3_bind_double(mStmt, parameter, double(value)) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));
}

void Database::Statement::bind(int parameter, double value)
{
	if(!parameter) return;
	if(sqlite3_bind_double(mStmt, parameter, value) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));
}

void Database::Statement::bind(int parameter, const String &value)
{
	if(!parameter) return;
	if(sqlite3_bind_text(mStmt, parameter, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));  
}

void Database::Statement::bind(int parameter, const BinaryString &value)
{
	if(!parameter) return;
	// TODO
	std::vector<char> tmp;
	tmp.assign(value.begin(), value.end());
	if(sqlite3_bind_blob(mStmt, parameter, &tmp[0], tmp.size(), SQLITE_TRANSIENT) != SQLITE_OK)
		throw DatabaseException(mDb, String("Unable to bind parameter ") + String::number(parameter));  
}

void Database::Statement::bind(int parameter, const Time &value)
{
	if(!parameter) return;
	bind(parameter, int64_t(value.toUnixTime()));
}

void Database::Statement::bindNull(int parameter)
{
	if(!parameter) return;
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
	const char *text = reinterpret_cast<const char*>(sqlite3_column_text(mStmt, column));
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

void Database::Statement::value(int column, BinaryString &v) const
{
	int size = sqlite3_column_bytes(mStmt, column);
	const char *data = reinterpret_cast<const char*>(sqlite3_column_text(mStmt, column));
	if(data) v.assign(data, data+size);
	else v.clear();
}

void Database::Statement::value(int column, Time &v) const
{
	int64_t t = 0;
	value(column, t);
	v = time_t(t);
}

bool Database::Statement::input(Serializable &s)
{
	if(mInputLevel == 0 || s.isInlineSerializable()) return s.deserialize(*this);
	else {
		String tmp;
		if(!input(tmp)) return false;
		YamlSerializer serializer(&tmp);
		s.deserialize(serializer);
		return true;
	}
}

bool Database::Statement::input(Element &element)
{
	++mInputLevel;
	bool result = element.deserialize(*this);
	--mInputLevel;
	return result;
}

bool Database::Statement::input(Pair &pair)
{
	if(mInputColumn >= columnsCount()) return false;
	String key = name(mInputColumn);
	key = key.afterLast('.');
	LineSerializer keySerializer(&key);
	pair.deserializeKey(keySerializer);
	
	++mInputLevel;
	bool result = pair.deserializeValue(*this);
	--mInputLevel;
	return result;
}

bool Database::Statement::input(String &str)
{
	if(mInputColumn >= columnsCount()) return false;
	value(mInputColumn++, str);
	return true;
}

bool Database::Statement::input(BinaryString &str)
{
	if(mInputColumn >= columnsCount()) return false;
	value(mInputColumn++, str);
	return true;
}

bool Database::Statement::input(int8_t &i)
{
	if(mInputColumn >= columnsCount()) return false;
	int v = 0;
	value(mInputColumn++, v);
	i = int8_t(v);
	return true;
}

bool Database::Statement::input(int16_t &i)
{
	if(mInputColumn >= columnsCount()) return false;
	int v = 0;
	value(mInputColumn++, v);
	i = int16_t(v);
	return true;
}
	
bool Database::Statement::input(int32_t &i)
{
	if(mInputColumn >= columnsCount()) return false;
	int v = 0;
	value(mInputColumn++, v);
	i = int32_t(v);
	return true;
}

bool Database::Statement::input(int64_t &i)
{
	if(mInputColumn >= columnsCount()) return false;
	value(mInputColumn++, i);
	return true;
}

bool Database::Statement::input(uint8_t &i)
{
	if(mInputColumn >= columnsCount()) return false;
	unsigned v = 0;
	value(mInputColumn++, v);
	i = uint8_t(v);
	return false;
}

bool Database::Statement::input(uint16_t &i)
{
	if(mInputColumn >= columnsCount()) return false;
	unsigned v = 0;
	value(mInputColumn++, v);
	i = uint16_t(v);
	return true;
}

bool Database::Statement::input(uint32_t &i)
{
	if(mInputColumn >= columnsCount()) return false;
	unsigned v = 0;
	value(mInputColumn++, v);
	i = uint32_t(v);
	return true;
}

bool Database::Statement::input(uint64_t &i)
{
	if(mInputColumn >= columnsCount()) return false;
	value(mInputColumn++, i);
	return true;
}

bool Database::Statement::input(bool &b)
{
	if(mInputColumn >= columnsCount()) return false;
	int8_t i = 0;
	input(i);
	b = (i != 0);
	return true;
}

bool Database::Statement::input(float &f)
{
	if(mInputColumn >= columnsCount()) return false;
	value(mInputColumn++, f);
	return true;
}

bool Database::Statement::input(double &f)
{
	if(mInputColumn >= columnsCount()) return false;
	value(mInputColumn++, f);
	return true;
}

void Database::Statement::output(const Serializable &s)
{
	if(mOutputLevel == 0 || s.isInlineSerializable()) s.serialize(*this);
	else {
		String tmp;
		YamlSerializer serializer(&tmp);
		s.serialize(serializer);
		output(tmp);
	}
}
	
void Database::Statement::output(const Element &element)
{
	++mOutputLevel;
	element.serialize(*this);
	--mOutputLevel;
}

void Database::Statement::output(const Pair &pair)
{
	String key;
	LineSerializer keySerializer(&key);
	pair.serializeKey(keySerializer);
	key.trim();
	
	int parameter = parameterIndex(key);
	if(parameter != 0) 
	{
		mOutputParameter = parameter;
		++mOutputLevel;
		pair.serializeValue(*this);
		--mOutputLevel;
	}
}

void Database::Statement::output(const String &str)
{
	bind(mOutputParameter++, str);
}

void Database::Statement::output(const BinaryString &str)
{
	bind(mOutputParameter++, str);
}

void Database::Statement::output(int8_t i)
{
	bind(mOutputParameter++, int(i));
}

void Database::Statement::output(int16_t i)
{
	bind(mOutputParameter++, int(i));
}

void Database::Statement::output(int32_t i)
{
	bind(mOutputParameter++, int(i));
}

void Database::Statement::output(int64_t i)
{
	bind(mOutputParameter++, i);
}

void Database::Statement::output(uint8_t i)
{
	bind(mOutputParameter++, unsigned(i));
}

void Database::Statement::output(uint16_t i)
{
	bind(mOutputParameter++, unsigned(i));
}

void Database::Statement::output(uint32_t i)
{
	bind(mOutputParameter++, unsigned(i));
}

void Database::Statement::output(uint64_t i)
{
	bind(mOutputParameter++, i);
}

void Database::Statement::output(bool b)
{
	int8_t i;
	if(b) i = 1;
	else i = 0;
	bind(mOutputParameter++, i);
}

void Database::Statement::output(float f)
{
	bind(mOutputParameter++, f);
}

void Database::Statement::output(double f)
{
	bind(mOutputParameter++, f);
}

DatabaseException::DatabaseException(sqlite3 *db, const String &message) :
	Exception(String("Database error: ") + message + String(": ") + String(sqlite3_errmsg(db)))
{

}

}
