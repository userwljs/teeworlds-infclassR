-- Snippet for LSP:
-- local infclass = require("library.Infclass")
-- local Game = infclass.Game
-- local Config = infclass.Config

Submod.SurvivalGame = "survival"
active_submod = Submod.SurvivalGame

function add_tweak(bot_conf, tweak)
    local tweaks = bot_conf:GetTweaks()
    tweaks:Add(tweak)
end

survival_difficulty_level = 0
survival_max_players = 0
survival_players = 0
survival_current_wave = 0

survival_setup_tweaks = function (bot_conf) end

function setup_wave1()
    print("Setup wave1 with difficulty", survival_difficulty_level)
    local wave = 1
    local bot_conf = nil
    local next_spawn_second
    local next_spawn_lives = Config.inf_bot_lives

    for i = 1,4 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
        survival_setup_tweaks(bot_conf)
    end

    if survival_difficulty_level >= 4 then
        for i = 1,2 do
            bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
            bot_conf.SpawnSecond = 5
        end
        if survival_difficulty_level >= 5 then
            for i = 1,2 do
                bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
                bot_conf.SpawnSecond = 7
                -- Limit the number of the late boring bats by 4
                bot_conf.Lives = 4
            end
        end
    else
        -- 1-3 players
        for i = 1,2 do
            bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
            bot_conf.SpawnSecond = 10
            survival_setup_tweaks(bot_conf)
        end
    end

    if survival_difficulty_level >= 5 then
        -- Spawn extra strong bats
        local bat_hp = 60 + 20 * (survival_difficulty_level - 4)
        -- lvl5 - 80hp
        -- lvl6 - 100hp

        for i = 1,2 do
            bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
            bot_conf.SpawnSecond = 10
            bot_conf.Lives = 1
            bot_conf.HP = bat_hp
        end

        -- Reduce the number of the late boring bats:
        next_spawn_lives = 2
    end

    if survival_difficulty_level <= 2 then
        next_spawn_second = 20
    else
        next_spawn_second = 12
    end

    for i = 1,4 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
        bot_conf.SpawnSecond = next_spawn_second
        bot_conf.Lives = next_spawn_lives
        survival_setup_tweaks(bot_conf)
    end
end

function setup_wave2()
    print("Setup wave2 with difficulty", survival_difficulty_level)
    local wave = 2
    local bot_conf = nil

    bot_conf = Game.Controller:SurvivalAddBot(wave, "smoker")
    survival_setup_tweaks(bot_conf)

    bot_conf = Game.Controller:SurvivalAddBot(wave, "voodoo")
    survival_setup_tweaks(bot_conf)

    if survival_difficulty_level > 1 then
        for i = 1,2 do
            bot_conf = Game.Controller:SurvivalAddBot(wave, "spider")
            bot_conf.SpawnSecond = 2 + i * 0.2
            survival_setup_tweaks(bot_conf)
        end

        bot_conf = Game.Controller:SurvivalAddBot(wave, "boomer")
        bot_conf.SpawnSecond = 5
        survival_setup_tweaks(bot_conf)
    end

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "hunter")
        bot_conf.SpawnSecond = 5 + i * 0.3
        survival_setup_tweaks(bot_conf)
    end

    local lvl1_slugs = {10, 20}
    local lvl2_slugs = {7, 15, 25}
    local lvl3_slugs = {7, 15, 20}
    local lvl4_slugs = {7, 10, 15, 20}
    local lvl5_slugs = {7, 10, 15, 20}
    local lvl6_slugs = {7, 10, 12, 15, 17, 20}
    local slugs_spawns = {lvl1_slugs, lvl2_slugs, lvl3_slugs, lvl4_slugs, lvl5_slugs, lvl6_slugs}

    for i,slug_spawn_time in ipairs(slugs_spawns[survival_difficulty_level]) do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "slug")
        bot_conf.SpawnSecond = slug_spawn_time
        survival_setup_tweaks(bot_conf)
    end

    bot_conf = Game.Controller:SurvivalAddBot(wave, "spider")
    bot_conf.SpawnSecond = 25
    bot_conf.Tag = "boss"
    bot_conf.Lives = 1

    local boss_hp = {80, 120, 160, 180, 240, 240}

    bot_conf.HP = boss_hp[survival_difficulty_level]
    bot_conf.DropLevel = 1
    add_tweak(bot_conf, "threat-aware")
    add_tweak(bot_conf, "strong-hook")
end

