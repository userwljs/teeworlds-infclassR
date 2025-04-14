-- Snippet for LSP:
-- local infclass = require("library.Infclass")
-- local Game = infclass.Game
-- local Config = infclass.Config
-- require("runtime.base")

Submod.SurvivalGame = "survival"
active_submod = Submod.SurvivalGame

DebugLevel1 = 1
DebugLevel2 = 2

Sleeper_pos_spider = vec2(146.5 * 32, 51.5 * 32)
Sleeper_pos_witch = vec2(154 * 32, 45.5 * 32)
Sleeper_pos_tank = vec2(254 * 32, 51.5 * 32)

Castle_boss_bat_hp = {80, 120, 160, 180, 240, 240}
Castle_boss_spitter_hp = {60, 90, 160, 180, 240, 240}
Castle_boss_spider_hp = {120, 160, 220, 280, 320, 360}
Castle_boss_witch_hp = {120, 180, 320, 420, 520, 640}
Castle_boss_tank_hp = {320, 480, 640, 720, 960, 1200}

Castle_cp1_pos = vec2(116.5 * 32, 32 * 32)
Castle_cp2_pos = vec2(164.5 * 32, 63 * 32)

EBroadcastPriority = {}
EBroadcastPriority.GAMEANNOUNCE = 3

function dbg_msg(level, text)
    if debug_messages_target ~= nil then
        Game.Context:SendChatTarget(debug_messages_target, string.format("Lua: %s", text))
    end
end

-- Utils
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

if Survival_map_initialized == nil then
    Survival_map_initialized = true

    survival_difficulty_level = 0
    survival_allow_extra_players = 0
    survival_max_players = 0
    survival_players = 0
    Survival_current_wave = 1 -- Always '1' for Castle
    survival_hp_multiplier = 1
    castle_hardmode = true
    Castle_sleepers = {}
    Survival_witch_next_check_time = nil
    Castle_witch_id = 0
    Castle_spider_id = 0

    Survival_tag_bat = "boss_bat"
    Survival_tag_spider = "spider"
    Survival_tag_witch = "witch"
    Survival_tag_tank = "tank"

    Survival_spawn_zones = {}

    Castle_first_spawn = {}
    Castle_second_spawn = {}
    Castle_third_spawn = {}

    Castle_progress = 0

    survival_default_tweaks = nil

    Survival_witch_hp_rate = 0
    Survival_witch_prev_spawn_time = nil
    Survival_witch_spawned_waves = 0

    local infected_spawns = Game.Controller:GetInfectedSpawns()

    for i = 1,infected_spawns:Size() do
        local position = infected_spawns:At(i)
        if position.x < 40 * 32 then
            table.insert(Castle_first_spawn, i)
        elseif position.x < 150 * 32 then
            table.insert(Castle_second_spawn, i)
        else -- if position.x < 162 * 32 then
            table.insert(Castle_third_spawn, i)
        end
    end

    Survival_spawn_zones = {}
    table.insert(Survival_spawn_zones, Castle_first_spawn)
    table.insert(Survival_spawn_zones, Castle_second_spawn)
    table.insert(Survival_spawn_zones, Castle_third_spawn)
end

---@return SurvivalBotConfiguration
function add_bot_with_tweaks(wave, player_class)
    local bot_conf = Game.Controller:SurvivalAddBot(wave, player_class)
    if survival_default_tweaks ~= nil then
        local tweaks = bot_conf:GetTweaks()
        for i,default_tweak in ipairs(survival_default_tweaks) do
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

function Castle_reset()
    Castle_progress = 0
    Castle_sleepers = {}
    Survival_witch_next_check_time = nil
    Survival_witch_spawned_waves = 0

    Survival_witch_hp_rate = 0
    Survival_witch_prev_spawn_time = nil

    Flags_zone_left_x = 0

    survival_cp1 = Game.Controller:AddControlPoint(Castle_cp1_pos)
    survival_cp2 = Game.Controller:AddControlPoint(Castle_cp2_pos)

    remove_all_doors()
    Castle_spawn_doors()
    Flags_zone_right_x = runtime_context.doors[1].Position.x
    Castle_update_spawns()
end

