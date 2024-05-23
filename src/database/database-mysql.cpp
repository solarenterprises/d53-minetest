/*
Copyright (C) 2016 Loic Blot <loic.blot@unix-experience.fr>

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

#include "config.h"

#include "database-mysql.h"

#ifdef _WIN32
	#include <windows.h>
	#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include "debug.h"
#include "exceptions.h"
#include "settings.h"
#include "remoteplayer.h"
#include "server/player_sao.h"
#include <cstdlib>

inline std::string mysql_to_string(MYSQL_RES* res, int row, int col)
{
	MYSQL_ROW mysql_row;
	mysql_data_seek(res, row);
	mysql_row = mysql_fetch_row(res);
	unsigned long* lengths;
	lengths = mysql_fetch_lengths(res);
	return std::string(mysql_row[col], lengths[col]);
}

Database_MySQL::Database_MySQL(
	const std::string &connect_string,
	const char *type) :
	m_connect_string(connect_string)
{
	m_type = type;

	if (m_connect_string.empty()) {
		// Use given type to reference the exact setting in the error message
		std::string s = type;
		std::string msg =
			"Set mysql" + s + "_connection string in world.mt to "
			"use the mysql backend\n"
			"Notes:\n"
			"mysql" + s + "_connection has the following form: \n"
			"\tpmysql" + s + "_connection = host=127.0.0.1 port=5432 "
			"user=mt_user password=mt_password dbname=minetest" + s + "\n"
			"mt_user should have CREATE TABLE, INSERT, SELECT, UPDATE and "
			"DELETE rights on the database. "
			"Don't create mt_user as a SUPERUSER!";
		throw SettingNotFoundException(msg);
	}
}

Database_MySQL::~Database_MySQL() {
	closeConnection();
}


void Database_MySQL::closeConnection() {
	if (m_conn) {
		mysql_close(m_conn);
		m_conn = nullptr;
	}
}

void Database_MySQL::handleMySQLError() {
	std::string error_msg = mysql_error(m_conn);
	errorstream
		<< "Database_MySQL:"
		<< error_msg.c_str()
		<< std::endl;
	throw std::runtime_error("MySQL Error: " + error_msg);
}

void Database_MySQL::execQuery(const std::string& query) {
	if (mysql_query(m_conn, query.c_str())) {
		handleMySQLError();
	}
}

MYSQL_RES* Database_MySQL::execQueryWithResult(const std::string& query) {
	if (mysql_query(m_conn, query.c_str())) {
		errorstream << "MySQL query error: " << mysql_error(m_conn) << std::endl;
		return nullptr;
	}
	return mysql_store_result(m_conn);
}

std::string Database_MySQL::buildQueryWithParam(const std::string& query, const std::vector<std::string>& params) {
	std::string preparedQuery = query;
	// Simple parameter substitution instead of real prepared statements for demonstration
	// Note: This is not safe against SQL injection and should be replaced with real prepared statement usage.
	for (size_t i = 0; i < params.size(); ++i) {
		size_t pos;
		do {
			pos = preparedQuery.find("$" + std::to_string(i + 1));
			if (pos != std::string::npos) {
				preparedQuery.replace(pos, 2, "'" + params[i] + "'");
			}
		} while (pos != std::string::npos);
	}

	size_t pos = preparedQuery.find("$");
	if (pos != std::string::npos) {
		errorstream << "MySQL all params were not replaced in query: " << query.c_str() << std::endl;
	}
				
	return preparedQuery;
}

void Database_MySQL::execWithParam(const std::string& query, const std::vector<std::string>& params) {
	execQuery(buildQueryWithParam(query, params));
}

MYSQL_RES* Database_MySQL::execWithParamAndResult(const std::string& query, const std::vector<std::string>& params) {
	return execQueryWithResult(buildQueryWithParam(query, params));
}

bool Database_MySQL::initialized() const {
	return m_conn != nullptr;
}

std::map<std::string, std::string> parse_connection_string(const std::string& conn_str) {
	std::map<std::string, std::string> conn_params;
	std::istringstream tokenStream(conn_str);
	std::string token;
	while (std::getline(tokenStream, token, ';')) {
		auto delimiterPos = token.find('=');
		auto key = token.substr(0, delimiterPos);
		auto value = token.substr(delimiterPos + 1);
		conn_params[key] = value;
	}
	return conn_params;
}

void Database_MySQL::connectToDatabase()
{
	if (initialized())
		return;

	m_conn = mysql_init(NULL);

	if (!m_conn) {
		throw std::runtime_error("Failed to initialize MySQL connection");
	}


	auto params = parse_connection_string(m_connect_string);
	auto database = params["database_prefix"] + m_type;

	if (mysql_real_connect(
		m_conn,
		params["host"].c_str(),
		params["user"].c_str(),
		params.find("password") != params.end() ? params["password"].c_str() : nullptr,
		database.c_str(),
		0,
		NULL,
		0) == NULL) {
		handleMySQLError();
	}

	infostream << "mySQL Database: Version " << " Connection made." << std::endl;

	createDatabase();
}

void Database_MySQL::verifyDatabase()
{
	ping();
}

void Database_MySQL::ping()
{
	// mysql_ping() checks the connection, and if it is not alive, attempts to reconnect.
	if (mysql_ping(m_conn)) {
		// If mysql_ping returns non-zero, the reconnection attempt failed.
		handleMySQLError(); // Handle the error (throw an exception or log it)
	}
}

void Database_MySQL::createTableIfNotExists(
	const std::string& table_name,
	const std::string& table_schema,
	const std::string& options)
{
	// Construct the SQL statement to create the table if it doesn't exist.
	// The table_schema parameter should contain the SQL statement defining the table columns and types.
	std::string sql = "CREATE TABLE IF NOT EXISTS " + table_name + " (" + table_schema + ")" + options + ";";

	// Execute the SQL statement.
	try {
		execQuery(sql);
	}
	catch (const std::runtime_error& e) {
		// Handle any errors that occur during table creation.
		// This catch block can log the error, rethrow the exception, or perform custom error handling.
		errorstream << "Error creating table '" << table_name << "': " << e.what() << std::endl;
		// Rethrow the exception or handle it as needed.
		throw std::runtime_error("Failed to create table MySQL:" + table_name);
	}
}

void Database_MySQL::beginSave()
{
	verifyDatabase();
	try {
		execQuery("START TRANSACTION");
	}
	catch (const std::runtime_error& e) {
		errorstream << "Failed to start transaction: " << e.what() << std::endl;
		throw std::runtime_error("Failed to start transaction"); // Rethrow the exception or handle it as needed.
	}
}

void Database_MySQL::endSave()
{
	try {
		execQuery("COMMIT");
	}
	catch (const std::runtime_error& e) {
		errorstream << "Failed to commit transaction: " << e.what() << std::endl;
		throw std::runtime_error("Failed to commit transaction: "); // Rethrow the exception or handle it as needed.
	}
}

void Database_MySQL::rollback()
{
	try {
		execQuery("ROLLBACK");
	}
	catch (const std::runtime_error& e) {
		errorstream << "Failed to rollback transaction: " << e.what() << std::endl;
		// It might be a good idea not to throw an exception here since rollback might be called
		// as part of an exception handling path already. But this depends on your error handling strategy.
	}
}

/*
 * Map Database
 */
