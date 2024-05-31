#include "l_network_packet.h"
#include "lua_api/l_internal.h"
#include "cpp_api/s_base.h"
#include "../common/c_converter.h"
#include "l_buffer.h"

#include "../network/networkpacket.h"
#include "../network/stream_packet.h"

//
// Lua Network Packet
//

LuaNetworkChannel::LuaNetworkChannel(std::string _channel_name) : channel_name(_channel_name) {
}

// garbage collector
int LuaNetworkChannel::gc_object(lua_State* L)
{
	LuaNetworkChannel* o = checkObject<LuaNetworkChannel>(L, 1);
	delete o;
	return 0;
}

// __tostring metamethod
int LuaNetworkChannel::mt_tostring(lua_State* L)
{
	LuaNetworkChannel* o = checkObject<LuaNetworkChannel>(L, 1);
	lua_pushstring(L, "network channel");
	return 1;
}

// to_string(self) -> string
int LuaNetworkChannel::l_to_string(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaNetworkChannel* o = checkObject<LuaNetworkChannel>(L, 1);
	lua_pushstring(L, "network channel");
	return 1;
}

int LuaNetworkChannel::create_object(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string current_mod_name = ScriptApiBase::getCurrentModNameInsecure(L);
	LuaNetworkChannel* o = new LuaNetworkChannel(current_mod_name);
	*(void**)(lua_newuserdata(L, sizeof(void*))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);

	return 1;
}

void LuaNetworkChannel::Register(lua_State* L)
{
	static const luaL_Reg metamethods[] = {
		{"__tostring", mt_tostring},
		{"__gc", gc_object},
		{"__eq", l_equals},
		{0, 0}
	};
	registerClass(L, className, methods, metamethods);

	// Can be created from Lua (ItemStack(itemstack or itemstring or table or nil))
	lua_register(L, className, create_object);
}

// equals(self, other) -> bool
int LuaNetworkChannel::l_equals(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaNetworkChannel* o1 = checkObject<LuaNetworkChannel>(L, 1);

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
	lua_getfield(L, LUA_REGISTRYINDEX, className);
	if (!lua_rawequal(L, -1, -2)) {
		lua_pushboolean(L, false);
		return 1;
	}

	LuaNetworkChannel* o2 = checkObject<LuaNetworkChannel>(L, 2);

	lua_pushboolean(L, o1 == o2);
	return 1;
}

int LuaNetworkChannel::l_createNetworkPacket(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkChannel* o = checkObject<LuaNetworkChannel>(L, 1);

	LuaNetworkPacket::create_object(L);
	LuaNetworkPacket* pkt = checkObject<LuaNetworkPacket>(L, -1);
	(*pkt->packet) << o->channel_name;
	
	return 1;
}

int LuaNetworkChannel::l_createNetworkStreamPacket(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkChannel* o = checkObject<LuaNetworkChannel>(L, 1);

	LuaNetworkStreamPacket::create_object(L);
	LuaNetworkStreamPacket* pkt = checkObject<LuaNetworkStreamPacket>(L, -1);
	pkt->packet->init(o->channel_name.c_str(), o->channel_name.length());
	
	return 1;
}


const char LuaNetworkChannel::className[] = "NetworkChannel";
const luaL_Reg LuaNetworkChannel::methods[] = {
	luamethod(LuaNetworkChannel, to_string),
	luamethod(LuaNetworkChannel, equals),
	luamethod(LuaNetworkChannel, createNetworkPacket),
	luamethod(LuaNetworkChannel, createNetworkStreamPacket),
	{0,0}
};

int ModApiNetworkChannel::l_NetworkChannel(lua_State* L) {
	LuaNetworkChannel::create_object(L);
	return 1;
}

void ModApiNetworkChannel::Initialize(lua_State* L, int top)
{
	API_FCT(NetworkChannel);
}


