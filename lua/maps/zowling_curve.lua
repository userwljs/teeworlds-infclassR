
if Zowling_loaded == nil then
    Zowling_loaded = true
    require("runtime.zowling")
end

laser_top_left = vec2(127 * 32, 10 * 32)
Zowling_class = "undead"

zowling_min_interval = 95
zowling_max_interval = 95
zowling_min_velocity = 7
zowling_max_velocity = 7
Zowling_freeze_tele_delay_ticks = 5

laser_1to2_p1 = vec2(9.5 * 32, 5.5 * 32)
laser_1to2_p2 = vec2(15.5 * 32, 5.5 * 32)
laser_1to2_velocity_y = 1.25
laser_1to2_max_y = 930

base_x = -10 * 32
Zowling_mines_max_x = 107 * 32
Zowling_first_mines_delay = 0
Zowling_global_score = 0
Zowling_default_armor = 20
Zowling_autostart = true

if Zowling_curve_vars_set == nil then
    Zowling_curve_vars_set = true

    Zowling_lvl2_mines_counter = 0
    Zowling_turret_velocity = 11

    laser_lvl1_2_instance1 = nil
    laser_lvl1_2_instance2 = nil
end

Zowling_finish_boundary = vec2(116 * 32, 18 * 32)

function spawn_high_lvl2()
    local ball = spawn_ball()
    ball.Position = vec2(base_x, 21 * 32)
    return ball
end

function spawn_mid_lvl2()
    local ball = spawn_ball()
    ball.Position = vec2(base_x, 24 * 32)
    return ball
end

function spawn_low_lvl2()
    local ball = spawn_ball()
    ball.Position = vec2(base_x, 27 * 32)
    return ball
end

function spawn_one_lvl2()
    local ind = math.random(1, 3)
    if ind == 1 then
        spawn_high_lvl2()
    elseif ind == 2 then
        spawn_mid_lvl2()
    else
        spawn_low_lvl2()
    end
end

function spawn_two_lvl2()
    local ind = math.random(1, 3)
    if ind == 1 then
        spawn_mid_lvl2()
        spawn_high_lvl2()
    elseif ind == 2 then
        spawn_mid_lvl2()
        spawn_low_lvl2()
    else
        spawn_high_lvl2()
        spawn_low_lvl2()
    end
end

function Zowling_reset_entities()
    if Zowling_entities ~= nil then
        for i,entity in ipairs(Zowling_entities) do
            entity:Destroy()
        end
    end

    Zowling_entities = {}

    laser_lvl1_2_instance1 = nil
    laser_lvl1_2_instance2 = nil
end

function Zowling_add_looper_wall(from, to)
    local wall = Game.Controller:AddLooperWall()
    wall.Position = from
    wall.SecondPosition = to
    table.insert(Zowling_entities, wall)
    return wall
end

function Zowling_add_laser_wall(from, to)
    local wall = Game.Controller:AddLaserWall()
    wall.MaxLength = 32 * 32
    wall.Position = from
    wall.SecondPosition = to
    table.insert(Zowling_entities, wall)
    return wall
end

function Zowling_add_sci_mine(at)
    local mine = Game.Controller:AddSciMine()
    mine.Position = at
    -- Do not store pointers to destructable objects
    -- table.insert(Zowling_entities, mine)
    return mine
end

function Zowling_add_turret(at)
    local turret = Game.Controller:AddTurret()
    turret.Position = at
    turret.ReloadDuration = 0.2
    turret.Destructable = false
    table.insert(Zowling_entities, turret)
    return turret
end