MapDatabaseMySQL::MapDatabaseMySQL(const std::string &connect_string):
	Database_MySQL(connect_string, "map"),
	MapDatabase()
{
	connectToDatabase();
}

MapDatabaseMySQL::~MapDatabaseMySQL() {
	if (stmt_load_block)
		mysql_stmt_close(stmt_load_block);
	if (stmt_save_block)
		mysql_stmt_close(stmt_save_block);
}

void MapDatabaseMySQL::createDatabase() {
	createTableIfNotExists(
		"blocks",
		"posX INT NOT NULL,"
		"posY INT NOT NULL,"
		"posZ INT NOT NULL,"
		"data BLOB,"
		"PRIMARY KEY (posX, posY, posZ)"
	);
}

bool MapDatabaseMySQL::saveBlock(const v3s16& pos, std::string_view data) {
	if (!stmt_save_block) {
		stmt_save_block = mysql_stmt_init(m_conn);

		std::string query = "INSERT INTO blocks (posX, posY, posZ, data) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE data = VALUES(data)";

		if (mysql_stmt_prepare(stmt_save_block, query.c_str(), query.length())) {
			fprintf(stderr, "mysql_stmt_prepare() failed\n");
			fprintf(stderr, " %s\n", mysql_stmt_error(stmt_save_block));

			mysql_stmt_close(stmt_save_block);
			stmt_save_block = nullptr;
			return false;
		}
	}

	MYSQL_BIND bind[4];
	memset(&bind, 0, sizeof(bind));
	bind[0].buffer_type = MYSQL_TYPE_SHORT;
	bind[0].buffer = (char*)&pos.X;

	bind[1].buffer_type = MYSQL_TYPE_SHORT;
	bind[1].buffer = (char*)&pos.Y;

	bind[2].buffer_type = MYSQL_TYPE_SHORT;
	bind[2].buffer = (char*)&pos.Z;

	unsigned long length = data.size();
	bind[3].buffer_type = MYSQL_TYPE_BLOB;
	bind[3].buffer = (char*)data.data();
	bind[3].buffer_length = data.size();
	bind[3].length = &length;

	mysql_stmt_bind_param(stmt_save_block, bind);
	if (mysql_stmt_execute(stmt_save_block)) {
		std::cerr << "Execute error: " << mysql_stmt_error(stmt_save_block) << std::endl;
		return false;
	}

	return true;
}

