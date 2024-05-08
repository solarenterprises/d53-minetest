#pragma once

#include "lua_api/l_base.h"
#include "../client/voice.h"

class LuaVoice : public ModApiBase {
private:
	Voice* voice = nullptr;

	LuaVoice() = default;

	static const luaL_Reg methods[];

	// garbage collector
	static int gc_object(lua_State* L);

	// equals(self, other) -> bool
	static int l_equals(lua_State* L);

	// __tostring metamethod
	static int mt_tostring(lua_State* L);

	static int l_to_string(lua_State* L);

	static int l_start(lua_State* L);
	static int l_stop(lua_State* L);
	static int l_setInputDevice(lua_State* L);
	static int l_isRunning(lua_State* L);

public:
	DISABLE_CLASS_COPY(LuaVoice)

	static int create_object(lua_State* L);

	static void Register(lua_State* L);

	static const char className[];
};

class ModApiVoice : public ModApiBase {
private:
	static int l_Voice(lua_State* L);
	static int l_getInputDevices(lua_State* L);

public:
	static void Initialize(lua_State* L, int top);
};
