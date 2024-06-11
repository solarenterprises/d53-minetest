#include "l_ogg.h"
#include "lua_api/l_internal.h"
#include "common/c_converter.h"
#include "l_network_packet.h"
#include "../network/networkpacket.h"
#include "l_buffer.h"

LuaOGGWriteStream::LuaOGGWriteStream(int sample_rate, int num_channels) : stream(audiorw::OGGWriteStream::SHORT, sample_rate, num_channels, false){
}

// garbage collector
int LuaOGGWriteStream::gc_object(lua_State* L)
{
	LuaOGGWriteStream* o = checkObject<LuaOGGWriteStream>(L, 1);
	delete o;
	return 0;
}

// __tostring metamethod
int LuaOGGWriteStream::mt_tostring(lua_State* L)
{
	lua_pushstring(L, "OGG Write Stream");
	return 1;
}

// to_string(self) -> string
int LuaOGGWriteStream::l_to_string(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	lua_pushstring(L, "OGG Write Stream");
	return 1;
}

int LuaOGGWriteStream::l_write(lua_State* L) {
	LuaOGGWriteStream* o = checkObject<LuaOGGWriteStream>(L, 1);

	// checks for non-userdata argument
	if (!lua_isuserdata(L, 2)) {
		lua_pushboolean(L, false);
		return 1;
	}

	// check that the argument is an ItemStack
	if (!lua_getmetatable(L, 2)) {
		lua_pushboolean(L, false);
		return 1;
	}

	lua_getfield(L, LUA_REGISTRYINDEX, LuaNetworkPacket::className);
	if (lua_rawequal(L, -1, -2)) {
		LuaNetworkPacket* lua_pkt = checkObject<LuaNetworkPacket>(L, 2);
		o->stream.write(lua_pkt->packet->getRemainingString(), lua_pkt->packet->getRemainingBytes());
		lua_pushboolean(L, true);
		return 1;
	}
	
	lua_pushboolean(L, false);
	return 1;
}

int LuaOGGWriteStream::l_get_buffer(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaOGGWriteStream* o = checkObject<LuaOGGWriteStream>(L, 1);

	std::string data = o->stream.data();
 	LuaBuffer::create_object(L, data);
	return 1;
}

int LuaOGGWriteStream::l_get_size(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaOGGWriteStream* o = checkObject<LuaOGGWriteStream>(L, 1);

	size_t size = o->stream.size();
	lua_pushinteger(L, size);
	return 1;
}