char blob_data[65'535];

void MapDatabaseMySQL::loadBlock(const v3s16& pos, std::string* block) {
	if (!stmt_load_block) {
		stmt_load_block = mysql_stmt_init(m_conn);
		const char* query = "SELECT data FROM blocks WHERE posX=? AND posY=? AND posZ=? LIMIT 1";
		if (mysql_stmt_prepare(stmt_load_block, query, strlen(query))) {
			fprintf(stderr, "mysql_stmt_prepare() failed\n");
			fprintf(stderr, " %s\n", mysql_stmt_error(stmt_load_block));

			mysql_stmt_close(stmt_load_block);
			stmt_load_block = nullptr;
			return;
		}
	}

	MYSQL_BIND bind_loadBlock[3];
	memset(&bind_loadBlock, 0, sizeof(bind_loadBlock));
	bind_loadBlock[0].buffer_type = MYSQL_TYPE_SHORT;
	bind_loadBlock[0].buffer = (char*)&pos.X;
	bind_loadBlock[0].is_null = 0;
	bind_loadBlock[0].length = 0;

	bind_loadBlock[1].buffer_type = MYSQL_TYPE_SHORT;
	bind_loadBlock[1].buffer = (char*)&pos.Y;
	bind_loadBlock[1].is_null = 0;
	bind_loadBlock[1].length = 0;

	bind_loadBlock[2].buffer_type = MYSQL_TYPE_SHORT;
	bind_loadBlock[2].buffer = (char*)&pos.Z;
	bind_loadBlock[2].is_null = 0;
	bind_loadBlock[2].length = 0;

	if (mysql_stmt_bind_param(stmt_load_block, bind_loadBlock)) {
		fprintf(stderr, "mysql_stmt_bind_param() failed\n");
		fprintf(stderr, " %s\n", mysql_stmt_error(stmt_load_block));
		return;
	}

	// Execute statement
	if (mysql_stmt_execute(stmt_load_block)) {
		std::cerr << "Execute error: " << mysql_stmt_error(stmt_load_block) << std::endl;
		return;
	}

	MYSQL_BIND result[1];
	memset(result, 0, sizeof(result));

	 // Adjust size as needed
	unsigned long data_length = 0;

	result[0].buffer_type = MYSQL_TYPE_BLOB;
	result[0].buffer = (char*)&blob_data;
	result[0].buffer_length = sizeof(blob_data);
	result[0].is_null = 0;
	result[0].length = &data_length;

	if (mysql_stmt_bind_result(stmt_load_block, result)) {
		fprintf(stderr, "mysql_stmt_bind_result() failed\n");
		fprintf(stderr, " %s\n", mysql_stmt_error(stmt_load_block));
		return;
	}

	int status = 0;
	while (status == 0) {
		status = mysql_stmt_fetch(stmt_load_block);
		if (status == 1) {
			fprintf(stderr, "mysql_stmt_fetch(), failed\n");
			fprintf(stderr, " %s\n", mysql_stmt_error(stmt_load_block));
			return;
		}

		if (status == MYSQL_NO_DATA)
			return;

		*block = std::string(blob_data, data_length);
	}

	/*std::string query = "SELECT data FROM blocks WHERE posX = " + std::to_string(pos.X) +
		" AND posY = " + std::to_string(pos.Y) +
		" AND posZ = " + std::to_string(pos.Z) + " LIMIT 1;";
	MYSQL_RES* result = execQueryWithResult(query);
	if (!result) return;

	MYSQL_ROW row = mysql_fetch_row(result);
	if (row && row[0]) {
		*block = std::string(row[0], mysql_fetch_lengths(result)[0]);
	}

	mysql_free_result(result);*/
}

bool MapDatabaseMySQL::deleteBlock(const v3s16& pos) {
	std::string query = "DELETE FROM blocks WHERE posX = " + std::to_string(pos.X) +
		" AND posY = " + std::to_string(pos.Y) +
		" AND posZ = " + std::to_string(pos.Z) + " LIMIT 1;";
	execQuery(query);
	return mysql_affected_rows(m_conn) > 0;
}

void MapDatabaseMySQL::listAllLoadableBlocks(std::vector<v3s16>& dst) {
	std::string query = "SELECT posX, posY, posZ FROM blocks;";
	MYSQL_RES* result = execQueryWithResult(query);
	if (!result) return;

	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		v3s16 pos;
		pos.X = atoi(row[0]);
		pos.Y = atoi(row[1]);
		pos.Z = atoi(row[2]);
		dst.push_back(pos);
	}

	mysql_free_result(result);
}