---@param enabled boolean
function Castle_set_infected_spawns_enabled(spawns_list, enabled)
    for i = 1,table.getn(spawns_list) do
        local spawn_id = spawns_list[i]
        Game.Controller:SetInfectedSpawnEnabled(spawn_id, enabled)
    end
end

function Castle_update_spawns()
    ---@param value number
    ---@param min_value number
    ---@param max_value number
    ---@return boolean
    local function in_range(value, min_value, max_value)
        if value < min_value then
            return false
        elseif value > max_value then
            return false
        end

        return true
    end

    Castle_set_infected_spawns_enabled(Castle_first_spawn, Castle_progress == 0)
    Castle_set_infected_spawns_enabled(Castle_second_spawn, Castle_progress == 1)
    Castle_set_infected_spawns_enabled(Castle_third_spawn, Castle_progress == 2)
end

function Castle_spawn_doors()
    local door1 = add_piercing_door_at(79.5 * 32)
    door1:SetOpen(false)

    local door2 = add_piercing_door_at(147 * 32)
    door2:SetOpen(false)

    local door3 = add_piercing_door_at(207.5 * 32)
    door3:SetOpen(false)

    local door4 = add_piercing_door_at(258.5 * 32)
    door4:SetOpen(false)
end

function Castle_add_progress(amount)
    if Game.Controller:IsGameOver() then
        return
    end

    Castle_progress = Castle_progress + amount
    -- Game.Context:SendChatTarget(-1, string.format("New progress: %d", Castle_progress))
    for i = 1,4 do
        if Castle_progress == i then
            Castle_unlock_door_n(i)
            _G[string.format("setup_wave%d", i + 1)](Game.Controller:GetSecondsAfterInfection() + 3)
        end
    end

    Castle_update_spawns()
end

---@return boolean
function is_flag_position_suitable(position)
    if position.x > Flags_zone_left_x then
        if position.x < Flags_zone_right_x then
            return true
        end
    end

    return false
end

---@arg flag_entity CIcEntity
function Is_flag_position_valid(flag_entity)
    dbg_msg(DebugLevel1, string.format("Validating flag at %.2f,%.2f", flag_entity.Position.x, flag_entity.Position.y))
    return is_flag_position_suitable(flag_entity.Position)
end

---@param player CIcPlayer
---@return Vec2
function Get_hero_flag_position(player)
    local flag_positions = Game.Controller:GetHeroFlagPositions()

    local suitable_positions = {}
    for i = 1,flag_positions:Size(),1 do
        local position = flag_positions:At(i)
        if is_flag_position_suitable(position) then
            table.insert(suitable_positions, position)
        end
    end
    local count = table.getn(suitable_positions)
    if count == 0 then
        return nil
    end

    return suitable_positions[math.random(1, count)]
end

---@param player CIcPlayer
---@return Vec2
function Get_character_spawn_position(player)
    if player.Tag == Survival_tag_spider then
        return Sleeper_pos_spider
    end
    if player.Tag == Survival_tag_witch then
        return Sleeper_pos_witch
    end
    if player.Tag == Survival_tag_tank then
        return Sleeper_pos_tank
    end

    return nil
end

function add_sleeper(sleeper_id)
    local sleeper_descriptor = {}
    sleeper_descriptor["sleeper_id"] = sleeper_id
    sleeper_descriptor["ignore"] = false

    table.insert(Castle_sleepers, sleeper_descriptor)
end

function check_sleepers()
    if Castle_sleepers == nil then
        return
    end

    for i,sleeper_descriptor in ipairs(Castle_sleepers) do
        if not sleeper_descriptor.ignore then
            local character = Game.Controller:GetCharacter(sleeper_descriptor.sleeper_id)
            if character == nil then
                sleeper_descriptor.ignore = true
            else
                if not character:IsSleeping() then
                    sleeper_descriptor.ignore = true
                    on_castle_sleeper_awaken(sleeper_descriptor.sleeper_id, character:AwakenedBy())
                end
            end
        end
    end
end

function Castle_on_tick()
    check_sleepers()
    local tick = Game.Server.Tick
    if (tick + 1) == Game.Controller.InfectionStartTick then
        print("Setup the waves with difficulty", survival_difficulty_level)
        setup_wave1()
    end

    local current_second = Game.Controller:GetSecondsAfterInfection()
    if Survival_witch_next_check_time ~= nil then
        if current_second >= Survival_witch_next_check_time then
            Survival_check_the_witch(Castle_witch_id)
            Survival_schedule_next_witch_check()
        end
    end
