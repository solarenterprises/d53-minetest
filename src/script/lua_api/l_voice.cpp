#include "l_voice.h"
#include "lua_api/l_internal.h"
#include "../audiorw/audiorw.hpp"
#include "l_network_packet.h"
#include "../util/stream.h"
#include "../network/stream_packet.h"
#include "common/c_converter.h"

// garbage collector
int LuaVoice::gc_object(lua_State* L)
{
	LuaVoice* o = checkObject<LuaVoice>(L, 1);
	delete o->voice;
	o->voice = nullptr;
	delete o;
	return 0;
}

// __tostring metamethod
int LuaVoice::mt_tostring(lua_State* L)
{
	LuaVoice* o = checkObject<LuaVoice>(L, 1);
	std::string str = "Voice ";
	if (o->voice)
		str += "(" + o->voice->getCurrentDeviceName() + ")";
	lua_pushstring(L, str.c_str());
	return 1;
}

// to_string(self) -> string
int LuaVoice::l_to_string(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaVoice* o = checkObject<LuaVoice>(L, 1);
	std::string str = "Voice ";
	if (o->voice)
		str += "(" + o->voice->getCurrentDeviceName() + ")";
	lua_pushstring(L, str.c_str());
	return 1;
}

int LuaVoice::create_object(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	int sample_rate = 22050;
	int channels = 1;
	if (lua_istable(L, 1)) {
		sample_rate = getintfield_default(L, 1, "sample_rate", 44100);
		channels = getintfield_default(L, 1, "num_channels", 1);
	}

	LuaVoice* o = new LuaVoice();
	*(void**)(lua_newuserdata(L, sizeof(void*))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	o->voice = new Voice(sample_rate, channels);

	return 1;
}

void LuaVoice::Register(lua_State* L)
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
int LuaVoice::l_equals(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaVoice* o1 = checkObject<LuaVoice>(L, 1);

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

	LuaVoice* o2 = checkObject<LuaVoice>(L, 2);

	lua_pushboolean(L, o1 == o2);
	return 1;
}

int LuaVoice::l_start(lua_State* L) {
	NO_MAP_LOCK_REQUIRED;
	LuaVoice* o = checkObject<LuaVoice>(L, 1);

	if (lua_isuserdata(L, 2) && lua_getmetatable(L, 2)) {
		lua_getfield(L, LUA_REGISTRYINDEX, LuaNetworkStreamPacket::className);
		if (lua_rawequal(L, -1, -2)) {
			LuaNetworkStreamPacket* lua_stream = checkObject<LuaNetworkStreamPacket>(L, 2);
			auto ptr = std::dynamic_pointer_cast<Stream>(lua_stream->packet);
			o->voice->start(ptr);
			return 0;
		}
	}

	std::shared_ptr<Stream> ptr((Stream*)new audiorw::OGGWriteFile(audiorw::OGGWriteStream::SHORT, "./test-output.ogg", o->voice->m_sampleRate, o->voice->m_numChannels));
	o->voice->start(ptr);
	return 0;
}

int LuaVoice::l_stop(lua_State* L) {
	NO_MAP_LOCK_REQUIRED;
	LuaVoice* o = checkObject<LuaVoice>(L, 1);
	o->voice->stop();
	return 0;
}

int LuaVoice::l_setInputDevice(lua_State* L) {
	NO_MAP_LOCK_REQUIRED;
	LuaVoice* o = checkObject<LuaVoice>(L, 1);
	std::string deviceName = readParam<std::string>(L, 2);
	o->voice->setInputDevice(deviceName);
	return 0;
}

int LuaVoice::l_isRunning(lua_State* L) {
	NO_MAP_LOCK_REQUIRED;
	LuaVoice* o = checkObject<LuaVoice>(L, 1);
	lua_pushboolean(L, o->voice->getIsRunning());
	return 1;
}

const char LuaVoice::className[] = "Voice";
const luaL_Reg LuaVoice::methods[] = {
	luamethod(LuaVoice, to_string),
	luamethod(LuaVoice, equals),
	luamethod(LuaVoice, start),
	luamethod(LuaVoice, stop),
	luamethod(LuaVoice, setInputDevice),
	luamethod(LuaVoice, isRunning),
	{0,0}
};

int ModApiVoice::l_Voice(lua_State* L) {
	LuaVoice::create_object(L);
	return 1;
}

int ModApiVoice::l_getInputDevices(lua_State* L) {
	std::vector<std::string> devices = Voice::getInputDevices();

	lua_createtable(L, devices.size(), 0);
	for (auto name : devices)
		lua_pushlstring(L, name.c_str(), name.size());

	return 1;
}

void ModApiVoice::Initialize(lua_State* L, int top)
{
	API_FCT(getInputDevices);
	API_FCT(Voice);
}
