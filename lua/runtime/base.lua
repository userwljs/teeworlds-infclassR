require("runtime.callbacks")

Submod = {}
Submod.Classic = "classic"

active_submod = Submod.Classic

function init_base()
    math.randomseed(os.time())
    math.random(); math.random(); math.random()

    reset_context()

    on_event("on_world_reset", reset_context)
end

function reset_context()
    runtime_context = {}
    runtime_context.doors = {}
end

-- Empty callback
function do_nothing()
end

function round_to_tile_center(value)
    return math.floor(value / 32) * 32 + 16
end

function find_ceiling_tile_center(pos)
    local collision = Game.Context.Collision
    local rounded = vec2(round_to_tile_center(pos.x), round_to_tile_center(pos.y))
    while not collision:CheckPoint(rounded) do
        rounded.y = rounded.y - 32
        if rounded.y <= 0 then
            return vec2(rounded.x, 0)
        end
    end
    return rounded
end

function find_ground_tile_center(pos)
    local collision = Game.Context.Collision
    local map_height = collision.Height * 32
    local rounded = vec2(round_to_tile_center(pos.x), round_to_tile_center(pos.y))
    while not collision:CheckPoint(rounded) do
        rounded.y = rounded.y + 32
        if rounded.y > map_height then
            return vec2(rounded.x, map_height + 1)
        end
    end
    return rounded
end

function add_door_at(pos)
    local door = Game.Controller:AddDoor(find_ceiling_tile_center(pos), find_ground_tile_center(pos))

    -- Lua convention is to numerate from 1
    table.insert(runtime_context.doors, door)
    return door
end

function add_door(player_id)
    local character = Game.Controller:GetCharacter(player_id)
    if character == nil then
        print("No character with CID ", player_id)
        return nil
    end
    local pos = character.Position
    local door = add_door_at(pos)
    door:SetOpen(true)
    return door
end

function add_piercing_door_at(pos_x)
    local pos_x = round_to_tile_center(pos_x)
    local map_height = Game.Context.Collision.Height * 32
    local door = Game.Controller:AddDoor(vec2(pos_x, map_height + 1), vec2(pos_x, 0))

    -- Lua convention is to numerate from 1
    table.insert(runtime_context.doors, door)

    return door
end

function add_piercing_door(player_id)
    local character = Game.Controller:GetCharacter(player_id)
    if character == nil then
        print("No character with CID ", player_id)
        return nil
    end
    local pos = character.Position
    local door = add_piercing_door_at(pos.x)
    door:SetOpen(true)
    return door
end

function open_all_doors()
    for i,door in ipairs(runtime_context.doors) do
        door:SetOpen(true)
    end
end

function close_all_doors()
    for i,door in ipairs(runtime_context.doors) do
        door:SetOpen(false)
    end
end

function remove_this_door(player_id)
    Game.Context:SendChatTarget(player_id, "Not implemented yet")
end

function remove_all_doors()
    for i,door in ipairs(runtime_context.doors) do
        door:Destroy()
    end

    runtime_context.doors = {}
end

function for_each_player(callback)
    for i = 0,63 do
        local player = Game.Controller:GetPlayer(i)
        if player ~= nil then
            callback(player)
        end
    end
end

function for_each_human_character(callback)
    ---@param ch CInfClassCharacter
    ---@return boolean
    local function acceptable(ch)
        if ch == nil then
            return false
        end
        if ch:IsInfected() or not ch:IsAlive() then
            return false
        end

        return true
    end

    for i = 0,63 do
        local character = Game.Controller:GetCharacter(i)
        if acceptable(character) then
            callback(character)
        end
    end
end

function is_in_range(value, from, to)
    if value < from then
        return false
    end
    if value > to then
        return false
    end
    return true
end

init_base()

print("Runtime v1.0 loaded")