/*
 * Player Database
 */
PlayerDatabaseMySQL::PlayerDatabaseMySQL(const std::string &connect_string):
	Database_MySQL(connect_string, "player"),
	PlayerDatabase()
{
	connectToDatabase();
}


void PlayerDatabaseMySQL::createDatabase()
{
	createTableIfNotExists("player",
		"name VARCHAR(60) NOT NULL,"
		"pitch DECIMAL(15, 7) NOT NULL,"
		"yaw DECIMAL(15, 7) NOT NULL,"
		"posX DECIMAL(15, 7) NOT NULL,"
		"posY DECIMAL(15, 7) NOT NULL,"
		"posZ DECIMAL(15, 7) NOT NULL,"
		"hp INT NOT NULL,"
		"breath INT NOT NULL,"
		"creation_date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
		"modification_date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
		"PRIMARY KEY (name)"
	);

	createTableIfNotExists("player_inventories",
		"player VARCHAR(60) NOT NULL,"
		"inv_id INT NOT NULL,"
		"inv_width INT NOT NULL,"
		"inv_name TEXT NOT NULL,"
		"inv_size INT NOT NULL,"
		"PRIMARY KEY(player, inv_id),"
		"FOREIGN KEY (player) REFERENCES player (name) ON DELETE CASCADE"
	);

	createTableIfNotExists("player_inventory_items",
		"player VARCHAR(60) NOT NULL,"
		"inv_id INT NOT NULL,"
		"slot_id INT NOT NULL,"
		"item TEXT NOT NULL,"
		"PRIMARY KEY(player, inv_id, slot_id),"
		"FOREIGN KEY (player) REFERENCES player (name) ON DELETE CASCADE"
	);

	createTableIfNotExists("player_metadata",
		"player VARCHAR(60) NOT NULL,"
		"attr VARCHAR(256) NOT NULL,"
		"value TEXT,"
		"PRIMARY KEY(player, attr),"
		"FOREIGN KEY (player) REFERENCES player (name) ON DELETE CASCADE"
	);

	infostream << "MySQL: Player Database was inited." << std::endl;
}

