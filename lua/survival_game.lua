-- Snippet for LSP:
-- local infclass = require("library.Infclass")
-- local Game = infclass.Game
-- local Config = infclass.Config

Submod.SurvivalGame = "survival"
active_submod = Submod.SurvivalGame

DebugLevel1 = 1

Player_limit = Config.inf_survival_player_limit ~= 0 and Config.inf_survival_player_limit ~= nil
if Config.inf_survival_player_limit == nil then
    print("Warning: 'Config.inf_survival_player_limit' is a nil value")
end

function dbg_msg(level, text)
    if debug_messages_target ~= nil then
        Game.Context:SendChatTarget(debug_messages_target, string.format("Lua: %s", text))
    end
end

function add_tweak(bot_conf, tweak)
    local tweaks = bot_conf:GetTweaks()
    tweaks:Add(tweak)
end

function get_seen_enemy_seconds_ago(player_id)
    local bot_player = Game.Controller:GetBot(player_id)
    if bot_player == nil then
        return 1000
    end
    local tick = bot_player.LastSeenTargetAtTick
    if tick == nil then
        return 1000
    end
    local current_tick = Game.Server.Tick
    return (current_tick - tick) / 50
end

if Survival_game_initialized == nil then
    Survival_game_initialized = true

    survival_difficulty_level = 0
    survival_allow_extra_players = 0
    survival_max_players = 0
    survival_players = 0
    Survival_current_wave = 0
    survival_hp_multiplier = 1
    survival_max_drop_level = 2
    Survival_witch_id = nil
    Survival_witch_spawned_waves = 0
    Survival_witch_next_check_time = nil

    survival_default_tweaks = nil

    Survival_tag_witch = "witch"

    OldConfig = {}

    Survival_spawn_zones = nil
end

---@return SurvivalBotConfiguration
function add_bot_with_tweaks(wave, player_class)
    local bot_conf = Game.Controller:SurvivalAddBot(wave, player_class)
    if survival_default_tweaks ~= nil then
        local tweaks = bot_conf:GetTweaks()
        for i, default_tweak in ipairs(survival_default_tweaks) do
            tweaks:Add(default_tweak)
        end
    end

    return bot_conf
end

---@return SurvivalBotConfiguration
function add_normal_infected(wave, player_class)
    local bot_conf = add_bot_with_tweaks(wave, player_class)
    bot_conf.HP = 10 * survival_hp_multiplier
    return bot_conf
end

---@return SurvivalBotConfiguration
function add_boss_infected(wave, player_class)
    local bot_conf = Game.Controller:SurvivalAddBot(wave, player_class)
    bot_conf.Tag = "boss"
    bot_conf.Lives = 1
    return bot_conf
end

---@param wave number
---@param player_class string
---@param witch_id number
---@return SurvivalBotConfiguration
function add_witch_infected(wave, player_class, witch_id)
    local bot_conf = add_bot_with_tweaks(wave, player_class)
    bot_conf.HP = 10 * survival_hp_multiplier
    bot_conf.SpawnWitchId = witch_id
    bot_conf.Lives = 1
    return bot_conf
end

function setup_wave1()
    print("Setup wave1 with difficulty", survival_difficulty_level)
    local wave = 1
    local bot_conf = nil
    local next_spawn_second
    local next_spawn_lives = Config.inf_bot_lives

    for i = 1, 4 do
        bot_conf = add_normal_infected(wave, "bat")
    end

    if survival_difficulty_level >= 4 then
        for i = 1, 2 do
            bot_conf = add_normal_infected(wave, "bat")
            bot_conf.SpawnSecond = 5
        end
        if survival_difficulty_level >= 5 then
            for i = 1, 2 do
                bot_conf = add_normal_infected(wave, "bat")
                bot_conf.SpawnSecond = 7
                -- Limit the number of the late boring bats by 4
                bot_conf.Lives = 4
            end
        end
    else
        -- 1-3 players
        for i = 1, 2 do
            bot_conf = add_normal_infected(wave, "bat")
            bot_conf.SpawnSecond = 10
        end
    end

    if survival_difficulty_level >= 5 then
        -- Spawn extra strong bats
        local bat_hp = 60 + 20 * (survival_difficulty_level - 4)
        -- lvl5 - 80hp
        -- lvl6 - 100hp

        for i = 1, 2 do
            bot_conf = add_normal_infected(wave, "bat")
            bot_conf.SpawnSecond = 10
            bot_conf.Lives = 1
            bot_conf.HP = bat_hp * survival_hp_multiplier
        end

        -- Reduce the number of the late boring bats:
        next_spawn_lives = 2
    end

    if survival_difficulty_level <= 2 then
        next_spawn_second = 20
    else
        next_spawn_second = 12
    end

    for i = 1, 4 do
        bot_conf = add_normal_infected(wave, "bat")
        bot_conf.SpawnSecond = next_spawn_second
        bot_conf.Lives = next_spawn_lives
    end
