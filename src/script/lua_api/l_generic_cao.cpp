#include "l_generic_cao.h"
#include "l_internal.h"
#include "lua_api/l_item.h"
#include "script/common/c_converter.h"
#include "common/c_content.h"
#include "client/content_cao.h"
#include "client/client.h"
#include "script/cpp_api/s_client.h"
#include "map.h"

#define CAO \
auto cao = getobject(L, 1); \
if (!cao) { \
	lua_pushnil(L); \
	return 1; \
}

LuaGenericCAO::LuaGenericCAO(std::shared_ptr<GenericCAO> m) : m_genericCAO(m)
{
}

int LuaGenericCAO::l_is_valid(lua_State* L)
{
	auto cao = getobject(L, 1);
	lua_pushboolean(L, cao.get() != nullptr);
	return 1;
}

int LuaGenericCAO::l_get_hp(lua_State* L)
{
	CAO;

	lua_pushinteger(L, cao->getHP());
	return 1;
}

int LuaGenericCAO::l_get_name(lua_State* L)
{
	CAO;

	auto name = cao->getName();
	lua_pushlstring(L, name.c_str(), name.length());
	return 1;
}

int LuaGenericCAO::l_is_attached(lua_State* L)
{
	CAO;

	lua_pushboolean(L, cao->getParent() != nullptr);
	return 1;
}

int LuaGenericCAO::l_get_velocity(lua_State* L)
{
	CAO;

	push_v3f(L, cao->getVelocity() / BS);
	return 1;
}

// get_pos(self)
int LuaGenericCAO::l_get_pos(lua_State* L)
{
	CAO;

	push_v3f(L, cao->getPosition() / BS);
	return 1;
}

// set_pos(self)
int LuaGenericCAO::l_set_pos_offset(lua_State* L)
{
	CAO;

	v3f offset = readParam<v3f>(L, 2);

	cao->setPositionOffset(offset);

	return 0;
}

int LuaGenericCAO::l_set_rot_offset(lua_State* L)
{
	CAO;

	v3f offset = readParam<v3f>(L, 2);

	cao->setRotationOffset(offset);

	return 0;
}

// get_rotation(self)
int LuaGenericCAO::l_get_rot(lua_State* L)
{
	CAO;

	push_v3f(L, cao->getRotation());
	return 1;
}

// is_immortal(self)
int LuaGenericCAO::l_is_immortal(lua_State* L)
{
	CAO;

	lua_pushboolean(L, cao->isImmortal());
	return 1;
}

// get_armor_groups(self)
int LuaGenericCAO::l_get_armor_groups(lua_State* L)
{
	CAO;

	push_groups(L, cao->getGroups());
	return 1;
}

int LuaGenericCAO::l_get_underground(lua_State* L)
{
	CAO;

	v3s16 node_pos = floatToInt(cao->getPosition(), BS);
	

	ClientEnvironment& env = getClient(L)->getEnv();
	Map& map = env.getMap();

	lua_createtable(L, 0, 0);

	try {
		u32 tested_count = 0;
		u32 above_count = 0;

		std::unordered_set<v3s16> checked;


		for (int x = -1; x < 1; x++)
			for (int y = 0; y < 2; y++)
				for (int z = -1; z < 1; z++) {
					v3s16 block_pos = v3s16(node_pos.X + x* MAP_BLOCKSIZE, node_pos.Y + y * MAP_BLOCKSIZE, node_pos.Z + z * MAP_BLOCKSIZE) / MAP_BLOCKSIZE;

					if (checked.find(block_pos) != checked.end())
						continue;
					checked.insert(block_pos);

					tested_count++;

					MapBlock* block = nullptr;
					try {
						block = map.getBlockNoCreate(block_pos);
						if (!block)
							throw "block not found";
					}
					catch (const std::exception& e) {
						continue;
					}

					if (block->isAir())
						break;

					auto f = ContentLightingFlags();
					f.has_light = true;

					for (int nx = 0; nx < MAP_BLOCKSIZE; nx++)
						for (int ny = 0; ny < MAP_BLOCKSIZE-1; ny++)
							for (int nz = 0; nz < MAP_BLOCKSIZE; nz++) {
								MapNode node_above = block->getNodeNoEx(v3s16(nx, ny+1, nz));
								if (node_above.getContent() != CONTENT_AIR)
									above_count++;
							}
				}

		const int nodes3 = MAP_BLOCKSIZE * (MAP_BLOCKSIZE-1) * MAP_BLOCKSIZE;
		double alpha = (double)above_count / (tested_count * nodes3);

		lua_pushboolean(L, alpha > 0.5);
		lua_setfield(L, -2, "is_underground");
		lua_pushnumber(L, alpha);
		lua_setfield(L, -2, "value");
		return 1;
	}
	catch (const std::exception& e) {
		lua_pushboolean(L, false);
		lua_setfield(L, -2, "is_underground");
		return 1;
	}
}

std::shared_ptr<GenericCAO> LuaGenericCAO::getobject(LuaGenericCAO* ref)
{
	return ref->m_genericCAO.lock();
}

std::shared_ptr<GenericCAO> LuaGenericCAO::getobject(lua_State* L, int narg)
{
	LuaGenericCAO* ref = checkObject<LuaGenericCAO>(L, narg);
	assert(ref);
	return getobject(ref);
}

int LuaGenericCAO::gc_object(lua_State* L)
{
	LuaGenericCAO* o = *(LuaGenericCAO**)(lua_touserdata(L, 1));
	delete o;
	return 0;
}

void LuaGenericCAO::Register(lua_State* L)
{
	static const luaL_Reg metamethods[] = {
		{"__gc", gc_object},
		{0, 0}
	};
	registerClass(L, className, methods, metamethods);
}

const char LuaGenericCAO::className[] = "GenericCAO";
const luaL_Reg LuaGenericCAO::methods[] = {
		luamethod(LuaGenericCAO, is_valid),
		luamethod(LuaGenericCAO, get_velocity),
		luamethod(LuaGenericCAO, get_hp),
		luamethod(LuaGenericCAO, get_name),
		luamethod(LuaGenericCAO, is_attached),
		luamethod(LuaGenericCAO, get_pos),
		luamethod(LuaGenericCAO, get_velocity),
		luamethod(LuaGenericCAO, get_rot),
		luamethod(LuaGenericCAO, set_pos_offset),
		luamethod(LuaGenericCAO, set_rot_offset),
		luamethod(LuaGenericCAO, is_immortal),
		luamethod(LuaGenericCAO, get_underground),
		{0, 0}
};

//
// ModApiGenericCAO
//
int ModApiGenericCAO::l_get_generic_cao(lua_State* L)
{
	Client* client = getClient(L);

	int id = readParam<int>(L, 1);
	if (id < 0) {
		lua_pushboolean(L, false);
		return 1;
	}

	// Read the callback function from Lua stack
	if (!lua_isfunction(L, 2)) {
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushvalue(L, 2); // Copy the callback to the top of the stack
	int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);
	client->getEnv().add_lua_activeObjectCallback(id, callbackRef);

	lua_pushboolean(L, true);
	return 1;
}

void ModApiGenericCAO::Initialize(lua_State* L, int top)
{
	API_FCT(get_generic_cao);
}
