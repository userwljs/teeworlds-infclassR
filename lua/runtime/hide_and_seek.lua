-- Snippet for LSP:
if false then
    infclass = require("library.Infclass")
    Game = infclass.Game
    Config = infclass.Config
end

Submod.HideAndSeek = "hide-and-seek"
active_submod = Submod.HideAndSeek

function choose_human_class(player)
    return "medic"
end

function choose_infected_class(player)
    return "ghost"
end

Config.sv_timelimit_in_seconds = 100
Config.sv_rounds_per_map = 8
Config.hs_medics_limit = 2
Config.inf_default_round_type = "hide-and-seek"

Game.Controller:QueueRound("hide-and-seek")
Game.Controller:DoWarmup(3)

print("Hide and Seek runtime loaded")