end

function setup_wave2()
    print("Setup wave2 with difficulty", survival_difficulty_level)
    local wave = 2
    local bot_conf = nil

    bot_conf = add_normal_infected(wave, "smoker")
    bot_conf = add_normal_infected(wave, "voodoo")

    if survival_difficulty_level > 1 then
        for i = 1, 2 do
            bot_conf = add_normal_infected(wave, "spider")
            bot_conf.SpawnSecond = 2 + i * 0.2
        end

        bot_conf = add_normal_infected(wave, "boomer")
        bot_conf.SpawnSecond = 5
    end

    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "hunter")
        bot_conf.SpawnSecond = 5 + i * 0.3
    end

    local lvl1_slugs = { 10, 20 }
    local lvl2_slugs = { 7, 15, 25 }
    local lvl3_slugs = { 7, 15, 20 }
    local lvl4_slugs = { 7, 10, 15, 20 }
    local lvl5_slugs = { 7, 10, 15, 20 }
    local lvl6_slugs = { 7, 10, 12, 15, 17, 20 }
    local slugs_spawns = { lvl1_slugs, lvl2_slugs, lvl3_slugs, lvl4_slugs, lvl5_slugs, lvl6_slugs }

    for i, slug_spawn_time in ipairs(slugs_spawns[survival_difficulty_level]) do
        bot_conf = add_normal_infected(wave, "slug")
        bot_conf.SpawnSecond = slug_spawn_time
    end

    bot_conf = add_boss_infected(wave, "spider")
    bot_conf.SpawnSecond = 25 - survival_difficulty_level

    local boss_hp = { 60, 80, 120, 180, 240, 240 }

    bot_conf.HP = boss_hp[survival_difficulty_level] * survival_hp_multiplier
    bot_conf.DropLevel = 1
    add_tweak(bot_conf, "threat-aware")
    add_tweak(bot_conf, "strong-hook")
    add_tweak(bot_conf, "can-flee")
end

function setup_wave3()
    print("Setup wave3 with difficulty", survival_difficulty_level)
    local wave = 3
    local bot_conf = nil

    bot_conf = add_normal_infected(wave, "smoker")
    bot_conf = add_normal_infected(wave, "hunter")
    bot_conf = add_normal_infected(wave, "bat")
    bot_conf = add_normal_infected(wave, "slug")

    bot_conf = add_normal_infected(wave, "boomer")
    bot_conf.SpawnSecond = 5

    bot_conf = add_normal_infected(wave, "voodoo")
    bot_conf.SpawnSecond = 5

    local ghost_lives = 2
    if survival_difficulty_level > 1 then
        ghost_lives = 3
    end

    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "ghost")
        bot_conf.SpawnSecond = 12 + i * 0.3
        bot_conf.RespawnInterval = 2.5
        bot_conf.Lives = ghost_lives
        bot_conf.HP = 2
    end

    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "ghost")
        bot_conf.SpawnSecond = 15 + i * 0.3
        bot_conf.RespawnInterval = 2.5
        bot_conf.Lives = ghost_lives
        bot_conf.HP = 2
    end

    local boss_hp = { 80, 120, 180, 220, 240, 240 }
    local spawns = 1
    if survival_difficulty_level >= 6 then
        spawns = 2
    end

    for i = 1, spawns do
        bot_conf = add_boss_infected(wave, "ghost")
        bot_conf.SpawnSecond = 20
        bot_conf.HP = boss_hp[survival_difficulty_level] * survival_hp_multiplier
        bot_conf.DropLevel = 1
        add_tweak(bot_conf, "threat-aware")
    end

    for i = 1, 3 do
        bot_conf = add_normal_infected(wave, "ghost")
        bot_conf.SpawnSecond = 22 + i * 0.3
        bot_conf.RespawnInterval = 0.5
        bot_conf.Lives = ghost_lives
        bot_conf.HP = 2
    end