end

function spawn_boss_bat()
    local wave = 1
    local bot_conf = nil

    bot_conf = add_boss_infected(wave, "bat")
    bot_conf.SpawnSecond = 25
    bot_conf.HP = Castle_boss_bat_hp[survival_difficulty_level]
    bot_conf.DropLevel = 1
    bot_conf.Tag = Survival_tag_bat
    add_tweak(bot_conf, "threat-aware")
    add_tweak(bot_conf, "strong-hook")

    return bot_conf
end

function spawn_boss_spider()
    local wave = 1
    local bot_conf = nil

    bot_conf = add_boss_infected(wave, "spider")
    bot_conf.Tag = Survival_tag_spider
    bot_conf.SpawnSecond = 75
    bot_conf.HP = Castle_boss_spider_hp[survival_difficulty_level]
    bot_conf.DropLevel = 2
    bot_conf.ScriptedSpawn = true
    add_tweak(bot_conf, "threat-aware")
    add_tweak(bot_conf, "strong-hook")
end

function spawn_boss_witch()
    local wave = 1
    local bot_conf = nil

    bot_conf = add_boss_infected(wave, "witch")
    bot_conf.Tag = Survival_tag_witch
    bot_conf.SpawnSecond = 90
    bot_conf.HP = Castle_boss_witch_hp[survival_difficulty_level]
    bot_conf.DropLevel = 3
    bot_conf.ScriptedSpawn = true
    add_tweak(bot_conf, "threat-aware")
    add_tweak(bot_conf, "can-flee")
end

function spawn_boss_tank()
    local wave = 1
    local bot_conf = nil

    bot_conf = add_boss_infected(wave, "tank")
    bot_conf.Tag = Survival_tag_tank
    bot_conf.SpawnSecond = 120
    bot_conf.HP = Castle_boss_tank_hp[survival_difficulty_level]
    bot_conf.DropLevel = 3
    bot_conf.ScriptedSpawn = true
    add_tweak(bot_conf, "threat-aware")
end

function setup_wave1()
    print("Castle setup_wave1()")
    local wave = 1
    local bot_conf = nil

    local bats_n = {4, 6, 8, 10, 12, 16}
    for i = 1,bats_n[survival_difficulty_level] do
        bot_conf = add_normal_infected(wave, "bat")
    end

    if survival_difficulty_level >= 3 then
        for i = 1,2 do
            bot_conf = add_normal_infected(wave, "bat")
            bot_conf.SpawnSecond = 5
        end
        if survival_difficulty_level >= 4 then
            for i = 1,2 do
                bot_conf = add_normal_infected(wave, "bat")
                bot_conf.SpawnSecond = 7
                -- Limit the number of the late boring bats by 4
                bot_conf.Lives = 4
            end
        end
    else
        -- 1-2 players
        for i = 1,survival_difficulty_level do
            bot_conf = add_normal_infected(wave, "bat")
            bot_conf.SpawnSecond = 10
        end
    end

    -- Active bosses
    spawn_boss_bat() -- wave 1

    -- Sleeping bosses
    spawn_boss_spider() -- wave 3
    spawn_boss_witch() -- wave 4
    spawn_boss_tank() -- wave 5
end