//
// Lua Network Packet
//
LuaNetworkPacket::LuaNetworkPacket(NetworkPacket* packet) {
	this->packet = std::shared_ptr<NetworkPacket>(packet);
}
 
// garbage collector
int LuaNetworkPacket::gc_object(lua_State* L)
{
	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);
	delete o;
	return 0;
}

// __tostring metamethod
int LuaNetworkPacket::mt_tostring(lua_State* L)
{
	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);

	auto size = o->packet->getSize();
	lua_pushlstring(L, o->packet->getString(0), size);
	return 1;
}

// to_string(self) -> string
int LuaNetworkPacket::l_to_string(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);
	auto size = o->packet->getSize();
	lua_pushlstring(L, o->packet->getString(0), size);
	return 1;
}

int LuaNetworkPacket::l_write_string(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);
	std::string str = readParam<std::string>(L, 2);
	(*o->packet) << str;
	return 0;
}

int LuaNetworkPacket::l_write_int(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);
	int i = readParam<int>(L, 2);
	(*o->packet) << i;
	return 0;
}

int LuaNetworkPacket::l_write_u8(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);
	u8 i = readParam<int>(L, 2);
	(*o->packet) << i;
	return 0;
}

int LuaNetworkPacket::l_write_v3f(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);
	v3f v = read_v3f(L, 2);
	(*o->packet) << v;
	return 0;
}

int LuaNetworkPacket::l_get_peer_id(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);
	lua_pushinteger(L, o->packet->getPeerId());
	return 1;
}

int LuaNetworkPacket::l_read_string(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);
	std::string str;
	(*o->packet) >> str;
	lua_pushlstring(L, str.c_str(), str.size());
	return 1;
}

int LuaNetworkPacket::l_read_int(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);
	int i;
	(*o->packet) >> i;
	lua_pushinteger(L, i);
	return 1;
}

int LuaNetworkPacket::l_read_u8(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);
	u8 i;
	(*o->packet) >> i;
	lua_pushinteger(L, (int)i);
	return 1;
}

int LuaNetworkPacket::l_read_v3f(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);
	v3f p;
	(*o->packet) >> p;
	push_v3f(L, p);
	return 1;
}

int LuaNetworkPacket::l_read_remaining_string(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);
	lua_pushlstring(L, o->packet->getRemainingString(), o->packet->getRemainingBytes());
	return 1;
}

int LuaNetworkPacket::l_read_remaining_buffer(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkPacket* o = checkObject<LuaNetworkPacket>(L, 1);
	std::string buffer(o->packet->getRemainingString(), o->packet->getRemainingBytes());
	LuaBuffer::create_object(L, buffer);
	return 1;
}

int LuaNetworkPacket::create_object(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	u16 command = 0;
	if (getClient(L))
		command = TOSERVER_LUA_PACKET;
	else
		command = TOCLIENT_LUA_PACKET;

	LuaNetworkPacket* o = new LuaNetworkPacket(new NetworkPacket(command, 0));
	*(void**)(lua_newuserdata(L, sizeof(void*))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);

	return 1;
}

int LuaNetworkPacket::create_object(lua_State* L, NetworkPacket* packet)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkPacket* o = new LuaNetworkPacket(packet);
	*(void**)(lua_newuserdata(L, sizeof(void*))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);

	return 1;
}

void LuaNetworkPacket::push_object(lua_State* L, LuaNetworkPacket* packet)
{
	*(void**)(lua_newuserdata(L, sizeof(void*))) = packet;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
}

void LuaNetworkPacket::Register(lua_State* L)
{
	static const luaL_Reg metamethods[] = {
		{"__tostring", mt_tostring},
		{"__gc", gc_object},
		{"__eq", l_equals},
		{0, 0}
	};
	registerClass(L, className, methods, metamethods);

	// Can be created from Lua (ItemStack(itemstack or itemstring or table or nil))
	lua_register(L, className, create_object);
}

