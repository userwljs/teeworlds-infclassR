Animations = {}

function Animation_make_context(entity, initial_pos, initial_velocity)
    local context = {}
    context.entity = entity
    context.current_state_id = 0
    context.states = {}
    if initial_pos ~= nil then
        entity:MoveTo(initial_pos)
        entity.Velocity = initial_velocity
    end
    return context
end

function Animation_state_wait_init(context)
    context.state.wait_till_tick = Game.Controller.RoundTick + math.floor(context.state.seconds * 50)
end

function Animation_state_wait_tick(context)
    if Game.Controller.RoundTick >= context.state.wait_till_tick then
        Animation_switch_state(context)
    end
end

function Animation_add_wait(context, seconds)
    local new_state = {}
    new_state.seconds = seconds
    new_state.init = Animation_state_wait_init
    new_state.tick = Animation_state_wait_tick

    table.insert(context.states, new_state)
end

function Animation_state_move_init(context)
    local diff = context.state.position - context.entity.Position
    context.entity.Velocity = diff:Normalized() * context.state.speed

    context.state.wait_till_tick = Game.Controller.RoundTick + diff:Length() / context.state.speed
end

function Animation_state_move_tick(context)
    if Game.Controller.RoundTick >= context.state.wait_till_tick then
        Animation_switch_state(context)
    end
end

function Animation_state_move_finalize(context)
    context.entity.Velocity = vec2(0, 0)
    context.entity:MoveTo(context.state.position)
end

function Animation_add_move(context, position, speed)
    local new_state = {}
    new_state.position = position
    new_state.speed = speed
    new_state.init = Animation_state_move_init
    new_state.tick = Animation_state_move_tick
    new_state.finalize = Animation_state_move_finalize

    table.insert(context.states, new_state)
end

function Animation_submit(context)
    table.insert(Animations, context)
    Animation_switch_state(context)
end

function Animation_switch_state(context)
    if context.state ~= nil then
        if context.state.finalize ~= nil then
            context.state.finalize(context)
        end
    end

    local next_state_id = context.current_state_id + 1
    if next_state_id > table.getn(context.states) then
        next_state_id = 1
    end
    context.current_state_id = next_state_id

    context.state = context.states[next_state_id]
    context.state.init(context)
end

function Animation_state_wait(context)
    if Game.Controller.RoundTick >= context.state.wait_till_tick then
        Animation_switch_state(context)
    end
end

function Animation_state_move_to(context)
    if Game.Controller.RoundTick >= context.state.wait_till_tick then
        Animation_switch_state(context)
    end
end

function Animation_on_tick()
    for i,animation_context in ipairs(Animations) do
        animation_context.state.tick(animation_context)
    end
end

function Animation_on_reset()
    Animations = {}
end

print("Animations v1.0 loaded")
