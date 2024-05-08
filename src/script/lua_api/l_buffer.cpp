#include "l_buffer.h"
#include "lua_api/l_internal.h"
#include "common/c_converter.h"

// garbage collector
int LuaBuffer::gc_object(lua_State* L)
{
	LuaBuffer* o = checkObject<LuaBuffer>(L, 1);
	return 0;
}

// __tostring metamethod
int LuaBuffer::mt_tostring(lua_State* L)
{
	LuaBuffer* o = checkObject<LuaBuffer>(L, 1);
	lua_pushlstring(L, o->buffer.c_str(), o->buffer.size());
	return 1;
}

// to_string(self) -> string
int LuaBuffer::l_to_string(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaBuffer* o = checkObject<LuaBuffer>(L, 1);
	lua_pushlstring(L, o->buffer.c_str(), o->buffer.size());
	return 1;
}

// to_string(self) -> int
int LuaBuffer::l_get_size(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaBuffer* o = checkObject<LuaBuffer>(L, 1);
	lua_pushinteger(L, o->buffer.size());
	return 1;
}

int LuaBuffer::create_object(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaBuffer* o = new LuaBuffer();
	*(void**)(lua_newuserdata(L, sizeof(void*))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);

	return 1;
}

int LuaBuffer::create_object(lua_State* L, std::string& buffer)
{
	NO_MAP_LOCK_REQUIRED;

	LuaBuffer* o = new LuaBuffer();
	o->buffer = std::move(buffer);

	*(void**)(lua_newuserdata(L, sizeof(void*))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);

	return 1;
}

void LuaBuffer::Register(lua_State* L)
{
	static const luaL_Reg metamethods[] = {
		{"__tostring", mt_tostring},
		{"__gc", gc_object},
		{"__eq", l_equals},
		{0, 0}
	};
	registerClass(L, className, methods, metamethods);

	// Can be created from Lua (LuaVoice(LuaVoice or itemstring or table or nil))
	lua_register(L, className, create_object);
}

// equals(self, other) -> bool
int LuaBuffer::l_equals(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaBuffer* o1 = checkObject<LuaBuffer>(L, 1);

	// checks for non-userdata argument
	if (!lua_isuserdata(L, 2)) {
		lua_pushboolean(L, false);
		return 1;
	}

	// check that the argument is an LuaVoice
	if (!lua_getmetatable(L, 2)) {
		lua_pushboolean(L, false);
		return 1;
	}
	lua_getfield(L, LUA_REGISTRYINDEX, className);
	if (!lua_rawequal(L, -1, -2)) {
		lua_pushboolean(L, false);
		return 1;
	}

	LuaBuffer* o2 = checkObject<LuaBuffer>(L, 2);

	lua_pushboolean(L, o1 == o2);
	return 1;
}

const char LuaBuffer::className[] = "Buffer";
const luaL_Reg LuaBuffer::methods[] = {
	luamethod(LuaBuffer, to_string),
	luamethod(LuaBuffer, equals),
	luamethod(LuaBuffer, get_size),
	{0,0}
};

int ModApiBuffer::l_Buffer(lua_State* L) {
	LuaBuffer::create_object(L);
	return 1;
}

void ModApiBuffer::Initialize(lua_State* L, int top)
{
	API_FCT(Buffer);
}