// equals(self, other) -> bool
int LuaNetworkPacket::l_equals(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaNetworkPacket* o1 = checkObject<LuaNetworkPacket>(L, 1);

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
	lua_getfield(L, LUA_REGISTRYINDEX, className);
	if (!lua_rawequal(L, -1, -2)) {
		lua_pushboolean(L, false);
		return 1;
	}

	LuaNetworkPacket* o2 = checkObject<LuaNetworkPacket>(L, 2);

	lua_pushboolean(L, o1 == o2);
	return 1;
}


const char LuaNetworkPacket::className[] = "NetworkPacket";
const luaL_Reg LuaNetworkPacket::methods[] = {
	luamethod(LuaNetworkPacket, to_string),
	luamethod(LuaNetworkPacket, equals),
	luamethod(LuaNetworkPacket, write_string),
	luamethod(LuaNetworkPacket, write_int),
	luamethod(LuaNetworkPacket, write_u8),
	luamethod(LuaNetworkPacket, write_v3f),
	luamethod(LuaNetworkPacket, read_string),
	luamethod(LuaNetworkPacket, read_int),
	luamethod(LuaNetworkPacket, read_v3f),
	luamethod(LuaNetworkPacket, read_u8),
	luamethod(LuaNetworkPacket, get_peer_id),
	luamethod(LuaNetworkPacket, read_remaining_string),
	luamethod(LuaNetworkPacket, read_remaining_buffer),
	{0,0}
};


//
// Lua Network Stream Packet
//
LuaNetworkStreamPacket::LuaNetworkStreamPacket(NetworkStreamPacket* packet) {
	this->packet = std::shared_ptr<NetworkStreamPacket>(packet);
}

// garbage collector
int LuaNetworkStreamPacket::gc_object(lua_State* L)
{
	LuaNetworkStreamPacket* o = checkObject<LuaNetworkStreamPacket>(L, 1);
	delete o;
	return 0;
}

// __tostring metamethod
int LuaNetworkStreamPacket::mt_tostring(lua_State* L)
{
	LuaNetworkStreamPacket* o = checkObject<LuaNetworkStreamPacket>(L, 1);
	lua_pushstring(L, "Stream packet");
	return 1;
}

// to_string(self) -> string
int LuaNetworkStreamPacket::l_to_string(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkStreamPacket* o = checkObject<LuaNetworkStreamPacket>(L, 1);
	lua_pushstring(L, "Stream packet");
	return 1;
}

int LuaNetworkStreamPacket::l_flush(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkStreamPacket* o = checkObject<LuaNetworkStreamPacket>(L, 1);
	o->packet->flush();

	return 0;
}

int LuaNetworkStreamPacket::l_get_id(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkStreamPacket* o = checkObject<LuaNetworkStreamPacket>(L, 1);
	lua_pushinteger(L, o->packet->get_id());
	return 1;
}

int LuaNetworkStreamPacket::l_write_string(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkStreamPacket* o = checkObject<LuaNetworkStreamPacket>(L, 1);
	std::string str = readParam<std::string>(L, 2);
	(*o->packet) << str;
	return 0;
}

int LuaNetworkStreamPacket::l_write_int(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkStreamPacket* o = checkObject<LuaNetworkStreamPacket>(L, 1);
	int i = readParam<int>(L, 2);
	(*o->packet) << i;
	return 0;
}

int LuaNetworkStreamPacket::l_write_v3f(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkStreamPacket* o = checkObject<LuaNetworkStreamPacket>(L, 1);
	v3f v = read_v3f(L, 2);
	(*o->packet) << v;
	return 0;
}

int LuaNetworkStreamPacket::l_write_packet(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkStreamPacket* o = checkObject<LuaNetworkStreamPacket>(L, 1);
	LuaNetworkPacket* lua_pkt = checkObject<LuaNetworkPacket>(L, 2);

	o->packet->write(lua_pkt->packet->getRemainingString(), lua_pkt->packet->getRemainingBytes());

	return 0;
}

