-- Snippet for LSP:
local infclass = require("library.Infclass")
local Game = infclass.Game
local Config = infclass.Config
require("runtime.base")

local EPlayerClass = {}
EPlayerClass.Bat = "bat"
EPlayerClass.Smoker = "smoker"

local ETweaks = {}
ETweaks.NoHook = "no-hook"
ETweaks.WeakHook = "weak-hook"
ETweaks.ThreatAware = "threat-aware"

Survival = {}
Survival.hard_max = 8
Survival.allowed_extra_players = 0
Survival.hp_multiplier = 1
Survival.difficulty_level = 1
Survival.default_tweaks = nil
Survival.max_players = 0
Survival.current_wave = 0

---@param bot_conf SurvivalBotConfiguration
---@param tweak string
function Survival:add_tweak(bot_conf, tweak)
    local tweaks = bot_conf:GetTweaks()
    tweaks:Add(tweak)
end

---@param wave number
---@param player_class string
---@return SurvivalBotConfiguration
function Survival:add_normal_infected(wave, player_class)
    local bot_conf = add_bot_with_tweaks(wave, player_class)
    bot_conf.HP = 10 * survival_hp_multiplier
    return bot_conf
end

---@param wave number
---@param player_class string
---@return SurvivalBotConfiguration
function Survival:add_boss_infected(wave, player_class)
    local bot_conf = Game.Controller:SurvivalAddBot(wave, player_class)
    bot_conf.Tag = "boss"
    bot_conf.Lives = 1
    return bot_conf
end

---@param base_difficulty number
---@param hp_multiplier number
function Survival:get_max_players_for_difficulty(base_difficulty, hp_multiplier)
    local max_players = base_difficulty + 2 * hp_multiplier
    if max_players > Survival.hard_max then
        max_players = Survival.hard_max
    end

    return max_players + Survival.allowed_extra_players
end

function Survival:update_max_players()
    Survival.max_players = Survival:get_max_players_for_difficulty(Survival.difficulty_level, Survival.hp_multiplier)

    local game_conf = Game.Controller:SurvivalGetGameConfiguration()
    game_conf.MaxPlayers = survival_max_players
end

function Survival:apply_difficulty()
    Survival.default_tweaks = nil
    if Survival.difficulty_level <= 4 then
        Survival.default_tweaks = {ETweaks.WeakHook}
    end

    local infected_lives = {1, 2, 3, 3, 4, 5, 6, 7, 10, 12}
    local spawn_delays   = {7, 7, 7, 6, 6, 6, 5, 4, 2, 1}
    Config.inf_bot_lives = infected_lives[Survival.difficulty_level]
    Config.inf_survival_infected_spawning_delay = spawn_delays[Survival.difficulty_level]

    if Survival.difficulty_level <= 3 then
        Config.inf_hive_hooks = 1
    elseif Survival.difficulty_level <= 7 then
        Config.inf_hive_hooks = 2
    else
        Config.inf_hive_hooks = 3
    end

    local game_conf = Game.Controller:SurvivalGetGameConfiguration()
    if Survival.difficulty_level > 9 then
        game_conf.Hardmode = true
    else
        game_conf.Hardmode = false
    end

    Survival:update_max_players()
end

---@param base_difficulty number
---@param hp_multiplier number
function Survival:set_difficulty(base_difficulty, hp_multiplier)
    Survival.difficulty_level = base_difficulty
    Survival.hp_multiplier = hp_multiplier

    Survival:apply_difficulty()
    update_max_players()
end

SurvivalBats = {}
function SurvivalBats:SetupWave1()
    local wave = 1
    local bot_conf = nil

    local bats_n = {4, 6, 8, 10, 12, 16, 16, 16, 16}
    for i = 1,bats_n[Survival.difficulty_level] do
        bot_conf = Survival:add_normal_infected(wave, EPlayerClass.Bat)
        Survival:add_tweak(bot_conf, ETweaks.WeakHook)
    end

    for i = 1,2 do
        bot_conf = Survival:add_normal_infected(wave, EPlayerClass.Bat)
        bot_conf.SpawnSecond = 10
        Survival:add_tweak(bot_conf, ETweaks.WeakHook)
    end

    for i = 1,2 do
        bot_conf = Survival:add_normal_infected(wave, EPlayerClass.Bat)
        bot_conf.SpawnSecond = 20
        Survival:add_tweak(bot_conf, ETweaks.WeakHook)
    end

    --- level        1,   2,   3,   4,   5,   6,   7,   8,   9,  10
    local boss_hp = {80, 120, 180, 240, 240, 240, 240, 280, 280, 320}
    bot_conf = Survival:add_boss_infected(wave, EPlayerClass.Bat)
    bot_conf.SpawnSecond = 30
    bot_conf.HP = boss_hp[survival_difficulty_level]
    bot_conf.DropLevel = 1
    Survival:add_tweak(bot_conf, ETweaks.NoHook)

    for i = 1,2 do
        bot_conf = Survival:add_normal_infected(wave, EPlayerClass.Bat)
        bot_conf.SpawnSecond = 45
    end

    for i = 1,2 do
        bot_conf = Survival:add_normal_infected(wave, EPlayerClass.Bat)
        bot_conf.SpawnSecond = 50
    end

    for i = 1,4 do
        bot_conf = Survival:add_normal_infected(wave, EPlayerClass.Bat)
        bot_conf.SpawnSecond = 60
    end

    for i = 1,6 do
        bot_conf = Survival:add_normal_infected(wave, EPlayerClass.Bat)
        bot_conf.SpawnSecond = 75
    end

    for i = 1,2 do
        bot_conf = Survival:add_normal_infected(wave, EPlayerClass.Bat)
        bot_conf.SpawnSecond = 80
    end
end

---@param base_difficulty number
---@param hp_multiplier number
function Survival:start_round(base_difficulty, hp_multiplier)
    local game_conf = Game.Controller:SurvivalGetGameConfiguration()
    game_conf:Reset()

    Survival:set_difficulty(base_difficulty, hp_multiplier)

    Game.Controller:SurvivalAddWave(1, "The Only")
    SurvivalBats:SetupWave1()

    Survival.current_wave = 1

    Game.Controller:PrepareSurvival()
    Game.Controller:QueueRound("survival")
    Game.Controller:DoWarmup(3)
    Game.Context:SendChatTarget(-1, string.format("Starting survival round (difficulty %d, max players %d)", Survival.difficulty_level, Survival.max_players))
end

function Survival:setup_votes()
    local vote_index = 0
    local hp_multiplier = 1
    for i = 1,6 do
        local vote_name = string.format("Start survival (level %d, %d players max)", i, Survival:get_max_players_for_difficulty(i, hp_multiplier))
        local vote_command = string.format("lua Survival:start_round(%d, %d)", i, hp_multiplier)
        Game.Context:RemoveVote(vote_command)
        Game.Context:InsertVote(vote_index, vote_name, vote_command)
        vote_index = vote_index + 1
    end
end

print("Survival Round runtime loaded")

Survival:setup_votes()
