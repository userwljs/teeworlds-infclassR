-- Snippet for LSP:
-- local infclass = require("library.Infclass")
-- local Game = infclass.Game
-- local Config = infclass.Config
-- require("runtime.base")

Submod.SurvivalGame = "survival"
active_submod = Submod.SurvivalGame

-- Utils
function add_tweak(bot_conf, tweak)
    local tweaks = bot_conf:GetTweaks()
    tweaks:Add(tweak)
end

survival_difficulty_level = 0
survival_allow_extra_players = 0
survival_max_players = 0
survival_players = 0
survival_current_wave = 0
survival_hp_multiplier = 1
castle_hardmode = true
castle_wait_bosses_n = 0
Castle_sleepers = {}
Castle_witch_next_wave_time = nil
Castle_witch_spawned_waves = 0
Castle_witch_id = 0

survival_default_tweaks = nil

OldConfig = {}

Castle_horde_spawn_intervals = {6, 5, 4, 3, 2, 1, 1}

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

function reset_castle()
    castle_wait_bosses_n = 0
    Castle_sleepers = {}
    Castle_witch_next_wave_time = nil
    Castle_witch_spawned_waves = 0
    safe_zone_left_x = 0

    survival_cp1 = Game.Controller:AddControlPoint(vec2(85 * 32, 40.5 * 32))

    sleeper_pos1 = vec2(142.5 * 32, 45.5 * 32)
    sleeper_pos2 = vec2(136 * 32, 44.5 * 32)
    sleeper_pos3 = vec2(124 * 32, 35.5 * 32)

    spawn_doors()
    right_border = runtime_context.doors[1].Position.x

    set_spawns()
end

function is_position_within_borders(position)
    if position.x > right_border then
        return false
    end

    return true
end

function set_spawns()
    print("Set spawns")
    local human_spawns = Game.Controller:GetHumanSpawns()

    for i = 1,human_spawns:Size() do
        local position = human_spawns:At(i)
        Game.Controller:SetHumanSpawnEnabled(i, is_position_within_borders(position))
    end
end

function spawn_doors()
    local door1 = add_piercing_door_at(77.5 * 32)
    door1:SetOpen(false)

    local door2 = add_piercing_door_at(118.5 * 32)
    door2:SetOpen(false)

    local door3 = add_piercing_door_at(140.5 * 32)
    door3:SetOpen(false)
end

function is_flag_position_suitable(position)
    if position.x > safe_zone_left_x then
        if position.x < right_border then
            return true
        end
    end

    return false
end

function get_hero_flag_position()
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
        return
    end

    local position = suitable_positions[math.random(1, count)]
    Game.Controller:ProvideHeroFlagPosition(position)
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
                    on_castle_sleeper_awaken(sleeper_descriptor.sleeper_id)
                end
            end
        end
    end
end

function on_castle_tick()
    check_sleepers()
    local tick = Game.Server.Tick
    if (tick + 1) == Game.Controller.InfectionStartTick then
        print("Setup the waves with difficulty", survival_difficulty_level)
        setup_wave1()
    end

    local current_second = Game.Controller:GetSecondsAfterInfection()
    if Castle_witch_next_wave_time ~= nil then
        if current_second >= Castle_witch_next_wave_time then
            Castle_spawn_witch_wave()
            Castle_schedule_next_witch_wave()
        end
    end
end

function setup_wave1()
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
        -- 1-3 players
        for i = 1,2 do
            bot_conf = add_normal_infected(wave, "bat")
            bot_conf.SpawnSecond = 10
        end
    end

    local boss_hp = {80, 120, 160, 180, 240, 240}
    bot_conf = add_boss_infected(wave, "bat")
    bot_conf.SpawnSecond = 25
    bot_conf.HP = boss_hp[survival_difficulty_level]
    bot_conf.DropLevel = 1
    add_tweak(bot_conf, "threat-aware")
    add_tweak(bot_conf, "strong-hook")
    castle_wait_bosses_n = 1

    local sleeper_hp = {80, 180, 320, 420, 520, 640}
    bot_conf = Game.Controller:SurvivalAddBot(wave, "witch")
    bot_conf.SpawnSecond = 15
    bot_conf.Tag = "sleeper"
    bot_conf.Lives = 1
    bot_conf.HP = sleeper_hp[survival_difficulty_level]
    add_tweak(bot_conf, "threat-aware")
    add_tweak(bot_conf, "can-flee")
end

