require("survival_game")

if Survival_map_initialized == nil then
    Survival_map_initialized = true

    local infected_spawns = Game.Controller:GetInfectedSpawns()
    local left_spawn = {}
    local right_spawn = {}

    for i = 1, infected_spawns:Size() do
        local position = infected_spawns:At(i)
        if position.x > 50 * 32 then
            table.insert(right_spawn, i)
        else
            table.insert(left_spawn, i)
        end
    end

    Survival_spawn_zones = {}
    table.insert(Survival_spawn_zones, right_spawn)
    table.insert(Survival_spawn_zones, left_spawn)
end
