require("survival_game")

if Survival_map_initialized == nil then
    Survival_map_initialized = true

    local infected_spawns = Game.Controller:GetInfectedSpawns()
    local top_left_spawn = {}
    local bottom_left_spawn = {}
    local right_spawn = {}

    for i = 1,infected_spawns:Size() do
        local position = infected_spawns:At(i)
        if position.x > 50 * 32 then
            table.insert(right_spawn, i)
        elseif position.y < 16 * 32 then
            table.insert(top_left_spawn, i)
        else
            table.insert(bottom_left_spawn, i)
        end
    end

    Survival_spawn_zones = {}
    table.insert(Survival_spawn_zones, right_spawn)
    table.insert(Survival_spawn_zones, top_left_spawn)
    table.insert(Survival_spawn_zones, bottom_left_spawn)
end