bool PlayerDatabaseMySQL::playerDataExists(const std::string &playername)
{
	verifyDatabase();

	std::vector<std::string> values = { playername };
	MYSQL_RES* results = execWithParamAndResult("SELECT pitch, yaw, posX, posY, posZ, hp, breath FROM player WHERE name = $1", values);

	bool res = (mysql_num_rows(results) > 0);
	mysql_free_result(results);
	return res;
}

void PlayerDatabaseMySQL::savePlayer(RemotePlayer *player)
{
	PlayerSAO* sao = player->getPlayerSAO();
	if (!sao)
		return;

	verifyDatabase();

	v3f pos = sao->getBasePosition();
	std::string pitch = ftos(sao->getLookPitch());
	std::string yaw = ftos(sao->getRotation().Y);
	std::string posx = ftos(pos.X);
	std::string posy = ftos(pos.Y);
	std::string posz = ftos(pos.Z);
	std::string hp = itos(sao->getHP());
	std::string breath = itos(sao->getBreath());
	std::vector<std::string> values = {
		player->getName(),
		pitch.c_str(),
		yaw.c_str(),
		posx.c_str(), posy.c_str(), posz.c_str(),
		hp.c_str(),
		breath.c_str()
	};

	std::vector<std::string> rmvalues = { player->getName() };
	beginSave();

	execWithParam("INSERT INTO player(name, pitch, yaw, posX, posY, posZ, hp, breath) VALUES "
		"($1, $2, $3, $4, $5, $6, $7, $8) "
		"ON DUPLICATE KEY UPDATE pitch = VALUES(pitch), yaw = VALUES(yaw), "
		"posX = VALUES(posX), posY = VALUES(posY), posZ = VALUES(posZ), hp = VALUES(hp), breath = VALUES(breath), "
		"modification_date = NOW()", values);

	// Write player inventories
	execWithParam("DELETE FROM player_inventories WHERE player = $1", rmvalues);
	execWithParam("DELETE FROM player_inventory_items WHERE player = $1", rmvalues);

	const auto &inventory_lists = sao->getInventory()->getLists();
	std::ostringstream oss;
	for (u16 i = 0; i < inventory_lists.size(); i++) {
		const InventoryList* list = inventory_lists[i];
		const std::string &name = list->getName();
		std::string width = itos(list->getWidth()),
			inv_id = itos(i), lsize = itos(list->getSize());

		std::vector<std::string> inv_values = {
			player->getName(),
			inv_id.c_str(),
			width.c_str(),
			name.c_str(),
			lsize.c_str()
		};
		execWithParam("INSERT INTO player_inventories (player, inv_id, inv_width, inv_name, inv_size) VALUES "
			"($1, $2, $3, $4, $5)", inv_values);

		for (u32 j = 0; j < list->getSize(); j++) {
			oss.str("");
			oss.clear();
			list->getItem(j).serialize(oss);
			std::string itemStr = oss.str(), slotId = itos(j);

			std::vector<std::string> invitem_values = {
				player->getName(),
				inv_id.c_str(),
				slotId.c_str(),
				itemStr.c_str()
			};
			execWithParam("INSERT INTO player_inventory_items (player, inv_id, slot_id, item) VALUES "
				"($1, $2, $3, $4)", invitem_values);
		}
	}

	execWithParam("DELETE FROM player_metadata WHERE player = $1", rmvalues);
	const StringMap &attrs = sao->getMeta().getStrings();
	for (const auto &attr : attrs) {
		std::vector<std::string> meta_values = {
			player->getName(),
			attr.first.c_str(),
			attr.second.c_str()
		};
		execWithParam("INSERT INTO player_metadata(player, attr, value) VALUES($1, $2, $3)", meta_values);
	}
	endSave();

	player->onSuccessfulSave();
}

