-- Snippet for LSP:
-- local infclass = require("library.Infclass")
-- local Game = infclass.Game
-- local Config = infclass.Config
-- require("runtime.base")

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
castle_hardmode = true
castle_wait_bosses_n = 0

survival_default_tweaks = nil

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

function reset_castle()
    castle_wait_bosses_n = 0
    safe_zone_left_x = 0

    survival_cp1 = Game.Controller:AddControlPoint(vec2(85 * 32, 40.5 * 32))

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

function on_castle_tick()
    local tick = Game.Server.Tick
    if (tick + 1) == Game.Controller.InfectionStartTick then
        print("Setup the waves with difficulty", survival_difficulty_level)
        setup_wave1()
    end
end

function setup_wave1()
    local wave = 1
    local bot_conf = nil

    local bats_n = {4, 6, 8, 10, 12, 16}
    for i = 1,bats_n[survival_difficulty_level] do
        bot_conf = add_bot_with_tweaks(wave, "bat")
    end

    if survival_difficulty_level >= 3 then
        for i = 1,2 do
            bot_conf = add_bot_with_tweaks(wave, "bat")
            bot_conf.SpawnSecond = 5
        end
        if survival_difficulty_level >= 4 then
            for i = 1,2 do
                bot_conf = add_bot_with_tweaks(wave, "bat")
                bot_conf.SpawnSecond = 7
                -- Limit the number of the late boring bats by 4
                bot_conf.Lives = 4
            end
        end
    else
        -- 1-3 players
        for i = 1,2 do
            bot_conf = add_bot_with_tweaks(wave, "bat")
            bot_conf.SpawnSecond = 10
        end
    end

    local boss_hp = {80, 120, 160, 180, 240, 240}
    bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
    bot_conf.SpawnSecond = 25
    bot_conf.Tag = "boss"
    bot_conf.Lives = 1
    bot_conf.HP = boss_hp[survival_difficulty_level]
    bot_conf.DropLevel = 1
    add_tweak(bot_conf, "threat-aware")
    add_tweak(bot_conf, "strong-hook")
    castle_wait_bosses_n = 1
end

---@param start_second number
function setup_wave2(start_second)
    print("Setup wave2 with difficulty", survival_difficulty_level)
    local wave = 1
    local bot_conf = nil

    for i = 1,2 do
        bot_conf = add_bot_with_tweaks(wave, "hunter")
        bot_conf.SpawnSecond = start_second

        bot_conf = add_bot_with_tweaks(wave, "smoker")
        bot_conf.SpawnSecond = start_second

        bot_conf = add_bot_with_tweaks(wave, "slug")
        bot_conf.SpawnSecond = start_second + 5
    end

    if survival_difficulty_level >= 3 then
        for i = 1,2 do
            bot_conf = add_bot_with_tweaks(wave, "voodoo")
            bot_conf.SpawnSecond = start_second + 5
            bot_conf = add_bot_with_tweaks(wave, "boomer")
            bot_conf.SpawnSecond = start_second + 5
        end
    end

    local num_spitters = 1
    if survival_difficulty_level >= 2 then
        num_spitters = 2
    end

    local boss_hp = {80, 120, 160, 180, 240, 240}
    for i = 1,num_spitters do
        bot_conf = add_bot_with_tweaks(wave, "spitter")
        bot_conf.SpawnSecond = start_second + 25
        bot_conf.Tag = "boss"
        bot_conf.Lives = 1
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
        bot_conf = add_bot_with_tweaks(wave, "voodoo")
        bot_conf.SpawnSecond = start_second
        bot_conf = add_bot_with_tweaks(wave, "smoker")
        bot_conf.SpawnSecond = start_second
        bot_conf = add_bot_with_tweaks(wave, "hunter")
        bot_conf.SpawnSecond = start_second

        start_second = start_second + 5

        bot_conf = add_bot_with_tweaks(wave, "spider")
        bot_conf.SpawnSecond = start_second
        bot_conf = add_bot_with_tweaks(wave, "ghost")
        bot_conf.SpawnSecond = start_second
    end

    local boss_hp = {120, 160, 220, 280, 320, 360}
    bot_conf = Game.Controller:SurvivalAddBot(wave, "spider")
    bot_conf.SpawnSecond = start_second + 25
    bot_conf.Lives = 1
    bot_conf.HP = boss_hp[survival_difficulty_level]
    bot_conf.Tag = "boss"
    bot_conf.DropLevel = 2
    add_tweak(bot_conf, "threat-aware")
    add_tweak(bot_conf, "strong-hook")
    castle_wait_bosses_n = 1
end

---@param start_second number
function setup_wave4(start_second)
    local wave = 1
    local bot_conf = nil

    local max_drop_level = 2
    if survival_difficulty_level > survival_players then
        max_drop_level = 3
    end

    local boss_hp = {180, 240, 320, 520, 640, 720}
    bot_conf = Game.Controller:SurvivalAddBot(wave, "tank")
    bot_conf.SpawnSecond = start_second
    bot_conf.Tag = "boss"
    bot_conf.Lives = 1
    bot_conf.HP = boss_hp[survival_difficulty_level]
    bot_conf.DropLevel = max_drop_level
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

function on_castle_character_death(victim_id, killer_id, weapon_str)
    local victim_player = Game.Controller:GetPlayer(victim_id)
    if victim_player == nil then
        return
    end
    if victim_player.Tag == "boss" then
        on_boss_killed(victim_id)
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
        safe_zone_left_x = runtime_context.doors[door_index].Position().x
        right_border = runtime_context.doors[door_index + 1].Position().x
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

function get_max_players_for_difficulty(difficulty)
    local max_players = difficulty + 2
    if max_players > 6 then
        max_players = 6
    end

    return max_players + survival_allow_extra_players
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

    survival_max_players = get_max_players_for_difficulty(survival_difficulty_level)

    local game_conf = Game.Controller:SurvivalGetGameConfiguration()
    game_conf.MaxPlayers = survival_max_players
    game_conf.Hardmode = castle_hardmode
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

function init_castle()
    runtime_context.castle_initialized = true
    -- Kill-based survival
    Config.inf_survival_mode = 1

    on_event("on_tick", on_castle_tick)
    -- on_event("on_round_started", on_castle_round_started)
    on_event("on_world_reset", reset_castle)
    -- on_event("on_character_spawned", on_castle_character_spawned)
    on_event("on_character_death", on_castle_character_death)

    reset_castle()
end

if runtime_context.castle_initialized == nil then
    init_castle()
end

function survival_setup_votes()
    for i = 1,5 do
        local vote_name = string.format("Start survival (%d, %d players max)", i, get_max_players_for_difficulty(i))
        local vote_command = string.format("lua start_survival_game(%d)", i)
        Game.Context:RemoveVote(vote_command)
        Game.Context:InsertVote(i - 1, vote_name, vote_command)
    end
    Game.Context:RemoveVote("lua start_survival_game(6)")
end

print("Castle runtime loaded")

survival_setup_votes()