---@param start_second number
function setup_wave2(start_second)
    Game.Context:SendBroadcast(-1, "Wave 2 is coming...", EBroadcastPriority.GAMEANNOUNCE, 5)
    print("Setup wave2 with difficulty", survival_difficulty_level)
    local wave = 1
    local bot_conf = nil

    start_second = start_second + 7.5

    for i = 1,2 do
        bot_conf = add_normal_infected(wave, "hunter")
        bot_conf.SpawnSecond = start_second

        bot_conf = add_normal_infected(wave, "smoker")
        bot_conf.SpawnSecond = start_second
    end

    if survival_difficulty_level >= 2 then
        bot_conf = add_normal_infected(wave, "slug")
        bot_conf.SpawnSecond = start_second + 3

        bot_conf = add_normal_infected(wave, "voodoo")
        bot_conf.SpawnSecond = start_second + 3

        bot_conf = add_normal_infected(wave, "slug")
        bot_conf.SpawnSecond = start_second + 4
    end

    bot_conf = add_normal_infected(wave, "boomer")
    bot_conf.SpawnSecond = start_second + 15 - survival_difficulty_level * 2
    bot_conf.RespawnInterval = 12 - survival_difficulty_level * 2

    local spitter_spawn_second = start_second + 15
    if survival_difficulty_level >= 3 then
        for i = 1,2 do
            bot_conf = add_normal_infected(wave, "hunter")
            bot_conf.SpawnSecond = start_second + 10 + i * 0.1

            bot_conf = add_normal_infected(wave, "smoker")
            bot_conf.SpawnSecond = start_second + 10 + i * 0.1

            bot_conf = add_normal_infected(wave, "slug")
            bot_conf.SpawnSecond = start_second + 10 + i * 0.1

            bot_conf = add_normal_infected(wave, "voodoo")
            bot_conf.SpawnSecond = start_second + 10 + i * 0.1
        end

        bot_conf = add_normal_infected(wave, "boomer")
        bot_conf.SpawnSecond = start_second + 12
        bot_conf.RespawnInterval = 10 - survival_difficulty_level

        spitter_spawn_second = start_second + 20
    end

    local num_spitters = 1

    for i = 1,num_spitters do
        bot_conf = add_boss_infected(wave, "spitter")
        bot_conf.SpawnSecond = spitter_spawn_second
        bot_conf.HP = Castle_boss_spitter_hp[survival_difficulty_level]
        bot_conf.DropLevel = 1
        add_tweak(bot_conf, "threat-aware")
        if survival_difficulty_level >= 3 then
            add_tweak(bot_conf, "can-flee")
        end
    end
end

---@param start_second number
function setup_wave3(start_second)
    Game.Context:SendBroadcast(-1, "Wave 3 is coming...", EBroadcastPriority.GAMEANNOUNCE, 5)

    print("Setup wave3 with difficulty", survival_difficulty_level)
    local wave = 1
    local bot_conf = nil

    start_second = start_second + 7.5

    for i = 1,2 do
        bot_conf = add_normal_infected(wave, "voodoo")
        bot_conf.SpawnSecond = start_second
        bot_conf = add_normal_infected(wave, "smoker")
        bot_conf.SpawnSecond = start_second
        bot_conf = add_normal_infected(wave, "hunter")
        bot_conf.SpawnSecond = start_second

        start_second = start_second + 2

        bot_conf = add_normal_infected(wave, "spider")
        bot_conf.SpawnSecond = start_second
        bot_conf = add_normal_infected(wave, "ghost")
        bot_conf.SpawnSecond = start_second
    end

    start_second = start_second + 2
    bot_conf = add_normal_infected(wave, "smoker")
    bot_conf.SpawnSecond = start_second
    bot_conf = add_normal_infected(wave, "bat")
    bot_conf.SpawnSecond = start_second
    bot_conf = add_normal_infected(wave, "slug")
    bot_conf.SpawnSecond = start_second

    bot_conf = add_normal_infected(wave, "boomer")
    bot_conf.SpawnSecond = start_second + 1
end

---@param start_second number
function setup_wave4(start_second)
end

---@return number
function Survival_get_witch_horde_interval()
    local intervals = {6, 5, 4, 3, 2, 1, 1}
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
    for i,infected_class in ipairs(classes) do
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
    for i,infected_class in ipairs(classes) do
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
    for i,infected_class in ipairs(classes) do
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
    for i,infected_class in ipairs(classes) do
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
    if survival_difficulty_level >= 4 then
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
    else
        if Survival_witch_hp_rate > 0.4 and witch_hp_rate <= 0.4 then
            Survival_spawn_severely_hit_witch_wave(start_second, witch_id)
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

---@param sleeper_id number
---@param awaken_by_id number
function on_witch_awaken(sleeper_id, awaken_by_id)
    local awaken_by_player = Game.Controller:GetPlayer(awaken_by_id)
    if awaken_by_player == nil then
        Game.Context:SendChatTarget(-1, "The Witch was awakened!")
    else
        local name = Game.Server:GetClientName(awaken_by_id)
        Game.Context:SendChatTarget(-1, string.format("The Witch was awakened by '%s'", name))
    end

    Castle_witch_id = sleeper_id
    Survival_check_the_witch(Castle_witch_id)
    Survival_schedule_next_witch_check()