function setup_wave3()
    print("Setup wave3 with difficulty", survival_difficulty_level)
    local wave = 3
    local bot_conf = nil

    bot_conf = Game.Controller:SurvivalAddBot(wave, "smoker")
    bot_conf = Game.Controller:SurvivalAddBot(wave, "hunter")
    bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
    bot_conf = Game.Controller:SurvivalAddBot(wave, "slug")

    bot_conf = Game.Controller:SurvivalAddBot(wave, "boomer")
    bot_conf.SpawnSecond = 5
    bot_conf = Game.Controller:SurvivalAddBot(wave, "voodoo")
    bot_conf.SpawnSecond = 5

    local ghost_lives = 2
    if survival_difficulty_level > 1 then
        ghost_lives = 3
    end

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "ghost")
        bot_conf.SpawnSecond = 12 + i * 0.3
        bot_conf.RespawnInterval = 2.5
        bot_conf.Lives = ghost_lives
        bot_conf.HP = 2
    end

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "ghost")
        bot_conf.SpawnSecond = 15 + i * 0.3
        bot_conf.RespawnInterval = 2.5
        bot_conf.Lives = ghost_lives
        bot_conf.HP = 2
    end

    for i = 1,3 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "ghost")
        bot_conf.SpawnSecond = 20 + i * 0.3
        bot_conf.RespawnInterval = 0.5
        bot_conf.Lives = ghost_lives
        bot_conf.HP = 2
    end

    local boss_hp = {80, 120, 180, 220, 240, 240}
    local spawns = 1
    if survival_difficulty_level >= 6 then
        spawns = 2
    end

    for i = 1,spawns do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "ghost")
        bot_conf.SpawnSecond = 25
        bot_conf.Tag = "boss"
        bot_conf.Lives = 1
        bot_conf.HP = boss_hp[survival_difficulty_level]
        bot_conf.DropLevel = 1
        add_tweak(bot_conf, "threat-aware")
    end
end

function setup_wave4()
    local wave = 4
    local bot_conf = nil

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "smoker")
        bot_conf = Game.Controller:SurvivalAddBot(wave, "hunter")
    end

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "spider")
        add_tweak(bot_conf, "strong-hook")
    end
    bot_conf = Game.Controller:SurvivalAddBot(wave, "slug")

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
        bot_conf.SpawnSecond = 15
        add_tweak(bot_conf, "strong-hook")
    end

    bot_conf = Game.Controller:SurvivalAddBot(wave, "witch")
    bot_conf.SpawnSecond = 20
    bot_conf.Tag = "boss"
    bot_conf.Lives = 1
    local boss_hp = {120, 160, 220, 280, 320, 360}
    bot_conf.HP = boss_hp[survival_difficulty_level]
    bot_conf.DropLevel = 2
    add_tweak(bot_conf, "threat-aware")

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
        bot_conf.SpawnSecond = 20
        bot_conf.RespawnInterval = 2
        add_tweak(bot_conf, "strong-hook")
    end
end

function setup_wave5()
    local wave = 5
    local bot_conf = nil

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "smoker")
        bot_conf = Game.Controller:SurvivalAddBot(wave, "hunter")
        bot_conf = Game.Controller:SurvivalAddBot(wave, "spider")
    end

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "slug")
    end
    bot_conf.RespawnInterval = 4

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "boomer")
    end
    bot_conf.RespawnInterval = 4

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "voodoo")
    end
    bot_conf.RespawnInterval = 4

    local ghouls_hp = {40, 40, 60, 60, 80, 80}

    local lvl1_ghouls = {10, 20}
    local lvl2_ghouls = {7, 15, 25}
    local lvl3_ghouls = {7, 15, 20}
    local lvl4_ghouls = {7, 10, 15, 20}
    local lvl5_ghouls = {7, 10, 15, 20}
    local lvl6_ghouls = {7, 10, 12, 15, 17, 20}
    local ghoul_spawns = {lvl1_ghouls, lvl2_ghouls, lvl3_ghouls, lvl4_ghouls, lvl5_ghouls, lvl6_ghouls}

    for i,ghoul_spawn_time in ipairs(ghoul_spawns[survival_difficulty_level]) do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "ghoul")
        bot_conf.SpawnSecond = ghoul_spawn_time
        bot_conf.HP= ghouls_hp[survival_difficulty_level]
        bot_conf.Lives = 1
    end
    -- Give a drop to the last ghoul
    bot_conf.DropLevel = 2

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "ghost")
        bot_conf.SpawnSecond = 20
    end

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
        bot_conf.SpawnSecond = 35
        bot_conf.HP = 4
    end

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
        bot_conf.SpawnSecond = 40
        bot_conf.HP = 4
    end
end

