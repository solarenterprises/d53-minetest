#pragma once

#include "lua_api/l_base.h"

class LuaBuffer : public ModApiBase {
private:
	LuaBuffer() = default;

	static const luaL_Reg methods[];

	// garbage collector
	static int gc_object(lua_State* L);

	// equals(self, other) -> bool
	static int l_equals(lua_State* L);

	// __tostring metamethod
	static int mt_tostring(lua_State* L);

	static int l_to_string(lua_State* L);
	static int l_get_size(lua_State* L);

public:
	std::string buffer;

	static int create_object(lua_State* L);
	static int create_object(lua_State* L, std::string& buffer);

	static void Register(lua_State* L);

	static const char className[];
};

class ModApiBuffer : public ModApiBase {
private:
	static int l_Buffer(lua_State* L);

public:
	static void Initialize(lua_State* L, int top);
};
