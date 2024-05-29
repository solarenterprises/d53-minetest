----------------------------------------------
-- Minetest
----------------------------------------------

-- will move all player data to the new player name
-- @return string
function minetest.rename_player(from_player, to_player) end

-- generates an unique key
-- @return string
function minetest.generate_key() end

-- @param player_name string or peer_id number
-- @return string or nil
function minetest.get_player_token(player_name_or_peer_id) end

-- @param player_name string
-- @param attr string
-- @return string or nil
function minetest.get_player_metadata(player_name, attr) end

-- @param peer_id number
-- @return ObjectRef
function minetest.get_player_by_peer_id(peer_id) end

-- Sends network packet to all to_peer_ids
-- @param to_peer_ids array
-- @param packet NetworkPacket
-- @return string or nil
function minetest.send(to_peer_ids, packet) end

-- @param def {
    -- id string
    -- display_name string
    -- default_key string "KEY_TAB" "KEY_KEY_X"
-- }
function minetest.register_input_key(def) end
function minetest.register_on_input(callback) end

-- @param callback function
function minetest.register_on_lua_packet(callback) end
-- @param callback function
function minetest.register_on_lua_packet_stream(callback) end

----------------------------------------------
-- Voice
----------------------------------------------

-- gets all available input devices
-- @return string array
function minetest.getInputDevices() end

--- @class Voice
Voice = {}
Voice.__index = Voice

-- starts recording voice to stream. Like push to talk
-- @param stream NetworkStreamPacket
function Voice:start(stream) end

-- stops recording
function Voice:stop() end

-- sets voice input device
-- @param deviceName
function Voice:setInputDevice(deviceName) end

-- check if is recording
-- @return bool
function Voice:isRunning() end

-- Creates a new voice api
-- @param def { sample_rate number }
-- @return Voice
function minetest.Voice(def) return Voice end

----------------------------------------------
-- Buffer
----------------------------------------------
--- @class Buffer
Buffer = {}
Buffer.__index = Buffer
function Buffer:to_string() end
function Buffer:get_size() end

----------------------------------------------
-- NetworkPacket
----------------------------------------------
--- @class NetworkPacket
NetworkPacket = {}
NetworkPacket.__index = NetworkPacket

function NetworkPacket:get_peer_id() end
function NetworkPacket:write_string(str) end
function NetworkPacket:write_int(i) end
function NetworkPacket:write_u8(u) end
function NetworkPacket:write_v3f(vec) end
function NetworkPacket:read_string() end
function NetworkPacket:read_int() end
function NetworkPacket:read_u8() end
function NetworkPacket:read_v3f() end
function NetworkPacket:read_remaining_string() end
function NetworkPacket:read_remaining_buffer() return Buffer end

----------------------------------------------
-- OGGWriteStream
----------------------------------------------
--- @class OGGWriteStream
OGGWriteStream = {}
OGGWriteStream.__index = OGGWriteStream

function OGGWriteStream:get_bufer() return Buffer end
function OGGWriteStream:get_size() end
-- @param NetworkPacket packet
function OGGWriteStream:write(packet) end

----------------------------------------------
-- NetworkStreamPacket
----------------------------------------------
--- @class NetworkStreamPacket
NetworkStreamPacket = {}
NetworkStreamPacket.__index = NetworkStreamPacket

function NetworkStreamPacket:flush() end
function NetworkStreamPacket:write_string(str) end
function NetworkStreamPacket:write_int(i) end
function NetworkStreamPacket:write_v3f(vec) end
-- @param NetworkPacket
function NetworkStreamPacket:write_packet(packet) end
function NetworkStreamPacket:get_id() end

----------------------------------------------
-- NetworkChannel
----------------------------------------------
--- @class NetworkChannel
NetworkChannel = {}
NetworkChannel.__index = NetworkChannel
-- Creates a new NetworkChannel
-- @return NetworkChannel
function minetest.NetworkChannel() return NetworkChannel end

-- @return NetworkPacket
function NetworkChannel:createNetworkPacket() return NetworkPacket end

-- @return NetworkStreamPacket
function NetworkChannel:createNetworkStreamPacket() return NetworkStreamPacket end

----------------------------------------------
-- MySQL
----------------------------------------------
-- @class mysql_connection
local mysql_cursor = {}
mysql_cursor.__index = mysql_cursor
function mysql_cursor:close() end
function mysql_cursor:getcolnames() end
function mysql_cursor:getcoltypes() end
function mysql_cursor:fetch() end
-- @return number
function mysql_cursor:numrows() end
function mysql_cursor:seek() end
function mysql_cursor:nextresult() end
-- @return boolean
function mysql_cursor:hasnextresult() end

-- @class mysql_connection
local mysql_connection = {}
mysql_connection.__index = mysql_connection
function mysql_connection:close() end
function mysql_connection:ping() end
function mysql_connection:escape(str) end
-- @return mysql_cursor or number or string
function mysql_connection:execute(query) return mysql_cursor end
function mysql_connection:commit() end
function mysql_connection:rollback() end
function mysql_connection:setautocommit() end
function mysql_connection:getlastautoid() end

-- @class mysql_env
local mysql_env = {}
mysql_env.__index = mysql_env
function mysql_env:connect(sourcename, username, password, host, port, unix_socket, client_Flag) return mysql_connection, "" end
function mysql_env:close() end

-- @class mysql_namespace
local mysql_namespace = {}
mysql_namespace.__index = mysql_namespace

function mysql_namespace.mysql() return mysql_env end
function mysql() return mysql_namespace end