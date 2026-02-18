-- Snippet for LSP:
if false then
    infclass = require("library.Infclass")
    Game = infclass.Game
    Config = infclass.Config
end

require("runtime.animations")

Submod.Zowling = "zowling"
active_submod = Submod.Zowling

base_x = 16 * 32
Zowling_mines_max_x = 54 * 32
laser_top_left = vec2(61 * 32, 10 * 32)
Zowling_first_mines_delay = 3
zowling_min_interval = 30
zowling_max_interval = 0
Zowling_interval = 0
Zowling_velocity = 0
zowling_min_velocity = 0
zowling_max_velocity = 0
Zowling_next_spawn_tick = 0
Zowling_next = false
Zowling_class = "smoker"
zowling_objects = {}

Zowling_autostart = false
Zowling_give_scores_fn = nil

OldConfig = {}

function for_each_infected_character(callback)
    ---@param ch CInfClassCharacter
    ---@return boolean
    local function acceptable(ch)
        if ch == nil then
            return false
        end
        if not ch:IsInfected() or not ch.IsAlive then
            return false
        end

        return true
    end

    for i = 0, 63 do
        local character = Game.Controller:GetCharacter(i)
        if acceptable(character) then
            callback(character)
        end
    end
end

function Zowling_preset_normal()
    zowling_max_interval = 75
    zowling_min_velocity = 6
    zowling_max_velocity = 10
end

function Zowling_preset_fast()
    zowling_max_interval = 50
    zowling_min_velocity = 8
    zowling_max_velocity = 12
end

function Zowling_start()
    Zowling_next = true
    Game.Controller:StartRound()
end

function spawn_ball()
    local ball = Game.Controller:AddSciMine()
    ball.Lifespan = (Zowling_mines_max_x - base_x) / Zowling_velocity / 50.0
    ball.Velocity = vec2(Zowling_velocity, 0)
    -- ball.ProximityRadius = 46 + math.random(-2, 3) * 4
    return ball
end

function spawn_high()
    local ball = spawn_ball()
    ball.Position = vec2(base_x, 10 * 32)
    return ball
end

function spawn_mid()
    local ball = spawn_ball()
    ball.Position = vec2(base_x, 13 * 32)
    return ball
end

function spawn_low()
    local ball = spawn_ball()
    ball.Position = vec2(base_x, 17 * 32)
    return ball
end

function spawn_one()
    local ind = math.random(1, 3)
    if ind == 1 then
        spawn_low()
    elseif ind == 2 then
        spawn_mid()
    else
        spawn_high()
    end
end

function spawn_two()
    local ind = math.random(1, 3)
    if ind == 1 then
        spawn_mid()
        spawn_high()
    elseif ind == 2 then
        spawn_mid()
        spawn_low()
    else
        spawn_high()
        spawn_low()
    end
end

function Zowling_give_scores()
    local tick = Game.Controller.InfectionTick
    local bonus = 3
    if tick > 2000 then
        bonus = 5
    end
    ---@param character CInfClassCharacter
    local function give_bonus(character)
        if not character:IsAlive() or character:IsFrozen() then
            return
        end
        local cid = character.CID
        local player = Game.Controller:GetPlayer(cid)
        player.Score = player.Score + bonus
    end
    for_each_infected_character(give_bonus)
end

function Zowling_on_tick()
    local tick = Game.Controller.InfectionTick
    Game.Controller.VotesEnabled = tick < (50 * 20)
    if tick < 50 then
        return
    end
    Game.Controller.RoundMinimumInfected = 0
    if tick % 750 == 0 then
        if Zowling_interval > 30 then
            Zowling_interval = Zowling_interval - 3
        else
            Zowling_interval = Zowling_interval - 1
        end

        if Zowling_interval < zowling_min_interval then
            Zowling_interval = zowling_min_interval
        end
    end
    if tick % 1000 == 0 then
        if Zowling_velocity < zowling_max_velocity then
            Zowling_velocity = Zowling_velocity + 0.25
        end
    end
    if tick < 2000 then
        if tick % 400 == 0 then
            spawn_low()
        end
    end
    if tick >= Zowling_next_spawn_tick then
        Zowling_next_spawn_tick = Zowling_next_spawn_tick + Zowling_interval
        if tick > 2000 then
            spawn_two()
        else
            spawn_one()
        end
        Zowling_give_scores_fn()
    end
    if tick == 3000 then
        local laser = Game.Controller:AddLaserWall()
        laser.Position = laser_top_left
        laser.SecondPosition = vec2(laser_top_left.x - 8 * 32, laser_top_left.y)
        laser.Velocity = vec2(0, 0.3)
        laser.Lifespan = 18
    end
    if tick == 4000 then
        local laser = Game.Controller:AddLaserWall()
        laser.Position = laser_top_left
        laser.SecondPosition = vec2(laser_top_left.x, laser_top_left.y + 7 * 32)
        laser.Velocity = vec2(-0.3, 0)
        laser.Lifespan = 45
    end

    if Game.Controller.RoundTick >= Game.Controller.TimeLimitSeconds * 50 then
        Game.Controller:FinishRound()
    end
