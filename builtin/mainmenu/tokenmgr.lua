tokenmgr = {}

--------------------------------------------------------------------------------
local function get_tokens_path(folder)
	return core.get_cache_path() .. DIR_DELIM .. "tokens.json"
end

--------------------------------------------------------------------------------
local function save_tokens()
	local filename = get_tokens_path()
	core.safe_file_write(filename, core.write_json(tokenmgr.tokens))
end

--------------------------------------------------------------------------------
local function read_tokens()
	local path = get_tokens_path()

	-- If new format configured fall back to reading the legacy file
	if path:sub(#path - 4):lower() == ".json" then
		local file = io.open(path, "r")
		if file then
			local json = file:read("*all")
			file:close()
            if json == "" then
                return {}
            end

			return core.parse_json(json)
		end
	end

	return {}
end

--------------------------------------------------------------------------------
function tokenmgr.get_tokens()
	if tokenmgr.tokens then
		return tokenmgr.tokens
	end

	tokenmgr.tokens = read_tokens()
	return tokenmgr.tokens
end

function tokenmgr.get_tokens_as_array()
    local tokens = tokenmgr.get_tokens()

	local res = {}
    for k, v in pairs(tokens) do
        res[#res + 1] = v
    end
    return res
end

function tokenmgr.get_num_tokens()
    return #tokenmgr.get_tokens_as_array()
end

--------------------------------------------------------------------------------
function tokenmgr.get_token_by_name(name)
    local tokens = tokenmgr.get_tokens()
	if not tokens then
        return nil
    end

    return tokens[name]
end
--------------------------------------------------------------------------------
function tokenmgr.add_token(new_token)
    --
    -- trim token
    local token = new_token.token

    token = token:gsub("%s+", "")
    token = string.gsub(token, "%s+", "")

    new_token.token = token

    tokenmgr.tokens[new_token.name] = new_token
	save_tokens()
end

--------------------------------------------------------------------------------
function tokenmgr.delete_token(del_token)
    tokenmgr.tokens[del_token.name] = nil
	save_tokens()
end
