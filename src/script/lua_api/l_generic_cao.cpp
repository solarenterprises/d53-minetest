#include "l_generic_cao.h"
#include "l_internal.h"
#include "lua_api/l_item.h"
#include "script/common/c_converter.h"
#include "common/c_content.h"
#include "client/content_cao.h"
#include "client/client.h"
#include "script/cpp_api/s_client.h"

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