---@param start_second number
function setup_wave2(start_second)
    print("Setup wave2 with difficulty", survival_difficulty_level)
    local wave = 1
    local bot_conf = nil

    for i = 1,2 do
        bot_conf = add_normal_infected(wave, "hunter")
        bot_conf.SpawnSecond = start_second

        bot_conf = add_normal_infected(wave, "smoker")
        bot_conf.SpawnSecond = start_second

        bot_conf = add_normal_infected(wave, "slug")
        bot_conf.SpawnSecond = start_second + 5
    end

    if survival_difficulty_level >= 3 then
        for i = 1,2 do
            bot_conf = add_normal_infected(wave, "voodoo")
            bot_conf.SpawnSecond = start_second + 5
            bot_conf = add_normal_infected(wave, "boomer")
            bot_conf.SpawnSecond = start_second + 5
        end
    end

    local num_spitters = 1
    if survival_difficulty_level >= 2 then
        num_spitters = 2
    end

    local boss_hp = {80, 120, 160, 180, 240, 240}
    for i = 1,num_spitters do
        bot_conf = add_boss_infected(wave, "spitter")
        bot_conf.SpawnSecond = start_second + 25
        bot_conf.HP = boss_hp[survival_difficulty_level]
        bot_conf.DropLevel = 1
        add_tweak(bot_conf, "threat-aware")
        if survival_difficulty_level >= 3 then
            add_tweak(bot_conf, "can-flee")
        end
    end
    castle_wait_bosses_n = num_spitters
end

---@param start_second number
function setup_wave3(start_second)
    print("Setup wave3 with difficulty", survival_difficulty_level)
    local wave = 1
    local bot_conf = nil

    for i = 1,2 do
        bot_conf = add_normal_infected(wave, "voodoo")
        bot_conf.SpawnSecond = start_second
        bot_conf = add_normal_infected(wave, "smoker")
        bot_conf.SpawnSecond = start_second
        bot_conf = add_normal_infected(wave, "hunter")
        bot_conf.SpawnSecond = start_second

        start_second = start_second + 5

        bot_conf = add_normal_infected(wave, "spider")
        bot_conf.SpawnSecond = start_second
        bot_conf = add_normal_infected(wave, "ghost")
        bot_conf.SpawnSecond = start_second
    end

    local boss_hp = {120, 160, 220, 280, 320, 360}
    bot_conf = add_boss_infected(wave, "spider")
    bot_conf.SpawnSecond = start_second + 25
    bot_conf.HP = boss_hp[survival_difficulty_level]
    bot_conf.DropLevel = 2
    add_tweak(bot_conf, "threat-aware")
    add_tweak(bot_conf, "strong-hook")
    castle_wait_bosses_n = 1
end

---@param start_second number
function setup_wave4(start_second)
    local wave = 1
    local bot_conf = nil
    local boss_hp = {180, 240, 320, 520, 640, 720}
    bot_conf = add_boss_infected(wave, "tank")
    bot_conf.SpawnSecond = start_second
    bot_conf.HP = boss_hp[survival_difficulty_level]
    bot_conf.DropLevel = 3
end

function Castle_spawn_witch_wave()
    local start_second = Game.Controller:GetSecondsAfterInfection() + 0.125

    Castle_witch_spawned_waves = Castle_witch_spawned_waves + 1
    local interval = Castle_horde_spawn_intervals[survival_difficulty_level]

    local wave = 1 -- Always '1' for Castle
    local spawn_wave = Castle_witch_spawned_waves

    local function spawn_with_lives(player_class)
        local bot_conf = add_witch_infected(wave, player_class, Castle_witch_id)
        bot_conf.SpawnSecond = start_second
        bot_conf.Lives = 4
        bot_conf.RespawnInterval = interval
        return bot_conf
    end

    local bot_conf = nil
    if spawn_wave % 4 == 1 then
        spawn_with_lives("voodoo")
        spawn_with_lives("smoker")
        spawn_with_lives("hunter")
        bot_conf = spawn_with_lives("spider")
        bot_conf.SpawnSecond = start_second + 1
    end

    bot_conf = nil
    if spawn_wave > 5 then
        bot_conf = add_witch_infected(wave, "ghoul", Castle_witch_id)
        bot_conf.SpawnSecond = start_second
    end
    if survival_difficulty_level < spawn_wave then
        if bot_conf ~= nil then
            return
        end
    end

    bot_conf = add_witch_infected(wave, "slug", Castle_witch_id)
    bot_conf.SpawnSecond = start_second

    if survival_difficulty_level < spawn_wave then
        if bot_conf ~= nil then
            return
        end
    end
    if spawn_wave % 3 == 2 then
        bot_conf = add_witch_infected(wave, "boomer", Castle_witch_id)
        bot_conf.SpawnSecond = start_second + 0.75
    end
    if survival_difficulty_level < spawn_wave then
        if bot_conf ~= nil then
            return
        end
    end
    if spawn_wave > 2 then
        bot_conf = add_witch_infected(wave, "slug", Castle_witch_id)
        bot_conf.SpawnSecond = start_second
    end
    if survival_difficulty_level < spawn_wave then
        if bot_conf ~= nil then
            return
        end
    end
    if spawn_wave > 1 then
        bot_conf = add_witch_infected(wave, "ghost", Castle_witch_id)
        bot_conf.SpawnSecond = start_second + 1.25
    end
