-- Snippet for LSP:
-- local infclass = require("library.Infclass")
-- local Game = infclass.Game
-- local Config = infclass.Config

function on_normandie_tick()
    local tick = Game.Server.Tick
    if (tick + 1) == Game.Controller.InfectionStartTick then
        if Game.Controller.RoundType == "survival" then
            normandie_spawn_cp()
        end
    end
end

function normandie_spawn_cp()
    survival_cp1 = Game.Controller:AddControlPoint(vec2(75.5 * 32, 52 * 32))
    survival_cp2 = Game.Controller:AddControlPoint(vec2(292 * 32, 35 * 32))
end

function on_control_point_effect(control_point)
    if control_point:IsInfected() then
        return
    end

    local give_bonus
    if control_point == survival_cp1 then
        give_bonus = function (character) character:GiveHealth(2, -1) end
    else
        give_bonus = function (character) character:GiveArmor(2, -1) end
    end
    for_each_human_character(give_bonus)
end

on_event("on_tick", on_normandie_tick)

print("Normandie runtime loaded")
