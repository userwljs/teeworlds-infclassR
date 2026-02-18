function init_callbacks()
    event_listeners = {}
    event_listeners["on_tick"] = {}
    event_listeners["on_loaded"] = {}
    event_listeners["on_round_started"] = {}
    event_listeners["on_round_end"] = {}
    event_listeners["on_world_reset"] = {}
    event_listeners["on_character_spawned"] = {}
    event_listeners["on_character_death"] = {}
    event_listeners["on_chat_message"] = {}
    event_listeners["on_shutdown"] = {}
end

function on_event(event, handler)
    local listeners = event_listeners[event]
    table.insert(listeners, handler)
end

function on_tick()
    local listeners = event_listeners["on_tick"]
    for i, handler in ipairs(listeners) do
        handler()
    end
end

---@param round_type string
function on_round_started(round_type)
    local listeners = event_listeners["on_round_started"]
    for i, handler in ipairs(listeners) do
        handler(round_type)
    end
end

---@param reason string The reason (e.g. "finished" or "canceled")
function on_round_end(reason)
    local listeners = event_listeners["on_round_end"]
    for i, handler in ipairs(listeners) do
        handler(reason)
    end
end

function on_world_reset()
    local listeners = event_listeners["on_world_reset"]
    for i, handler in ipairs(listeners) do
        handler()
    end
end

---@param player_id number
---@param spawn_type string
function on_character_spawned(player_id, spawn_type)
    local listeners = event_listeners["on_character_spawned"]
    for i, handler in ipairs(listeners) do
        handler(player_id, spawn_type)
    end
end

function on_character_death(victim_id, killer_id, weapon_str)
    local listeners = event_listeners["on_character_death"]
    for i, handler in ipairs(listeners) do
        handler(victim_id, killer_id, weapon_str)
    end
end

function on_chat_message(from, message_str)
    local listeners = event_listeners["on_chat_message"]
    for i, handler in ipairs(listeners) do
        handler(from, message_str)
    end
end

function on_loaded()
    local listeners = event_listeners["on_loaded"]
    for i, handler in ipairs(listeners) do
        handler()
    end
end

function on_shutdown()
    local listeners = event_listeners["on_shutdown"]
    for i, handler in ipairs(listeners) do
        handler()
    end
end

init_callbacks()
