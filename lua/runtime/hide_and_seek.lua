-- Snippet for LSP:
--local infclass = require("library.Infclass")
--local Game = infclass.Game
--local Config = infclass.Config

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
