
## Available objects

- Game.Server -> [IServer](#IServer)
- Game.Context -> [CGameServer](#CGameServer)
- Game.Controller -> [CInfClassGameController](#CInfClassGameController)

## Events

Use `on_event(event_str, handler_function)` to subscribe a function to an event. The list of available events and their arguments is the following:

- on_tick()
- on_character_spawned(player_id_int, [spawn_type_str](#Spawn-types))
- on_character_death(victim_int, killer_int, weapon_str)
- on_chat_message(from_int, message_str)
- on_teamchat_message(from_int, message_str)
- on_round_started([round_type_str](#Round-types))
- on_world_reset()
- on_loaded()
- on_shutdown()

For example:
```
on_event("on_character_death",
    function (victim_id, killer_id, weapon_str)
        Game.Context:SendChatTarget(-1, string.format("Player %d killed player %d with weapon '%s' on tick %d", victim_id, killer_id, weapon_str, Game.Server.Tick))
    end
)
```

Also there is a function `get_hero_flag_position()` which should provide the
position via `Game.Controller:ProvideHeroFlagPosition(position_vec2)`. The
flag won't be spawned if the function exists and does not call the method.

### Round types

`on_round_started` event argument can take one of the following values:

- `"classic"`
- `"fun"`
- `"survival"`

### Spawn types

- `"map"`
- `"witch"`
- `"control-point"`

## Classes

### IServer

Properties:
- Tick -> int
- TickSpeed -> int

Methods:
- GetClientName(int ClientID) -> string

### CGameServer

Properties:
- Collision -> [CCollision](#CCollision)
- Paused -> bool (available for read and write)

Methods:
- AddVote(string Text, string Command)
- RemoveVote(string Vote) (where `Vote` is the Text or Command)
- StartVote(string Text, string Command, string Reason)
- EndVote()
- SendChatTarget(int TargetID, string Text)
- SendEmoticon(int ClientID, int Emoticon)

### CInfClassGameController

Properties:
- TimeLimitSeconds
- InfectionDelaySeconds

Methods:
- GetPlayer(int ID) -> [CInfClassPlayer](#CInfClassPlayer)
- GetCharacter(int ID) -> [CInfClassCharacter](#CInfClassCharacter)
- GetSecondsElapsed() -> float
- GetSecondsRemaining() -> float
- GetPlayersNumber() -> [PlayersNumber](#PlayersNumber)
- GetHeroFlagPositions() -> [ArrayVec2](#ArrayVec2)
- ProvideHeroFlagPosition(vec2 Position)
- GetHumanSpawns() -> [ArrayVec2](#ArrayVec2)
- GetInfectedSpawns() -> [ArrayVec2](#ArrayVec2)
- SetHumanSpawnEnabled(int SpawnID, bool Enabled)
- SetInfectedSpawnEnabled(int SpawnID, bool Enabled)
- IsPositionAvailableForHumans(vec2 Position) -> bool
- AddDoor(vec2 From, vec2 To) -> [CDoor](#CDoor)

### CInfClassPlayer

Properties:
- CID -> int
- Class -> string
- MaxHP -> int

Methods:
- IsInfected() -> bool
- ApplyMaxHP()

### CInfClassCharacter

Properties:
- CID -> int

Methods:
- IsInfected() -> bool

### CDoor

Properties:
- Position -> vec2

Methods:
- Destroy()
- SetOpen(bool)

### CCollision

Properties:
- `Width -> int` is the map width in tiles
- `Height -> int` is the map height in tiles

Methods:
- `CheckPoint(vec2 Position) -> bool` returns true if the physical tile in the
  `Position` is a Solid or NoHook tile (including closed door tiles)

### PlayersNumber

Properties:
- Humans -> int
- Infected -> int
- Spectators -> int

### ArrayVec2

Methods:
- `Size() -> int`
- `At(int Index) -> vec2`

Example:
```lua
local human_spawns = Game.Controller:GetHumanSpawns()

for i = 1,human_spawns:Size() do
    local position = human_spawns:At(i)
    print("Position", i, position.x, position.y)
end
```

## Examples

### Console commands

```
lua print(Game.Controller:GetPlayer(0).CID)
lua print(Config.sv_map)

lua Game.Context:StartVote("Change map to infc_skull", "change_map infc_skull", "Fun vote reason")
lua Game.Context:EndVote()

lua my_player = Game.Controller:GetPlayer(0)
lua my_player.Class = "hero"
lua my_player.MaxHP = 200
lua my_player:ApplyMaxHP()

lua my_char = Game.Controller:GetCharacter(0)
lua my_char:PutToSleep(10, -1)

lua Config.sv_map = "infc_skull"
```
