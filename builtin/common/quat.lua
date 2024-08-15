--[[
Quat helpers
Note: The quat.*-functions must be able to accept old quats that had no metatables
]]

-- localize functions
local setmetatable = setmetatable

quat = {}

local metatable = {}
quat.metatable = metatable

local xyzw = {"x", "y", "z", "w"}

-- only called when rawget(v, key) returns nil
function metatable.__index(v, key)
	return rawget(v, xyzw[key]) or quat[key]
end

-- only called when rawget(v, key) returns nil
function metatable.__newindex(v, key, value)
	rawset(v, xyzw[key] or key, value)
end

-- constructors

local function fast_new(x, y, z, w)
	return setmetatable({x = x, y = y, z = z, w = w}, metatable)
end

function quat.new(a, b, c, d)
	if a and b and c and d then
		return fast_new(a, b, c, d)
	end

	-- deprecated, use quat.copy and quat.zero directly
	if type(a) == "table" then
		return quat.copy(a)
	else
		assert(not a, "Invalid arguments for quat.new()")
		return quat.identity()
	end
end

function quat.identity()
	return fast_new(0, 0, 0, 1)
end

function quat.zero()
	return fast_new(0, 0, 0, 0)
end

function quat.copy(v)
	assert(v.x and v.y and v.z and v.w, "Invalid quat passed to quat.copy()")
	return fast_new(v.x, v.y, v.z, v.w)
end

function quat.from_string(s, init)
	local x, y, z, w, np = string.match(
        s, 
        "^%s*%(%s*([^%s,]+)%s*[,%s]%s*([^%s,]+)%s*[,%s]%s*([^%s,]+)%s*[,%s]%s*([^%s,]+)[,%s]?%s*%)()", 
        init)
	x = tonumber(x)
	y = tonumber(y)
	z = tonumber(z)
	if not (x and y and z) then
		return nil
	end
	return fast_new(x, y, z), np
end

function quat.to_string(v)
	return string.format("(%g, %g, %g, %g)", v.x, v.y, v.z, v.w)
end
metatable.__tostring = quat.to_string

function quat.equals(a, b)
	return a.x == b.x and
	       a.y == b.y and
	       a.z == b.z and
	       a.w == b.w
end
metatable.__eq = quat.equals

-- unary operations

function quat.normalize(q)
    local mag = math.sqrt(quat.dot(q, q));

    if mag < 0.000001 then
        return fast_new(0, 0, 0, 1)
    end

    return fast_new(q.x / mag, q.y / mag, q.z / mag, q.w / mag);
end

function quat.floor(v)
	return quat.apply(v, math.floor)
end

function quat.round(v)
	return fast_new(
		math.round(v.x),
		math.round(v.y),
		math.round(v.z),
		math.round(v.w)
	)
end

function quat.apply(v, func)
	return fast_new(
		func(v.x),
		func(v.y),
		func(v.z),
		func(v.w)
	)
end

function quat.combine(a, b, func)
	return fast_new(
		func(a.x, b.x),
		func(a.y, b.y),
		func(a.z, b.z),
		func(a.w, b.w)
	)
end

function quat.angle(a, b)
    local dot = math.min(math.abs(quat.dot(a, b)), 1);
    return (dot > 0.9999999) and 0 or math.acos(dot) * 2;
end

function quat.dot(a, b)
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w
end

function metatable.__unm(v)
	return fast_new(-v.x, -v.y, -v.z, -v.w)
end

-- add, sub, mul, div operations

function quat.add(a, b)
	if type(b) == "table" then
		return fast_new(
			a.x + b.x,
			a.y + b.y,
			a.z + b.z,
			a.w + b.w
		)
	else
		return fast_new(
			a.x + b,
			a.y + b,
			a.z + b,
			a.w + b
		)
	end
end
function metatable.__add(a, b)
	return fast_new(
		a.x + b.x,
		a.y + b.y,
		a.z + b.z,
		a.w + b.w
	)
end

function quat.subtract(a, b)
	if type(b) == "table" then
		return fast_new(
			a.x - b.x,
			a.y - b.y,
			a.z - b.z,
			a.w - b.w
		)
	else
		return fast_new(
			a.x - b,
			a.y - b,
			a.z - b,
			a.w - b
		)
	end
end
function metatable.__sub(a, b)
	return fast_new(
		a.x - b.x,
		a.y - b.y,
		a.z - b.z,
		a.w - b.w
	)
end

function quat.multiply(a, b)
	if type(b) == "table" then
		return fast_new(
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z,
            a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
		)
	else
		return fast_new(
			a.x * b,
			a.y * b,
			a.z * b,
			a.w * b
		)
	end
end
function metatable.__mul(a, b)
	if type(a) == "table" then
		return fast_new(
			a.x * b,
			a.y * b,
			a.z * b,
			a.w * b
		)
	else
		return fast_new(
			a * b.x,
			a * b.y,
			a * b.z,
			a * b.w
		)
	end