end

function setup_wave4()
    local wave = 4
    local bot_conf = nil

    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "smoker")
        bot_conf = add_normal_infected(wave, "hunter")
    end

    for i = 1, 2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "spider")
    end
    bot_conf = add_normal_infected(wave, "slug")

    for i = 1, 2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
        bot_conf.SpawnSecond = 15
        add_tweak(bot_conf, "strong-hook")
    end

    bot_conf = add_boss_infected(wave, "witch")
    bot_conf.SpawnSecond = 20
    local boss_hp = { 80, 120, 180, 240, 300, 360 }
    bot_conf.HP = boss_hp[survival_difficulty_level] * survival_hp_multiplier
    bot_conf.DropLevel = 2
    bot_conf.Tag = Survival_tag_witch
    add_tweak(bot_conf, "threat-aware")

    if survival_difficulty_level > 1 then
        local n = survival_difficulty_level - 1
        for i = 1, n do
            bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
            bot_conf.SpawnSecond = 20 + i * 0.2
            bot_conf.RespawnInterval = 2
            add_tweak(bot_conf, "strong-hook")
        end
    end
end

function setup_wave5()
    local wave = 5
    local bot_conf = nil

    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "smoker")
        bot_conf = add_normal_infected(wave, "hunter")
        bot_conf = add_normal_infected(wave, "spider")
    end

    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "slug")
    end
    bot_conf.RespawnInterval = 4

    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "boomer")
    end
    bot_conf.RespawnInterval = 4

    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "voodoo")
    end
    bot_conf.RespawnInterval = 4

    local ghouls_hp = { 40, 40, 60, 60, 80, 80 }

    local lvl1_ghouls = { 10 }
    local lvl2_ghouls = { 10, 20 }
    local lvl3_ghouls = { 10, 15 }
    local lvl4_ghouls = { 7, 15, 20 }
    local lvl5_ghouls = { 7, 10, 15, 20 }
    local lvl6_ghouls = { 7, 10, 15, 20 }
    local lvl7_ghouls = { 7, 10, 12, 15, 17, 20 }
    local ghoul_spawns = { lvl1_ghouls, lvl2_ghouls, lvl3_ghouls, lvl4_ghouls, lvl5_ghouls, lvl6_ghouls, lvl7_ghouls }

    for i, ghoul_spawn_time in ipairs(ghoul_spawns[survival_difficulty_level]) do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "ghoul")
        bot_conf.SpawnSecond = ghoul_spawn_time
        bot_conf.HP = ghouls_hp[survival_difficulty_level] * survival_hp_multiplier
        bot_conf.Lives = 1
    end
    -- Give a drop to the last ghoul
    bot_conf.DropLevel = 2

    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "ghost")
        bot_conf.SpawnSecond = 20
    end

    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "bat")
        bot_conf.SpawnSecond = 35
        bot_conf.HP = 4
    end

    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "bat")
        bot_conf.SpawnSecond = 40
        bot_conf.HP = 4
    end
end

