/*
Minetest
Copyright (C) 2017 Dumbeldor, Vincent Glize <vincent.glize@live.fr>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include "l_base.h"
#include "client/content_cao.h"

//
// Lua class for Generic ClientActiveObject
//
class LuaGenericCAO : public ModApiBase
{
private:
	static const luaL_Reg methods[];

	// garbage collector
	static int gc_object(lua_State* L);

	// is_valid(self)
	static int l_is_valid(lua_State* L);

	// get_velocity(self)
	static int l_get_velocity(lua_State* L);

	// get_hp(self)
	static int l_get_hp(lua_State* L);

	// get_name(self)
	static int l_get_name(lua_State* L);

	static int l_is_attached(lua_State* L);

	// get_pos(self)
	static int l_get_pos(lua_State* L);

	static int l_set_pos_offset(lua_State* L);
	static int l_set_rot_offset(lua_State* L);

	// is_immortal(self)
	static int l_is_immortal(lua_State* L);

	// get_rotation(self)
	static int l_get_rot(lua_State* L);

	// get_armor_groups(self)
	static int l_get_armor_groups(lua_State* L);

	static std::shared_ptr<GenericCAO> getobject(LuaGenericCAO* ref);
	static std::shared_ptr<GenericCAO> getobject(lua_State* L, int narg);

	std::weak_ptr<GenericCAO> m_genericCAO;

public:
	LuaGenericCAO(std::shared_ptr<GenericCAO> m);
	~LuaGenericCAO() = default;

	static void Register(lua_State* L);

	static const char className[];
};

class ModApiGenericCAO : public ModApiBase {
private:
	static int l_get_generic_cao(lua_State* L);

public:
	static void Initialize(lua_State* L, int top);
};