end

function quat.divide(a, b)
	if type(b) == "table" then
		return fast_new(
			a.x / b.x,
			a.y / b.y,
			a.z / b.z,
			a.w / b.w
		)
	else
		return fast_new(
			a.x / b,
			a.y / b,
			a.z / b,
			a.w / b
		)
	end
end
function metatable.__div(a, b)
	-- scalar/quat makes no sense
	return fast_new(
		a.x / b,
		a.y / b,
		a.z / b,
		a.w / b
	)
end

-- misc stuff

function quat.offset(v, x, y, z, w)
	return fast_new(
		v.x + x,
		v.y + y,
		v.z + z,
		v.w + w
	)
end

function quat.check(v)
	return getmetatable(v) == metatable
end

local function lerp(x, y, a)
    return x * (1 - a) + y * a;
end

function quat.lerp(x, y, a)
    return fast_new(
        lerp(x.x, y.x, a),
        lerp(x.y, y.y, a),
        lerp(x.z, y.z, a),
        lerp(x.w, y.w, a)
    )
end

function quat.from_euler(pitch, yaw, roll)
    -- Calculate the half angles
    local cy = math.cos(yaw * 0.5)
    local sy = math.sin(yaw * 0.5)
    local cp = math.cos(pitch * 0.5)
    local sp = math.sin(pitch * 0.5)
    local cr = math.cos(roll * 0.5)
    local sr = math.sin(roll * 0.5)

    -- Compute the quaternion components
    local w = cr * cp * cy + sr * sp * sy
    local x = sr * cp * cy - cr * sp * sy
    local y = cr * sp * cy + sr * cp * sy
    local z = cr * cp * sy - sr * sp * cy

    return fast_new(x, y, z, w)
end

function quat.to_euler(q)
    local x, y, z, w = q[1], q[2], q[3], q[4]

    -- Roll (x-axis rotation)
    local sinr_cosp = 2 * (w * x + y * z)
    local cosr_cosp = 1 - 2 * (x * x + y * y)
    local roll = math.atan2(sinr_cosp, cosr_cosp)

    -- Pitch (y-axis rotation)
    local sinp = 2 * (w * y - z * x)
    local pitch
    if math.abs(sinp) >= 1 then
        pitch = math.pi / 2 * math.sign(sinp) -- Use 90 degrees if out of range
    else
        pitch = math.asin(sinp)
    end

    -- Yaw (z-axis rotation)
    local siny_cosp = 2 * (w * z + x * y)
    local cosy_cosp = 1 - 2 * (y * y + z * z)
    local yaw = math.atan2(siny_cosp, cosy_cosp)

    return vector.new(pitch, yaw, roll)
end

function quat.slerp(q1, q2, t)
    -- Compute the dot product between q1 and q2
    local dot = q1[1] * q2[1] + q1[2] * q2[2] + q1[3] * q2[3] + q1[4] * q2[4]

    -- If the dot product is negative, invert one quaternion to take the shorter path
    if dot < 0.0 then
        dot = -dot
        q2 = {-q2[1], -q2[2], -q2[3], -q2[4]}
    end

    -- If the quaternions are very close, just linearly interpolate to avoid numerical instability
    if dot > 0.9995 then
        local result = {
            q1[1] + t * (q2[1] - q1[1]),
            q1[2] + t * (q2[2] - q1[2]),
            q1[3] + t * (q2[3] - q1[3]),
            q1[4] + t * (q2[4] - q1[4])
        }
        -- Normalize the result
        local norm = math.sqrt(result[1] * result[1] + result[2] * result[2] + result[3] * result[3] + result[4] * result[4])
        return {result[1] / norm, result[2] / norm, result[3] / norm, result[4] / norm}
    end

    -- Calculate the angle between the quaternions
    local theta_0 = math.acos(dot)
    local sin_theta_0 = math.sin(theta_0)

    -- Compute the interpolation factors
    local factor1 = math.sin((1.0 - t) * theta_0) / sin_theta_0
    local factor2 = math.sin(t * theta_0) / sin_theta_0

    -- Compute the final interpolated quaternion
    return fast_new(
        factor1 * q1[1] + factor2 * q2[1],
        factor1 * q1[2] + factor2 * q2[2],
        factor1 * q1[3] + factor2 * q2[3],
        factor1 * q1[4] + factor2 * q2[4]
    )
end

if rawget(_G, "core") and core.set_read_quat and core.set_push_quat then
	local function read_quat(v)
		return v.x, v.y, v.z, v.w
	end
	core.set_read_quat(read_quat)
	core.set_read_quat = nil

	if rawget(_G, "jit") then
		-- This is necessary to prevent trace aborts.
		local function push_quat(x, y, z, w)
			return (fast_new(x, y, z, w))
		end
		core.set_push_quat(push_quat)
	else
		core.set_push_quat(fast_new)
	end
	core.set_push_quat = nil
end
