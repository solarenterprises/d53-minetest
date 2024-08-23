#include "l_generic_cao.h"
#include "l_internal.h"
#include "lua_api/l_item.h"
#include "script/common/c_converter.h"
#include "common/c_content.h"
#include "client/content_cao.h"
#include "client/client.h"
#include "script/cpp_api/s_client.h"
#include "map.h"
#include "client/localplayer.h"

#define checkCSMRestrictionFlag(flag) \
	( getClient(L)->checkCSMRestrictionFlag(CSMRestrictionFlags::flag) )

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
	lua_pushboolean(L, cao != nullptr);
	return 1;
}

int LuaGenericCAO::l_is_local_player(lua_State* L)
{
	CAO;
	lua_pushboolean(L, cao->isLocalPlayer());
	return 1;
}

int LuaGenericCAO::l_is_player(lua_State* L)
{
	CAO;
	lua_pushboolean(L, cao->isPlayer());
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

int LuaGenericCAO::l_get_pos_offset(lua_State* L)
{
	CAO;
	push_v3f(L, cao->getPositionOffset());
	return 1;
}

int LuaGenericCAO::l_get_rot_offset(lua_State* L)
{
	CAO;
	v3f euler;
	cao->getRotationOffset().toEuler(euler);
	push_v3f(L, euler*core::RADTODEG);
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

	cao->setRotationOffset(offset*core::DEGTORAD);

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

// get_properties(self)
int LuaGenericCAO::l_get_properties(lua_State *L)
{
	CAO;

	const ObjectProperties& prop = cao->accessObjectProperties();
	push_object_properties(L, &prop);
	return 1;
}

static void push_bone_override(lua_State *L, const BoneOverride &props)
{
	lua_newtable(L);

	auto push_prop = [L](const char *name, const auto &prop, v3f vec) {
		lua_newtable(L);
		push_v3f(L, vec);
		lua_setfield(L, -2, "vec");
		lua_pushnumber(L, prop.interp_timer);
		lua_setfield(L, -2, "interpolate");
		lua_pushboolean(L, prop.absolute);
		lua_setfield(L, -2, "absolute");
		lua_setfield(L, -2, name);
	};

	push_prop("position", props.position, props.position.vector);

	v3f euler_rot;
	props.rotation.next.toEuler(euler_rot);
	push_prop("rotation", props.rotation, euler_rot);

	push_prop("scale", props.scale, props.scale.vector);

	// leave only override table on top of the stack
}

// get_bone_override(self, bone)
int LuaGenericCAO::l_get_bone_override(lua_State *L)
{
	CAO;

	std::string bone = readParam<std::string>(L, 2);
	push_bone_override(L, cao->getBoneOverride(bone));
	return 1;
}

// set_bone_override(self, bone, override)
int LuaGenericCAO::l_set_bone_override(lua_State *L)
{
	CAO;

	std::string bone = readParam<std::string>(L, 2);

	BoneOverride props;
	if (lua_isnoneornil(L, 3)) {
		cao->setBoneOverride(bone, props);
		return 0;
	}

	auto read_prop_attrs = [L](auto &prop) {
		lua_getfield(L, -1, "absolute");
		prop.absolute = lua_toboolean(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "interpolate");
		if (lua_isnumber(L, -1))
			prop.interp_timer = lua_tonumber(L, -1);
		lua_pop(L, 1);
	};

	lua_getfield(L, 3, "position");
	if (!lua_isnil(L, -1)) {
		lua_getfield(L, -1, "vec");
		if (!lua_isnil(L, -1))
			props.position.vector = check_v3f(L, -1);
		lua_pop(L, 1);

		read_prop_attrs(props.position);
	}
	lua_pop(L, 1);

	lua_getfield(L, 3, "rotation");
	if (!lua_isnil(L, -1)) {
		lua_getfield(L, -1, "vec");
		if (!lua_isnil(L, -1))
			props.rotation.next = core::quaternion(check_v3f(L, -1));
		lua_pop(L, 1);

		read_prop_attrs(props.rotation);
	}
	lua_pop(L, 1);

	lua_getfield(L, 3, "scale");
	if (!lua_isnil(L, -1)) {
		lua_getfield(L, -1, "vec");
		props.scale.vector = lua_isnil(L, -1) ? v3f(1) : check_v3f(L, -1);
		lua_pop(L, 1);

		read_prop_attrs(props.scale);
	}
	lua_pop(L, 1);

	cao->setBoneOverride(bone, props);
	return 0;
}

GenericCAO* LuaGenericCAO::getobject(LuaGenericCAO* ref)
{
	if (ref->m_genericCAO.expired())
		return nullptr;
	return ref->m_genericCAO.lock().get();
}

GenericCAO* LuaGenericCAO::getobject(lua_State* L, int narg)
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
		luamethod(LuaGenericCAO, get_pos_offset),
		luamethod(LuaGenericCAO, get_rot_offset),
		luamethod(LuaGenericCAO, set_pos_offset),
		luamethod(LuaGenericCAO, set_rot_offset),
		luamethod(LuaGenericCAO, is_immortal),
		luamethod(LuaGenericCAO, get_properties),
		luamethod(LuaGenericCAO, get_bone_override),
		luamethod(LuaGenericCAO, set_bone_override),
		luamethod(LuaGenericCAO, is_local_player),
		luamethod(LuaGenericCAO, is_player),
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

int ModApiGenericCAO::l_get_objects(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	if (checkCSMRestrictionFlag(CSM_RF_READ_PLAYERINFO))
		return 0;

	Client* client = getClient(L);
	auto objects = client->getEnv().getActiveObjects();

	int index = 1;
	lua_newtable(L);
	for (auto weakptr_obj : objects) {
		auto ptr = weakptr_obj.lock();
		auto obj = ptr.get();
		if (obj->getType() != ACTIVEOBJECT_TYPE_GENERIC)
			continue;

		LuaGenericCAO* o = new LuaGenericCAO(std::static_pointer_cast<GenericCAO>(ptr));
		*(void**)(lua_newuserdata(L, sizeof(void*))) = o;
		luaL_getmetatable(L, LuaGenericCAO::className);
		lua_setmetatable(L, -2);

		lua_rawseti(L, -2, index);
		index++;
	}

	return 1;
}

int ModApiGenericCAO::l_get_players(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;

	if (checkCSMRestrictionFlag(CSM_RF_READ_PLAYERINFO))
		return 0;

	Client* client = getClient(L);
	auto objects = client->getEnv().getActiveObjects();

	int index = 1;
	lua_newtable(L);
	for (auto& weakptr_obj : objects) {
		if (weakptr_obj.expired())
			continue;

		auto ptr = weakptr_obj.lock();
		auto obj = ptr.get();

		if (obj->getType() != ACTIVEOBJECT_TYPE_GENERIC)
			continue;

		GenericCAO* cao = (GenericCAO*)obj;
		if (!cao->isPlayer())
			continue;
		
		LuaGenericCAO* o = new LuaGenericCAO(std::static_pointer_cast<GenericCAO>(ptr));
		*(void**)(lua_newuserdata(L, sizeof(void*))) = o;
		luaL_getmetatable(L, LuaGenericCAO::className);
		lua_setmetatable(L, -2);

		lua_rawseti(L, -2, index);
		index++;
	}

	return 1;
}


// get_objects_inside_radius(pos, radius)
int ModApiGenericCAO::l_get_objects_inside_radius(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	Client *client = getClient(L);

	// Do it
	v3f pos = checkFloatPos(L, 1);
	float radius = readParam<float>(L, 2) * BS;
	bool player = true;
	bool should_filter_objects = false;
	u32 collision_mask = 0xFFFFFF;

	if (lua_istable(L, 3)) {
		lua_getfield(L, 3, "player");
		if (lua_isboolean(L, -1)) {
			player = lua_toboolean(L, -1);
			should_filter_objects = true;
		}
		lua_pop(L, 1);

		lua_getfield(L, 3, "object");
		if (lua_isboolean(L, -1)) {
			player = !lua_toboolean(L, -1);
			should_filter_objects = true;
		}
		lua_pop(L, 1);

		lua_getfield(L, 3, "collision_mask");
		if (lua_isnumber(L, -1))
			collision_mask = lua_tointeger(L, -1);
		lua_pop(L, 1);
	}

	auto include_obj_cb = [player, should_filter_objects, collision_mask](GenericCAO *obj) {
		if (should_filter_objects) {
			if (obj->isPlayer() != player)
				return false;
		}

		if (collision_mask != 0xFFFFFF)
			if ((obj->getProperties().collision_group & collision_mask) == 0)
				return false;

		return true;
	};

	std::vector<DistanceSortedActiveObject> objs;
	client->getEnv().getActiveObjects(pos, radius, objs);

	int i = 0;
	lua_createtable(L, objs.size(), 0);
	for (const auto o : objs) {
		if (o.obj->getType() != ACTIVEOBJECT_TYPE_GENERIC)
			continue;

		if (!include_obj_cb((GenericCAO*)o.obj))
			continue;

		auto ptr = client->getEnv().getActiveObjectWeakPtr(o.obj->getId());
		LuaGenericCAO* o = new LuaGenericCAO(std::static_pointer_cast<GenericCAO>(ptr.lock()));
		*(void**)(lua_newuserdata(L, sizeof(void*))) = o;
		luaL_getmetatable(L, LuaGenericCAO::className);
		lua_setmetatable(L, -2);

		lua_rawseti(L, -2, ++i);
	}
	return 1;
}

void ModApiGenericCAO::Initialize(lua_State* L, int top)
{
	API_FCT(get_generic_cao);
	API_FCT(get_objects);
	API_FCT(get_players);
	API_FCT(get_objects_inside_radius);
}