bool PlayerDatabaseMySQL::loadPlayer(RemotePlayer *player, PlayerSAO *sao)
{
	sanity_check(sao);
	verifyDatabase();

	std::vector<std::string> values = { player->getName() };
	MYSQL_RES* results = execWithParamAndResult("SELECT pitch, yaw, posX, posY, posZ, hp, breath FROM player WHERE name = $1", values);

	// Player not found, return not found
	if (mysql_num_rows(results) == 0) {
		mysql_free_result(results);
		return false;
	}

	MYSQL_ROW row = mysql_fetch_row(results);
	sao->setLookPitch(atof(row[0]));
	sao->setRotation(v3f(0, atof(row[1]), 0));
	sao->setBasePosition(v3f(
		atof(row[2]),
		atof(row[3]),
		atof(row[4])
	));
	sao->setHPRaw(static_cast<u16>(atoi(row[5])));
	sao->setBreath(static_cast<u16>(atoi(row[6])), false);

	mysql_free_result(results);

	// Load inventory
	results = execWithParamAndResult("SELECT inv_id, inv_width, inv_name, inv_size FROM player_inventories WHERE player = $1 ORDER BY inv_id", values);

	while ((row = mysql_fetch_row(results)) != nullptr) {
		InventoryList* invList = player->inventory.addList(row[2], atoi(row[3]));
		invList->setWidth(atoi(row[1]));

		u32 invId = atoi(row[0]);
		std::vector<std::string> values2 = { player->getName(), std::to_string(invId) };
		MYSQL_RES* results2 = execWithParamAndResult("SELECT slot_id, item FROM player_inventory_items WHERE player = $1 AND inv_id = $2", values2);

		MYSQL_ROW row2;
		while ((row2 = mysql_fetch_row(results2)) != nullptr) {
			const std::string itemStr = row2[1];
			if (!itemStr.empty()) {
				ItemStack stack;
				stack.deSerialize(itemStr);
				invList->changeItem(atoi(row2[0]), stack);
			}
		}
		mysql_free_result(results2);
	}

	mysql_free_result(results);

	// Load player metadata
	results = execWithParamAndResult("SELECT attr, value FROM player_metadata WHERE player = $1", values);

	while ((row = mysql_fetch_row(results)) != nullptr) {
		sao->getMeta().setString(row[0], row[1]);
	}
	sao->getMeta().setModified(false);

	mysql_free_result(results);

	return true;
}

bool PlayerDatabaseMySQL::removePlayer(const std::string &name)
{
	if (!playerDataExists(name))
		return false;

	verifyDatabase();

	std::vector<std::string> values = { name };
	execWithParam("DELETE FROM player WHERE name = $1", values);

	return true;
}

void PlayerDatabaseMySQL::listPlayers(std::vector<std::string> &res)
{
	verifyDatabase();

	MYSQL_RES* results = execQueryWithResult("SELECT name FROM player");

	MYSQL_ROW row;
	while ((row = mysql_fetch_row(results)) != NULL) {
		res.emplace_back(row[0]);
	}

	mysql_free_result(results);
}

bool PlayerDatabaseMySQL::get_player_meta_data(const std::string& player_name, const std::string& attr, std::string& result)
{
	result = "";

	// Load player metadata
	MYSQL_RES* results = execWithParamAndResult("SELECT value FROM player_metadata WHERE player = $1 AND attr = $2 LIMIT 1", { player_name, attr });

	MYSQL_ROW row;
	while ((row = mysql_fetch_row(results)) != nullptr) {
		result = row[0];
		break;
	}

	mysql_free_result(results);

	return !result.empty();
}

/*
 * Auth Database
 */
AuthDatabaseMySQL::AuthDatabaseMySQL(const std::string &connect_string) :
	Database_MySQL(connect_string, "auth"),
	AuthDatabase()
{
	connectToDatabase();
}

void AuthDatabaseMySQL::createDatabase()
{
	createTableIfNotExists("auth",
		"id INT AUTO_INCREMENT,"
		"name VARCHAR(255) UNIQUE,"
		"password TEXT,"
		"last_login INT NOT NULL DEFAULT 0,"
		"PRIMARY KEY (id)"
		);

	createTableIfNotExists("user_privileges",
		"id INT,"
		"privilege VARCHAR(255),"
		"PRIMARY KEY (id, privilege),"
		"CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES auth (id) ON DELETE CASCADE"
		);
}