end

---@param sleeper_id number
---@param awaken_by_id number
function on_castle_sleeper_awaken(sleeper_id, awaken_by_id)
    local sleeper_player = Game.Controller:GetPlayer(sleeper_id)
    if sleeper_player.Tag == Survival_tag_witch then
        on_witch_awaken(sleeper_id, awaken_by_id)
    end
end

function Castle_on_character_spawned(player_id, spawn_type)
    local spawned_character = Game.Controller:GetCharacter(player_id)
    if spawned_character == nil then
        return
    end

    if spawned_character:IsInfected() == false then
        return
    end
    local spawned_player = Game.Controller:GetPlayer(player_id)
    if spawned_player.Tag == Survival_tag_spider then
        Castle_spider_id = player_id
        spawned_character:PutToSleep(10000)
        add_sleeper(player_id)
    end
    if spawned_player.Tag == Survival_tag_witch then
        Castle_witch_id = player_id
        Survival_witch_hp_rate = 1
        spawned_character:PutToSleep(10000)
        add_sleeper(player_id)
    end
    if spawned_player.Tag == Survival_tag_tank then
        spawned_character:PutToSleep(10000)
        add_sleeper(player_id)
    end
end

function Castle_on_character_death(victim_id, killer_id, weapon_str)
    dbg_msg(DebugLevel2, string.format("Castle_on_character_death(%d, %d, %s)", victim_id, killer_id, weapon_str))

    local victim_player = Game.Controller:GetPlayer(victim_id)
    if victim_player == nil then
        return
    end

    if victim_player.Tag == Survival_tag_bat then
        Castle_add_progress(1.0)
    elseif victim_player.Tag == Survival_tag_spider then
        Castle_add_progress(1.0)
    elseif victim_player.Tag == Survival_tag_witch then
        Castle_add_progress(1.0)
    elseif victim_player.Tag == Survival_tag_tank then
        Castle_add_progress(1.0)
    end

    if victim_id == Castle_witch_id then
        Survival_witch_next_check_time = nil
    end
end

---@param door_index number
function Castle_unlock_door_n(door_index)
    local doors_n = table.getn(runtime_context.doors)
    if doors_n < door_index then
        return
    end

    runtime_context.doors[door_index]:SetOpen(true)
    Game.Context:SendChatTarget(-1, string.format("Door number %d unlocked", door_index))

    if doors_n > door_index then
        Flags_zone_left_x = runtime_context.doors[door_index].Position.x
        Flags_zone_right_x = runtime_context.doors[door_index + 1].Position.x
    else
        Flags_zone_right_x = Game.Context.Collision.Width * 32
    end
    dbg_msg(DebugLevel1, string.format("Safe zone is set to range %.2f - %.2f", Flags_zone_left_x / 32, Flags_zone_right_x/ 32))
    Game.Controller:UpdateHeroFlags()

    if Flags_zone_left_x > survival_cp1.Position.x then
        survival_cp1:Destroy()
    end
    if Flags_zone_left_x > survival_cp2.Position.x then
        survival_cp2:Destroy()
    end
end

function on_control_point_effect(control_point)
    if control_point:IsInfected() then
        return
    end
    ---@param character CInfClassCharacter
    local function give_bonus(character)
        character:GiveHealth(1, -1)
        character:GiveArmor(2, -1)
    end
    for_each_human_character(give_bonus)
end

function get_max_players_for_difficulty(difficulty, hp_multiplier)
    local max_players = difficulty + 1
    if max_players > 6 then
        max_players = 6
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
        survival_default_tweaks = {"weak-hook"}
    end

    if survival_difficulty_level == 1 then
        Config.inf_bot_lives = 2
        Config.inf_survival_infected_spawning_delay = 6
        Config.inf_stunning_hammer_duration = 0.5
    elseif survival_difficulty_level == 2 then
        Config.inf_bot_lives = 3
        Config.inf_survival_infected_spawning_delay = 5
        Config.inf_stunning_hammer_duration = 0.75
    elseif survival_difficulty_level == 3 then
        Config.inf_bot_lives = 4
        Config.inf_survival_infected_spawning_delay = 4
        Config.inf_stunning_hammer_duration = 0.85
    else
        Config.inf_bot_lives = 5
        Config.inf_survival_infected_spawning_delay = 3
        Config.inf_stunning_hammer_duration = 1.0
    end

    if survival_difficulty_level <= 2 then
        Config.inf_hive_hooks = 2
    else
        Config.inf_hive_hooks = 3
    end

    local game_conf = Game.Controller:SurvivalGetGameConfiguration()
    game_conf.Hardmode = castle_hardmode

    update_max_players()
