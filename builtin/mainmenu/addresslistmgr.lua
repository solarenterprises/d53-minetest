--Minetest
--Copyright (C) 2020 rubenwardy
--
--This program is free software; you can redistribute it and/or modify
--it under the terms of the GNU Lesser General Public License as published by
--the Free Software Foundation; either version 2.1 of the License, or
--(at your option) any later version.
--
--This program is distributed in the hope that it will be useful,
--but WITHOUT ANY WARRANTY; without even the implied warranty of
--MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--GNU Lesser General Public License for more details.
--
--You should have received a copy of the GNU Lesser General Public License along
--with this program; if not, write to the Free Software Foundation, Inc.,
--51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

addresslistmgr = {}

--------------------------------------------------------------------------------
local function get_addresses_path(folder)
	local base = core.get_user_path() .. DIR_DELIM .. "client" .. DIR_DELIM .. "addresslist" .. DIR_DELIM
	if folder then
		return base
	end
	return base .. core.settings:get("addresslist_file")
end

--------------------------------------------------------------------------------
local function save_addresses(addresses)
	local filename = core.settings:get("addresslist_file")
	-- If setting specifies legacy format change the filename to the new one
	if filename:sub(#filename - 3):lower() == ".txt" then
		core.settings:set("addresslist_file", filename:sub(1, #filename - 4) .. ".json")
	end

	assert(core.create_dir(get_addresses_path(true)))
	core.safe_file_write(get_addresses_path(), core.write_json(addresses))
end

--------------------------------------------------------------------------------
local function read_addresses()
	local path = get_addresses_path()

	-- If new format configured fall back to reading the legacy file
	if path:sub(#path - 4):lower() == ".json" then
		local file = io.open(path, "r")
		if file then
			local json = file:read("*all")
			file:close()
			return core.parse_json(json)
		end
	end

	return
end

--------------------------------------------------------------------------------
local function delete_address(addresses, del_address)
	for i=1, #addresses do
		local addr = addresses[i]

		if addr.address == del_address then
			table.remove(addresses, i)
			return
		end
	end
end

--------------------------------------------------------------------------------
function addresslistmgr.get_addresses()
	if addresslistmgr.addresses then
		return addresslistmgr.addresses
	end

	addresslistmgr.addresses = {}

	-- Add addresses, removing duplicates
	local seen = {}
	for _, addr in ipairs(read_addresses() or {}) do
		local key = addr.address
		if not seen[key] then
			seen[key] = true
			addresslistmgr.addresses[#addresslistmgr.addresses + 1] = addr
		end
	end

	return addresslistmgr.addresses
end

--------------------------------------------------------------------------------
function addresslistmgr.get_address(get_address)
	if addresslistmgr.addresses then
		for i=1, #addresslistmgr.addresses do
			local addr = addresslistmgr.addresses[i]
	
			if addr.address == get_address then
				return addr
			end
		end
	end

	addresslistmgr.addresses = {}

	-- Add addresses, removing duplicates
	local seen = {}
	for _, addr in ipairs(read_addresses() or {}) do
		local key = ("%s"):format(addr.address:lower())
		if not seen[key] then
			seen[key] = true
			addresslistmgr.addresses[#addresslistmgr.addresses + 1] = addr
		end
	end

	for i=1, #addresslistmgr.addresses do
		local addr = addresslistmgr.addresses[i]

		if addr.address == get_address.address then
			return addr
		end
	end
end
--------------------------------------------------------------------------------
function addresslistmgr.add_address(new_address)

	-- Whitelist favorite keys
	new_address = {
		address = new_address.address,
		iv = new_address.iv,
		encrypted_key = new_address.encrypted_key,
	}

	local addresses = addresslistmgr.get_addresses()
	delete_address(addresses, new_address)
	table.insert(addresses, 1, new_address)
	save_addresses(addresses)
end

--------------------------------------------------------------------------------
function addresslistmgr.delete_address(del_address)
	local addresses = addresslistmgr.get_addresses()
	delete_address(addresses, del_address)
	save_addresses(addresses)
end