bool AuthDatabaseMySQL::getAuth(const std::string &name, AuthEntry &res)
{
	verifyDatabase();

	std::vector<std::string> values = { name };
	MYSQL_RES* result = execWithParamAndResult("SELECT id, name, password, last_login FROM auth WHERE name = $1", values);
	int numrows = mysql_num_rows(result);
	if (numrows == 0) {
		mysql_free_result(result);
		return false;
	}

	MYSQL_ROW row = mysql_fetch_row(result);
	res.id = static_cast<unsigned int>(std::stoul(row[0]));
	res.name = row[1];
	res.password = row[2];
	res.last_login = atoi(row[3]);

	mysql_free_result(result);

	std::string playerIdStr = std::to_string(res.id);
	std::vector<std::string> privsValues = { playerIdStr };
	MYSQL_RES* results = execWithParamAndResult("SELECT privilege FROM user_privileges WHERE id = $1", privsValues);

	numrows = mysql_num_rows(results);
	for (int row = 0; row < numrows; row++) {
		MYSQL_ROW row_priv = mysql_fetch_row(results);
		res.privileges.emplace_back(row_priv[0]);
	}

	mysql_free_result(results);

	return true;
}

bool AuthDatabaseMySQL::saveAuth(const AuthEntry &authEntry)
{
	verifyDatabase();

	beginSave();

	std::string lastLoginStr = std::to_string(authEntry.last_login);
	std::string idStr = std::to_string(authEntry.id);
	std::vector<std::string> values = {
		authEntry.name,
		authEntry.password,
		lastLoginStr,
		idStr,
	};
	execWithParam("UPDATE auth SET name = $1, password = $2, last_login = $3 WHERE id = $4", values);

	writePrivileges(authEntry);

	endSave();
	return true;
}

bool AuthDatabaseMySQL::createAuth(AuthEntry &authEntry)
{
	verifyDatabase();

	std::string lastLoginStr = std::to_string(authEntry.last_login);
	std::vector<std::string> values = {
		authEntry.name,
		authEntry.password,
		lastLoginStr
	};

	beginSave();

	execWithParam("INSERT INTO auth (name, password, last_login) VALUES ($1, $2, $3)", values);
	unsigned long authId = mysql_insert_id(m_conn);
	if (authId == 0) {
		errorstream << "Strange behavior on auth creation, no ID returned." << std::endl;
		rollback();
		return false;
	}

	authEntry.id = authId;

	writePrivileges(authEntry);

	endSave();
	return true;
}

bool AuthDatabaseMySQL::deleteAuth(const std::string &name)
{
	verifyDatabase();

	std::vector<std::string> values = { name };
	execWithParam("DELETE FROM auth WHERE name = $1", values);

	// privileges deleted by foreign key on delete cascade
	return true;
}

void AuthDatabaseMySQL::listNames(std::vector<std::string> &res)
{
	verifyDatabase();

	MYSQL_RES* results = execQueryWithResult("SELECT name FROM auth ORDER BY name DESC");

	int numrows = mysql_num_rows(results);

	for (int i = 0; i < numrows; ++i) {
		MYSQL_ROW row = mysql_fetch_row(results);
		res.emplace_back(row[0]);
	}

	mysql_free_result(results);
}

void AuthDatabaseMySQL::reload()
{
	// noop for MySQL
}


void AuthDatabaseMySQL::writePrivileges(const AuthEntry &authEntry)
{
	std::string authIdStr = std::to_string(authEntry.id);
	std::vector<std::string> values = { authIdStr };
	execWithParam("DELETE FROM user_privileges WHERE id = $1", values);

	for (const std::string& privilege : authEntry.privileges) {
		std::vector<std::string> values = { authIdStr, privilege };
		execWithParam("INSERT INTO user_privileges (id, privilege) VALUES ($1, $2)", values);
	}
}

/*
 * Mod Database
 */
ModStorageDatabaseMySQL::ModStorageDatabaseMySQL(const std::string &connect_string):
	Database_MySQL(connect_string, "mod_storage"),
	ModStorageDatabase()
{
	connectToDatabase();
}