function setup_wave6()
    local wave = 6
    local bot_conf = nil

    local set_zone = do_nothing

    if Survival_spawn_zones ~= nil then
        local zones_count = table.getn(Survival_spawn_zones)

        local zone = Survival_spawn_zones[math.random(1, zones_count)]
        local spawns_count = table.getn(zone)

        set_zone = function(bot_conf)
            local spawn_point_id = zone[math.random(1, spawns_count)]
            bot_conf.SpawnPointId = spawn_point_id
        end
    end

    local spawn_second = 1
    local ghosts_count = 3 + survival_difficulty_level
    for i = 1, ghosts_count do
        bot_conf = add_normal_infected(wave, "ghost")
        bot_conf.Lives = 2
        bot_conf.SpawnSecond = spawn_second
        bot_conf.RespawnInterval = 1
        set_zone(bot_conf)
        spawn_second = spawn_second + 0.1
    end

    if survival_difficulty_level >= 4 then
        for i = 1, 2 do
            bot_conf = add_normal_infected(wave, "smoker")
            bot_conf.SpawnSecond = 5
            bot_conf.Lives = 2
        end
        for i = 1, 2 do
            bot_conf = add_normal_infected(wave, "spider")
            bot_conf.SpawnSecond = 5
            bot_conf.Lives = 2
        end
        for i = 1, 2 do
            bot_conf = add_normal_infected(wave, "hunter")
            bot_conf.SpawnSecond = 5
            bot_conf.Lives = 2
        end
    end

    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "slug")
        bot_conf.SpawnSecond = 10
        bot_conf.Lives = 2
    end
    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "boomer")
        bot_conf.SpawnSecond = 10
        bot_conf.Lives = 2
    end
    for i = 1, 2 do
        bot_conf = add_normal_infected(wave, "voodoo")
        bot_conf.SpawnSecond = 10
        bot_conf.Lives = 2
    end

    local max_drop_level = survival_max_drop_level

    local boss_hp = { 180, 240, 320, 520, 640, 720 }
    bot_conf = add_boss_infected(wave, "tank")
    bot_conf.SpawnSecond = 25
    bot_conf.HP = boss_hp[survival_difficulty_level] * survival_hp_multiplier
    bot_conf.DropLevel = max_drop_level

    local after_tank_sec = 50

    if Survival_spawn_zones ~= nil then
        local zones_count = table.getn(Survival_spawn_zones)

        local zone = Survival_spawn_zones[math.random(1, zones_count)]
        local spawns_count = table.getn(zone)

        set_zone = function(bot_conf)
            local spawn_point_id = zone[math.random(1, spawns_count)]
            bot_conf.SpawnPointId = spawn_point_id
        end
    end

    local pre_witch_sec = after_tank_sec

    local infected_n = 2

    for i = 1, infected_n do
        bot_conf = add_normal_infected(wave, "slug")
        bot_conf.SpawnSecond = pre_witch_sec + i * 0.2
        set_zone(bot_conf)
    end
    for i = 1, infected_n do
        bot_conf = add_normal_infected(wave, "spider")
        bot_conf.SpawnSecond = pre_witch_sec + 1 + i * 0.2
        set_zone(bot_conf)
    end
    for i = 1, infected_n do
        bot_conf = add_normal_infected(wave, "boomer")
        bot_conf.SpawnSecond = pre_witch_sec + 2 + i * 0.2
        set_zone(bot_conf)
    end
    for i = 1, infected_n do
        bot_conf = add_normal_infected(wave, "hunter")
        bot_conf.SpawnSecond = pre_witch_sec + 2 + i * 0.2
        set_zone(bot_conf)
    end
    for i = 1, infected_n do
        bot_conf = add_normal_infected(wave, "bat")
        bot_conf.SpawnSecond = pre_witch_sec + 2 + i * 0.1
        set_zone(bot_conf)
    end

    if survival_difficulty_level >= 3 then
        local n = survival_difficulty_level - 2
        for i = 1, n do
            bot_conf = add_normal_infected(wave, "smoker")
            bot_conf.SpawnSecond = pre_witch_sec + 5 + i * 0.4
            set_zone(bot_conf)
        end
        for i = 1, n do
            bot_conf = add_normal_infected(wave, "voodoo")
            bot_conf.SpawnSecond = pre_witch_sec + 5 + i * 0.5
            set_zone(bot_conf)
        end

        local witch_hp = { 0, 0, 80, 120, 180, 240 }
        bot_conf = Game.Controller:SurvivalAddBot(wave, "witch")
        bot_conf.SpawnSecond = after_tank_sec + 2
        bot_conf.Lives = 1
        bot_conf.HP = witch_hp[survival_difficulty_level] * survival_hp_multiplier
        bot_conf.Tag = Survival_tag_witch
        add_tweak(bot_conf, "threat-aware")
        add_tweak(bot_conf, "can-flee")
        set_zone(bot_conf)
    end
