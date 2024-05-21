#pragma once

#include "lua_api/l_base.h"
#include <memory>

class NetworkPacket;
class NetworkStreamPacket;

class LuaNetworkChannel : public ModApiBase {
private:
	std::string channel_name;

	LuaNetworkChannel(std::string channel_name);

	static const luaL_Reg methods[];

	// garbage collector
	static int gc_object(lua_State* L);

	// equals(self, other) -> bool
	static int l_equals(lua_State* L);

	// __tostring metamethod
	static int mt_tostring(lua_State* L);

	static int l_to_string(lua_State* L);

	static int l_createNetworkPacket(lua_State* L);
	static int l_createNetworkStreamPacket(lua_State* L);

public:
	static int create_object(lua_State* L);
	static void Register(lua_State* L);

	static const char className[];
};

class ModApiNetworkChannel : public ModApiBase {
private:
	static int l_NetworkChannel(lua_State* L);

public:
	static void Initialize(lua_State* L, int top);
};

class LuaNetworkPacket : public ModApiBase {
private:
	LuaNetworkPacket(NetworkPacket* packet);

	static const luaL_Reg methods[];

	// garbage collector
	static int gc_object(lua_State* L);

	// equals(self, other) -> bool
	static int l_equals(lua_State* L);

	// __tostring metamethod
	static int mt_tostring(lua_State* L);

	static int l_to_string(lua_State* L);

	static int l_get_peer_id(lua_State* L);
	static int l_write_string(lua_State* L);
	static int l_write_int(lua_State* L);
	static int l_write_u8(lua_State* L);
	static int l_write_v3f(lua_State* L);
	static int l_read_string(lua_State* L);
	static int l_read_int(lua_State* L);
	static int l_read_u8(lua_State* L);
	static int l_read_v3f(lua_State* L);
	static int l_read_remaining_string(lua_State* L);
	static int l_read_remaining_buffer(lua_State* L);

public:
	//DISABLE_CLASS_COPY(LuaNetworkPacket);

	std::shared_ptr<NetworkPacket> packet;

	static int create_object(lua_State* L);
	static int create_object(lua_State* L, NetworkPacket *packet);
	static void push_object(lua_State* L, LuaNetworkPacket* packet);

	static void Register(lua_State* L);

	static const char className[];
};


class LuaNetworkStreamPacket : public ModApiBase {
private:
	LuaNetworkStreamPacket(NetworkStreamPacket* packet);

	static const luaL_Reg methods[];

	// garbage collector
	static int gc_object(lua_State* L);

	// equals(self, other) -> bool
	static int l_equals(lua_State* L);

	// __tostring metamethod
	static int mt_tostring(lua_State* L);

	static int l_to_string(lua_State* L);

	static int l_flush(lua_State* L);
	static int l_write_string(lua_State* L);
	static int l_write_int(lua_State* L);
	static int l_write_v3f(lua_State* L);
	static int l_write_packet(lua_State* L);

	static int l_get_id(lua_State* L);

public:
	//DISABLE_CLASS_COPY(LuaNetworkPacket);

	std::shared_ptr<NetworkStreamPacket> packet;

	static int create_object(lua_State* L);
	static int create_object(lua_State* L, NetworkStreamPacket*packet);
	static void push_object(lua_State* L, LuaNetworkStreamPacket* packet);

	static void Register(lua_State* L);

	static const char className[];
};