end

function Castle_schedule_next_witch_wave()
    Castle_witch_next_wave_time = Game.Controller:GetSecondsAfterInfection() + Castle_horde_spawn_intervals[survival_difficulty_level]
end

function on_witch_awaken(sleeper_id)
    Game.Context:SendChatTarget(-1, "The Witch was awakened!")

    Castle_witch_id = sleeper_id
    Castle_spawn_witch_wave()
    Castle_schedule_next_witch_wave()
end

function on_castle_sleeper_awaken(sleeper_id)
    on_witch_awaken(sleeper_id)
end

function on_boss_killed(victim_id)
    Game.Context:SendChatTarget(-1,string.format("Boss killed on wave %d", survival_current_wave))
    if castle_wait_bosses_n then
        castle_wait_bosses_n = castle_wait_bosses_n - 1
        if castle_wait_bosses_n == 0 then
            unlock_door_n(survival_current_wave)
            survival_current_wave = survival_current_wave + 1
            _G[string.format("setup_wave%d", survival_current_wave)](Game.Controller:GetSecondsAfterInfection() + 3)
        end
    end
end

function on_castle_character_spawned(player_id, spawn_type)
    local spawned_character = Game.Controller:GetCharacter(player_id)
    if spawned_character == nil then
        return
    end

    if spawned_character:IsInfected() == false then
        return
    end
    local spawned_player = Game.Controller:GetPlayer(player_id)
    if spawned_player.Tag == "sleeper" then
        spawned_character.Position = sleeper_pos2
        spawned_character:PutToSleep(10000)
        add_sleeper(player_id)
    end
end

function on_castle_character_death(victim_id, killer_id, weapon_str)
    local victim_player = Game.Controller:GetPlayer(victim_id)
    if victim_player == nil then
        return
    end
    if victim_player.Tag == "boss" then
        on_boss_killed(victim_id)
    end

    if victim_id == Castle_witch_id then
        Castle_witch_next_wave_time = nil
    end
end


function unlock_door_n(door_index)
    local doors_n = table.getn(runtime_context.doors)
    if doors_n < door_index then
        return
    end

    runtime_context.doors[door_index]:SetOpen(true)
    Game.Context:SendChatTarget(-1, string.format("Door number %d unlocked", door_index))

    if doors_n > door_index then
        safe_zone_left_x = runtime_context.doors[door_index].Position.x
        right_border = runtime_context.doors[door_index + 1].Position.x
    else
        right_border = Game.Context.Collision.Width * 32
    end
end

function on_control_point_effect(control_point)
    if control_point:IsInfected() then
        return
    end
    ---@param character CInfClassCharacter
    local function give_bonus(character)
        character:GiveArmor(2, -1)
    end
    for_each_human_character(give_bonus)
end

function get_max_players_for_difficulty(difficulty, hp_multiplier)
    local max_players = difficulty + 2
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
        Config.inf_survival_infected_spawning_delay = 5
    elseif survival_difficulty_level == 2 then
        Config.inf_bot_lives = 3
        Config.inf_survival_infected_spawning_delay = 4
    elseif survival_difficulty_level == 3 then
        Config.inf_bot_lives = 4
        Config.inf_survival_infected_spawning_delay = 4
    else
        Config.inf_bot_lives = 5
        Config.inf_survival_infected_spawning_delay = 3
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

    survival_current_wave = 1

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
    OldConfig.inf_enable_tranquilizer_rifle = Config.inf_enable_tranquilizer_rifle
    OldConfig.inf_tranquilizer_dose = Config.inf_tranquilizer_dose
    OldConfig.inf_proba_spawn_near_witch = Config.inf_proba_spawn_near_witch

    Config.inf_enable_tranquilizer_rifle = 1
    Config.inf_tranquilizer_dose = 7.5
    Config.inf_proba_spawn_near_witch = 0

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

if runtime_context.game_initialized == nil then
    runtime_context.game_initialized = true
    -- on_event("on_character_death", on_game_character_death)

    -- on_event("on_tick", survival_on_tick)
end

function init_castle()
    runtime_context.castle_initialized = true
    survival_init()

    on_event("on_tick", on_castle_tick)
    -- on_event("on_round_started", on_castle_round_started)
    on_event("on_world_reset", reset_castle)
    on_event("on_shutdown", Castle_on_shutdown)
    on_event("on_character_spawned", on_castle_character_spawned)
    on_event("on_character_death", on_castle_character_death)

    reset_castle()
end

if runtime_context.castle_initialized == nil then
    init_castle()
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