void ModStorageDatabaseMySQL::createDatabase()
{
	createTableIfNotExists("mod_storage",
		"modname TEXT NOT NULL,"
		"id BLOB NOT NULL,"
		"value BLOB NOT NULL,"
		"PRIMARY KEY (modname(255), id(255))");
		//"ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

	infostream << "MySQL: Mod Storage Database was initialized." << std::endl;
}

void ModStorageDatabaseMySQL::getModEntries(const std::string &modname, StringMap *storage)
{
	verifyDatabase();

	std::vector<std::string> args = { modname };
	MYSQL_RES* results = execWithParamAndResult("SELECT id, value FROM mod_storage WHERE modname = $1", args);

	int numrows = mysql_num_rows(results);

	for (int row = 0; row < numrows; ++row)
		(*storage)[mysql_to_string(results, row, 0)] = mysql_to_string(results, row, 1);

	mysql_free_result(results);
}

void ModStorageDatabaseMySQL::getModKeys(const std::string &modname,
		std::vector<std::string> *storage)
{
	verifyDatabase();

	std::vector<std::string> args = { modname };
	MYSQL_RES* results = execWithParamAndResult("SELECT id FROM mod_storage WHERE modname = $1", args);

	int numrows = mysql_num_rows(results);

	storage->reserve(storage->size() + numrows);
	for (int row = 0; row < numrows; ++row)
		storage->push_back(mysql_to_string(results, row, 0));

	mysql_free_result(results);
}

bool ModStorageDatabaseMySQL::getModEntry(const std::string &modname,
	const std::string &key, std::string *value)
{
	verifyDatabase();

	std::vector<std::string> args = { modname, key };
	MYSQL_RES* results = execWithParamAndResult("SELECT value FROM mod_storage WHERE modname = $1 AND id = $2", args);

	int numrows = mysql_num_rows(results);
	bool found = numrows > 0;

	if (found)
		*value = mysql_to_string(results, 0, 0);

	mysql_free_result(results);

	return found;
}

bool ModStorageDatabaseMySQL::hasModEntry(const std::string &modname,
		const std::string &key)
{
	verifyDatabase();

	std::vector<std::string> args = { modname, key };
	MYSQL_RES* results = execWithParamAndResult("SELECT 1 FROM mod_storage WHERE modname = $1 AND id = $2", args);

	int numrows = mysql_num_rows(results);
	bool found = numrows > 0;

	mysql_free_result(results);

	return found;
}

bool ModStorageDatabaseMySQL::setModEntry(const std::string &modname,
	const std::string& key, std::string_view value)
{
	verifyDatabase();

	std::vector<std::string> args = { modname, key, value.data() };
	execWithParam("INSERT INTO mod_storage (modname, id, value) VALUES ($1, $2, $3) "
		"ON DUPLICATE KEY UPDATE value = $3", args);

	return true;
}

bool ModStorageDatabaseMySQL::removeModEntry(const std::string &modname,
		const std::string &key)
{
	verifyDatabase();

	std::vector<std::string> args = { modname, key };
	MYSQL_RES* results = execWithParamAndResult("DELETE FROM mod_storage WHERE modname = $1 AND id = $2", args);

	int affected = mysql_affected_rows(m_conn);

	mysql_free_result(results);

	return affected > 0;
}

bool ModStorageDatabaseMySQL::removeModEntries(const std::string &modname)
{
	verifyDatabase();

	std::vector<std::string> args = { modname };
	MYSQL_RES* results = execWithParamAndResult("DELETE FROM mod_storage WHERE modname = $1", args);

	int affected = mysql_affected_rows(m_conn);

	mysql_free_result(results);

	return affected > 0;
}

void ModStorageDatabaseMySQL::listMods(std::vector<std::string> *res)
{
	verifyDatabase();

	MYSQL_RES* results = execQueryWithResult("SELECT DISTINCT modname FROM mod_storage");

	int numrows = mysql_num_rows(results);

	for (int row = 0; row < numrows; ++row)
		res->push_back(mysql_to_string(results, row, 0));

	mysql_free_result(results);
}
