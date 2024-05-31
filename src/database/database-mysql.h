/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include <string>
#include "database.h"
//#include "util/basic_macros.h"
#include <mysql.h>

class Settings;

class Database_MySQL : public Database
{
public:
	typedef std::vector<std::pair<std::string, std::vector<std::string>>> Transaction;

public:
	Database_MySQL(const std::string &connect_string, const char *type);
	~Database_MySQL();

	void beginSave();
	void endSave();
	void rollback();

	bool initialized() const;

	bool execTransaction(const std::vector<std::string>& query);
	// Simplified query execution function
	bool execQuery(const std::string& query);
	// Simplified function to execute a query that expects results (e.g., SELECT)
	MYSQL_RES* execQueryWithResult(const std::string& query);

	MYSQL_RES* execWithParamAndResult(const std::string& query, const std::vector<std::string>& params);
	bool execWithParam(const std::string& query, const std::vector<std::string>& params);
	bool execTransactionWithParam(const Transaction& query_and_params);
	std::string buildQueryWithParam(const std::string& query, const std::vector<std::string>& params);


protected:
	void createTableIfNotExists(const std::string &table_name, const std::string &table_schema, const std::string& options = "");
	void verifyDatabase();

	// Database initialization
	void connectToDatabase();
	virtual void createDatabase() = 0;

	MYSQL* m_conn = nullptr;

private:
	// Database connectivity checks
	void ping();
	
	std::string m_connect_string;
	std::string m_type;

	void initConnection();
	void closeConnection();
	void handleMySQLError();
	bool doQueries(const std::vector<std::string>& query);
};

class MapDatabaseMySQL : private Database_MySQL, public MapDatabase
{
public:
	MapDatabaseMySQL(const std::string &connect_string);
	virtual ~MapDatabaseMySQL();

	bool saveBlock(const v3s16& pos, std::string_view data);
	void loadBlock(const v3s16 &pos, std::string *block);
	bool deleteBlock(const v3s16 &pos);
	void listAllLoadableBlocks(std::vector<v3s16> &dst);

	void beginSave() { Database_MySQL::beginSave(); }
	void endSave() { Database_MySQL::endSave(); }

protected:
	virtual void createDatabase();

private:
	MYSQL_STMT* stmt_load_block = nullptr;
	MYSQL_STMT* stmt_save_block = nullptr;
};

class PlayerDatabaseMySQL : private Database_MySQL, public PlayerDatabase
{
public:
	PlayerDatabaseMySQL(const std::string &connect_string);
	virtual ~PlayerDatabaseMySQL() = default;

	void savePlayer(RemotePlayer *player);
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao);
	bool removePlayer(const std::string &name);
	void listPlayers(std::vector<std::string> &res);
	bool set_player_metadata(const std::string& player_name, const std::unordered_map<std::string, std::string>& metadata);
	bool get_player_metadata(const std::string& player_name, const std::string& attr, std::string& result);
	bool rename_player(const std::string& old_name, const std::string& new_name);

protected:
	virtual void createDatabase();

private:
	bool playerDataExists(const std::string &playername);
};

class AuthDatabaseMySQL : private Database_MySQL, public AuthDatabase
{
public:
	AuthDatabaseMySQL(const std::string &connect_string);
	virtual ~AuthDatabaseMySQL() = default;

	virtual void verifyDatabase() { Database_MySQL::verifyDatabase(); }

	virtual bool getAuth(const std::string &name, AuthEntry &res);
	virtual bool saveAuth(const AuthEntry &authEntry);
	virtual bool createAuth(AuthEntry &authEntry);
	virtual bool deleteAuth(const std::string &name);
	virtual void listNames(std::vector<std::string> &res);
	virtual void reload();

protected:
	virtual void createDatabase();

private:
	virtual void writePrivileges(const AuthEntry &authEntry);
};

class ModStorageDatabaseMySQL : private Database_MySQL, public ModStorageDatabase
{
public:
	ModStorageDatabaseMySQL(const std::string &connect_string);
	~ModStorageDatabaseMySQL() = default;

	void getModEntries(const std::string &modname, StringMap *storage);
	void getModKeys(const std::string &modname, std::vector<std::string> *storage);
	bool getModEntry(const std::string &modname, const std::string &key, std::string *value);
	bool hasModEntry(const std::string &modname, const std::string &key);
	bool setModEntry(const std::string &modname,
			const std::string &key, std::string_view value);
	bool removeModEntry(const std::string &modname, const std::string &key);
	bool removeModEntries(const std::string &modname);
	void listMods(std::vector<std::string> *res);

	void beginSave() { Database_MySQL::beginSave(); }
	void endSave() { Database_MySQL::endSave(); }

protected:
	virtual void createDatabase();
};