int LuaNetworkStreamPacket::create_object(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkStreamPacket* o;
	if (getClient(L)) {
		size_t max_packet_size = 1024;

		if (lua_isnumber(L, 2)) {
			max_packet_size = lua_tointeger(L, 2);
			max_packet_size = std::max(size_t(1), max_packet_size);
		}

		o = new LuaNetworkStreamPacket(new StreamPacketClient(getClient(L), TOSERVER_LUA_PACKET_STREAM, max_packet_size));
	} else {
		int arg_offset = 1;
		if (!lua_istable(L, arg_offset+1))
			throw "NetworkStreamPacket arg(1) needs to be peer array";

		// Get the length of the table (number of elements)
		int tableLength = lua_objlen(L, arg_offset + 1);

		std::vector<session_t> to_peers;
		to_peers.reserve(tableLength);

		for (int i = 1; i <= tableLength; i++) {
			lua_rawgeti(L, arg_offset + 1, i);

			// Check if the value is an integer
			if (lua_type(L, -1) != LUA_TNUMBER)
				throw "peer ID must be an integer";

			// Retrieve the peer ID
			to_peers.push_back(lua_tointeger(L, -1));
			lua_pop(L, 1); // Pop the peer ID from the stack
		}

		size_t max_packet_size = 1024;

		if (lua_isnumber(L, arg_offset + 2)) {
			max_packet_size = lua_tointeger(L, arg_offset + 2);
			max_packet_size = std::max(size_t(1), max_packet_size);
		}

		o = new LuaNetworkStreamPacket(new StreamPacketServer(getServer(L), to_peers, TOCLIENT_LUA_PACKET_STREAM, max_packet_size));
	}

	*(void**)(lua_newuserdata(L, sizeof(void*))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);

	return 1;
}

int LuaNetworkStreamPacket::create_object(lua_State* L, NetworkStreamPacket* packet)
{
	NO_MAP_LOCK_REQUIRED;

	LuaNetworkStreamPacket* o = new LuaNetworkStreamPacket(packet);
	*(void**)(lua_newuserdata(L, sizeof(void*))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);

	return 1;
}

void LuaNetworkStreamPacket::push_object(lua_State* L, LuaNetworkStreamPacket* packet)
{
	*(void**)(lua_newuserdata(L, sizeof(void*))) = packet;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
}

void LuaNetworkStreamPacket::Register(lua_State* L)
{
	static const luaL_Reg metamethods[] = {
		{"__tostring", mt_tostring},
		{"__gc", gc_object},
		{"__eq", l_equals},
		{0, 0}
	};
	registerClass(L, className, methods, metamethods);

	// Can be created from Lua (ItemStack(itemstack or itemstring or table or nil))
	lua_register(L, className, create_object);
}

// equals(self, other) -> bool
int LuaNetworkStreamPacket::l_equals(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaNetworkStreamPacket* o1 = checkObject<LuaNetworkStreamPacket>(L, 1);

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
	lua_getfield(L, LUA_REGISTRYINDEX, className);
	if (!lua_rawequal(L, -1, -2)) {
		lua_pushboolean(L, false);
		return 1;
	}

	LuaNetworkStreamPacket* o2 = checkObject<LuaNetworkStreamPacket>(L, 2);

	lua_pushboolean(L, o1 == o2);
	return 1;
}


const char LuaNetworkStreamPacket::className[] = "NetworkStreamPacket";
const luaL_Reg LuaNetworkStreamPacket::methods[] = {
	luamethod(LuaNetworkStreamPacket, to_string),
	luamethod(LuaNetworkStreamPacket, equals),
	luamethod(LuaNetworkStreamPacket, flush),
	luamethod(LuaNetworkStreamPacket, get_id),
	luamethod(LuaNetworkStreamPacket, write_string),
	luamethod(LuaNetworkStreamPacket, write_int),
	luamethod(LuaNetworkStreamPacket, write_v3f),
	luamethod(LuaNetworkStreamPacket, write_packet),
	{0,0}
};