end

---@return number
function Survival_get_witch_horde_interval()
    if Survival_current_wave < 5 then
        local intervals = { 10, 8, 7, 7, 6, 6, 5 }
        return intervals[survival_difficulty_level]
    end

    local intervals = { 6, 5, 4, 3, 2, 1, 1 }
    return intervals[survival_difficulty_level]
end

---@param start_second number
function Survival_spawn_initial_witch_wave(start_second, witch_id)
    dbg_msg(DebugLevel1, "Survival_spawn_initial_witch_wave")
    local wave = Survival_current_wave

    local spawned_count = 0

    local function add_infected(infected_class)
        local bot_conf = add_witch_infected(wave, infected_class, witch_id)
        bot_conf.SpawnSecond = start_second
        spawned_count = spawned_count + 1
        return bot_conf
    end

    local classes = {
        "smoker", "smoker", "bat", "bat",
        "hunter", "hunter", "voodoo", "voodoo",
        "hunter", "smoker", "bat", "voodoo",
        "spider", "spider", "smoker", "smoker",
        "voodoo", "voodoo", "boomer", "boomer",
        "slug", "slug", "bat", "bat",
    }
    local wanted_count = survival_difficulty_level * 4
    for i, infected_class in ipairs(classes) do
        add_infected(infected_class)
        if spawned_count >= wanted_count then
            return
        end
    end
end

---@param start_second number
function Survival_spawn_hit_witch_wave(start_second, witch_id)
    dbg_msg(DebugLevel1, "Survival_spawn_hit_witch_wave")
    local wave = Survival_current_wave

    local spawned_count = 0

    local function add_infected(infected_class)
        local bot_conf = add_witch_infected(wave, infected_class, witch_id)
        bot_conf.SpawnSecond = start_second
        spawned_count = spawned_count + 1
        return bot_conf
    end

    local classes = {
        "smoker", "smoker", "bat", "bat",
        "hunter", "hunter", "voodoo", "voodoo",
        "hunter", "smoker", "bat", "voodoo",
        "spider", "spider", "smoker", "smoker",
        "voodoo", "voodoo", "boomer", "boomer",
        "slug", "slug", "bat", "bat",
    }
    local wanted_count = survival_difficulty_level * 2
    for i, infected_class in ipairs(classes) do
        add_infected(infected_class)
        if spawned_count >= wanted_count then
            return
        end
    end
end

---@param start_second number
function Survival_spawn_severely_hit_witch_wave(start_second, witch_id)
    dbg_msg(DebugLevel1, "Survival_spawn_severely_hit_witch_wave")
    local wave = Survival_current_wave

    local spawned_count = 0

    local function add_infected(infected_class)
        local bot_conf = add_witch_infected(wave, infected_class, witch_id)
        bot_conf.SpawnSecond = start_second
        spawned_count = spawned_count + 1
        return bot_conf
    end

    local classes = {
        "smoker", "smoker", "bat", "bat",
        "hunter", "hunter", "voodoo", "voodoo",
        "hunter", "smoker", "bat", "voodoo",
        "spider", "spider", "smoker", "smoker",
        "voodoo", "voodoo", "boomer", "boomer",
        "slug", "slug", "bat", "bat",
    }
    local wanted_count = survival_difficulty_level * 3
    for i, infected_class in ipairs(classes) do
        add_infected(infected_class)
        if spawned_count >= wanted_count then
            return
        end
    end
end

