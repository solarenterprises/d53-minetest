#pragma once

#include "lua_api/l_base.h"
#include "../../audiorw/audiorw.hpp"

class NetworkPacket;

class LuaOGGWriteStream : public ModApiBase {
private:
	audiorw::OGGWriteStream stream;

	LuaOGGWriteStream(int sample_rate, int num_channels);

	static const luaL_Reg methods[];

	// garbage collector
	static int gc_object(lua_State* L);

	// equals(self, other) -> bool
	static int l_equals(lua_State* L);

	// __tostring metamethod
	static int mt_tostring(lua_State* L);

	static int l_to_string(lua_State* L);

	static int l_write(lua_State* L);
	static int l_get_buffer(lua_State* L);
	static int l_get_size(lua_State* L);

public:
	//DISABLE_CLASS_COPY(LuaOGGWriteStream)

	static int create_object(lua_State* L);

	static void Register(lua_State* L);

	static const char className[];
};


class ModApiOGG : public ModApiBase
{
private:
	// sound_create_ogg({ buffer, sample_rate, channels})
	static int l_sound_create_ogg(lua_State* L);

	// sound_convert_to_ogg({buffer, type})
	static int l_sound_convert_to_ogg(lua_State* L);


	static int l_OGGWriteStream(lua_State* L);

public:
	static void Initialize(lua_State* L, int top);
};
