-- Snippet for LSP:
-- local infclass = require("library.Infclass")
-- local Game = infclass.Game
-- local Config = infclass.Config

function on_hq_tick()
    local tick = Game.Server.Tick
    if (tick + 1) == Game.Controller.InfectionStartTick then
        if Game.Controller.RoundType == "survival" then
            hq_spawn_cp()
        end
    end
end

function hq_spawn_cp()
    survival_cp1 = Game.Controller:AddControlPoint(vec2(220 * 32, 48 * 32))
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

on_event("on_tick", on_hq_tick)

print("HQ runtime loaded")