---@param start_second number
function Survival_spawn_critical_hp_witch_wave(start_second, witch_id)
    dbg_msg(DebugLevel1, "Survival_spawn_critical_hp_witch_wave")
    local wave = Survival_current_wave

    local spawned_count = 0

    local function add_infected(infected_class)
        local bot_conf = add_witch_infected(wave, infected_class, witch_id)
        bot_conf.SpawnSecond = start_second
        spawned_count = spawned_count + 1
        return bot_conf
    end

    local classes = {
        "bat", "bat", "bat", "bat",
        "boomer", "boomer", "boomer", "boomer",
        "ghoul", "ghoul", "slug", "slug",
        "ghost", "ghost", "ghost", "ghost",
        "slug", "ghost", "ghost", "ghost",
        "ghost", "ghost", "ghost", "ghost",
    }
    local wanted_count = survival_difficulty_level * 4
    for i, infected_class in ipairs(classes) do
        local bot_conf = add_infected(infected_class)
        if i > 4 then
            bot_conf.SpawnSecond = start_second + i * 0.1
        end
        if bot_conf.Class == "ghoul" then
            bot_conf.HP = survival_difficulty_level * 10
            bot_conf.SpawnSecond = start_second + 3 + i * 0.1
        end
        if spawned_count >= wanted_count then
            return
        end
    end
end

-- Check the witch status and maybe spawn a wave
function Survival_check_the_witch(witch_id)
    dbg_msg(DebugLevel1, "Checking the witch")
    local start_second = Game.Controller:GetSecondsAfterInfection() + 0.125
    if Survival_witch_prev_spawn_time == nil then
        Survival_spawn_initial_witch_wave(start_second, witch_id)
        Survival_witch_prev_spawn_time = start_second
        return
    end

    local witch_hp_rate = get_character_hp_rate(witch_id)
    if Survival_current_wave < 5 then
        if Survival_witch_hp_rate > 0.4 and witch_hp_rate <= 0.4 then
            Survival_spawn_severely_hit_witch_wave(start_second, witch_id)
            Survival_witch_prev_spawn_time = start_second
            Survival_witch_hp_rate = witch_hp_rate
            return
        end
    else
        if Survival_witch_hp_rate > 0.6 and witch_hp_rate <= 0.6 then
            Survival_spawn_severely_hit_witch_wave(start_second, witch_id)
            Survival_witch_prev_spawn_time = start_second
            Survival_witch_hp_rate = witch_hp_rate
            return
        end
        if Survival_witch_hp_rate > 0.25 and witch_hp_rate <= 0.25 then
            Survival_spawn_critical_hp_witch_wave(start_second, witch_id)
            Survival_witch_prev_spawn_time = start_second
            Survival_witch_hp_rate = witch_hp_rate
            return
        end
    end

    local interval = Survival_get_witch_horde_interval()
    if start_second > Survival_witch_prev_spawn_time + interval then
        local damaged = witch_hp_rate < Survival_witch_hp_rate
        local seen_enemy = get_seen_enemy_seconds_ago(witch_id) < 1.0
        if damaged or seen_enemy then
            Survival_spawn_hit_witch_wave(start_second, witch_id)
            Survival_witch_prev_spawn_time = start_second
            Survival_witch_hp_rate = witch_hp_rate
        end
    end
end

function Survival_schedule_next_witch_check()
    Survival_witch_next_check_time = Game.Controller:GetSecondsAfterInfection() + 0.5
end

function on_control_point_effect(control_point)
    if control_point:IsInfected() then
        return
    end
    ---@param character CIcCharacter
    local function give_bonus(character)
        character:GiveArmor(2, -1)
    end
    for_each_human_character(give_bonus)
end

function get_max_players_for_difficulty(difficulty, multiplier)
    if not Player_limit then
        return 64
    end
    local max_players = difficulty + 2 * multiplier
    local hard_max = 4 + multiplier * 3
    if max_players > hard_max then
        max_players = hard_max
    end

    return max_players + survival_allow_extra_players
end

function update_max_players()
    survival_max_players = get_max_players_for_difficulty(survival_difficulty_level, survival_hp_multiplier)

    local game_conf = Game.Controller:SurvivalGetGameConfiguration()
    game_conf.MaxPlayers = survival_max_players
end