function Zowling_respawn_entities()
    Zowling_reset_entities()
    Animation_on_reset()

    -- Level1
    local looperWall1_1 = Zowling_add_looper_wall(vec2(107*32, 10*32), vec2(107*32, 17*32))
    local laser1_1 = Zowling_add_laser_wall(vec2(109 * 32, 10* 32), vec2(109 * 32, 16* 32))

    -- top way blocker
    local lvl1_mine1 = Zowling_add_sci_mine(vec2(38 * 32, 9.5 * 32))

    local entrance_mine1 = Zowling_add_sci_mine(vec2(24*32, 13*32))

    local enterance_laser1 = Zowling_add_laser_wall(vec2(15.5 * 32, 11.5 *32), vec2(15.5 * 32, 18.5 * 32))

    -- Level2
    laser_lvl1_2_instance1 = Zowling_add_laser_wall(laser_1to2_p1, laser_1to2_p2)
    laser_lvl1_2_instance1.Velocity = vec2(0, laser_1to2_velocity_y)

    local yHalfDistance = (laser_1to2_max_y - laser_1to2_p1.y) / 2
    laser_lvl1_2_instance2 = Zowling_add_laser_wall(laser_1to2_p1, laser_1to2_p2)
    laser_lvl1_2_instance2.Position = vec2(laser_1to2_p1.x, laser_1to2_p1.y + yHalfDistance)
    laser_lvl1_2_instance2.SecondPosition = vec2(laser_1to2_p2.x, laser_1to2_p2.y + yHalfDistance)
    laser_lvl1_2_instance2.Velocity = vec2(0, laser_1to2_velocity_y)

    local entrance_mine2 = Zowling_add_sci_mine(vec2(24*32, 21*32))
    local entrance_mine3 = Zowling_add_sci_mine(vec2(24*32, 27*32))

    local laser2_1 = Zowling_add_laser_wall(vec2(24.5 * 32, 24.5 * 32), vec2(38.5 * 32, 24.5 * 32))

    local looperWall2_1 = Zowling_add_looper_wall(vec2(64 * 32, 20 * 32), vec2(64 * 32, 22 * 32))
    local looperWall2_2 = Zowling_add_looper_wall(vec2(67 * 32, 20 * 32), vec2(67 * 32, 22 * 32))

    local looperWall2_3 = Zowling_add_looper_wall(vec2(74 * 32, 27 * 32), vec2(82 * 32, 27 * 32))

    local mine2_1 = Zowling_add_sci_mine(vec2(78 * 32, 27 * 32))

    local laser2_2 = Zowling_add_laser_wall(vec2(94.5 * 32, 18.5 * 32), vec2(103.5 * 32, 18.5 * 32))

    local laser2_3 = Zowling_add_laser_wall(vec2(89 * 32, 23 * 32), vec2(105.5 * 32, 23 * 32))

    local laser2_3_animation = Animation_make_context(laser2_3, vec2(89 * 32, 23 * 32), vec2(0, 0))
    Animation_add_move(laser2_3_animation, vec2(89 * 32, 23 * 32), laser_1to2_velocity_y)
    Animation_add_move(laser2_3_animation, vec2(89 * 32, 25 * 32), laser_1to2_velocity_y)
    Animation_submit(laser2_3_animation)

    -- Level 3
    local laser3_1 = Zowling_add_laser_wall(vec2(104.5 * 32, 33 * 32), vec2(118.5 * 32, 33 * 32))

    local laser3_2 = Zowling_add_laser_wall(vec2(104 * 32, 41 * 32), vec2(108 * 32, 41 * 32))

    local turret_3_1 = Zowling_add_turret(vec2(106 * 32, 31 * 32))
    local animation_context = Animation_make_context(turret_3_1)
    Animation_add_wait(animation_context, 1)
    Animation_add_move(animation_context, vec2(106 * 32, 31 * 32), Zowling_turret_velocity)
    Animation_add_wait(animation_context, 1)
    Animation_add_move(animation_context, vec2(117 * 32, 31 * 32), Zowling_turret_velocity)
    Animation_submit(animation_context)

    local laser3_3 = Zowling_add_laser_wall(vec2(100 * 32, 37 * 32), vec2(104 * 32, 37 * 32))

    local laser3_4 = Zowling_add_laser_wall(vec2(84 * 32, 32.5 * 32), vec2(100 * 32, 32.5 * 32))

    local turret_3_2 = Zowling_add_turret(vec2(102 * 32, 31 * 32))
    local animation_context3_2 = Animation_make_context(turret_3_2)
    Animation_add_wait(animation_context3_2, 0.75)
    Animation_add_move(animation_context3_2, vec2(86 * 32, 31 * 32), Zowling_turret_velocity)
    Animation_add_wait(animation_context3_2, 0.5)
    Animation_add_move(animation_context3_2, vec2(86 * 32, 43 * 32), Zowling_turret_velocity)
    Animation_add_wait(animation_context3_2, 0.5)
    Animation_add_move(animation_context3_2, vec2(102 * 32, 43 * 32), Zowling_turret_velocity)
    Animation_add_wait(animation_context3_2, 0.5)
    Animation_add_move(animation_context3_2, vec2(102 * 32, 35 * 32), Zowling_turret_velocity)
    Animation_add_wait(animation_context3_2, 1.5)
    Animation_add_move(animation_context3_2, vec2(102 * 32, 31 * 32), Zowling_turret_velocity)
    Animation_submit(animation_context3_2)

    -- Mirrored room
    local turret_3_3 = Zowling_add_turret(vec2(82 * 32, 43 * 32))
    turret_3_3.Damage = 50
    local animation_context3_3 = Animation_make_context(turret_3_3)
    Animation_add_move(animation_context3_3, vec2(82 * 32, 39 * 32), Zowling_turret_velocity)
    Animation_add_wait(animation_context3_3, 0.5)
    Animation_add_move(animation_context3_3, vec2(82 * 32, 43 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_3, vec2(76 * 32, 43 * 32), Zowling_turret_velocity)
    Animation_add_wait(animation_context3_3, 0.5)
    Animation_add_move(animation_context3_3, vec2(82 * 32, 43 * 32), Zowling_turret_velocity)
    Animation_submit(animation_context3_3)

    local turret_3_4 = Zowling_add_turret(vec2(28 * 32, 43 * 32))
    turret_3_4.Damage = 50
    local animation_context3_4 = Animation_make_context(turret_3_4)
    Animation_add_move(animation_context3_4, vec2(28 * 32, 39 * 32), Zowling_turret_velocity)
    Animation_add_wait(animation_context3_4, 0.5)
    Animation_add_move(animation_context3_4, vec2(28 * 32, 43 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_4, vec2(34 * 32, 43 * 32), Zowling_turret_velocity)
    Animation_add_wait(animation_context3_4, 0.5)
    Animation_add_move(animation_context3_4, vec2(28 * 32, 43 * 32), Zowling_turret_velocity)
    Animation_submit(animation_context3_4)

    -- Last room on the Level3
    local laser3_5 = Zowling_add_looper_wall(vec2(24 * 32, 32.5 * 32), vec2(24 * 32, 41.5 * 32))

    local turret_3_5 = Zowling_add_turret(vec2(24 * 32, 43 * 32))
    local animation_context3_5 = Animation_make_context(turret_3_5)
    Animation_add_move(animation_context3_5, vec2(24 * 32, 31 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_5, vec2(20 * 32, 31 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_5, vec2(20 * 32, 33 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_5, vec2(15 * 32, 33 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_5, vec2(15 * 32, 31 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_5, vec2(11 * 32, 31 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_5, vec2(11 * 32, 35 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_5, vec2(09 * 32, 35 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_5, vec2(09 * 32, 39 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_5, vec2(11 * 32, 39 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_5, vec2(11 * 32, 43 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_5, vec2(24 * 32, 43 * 32), Zowling_turret_velocity)
    Animation_submit(animation_context3_5)

    local turret_3_6 = Zowling_add_turret(vec2(11 * 32, 31 * 32))
    local animation_context3_6 = Animation_make_context(turret_3_6)
    Animation_add_move(animation_context3_6, vec2(11 * 32, 35 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_6, vec2(09 * 32, 35 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_6, vec2(09 * 32, 39 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_6, vec2(11 * 32, 39 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_6, vec2(11 * 32, 43 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_6, vec2(24 * 32, 43 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_6, vec2(24 * 32, 31 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_6, vec2(20 * 32, 31 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_6, vec2(20 * 32, 33 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_6, vec2(15 * 32, 33 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_6, vec2(15 * 32, 31 * 32), Zowling_turret_velocity)
    Animation_add_move(animation_context3_6, vec2(11 * 32, 31 * 32), Zowling_turret_velocity)
    Animation_submit(animation_context3_6)
end

function Zowling_on_world_reset_override()
    -- The entities are destroyed by the game server.
    -- Cleanup dangling pointers
    Zowling_entities = {}
    Animation_on_reset()

    Zowling_lvl2_mines_counter = 0
    Zowling_respawn_entities()
end

function Zowling_on_tick2()
    Animation_on_tick()

    local tick = Game.Controller.InfectionTick
    if tick < 50 then
        Zowling_global_score = 0
        return
    end

    if laser_lvl1_2_instance1.Position.y > laser_1to2_max_y then
        laser_lvl1_2_instance1.Position = laser_1to2_p1
        laser_lvl1_2_instance1.SecondPosition = laser_1to2_p2
    end
    if laser_lvl1_2_instance2.Position.y > laser_1to2_max_y then
        laser_lvl1_2_instance2.Position = laser_1to2_p1
        laser_lvl1_2_instance2.SecondPosition = laser_1to2_p2
    end

    if tick + 1 == Zowling_next_spawn_tick then
        Zowling_lvl2_mines_counter = Zowling_lvl2_mines_counter + 1
        if (Zowling_lvl2_mines_counter % 6) == 0 then
            -- Spawn only one mine each 6 spawns
            spawn_one_lvl2()
        elseif (Zowling_lvl2_mines_counter % 7) ~= 0 then
            -- Skip each 7 spawn
            spawn_two_lvl2()
        end
    end

    ---@param character CInfClassCharacter
    local function grant_safe_unfreeze(character)
        if character:IsFrozen() then
            local cid = character.CID
            local player = Game.Controller:GetPlayer(cid)
            player.MaxHP = 30
            player:ApplyMaxHP()
            character:GiveArmor(Zowling_default_armor)

            if Game.Server.Tick == (character.FreezeStartTick + Zowling_freeze_tele_delay_ticks) then
                local teleId = character.TeleCheckpoint
                if teleId ~= 0 then
                    character:TeleportToTeleId(teleId, 30)
                    character:Unfreeze()
                end
            end
        end
    end
    for_each_infected_character(grant_safe_unfreeze)
end

function tele_everyone(tele_id)
    ---@param character CInfClassCharacter
    local function grant_safe_unfreeze(character)
        character:TeleportToTeleId(tele_id, 30)
    end
    for_each_infected_character(grant_safe_unfreeze)
end

function Zowling_give_scores2()
    local tick = Game.Controller.InfectionTick
    local bonus = 3
    if tick > 2000 then
        bonus = 5
    end

    local Zowling_chars_on_2nd_floor = 0
    local Zowling_chars_on_finish = 0
    ---@param character CInfClassCharacter
    local function collect_progress(character)
        if not character:IsAlive() or character:IsFrozen() then
            return
        end
        if character.Position.y > Zowling_finish_boundary.y then
            Zowling_chars_on_2nd_floor = Zowling_chars_on_2nd_floor + 1

            if character.Position.x > Zowling_finish_boundary.x then
                Zowling_chars_on_finish = Zowling_chars_on_finish + 1
            end
        end
    end
    for_each_infected_character(collect_progress)

    local players_number = Game.Controller:GetPlayersNumber()
    if players_number.Infected == 0 then
        return
    end
    Zowling_global_score = Zowling_global_score + (Zowling_chars_on_2nd_floor * 3 + Zowling_chars_on_finish * 10) / players_number.Infected

    ---@param character CInfClassCharacter
    local function give_bonus(character)
        if not character:IsAlive() or character:IsFrozen() then
            return
        end
        local cid = character.CID
        local player = Game.Controller:GetPlayer(cid)
        player.Score = math.floor(Zowling_global_score)
    end
    for_each_infected_character(give_bonus)
end

if Zowling_curve_initialized == nil then
    Zowling_curve_initialized = true
    on_event("on_tick", Zowling_on_tick2)

    Zowling_give_scores_fn = Zowling_give_scores2

    Config.sv_timelimit = 5

    Game.Context:RemoveVote("lua start_zowling(\"normal\")")
    Game.Context:RemoveVote("lua start_zowling(\"fast\")")
    Game.Controller.RaceEnabled = true
end

Zowling_respawn_entities()