end

function start_survival_game(base_difficulty)
    survival_difficulty_level = base_difficulty

    local game_conf = Game.Controller:SurvivalGetGameConfiguration()
    game_conf:Reset()

    update_difficulty()

    Game.Controller:SurvivalAddWave(1, "The Castle")

    Game.Controller:PrepareSurvival()
    Game.Controller:QueueRound("survival")
    Game.Controller:DoWarmup(3)
    Game.Context:SendChatTarget(-1, string.format("Starting survival game (difficulty %d, max players %d)", survival_difficulty_level, survival_max_players))
end

function start_survival_auto()
    local players_number = Game.Controller:GetPlayersNumber()
    local total = players_number.Humans + players_number.Infected + players_number

    survival_players = total

    start_survival_game(survival_players)
end

function survival_init()
    OldConfig = {}

    OldConfig.inf_enable_tranquilizer_rifle = Config.inf_enable_tranquilizer_rifle
    OldConfig.inf_tranquilizer_dose = Config.inf_tranquilizer_dose
    OldConfig.inf_proba_spawn_near_witch = Config.inf_proba_spawn_near_witch
    OldConfig.inf_cp_global_effect_interval = Config.inf_cp_global_effect_interval
    OldConfig.inf_tile_damage = Config.inf_tile_damage
    OldConfig.inf_stunning_hammer_duration = Config.inf_stunning_hammer_duration

    Config.inf_enable_tranquilizer_rifle = 1
    Config.inf_tranquilizer_dose = 7.5
    Config.inf_proba_spawn_near_witch = 0
    Config.inf_cp_global_effect_interval = 10
    Config.inf_tile_damage = 3

    -- Kill-based survival
    Config.inf_survival_mode = 1
    Config.inf_white_hole_num_particles = 60
    Config.inf_white_hole_radius = 240

    Config.sv_vote_spectate = 1
end

function Castle_on_shutdown()
    for key,value in pairs(OldConfig) do
        Config[key] = value
    end

    Config.inf_survival_mode = 0
    Config.inf_survival_hardmode = 0
    Config.inf_white_hole_num_particles = 100
    Config.inf_white_hole_radius = 430

    Config.sv_vote_spectate = 0

    survival_remove_votes()
end

function Castle_init()
    Castle_initialized = true
    survival_init()

    on_event("on_tick", Castle_on_tick)
    -- on_event("on_round_started", on_castle_round_started)
    on_event("on_world_reset", Castle_reset)
    on_event("on_shutdown", Castle_on_shutdown)
    on_event("on_character_spawned", Castle_on_character_spawned)
    on_event("on_character_death", Castle_on_character_death)

    Castle_reset()
end

if Castle_initialized == nil then
    Castle_init()
end

function survival_remove_votes()
    for i = 1,6 do
        local vote_command = string.format("lua start_survival_game(%d)", i)
        Game.Context:RemoveVote(vote_command)
    end
    for i = 1,6 do
        local vote_command = string.format("lua start_survival_game(%d)", i)
        Game.Context:RemoveVote(vote_command)
    end
end

function survival_setup_votes()
    for i = 1,6 do
        local vote_name = string.format("Start survival (%d, %d players max)", i, get_max_players_for_difficulty(i, 1))
        local vote_command = string.format("lua start_survival_game(%d)", i)
        Game.Context:RemoveVote(vote_command)
        Game.Context:InsertVote(i - 1, vote_name, vote_command)
    end
    -- Game.Context:RemoveVote("lua start_survival_game(6)")
end

function set_extra_players(num)
    survival_allow_extra_players = num
    survival_setup_votes()
    update_max_players()
end

print("Castle runtime loaded")

survival_setup_votes()
