-- Minetest: builtin/static_spawn.lua

local static_spawnpoint_string = core.settings:get("static_spawnpoint")
if static_spawnpoint_string and
		static_spawnpoint_string ~= "" and
		not core.setting_get_pos("static_spawnpoint") then
	error('The static_spawnpoint setting is invalid: "' ..
			static_spawnpoint_string .. '"')
end

function core.put_player_in_spawn(player_obj)
	local static_spawnpoint = core.setting_get_pos("static_spawnpoint")
	if not static_spawnpoint then
		return false
	end
	core.log("action", "Moving " .. player_obj:get_player_name() ..
		" to static spawnpoint at " .. core.pos_to_string(static_spawnpoint))
	player_obj:set_pos(static_spawnpoint)
	return true
end

local function respawn(player_obj)
    core.put_player_in_spawn(player_obj)
end

core.register_on_newplayer(respawn)
core.register_on_respawnplayer(respawn)