function setup_wave6()
    local wave = 6
    local bot_conf = nil

    for i = 1,4 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "ghost")
        bot_conf.Lives = 2
        bot_conf.RespawnInterval = 1
    end

    if survival_difficulty_level >= 4 then
        for i = 1,2 do
            bot_conf = Game.Controller:SurvivalAddBot(wave, "smoker")
            bot_conf.SpawnSecond = 5
            bot_conf.Lives = 2
        end
        for i = 1,2 do
            bot_conf = Game.Controller:SurvivalAddBot(wave, "spider")
            bot_conf.SpawnSecond = 5
            bot_conf.Lives = 2
        end
        for i = 1,2 do
            bot_conf = Game.Controller:SurvivalAddBot(wave, "hunter")
            bot_conf.SpawnSecond = 5
            bot_conf.Lives = 2
        end
    end

    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "slug")
        bot_conf.SpawnSecond = 10
        bot_conf.Lives = 2
    end
    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "boomer")
        bot_conf.SpawnSecond = 10
        bot_conf.Lives = 2
    end
    for i = 1,2 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "voodoo")
        bot_conf.SpawnSecond = 10
        bot_conf.Lives = 2
    end

    local max_drop_level = 2
    if survival_difficulty_level > survival_players then
        max_drop_level = 3
    end

    local boss_hp = {180, 240, 320, 520, 640, 720}
    bot_conf = Game.Controller:SurvivalAddBot(wave, "tank")
    bot_conf.SpawnSecond = 25
    bot_conf.Tag = "boss"
    bot_conf.Lives = 1
    bot_conf.HP = boss_hp[survival_difficulty_level]
    bot_conf.DropLevel = max_drop_level

    for i = 1,3 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "slug")
        bot_conf.SpawnSecond = 45 + i * 0.2
        bot_conf.HP = 5
    end
    for i = 1,3 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "spider")
        bot_conf.SpawnSecond = 46 + i * 0.2
        bot_conf.HP = 5
    end

    if survival_difficulty_level >= 3 then
        local witch_hp = {0, 0, 80, 120, 180, 240}
        bot_conf = Game.Controller:SurvivalAddBot(wave, "witch")
        bot_conf.SpawnSecond = 47
        bot_conf.Lives = 1
        bot_conf.HP = witch_hp[survival_difficulty_level]
        add_tweak(bot_conf, "threat-aware")
        add_tweak(bot_conf, "can-flee")
    end

    if survival_difficulty_level >= 4 then
        for i = 1,4 do
            bot_conf = Game.Controller:SurvivalAddBot(wave, "boomer")
            bot_conf.SpawnSecond = 47 + i * 0.2
        end
    else
        for i = 1,2 do
            bot_conf = Game.Controller:SurvivalAddBot(wave, "boomer")
            bot_conf.SpawnSecond = 47 + i * 0.2
        end
    end
    for i = 1,4 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "hunter")
        bot_conf.SpawnSecond = 47 + i * 0.2
    end
    for i = 1,4 do
        bot_conf = Game.Controller:SurvivalAddBot(wave, "bat")
        bot_conf.SpawnSecond = 48 + i * 0.1
    end

end

-- function on_game_character_death(victim_id, killer_id, weapon_str)
-- end

function get_max_players_for_difficulty(difficulty)
    local max_players = difficulty + 2
    if max_players > 6 then
        max_players = 6
    end

    return max_players
end

function update_difficulty()
    survival_setup_tweaks = function (bot_conf) end
    if survival_difficulty_level <= 4 then
        survival_setup_tweaks = function (bot_conf)
            local tweaks = bot_conf:GetTweaks()
            tweaks:Add("weak-hook")
        end
    end

    if survival_difficulty_level <= 2 then
        Config.inf_bot_lives = 2
        Config.inf_survival_infected_spawning_delay = 5
    elseif survival_difficulty_level == 3 then
        Config.inf_bot_lives = 3
        Config.inf_survival_infected_spawning_delay = 4
    elseif survival_difficulty_level == 4 then
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
    game_conf.Hardmode = false
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
    setup_wave_n(survival_current_wave)
    survival_current_wave = survival_current_wave + 1
end

function survival_on_tick()
    local tick = Game.Server.Tick
    if (tick + 1) == Game.Controller.InfectionStartTick then
        setup_this_wave()
    end
end

function start_survival_game(base_difficulty)
    survival_difficulty_level = base_difficulty

    update_difficulty()

    local game_conf = Game.Controller:SurvivalGetGameConfiguration()
    game_conf:Reset()
    Game.Controller:SurvivalAddWave(1, "The Meeting")
    Game.Controller:SurvivalAddWave(2, "The Green Smoke")
    Game.Controller:SurvivalAddWave(3, "Who's there?")
    Game.Controller:SurvivalAddWave(4, "The Tomato")
    Game.Controller:SurvivalAddWave(5, "The Gathering")
    Game.Controller:SurvivalAddWave(6, "The Goodbye")

    survival_current_wave = 1

    Game.Controller:PrepareSurvival()
    Game.Controller:QueueRound("survival")
    Game.Controller:DoWarmup(3)
    Game.Context:SendChatTarget(-1, string.format("Starting survival game (difficulty %d, max players %d)", survival_difficulty_level, survival_max_players))
end

function start_survival_auto()
    local players_number = Game.Controller:GetPlayersNumber()
    local total = players_number.Humans + players_number.Infected

    survival_players = total

    start_survival_game(survival_players)
end

if runtime_context.game_initialized == nil then
    runtime_context.game_initialized = true
    -- Kill-based survival
    Config.inf_survival_mode = 1

    -- on_event("on_character_death", on_game_character_death)

    on_event("on_tick", survival_on_tick)
end

function survival_setup_votes()
    for i = 1,6 do
        local vote_name = string.format("Start survival (%d, %d players max)", i, get_max_players_for_difficulty(i))
        local vote_command = string.format("lua start_survival_game(%d)", i)
        Game.Context:RemoveVote(vote_command)
        Game.Context:InsertVote(i - 1, vote_name, vote_command)
    end
end

print("Survival Game runtime loaded")

survival_setup_votes()