function update_difficulty()
    survival_default_tweaks = nil
    if survival_difficulty_level <= 4 then
        survival_default_tweaks = { "weak-hook" }
    end

    if survival_difficulty_level <= 2 then
        Config.inf_bot_lives = 2
        Config.inf_survival_infected_spawning_delay = 5
        Config.inf_stunning_hammer_duration = 0.5
    elseif survival_difficulty_level == 3 then
        Config.inf_bot_lives = 3
        Config.inf_survival_infected_spawning_delay = 4
        Config.inf_stunning_hammer_duration = 0.75
    elseif survival_difficulty_level == 4 then
        Config.inf_bot_lives = 4
        Config.inf_survival_infected_spawning_delay = 4
        Config.inf_stunning_hammer_duration = 0.85
    else
        Config.inf_bot_lives = 5
        Config.inf_survival_infected_spawning_delay = 3
        Config.inf_stunning_hammer_duration = 1.0
    end

    if survival_difficulty_level <= 3 then
        Config.inf_hive_hooks = 2
    else
        Config.inf_hive_hooks = 3
    end

    local game_conf = Game.Controller:SurvivalGetGameConfiguration()
    game_conf.Hardmode = false

    if survival_difficulty_level >= 6 then
        survival_max_drop_level = 3
    else
        survival_max_drop_level = 2
    end
end

function setup_wave_n(wave_number)
    if wave_number == 1 then
        setup_wave1()
    elseif wave_number == 2 then
        setup_wave2()
    elseif wave_number == 3 then
        setup_wave3()
    elseif wave_number == 4 then
        setup_wave4()
    elseif wave_number == 5 then
        setup_wave5()
    elseif wave_number == 6 then
        setup_wave6()
    end
end

function setup_this_wave()
    setup_wave_n(Survival_current_wave)
end

function Survival_on_tick()
    local tick = Game.Server.Tick
    if (tick + 1) == Game.Controller.InfectionStartTick then
        Survival_current_wave = Survival_current_wave + 1
        setup_this_wave()
    end

    local current_second = Game.Controller:GetSecondsAfterInfection()
    if Survival_witch_next_check_time ~= nil then
        if current_second >= Survival_witch_next_check_time then
            Survival_check_the_witch(Survival_witch_id)
            Survival_schedule_next_witch_check()
        end
    end
end

function Survival_on_character_spawned(player_id, spawn_type)
    local spawned_character = Game.Controller:GetCharacter(player_id)
    if spawned_character == nil then
        return
    end

    if spawned_character:IsInfected() == false then
        return
    end

    local spawned_player = Game.Controller:GetPlayer(player_id)
    if spawned_player.Tag == Survival_tag_witch then
        Survival_witch_id = player_id
        Survival_witch_prev_spawn_time = nil
        Survival_witch_hp_rate = 1
        Survival_check_the_witch(Survival_witch_id)
        Survival_schedule_next_witch_check()
    end
end

function Survival_on_character_death(victim_id, killer_id, weapon_str)
    if victim_id == Survival_witch_id then
        Survival_witch_next_check_time = nil
    end
end

function Survival_set_difficulty(base_difficulty, hp_multiplier)
    survival_difficulty_level = base_difficulty
    survival_hp_multiplier = hp_multiplier

    update_difficulty()
    update_max_players()
end

function start_survival_game(base_difficulty, hp_multiplier)
    local game_conf = Game.Controller:SurvivalGetGameConfiguration()
    game_conf:Reset()

    Survival_set_difficulty(base_difficulty, hp_multiplier)

    Game.Controller:SurvivalAddWave(1, "The Meeting")
    Game.Controller:SurvivalAddWave(2, "The Green Smoke")
    Game.Controller:SurvivalAddWave(3, "Who's there?")
    Game.Controller:SurvivalAddWave(4, "The Gathering")
    Game.Controller:SurvivalAddWave(5, "The Tomato")
    Game.Controller:SurvivalAddWave(6, "The Goodbye")

    Survival_current_wave = 0

    Game.Controller:PrepareSurvival()
    Game.Controller:QueueRound("survival")
    Game.Controller:DoWarmup(3)
    if Player_limit then
        Game.Context:SendChatTarget(-1,
            string.format("Starting survival game (difficulty %d, max players %d)", survival_difficulty_level,
                survival_max_players))
    else
        Game.Context:SendChatTarget(-1,
            string.format("Starting survival game (difficulty %d)", survival_difficulty_level))
    end
end

