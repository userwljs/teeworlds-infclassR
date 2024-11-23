require("survival_game")

if Survival_map_initialized == nil then
    Survival_map_initialized = true

    local infected_spawns = Game.Controller:GetInfectedSpawns()
    local left_mid_spawn = {}
    local top_spawn = {}
    local top_right_spawn = {}
    local bottom_spawn = {}

    for i = 1,infected_spawns:Size() do
        local position = infected_spawns:At(i)
        if position.y > 60 * 32 then
            table.insert(bottom_spawn, i)
        elseif position.x < 9.5 * 32 then
            table.insert(left_mid_spawn, i)
        elseif position.x < 18.5 * 32 then
            table.insert(top_right_spawn, i)
        else
            table.insert(top_spawn, i)
        end
    end

    Survival_spawn_zones = {}
    table.insert(Survival_spawn_zones, top_right_spawn)
    table.insert(Survival_spawn_zones, left_mid_spawn)
end