int LuaOGGWriteStream::create_object(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	int sample_rate = 22050;
	int channels = 1;
	if (lua_istable(L, 1)) {
		sample_rate = getintfield_default(L, 1, "sample_rate", 44100);
		channels = getintfield_default(L, 1, "num_channels", 1);
	}

	LuaOGGWriteStream* o = new LuaOGGWriteStream(sample_rate, channels);
	*(void**)(lua_newuserdata(L, sizeof(void*))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	
	return 1;
}

void LuaOGGWriteStream::Register(lua_State* L)
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
int LuaOGGWriteStream::l_equals(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaOGGWriteStream* o1 = checkObject<LuaOGGWriteStream>(L, 1);

	// checks for non-userdata argument
	if (!lua_isuserdata(L, 2)) {
		lua_pushboolean(L, false);
		return 1;
	}

	// check that the argument is an LuaOGGWriteStream
	if (!lua_getmetatable(L, 2)) {
		lua_pushboolean(L, false);
		return 1;
	}
	lua_getfield(L, LUA_REGISTRYINDEX, className);
	if (!lua_rawequal(L, -1, -2)) {
		lua_pushboolean(L, false);
		return 1;
	}

	LuaOGGWriteStream* o2 = checkObject<LuaOGGWriteStream>(L, 2);

	lua_pushboolean(L, o1 == o2);
	return 1;
}

const char LuaOGGWriteStream::className[] = "OGGWriteStream";
const luaL_Reg LuaOGGWriteStream::methods[] = {
	luamethod(LuaOGGWriteStream, to_string),
	luamethod(LuaOGGWriteStream, equals),
	luamethod(LuaOGGWriteStream, write),
	luamethod(LuaOGGWriteStream, get_buffer),
	luamethod(LuaOGGWriteStream, get_size),
	{0,0}
};

//
// ModAPI
//

// sound_create_ogg({ buffer, sample_rate, channels})
int ModApiOGG::l_sound_create_ogg(lua_State* L)
{
	if (lua_isnil(L, 1) || !lua_istable(L, 1))
		throw LuaError("first argument needs to be table");

	std::string buffer;
	getstringfield(L, 1, "buffer", buffer);

	if (buffer.length() % sizeof(short) != 0)
		throw "buffer only accepts shorts";

	int sample_rate = getintfield_default(L, 1, "sample_rate", 44100);
	int channels = getintfield_default(L, 1, "num_channels", 1);

	std::vector<std::vector<short>> data;
	data.push_back(std::vector<short>());
	std::vector<short>& audio_buffer = data[0];
	audio_buffer.resize(buffer.length() / sizeof(short));
	memcpy(audio_buffer.data(), buffer.c_str(), buffer.size());

	std::string ogg_buffer;
	audiorw::write_ogg(data, sample_rate, ogg_buffer);

	if (ogg_buffer.empty()) {
		lua_pushnil(L);
		lua_pushstring(L, "Failed to create ogg");
		return 2; // Returning 2 values, nil and an error message
	}

	// Push the data to Lua
	lua_pushlstring(L, ogg_buffer.data(), ogg_buffer.size());
	return 1;
}


int ModApiOGG::l_OGGWriteStream(lua_State* L) {
	LuaOGGWriteStream::create_object(L);
	return 1;
}

// convert_to_ogg({buffer, type})
int ModApiOGG::l_sound_convert_to_ogg(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string buffer;
	std::string type;
	if (lua_isnil(L, 1) || !lua_istable(L, 1))
		throw LuaError("first argument needs to be table");

	getstringfield(L, 1, "buffer", buffer);
	getstringfield(L, 1, "type", type);

	if (buffer.empty()) {
		lua_pushnil(L);
		return 1;
	}

	std::ofstream file("./_tmpconvert.wav", std::ios::binary);
	if (!file.is_open()) {
		errorstream << "Failed to open './_tmpconvert.wav'" << std::endl;
		lua_pushnil(L);
		return 1;
	}

	file << buffer;
	file.close();

	double sample_rate;
	std::vector<std::vector<double>> data = audiorw::read("./_tmpconvert.wav", sample_rate);

	if (data.empty()) {
		errorstream << "Failed to read './_tmpconvert.wav'" << std::endl;
		lua_pushnil(L);
		return 1;
	}

	if (data.size() == 2) {
		std::vector<double> monoData;
		monoData.reserve(data[0].size());

		// Convert stereo to mono by averaging the left and right channels
		for (size_t i = 0; i < data[0].size(); ++i) {
			double leftSample = data[0][i];
			double rightSample = data[1][i];
			double monoSample = (leftSample + rightSample) / 2.0;
			monoData.push_back(monoSample);
		}

		data = { monoData };
	}

	std::string ogg_buffer;
	audiorw::write_ogg(data, sample_rate, ogg_buffer);

	if (ogg_buffer.empty()) {
		lua_pushnil(L);
		lua_pushstring(L, "Failed to read file");
		return 2; // Returning 2 values, nil and an error message
	}

	// Push the data to Lua
	LuaBuffer::create_object(L, ogg_buffer);
	return 1;
}

void ModApiOGG::Initialize(lua_State* L, int top) {
	API_FCT(sound_create_ogg);
	API_FCT(sound_convert_to_ogg);
	API_FCT(OGGWriteStream);
}