function start_survival_auto()
    local players_number = Game.Controller:GetPlayersNumber()
    local total = players_number.Humans + players_number.Infected

    survival_players = total

    start_survival_game(survival_players)
end

function survival_init()
    OldConfig.inf_enable_tranquilizer_rifle = Config.inf_enable_tranquilizer_rifle
    OldConfig.inf_tranquilizer_dose = Config.inf_tranquilizer_dose
    OldConfig.inf_proba_spawn_near_witch = Config.inf_proba_spawn_near_witch
    OldConfig.inf_stunning_hammer_duration = Config.inf_stunning_hammer_duration

    Config.inf_enable_tranquilizer_rifle = 1
    Config.inf_tranquilizer_dose = 7.5
    Config.inf_proba_spawn_near_witch = 0

    -- Kill-based survival
    Config.inf_survival_mode = 1
    Config.inf_white_hole_num_particles = 60
    Config.inf_white_hole_radius = 240

    Config.sv_vote_spectate = 1
end

---@param reason string
function Survival_on_round_end(reason)
    if reason ~= "finished" then
        return
    end

    if Survival_current_wave == 6 then
        return
    end


    ---@param character CIcCharacter
    local function give_bonus(character)
        local gain = { 10, 8, 6, 4, 2, 2 }
        character:IncreaseOverallHp(gain[survival_difficulty_level])
    end
    for_each_human_character(give_bonus)
end

function Survival_on_shutdown()
    for key, value in pairs(OldConfig) do
        Config[key] = value
    end

    Config.inf_survival_mode = 0
    Config.inf_survival_hardmode = 0
    Config.inf_white_hole_num_particles = 100
    Config.inf_white_hole_radius = 430

    Config.sv_vote_spectate = 0

    survival_remove_votes()
end

function Survival_init()
    Survival_initialized = true
    survival_init()

    on_event("on_tick", Survival_on_tick)
    on_event("on_round_end", Survival_on_round_end)
    on_event("on_shutdown", Survival_on_shutdown)
    on_event("on_character_spawned", Survival_on_character_spawned)
    on_event("on_character_death", Survival_on_character_death)
end

if Survival_initialized == nil then
    Survival_init()
end

function survival_remove_votes()
    for i = 1, 6 do
        local vote_command = string.format("lua start_survival_game(%d, 1)", i)
        Game.Context:RemoveVote(vote_command)
    end
    for i = 1, 6 do
        local vote_command = string.format("lua start_survival_game(%d, 2)", i)
        Game.Context:RemoveVote(vote_command)
    end
end

---@param difficulty number
---@param multiplier number
---@return string
function Get_vote_name(difficulty, multiplier)
    if multiplier == 1 then
        if Player_limit then
            return string.format("Start survival (%d, %d players max)", difficulty,
                get_max_players_for_difficulty(difficulty, multiplier))
        else
            return string.format("Start survival (difficulty %d)", difficulty)
        end
    else
        if Player_limit then
            return string.format("Start survival (%d, %dx HP, %d players max)", difficulty, multiplier,
                get_max_players_for_difficulty(difficulty, multiplier))
        else
            return string.format("Start survival (difficulty %d, %dx HP)", difficulty, multiplier)
        end
    end
end

function survival_setup_votes()
    local vote_index = 0
    local multiplier = 1
    for i = 1, 6 do
        local vote_name = Get_vote_name(i, multiplier)
        local vote_command = string.format("lua start_survival_game(%d, %d)", i, multiplier)
        Game.Context:RemoveVote(vote_command)
        Game.Context:InsertVote(vote_index, vote_name, vote_command)
        vote_index = vote_index + 1
    end
    multiplier = 2
    for i = 4, 6 do
        local vote_name = Get_vote_name(i, multiplier)
        local vote_command = string.format("lua start_survival_game(%d, %d)", i, multiplier)
        Game.Context:RemoveVote(vote_command)
        Game.Context:InsertVote(vote_index, vote_name, vote_command)
        vote_index = vote_index + 1
    end
end

function set_extra_players(num)
    survival_allow_extra_players = num
    survival_setup_votes()
    update_max_players()
end

print("Survival Game runtime loaded")

survival_setup_votes()
