require("runtime.base")

Votes = {}

Votes.fun_round_allowed = true

function Votes_not_the_same_map(vote_descriptor)
    local map = Config.sv_map
    if vote_descriptor.command == "sv_map " .. map then
        return false
    end
    if vote_descriptor.command == "change_map " .. map then
        return false
    end
    if vote_descriptor.command == "queue_map " .. map then
        return false
    end

    return true
end

function Votes_fun_round_condition()
    if active_submod ~= Submod.Classic then
        return false
    end

    return Votes.fun_round_allowed
end

function Votes_submod_is_custom()
    return active_submod ~= Submod.Classic
end

---@param description string The vote description text
---@param command string The vote command to execute on voted yes
function Votes_add_vote(description, command, conditions)
    local vote_descriptor = {}
    vote_descriptor["command"] = command
    vote_descriptor["description"] = description
    vote_descriptor["conditions"] = conditions
    table.insert(scripted_votes, vote_descriptor)
end

function Votes_update_votes()
    for i,vote_descriptor in ipairs(scripted_votes) do
        local allowed = true
        for j,condition_cb in ipairs(vote_descriptor.conditions) do
            if not condition_cb(vote_descriptor) then
                allowed = false
            end
        end
        if allowed then
            Game.Context:AddVote(vote_descriptor.description, vote_descriptor.command)
        else
            Game.Context:RemoveVote(vote_descriptor.command)
        end
    end
end

function setup_default_votes()
    Votes_add_vote("Queue fun round", "queue_fun_round", {Votes_fun_round_condition})
end

function setup_submods_votes()
    Votes_add_vote("Go back to normal game", "exec normalize.cfg", {Votes_submod_is_custom})
end

function Votes_on_round_started(round_type_str)
    print("votes_on_round_started", round_type_str)
    if round_type_str == "fun" then
        Votes.fun_round_allowed = false
    end
    Votes_update_votes()
end

if Votes_cb_registered == nil then
    on_event("on_loaded", Votes_update_votes)
    on_event("on_round_started", Votes_on_round_started)
    Votes_cb_registered = true
end

function init_votes()
    scripted_votes = {}

    setup_default_votes()
    setup_submods_votes()
end

if scripted_votes == nil then
    init_votes()
end

print("Votes v1.0 loaded")