end

function Zowling_on_round_started()
    Game.Controller.WinCheckEnabled = false
    if not Zowling_autostart then
        if not Zowling_next then
            print("no zowling yet")
            Game.Controller.RoundMinimumPlayers = 60
            return
        end
        Zowling_next = false
    end
    print("Zowling_on_round_started")
    Zowling_next_spawn_tick = Zowling_first_mines_delay * 50
    Zowling_velocity = zowling_min_velocity
    Zowling_interval = zowling_max_interval
    local players_number = Game.Controller:GetPlayersNumber()
    local total = players_number.Humans + players_number.Infected

    Game.Controller.RoundMinimumPlayers = 1
    Game.Controller.RoundMinimumInfected = total
    Game.Controller.InfectionDelaySeconds = 0

    ---@param player CInfClassPlayer
    local function reset_score(player)
        player.Score = 0
    end
    for_each_player(reset_score)
end

function Zowling_on_world_reset()
    if Zowling_on_world_reset_override ~= nil then
        Zowling_on_world_reset_override()
        return
    end

    local wall = Game.Controller:AddLaserWall()
    wall.Position = vec2(17.5 * 32, 10 * 32)
    wall.SecondPosition = vec2(17.5 * 32, 16 * 32)
end

function Zowling_on_character_spawned(player_id, spawn_type)
    local player = Game.Controller:GetPlayer(player_id)
    player.Class = Zowling_class
end

function Zowling_on_character_death(victim_int, killer_int, weapon_str)
    local tick = Game.Controller.InfectionTick
    if tick < 50 then
        return
    end
    local player = Game.Controller:GetPlayer(victim_int)
    if player == nil then
        return
    end
    player.Score = player.Score - 20
end

function Zowling_setup_votes()
    Game.Context:InsertVote(0, "Start zowling (normal)", "lua start_zowling(\"normal\")")
    Game.Context:InsertVote(1, "Start zowling (fast and furious)", "lua start_zowling(\"fast\")")
end

function Zowling_init()
    OldConfig.inf_class_chooser = Config.inf_class_chooser
    OldConfig.inf_undead_freeze_duration = Config.inf_undead_freeze_duration
    OldConfig.inf_turret_dmg_health_laser = Config.inf_turret_dmg_health_laser
    OldConfig.inf_inactive_humans_kick_time = Config.inf_inactive_humans_kick_time
    OldConfig.inf_inactive_infected_kick_time = Config.inf_inactive_infected_kick_time

    Config.inf_class_chooser = 0
    Config.inf_undead_freeze_duration = 4
    Config.inf_turret_dmg_health_laser = 16

    Config.inf_inactive_humans_kick_time = 180
    Config.inf_inactive_infected_kick_time = 180

    Zowling_setup_votes()
end

function Zowling_on_shutdown()
    for key, value in pairs(OldConfig) do
        Config[key] = value
    end

    Game.Context:RemoveVote("lua start_zowling(\"normal\")")
    Game.Context:RemoveVote("lua start_zowling(\"fast\")")
end

function choose_infected_class(player)
    return Zowling_class
end

function start_zowling(speed_str)
    if speed_str == "fast" then
        Zowling_preset_fast()
    elseif speed_str == "normal" then
        Zowling_preset_normal()
    end
    Zowling_start()
end

if runtime_context.zowling_runtime_initialized == nil then
    runtime_context.zowling_runtime_initialized = true

    Zowling_give_scores_fn = Zowling_give_scores

    on_event("on_tick", Zowling_on_tick)
    on_event("on_round_started", Zowling_on_round_started)
    on_event("on_world_reset", Zowling_on_world_reset)
    on_event("on_character_death", Zowling_on_character_death)
    on_event("on_character_spawned", Zowling_on_character_spawned)
    on_event("on_shutdown", Zowling_on_shutdown)

    Game.Controller.RoundMinimumPlayers = 60
    Game.Controller.AmmoHudEnabled = false
    Config.sv_timelimit = 2

    Zowling_init()
end

print("Zowling Game runtime loaded")
