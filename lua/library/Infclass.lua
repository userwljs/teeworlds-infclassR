---@meta
local Config = require("config")

local infclass = {}

---@class vec2
---@field x number
---@field y number
vec2 = {}

---@return number
function vec2:Length() end

---@return vec2
function vec2:Normalized() end

---@class ArrayVec2
local ArrayVec2 = {}

---@return number
function ArrayVec2:Size() end

---@param index number
---@return vec2
function ArrayVec2:At(index) end

---@class PlayersNumber
---@field Humans number
---@field Infected number
---@field Spectators number

--- Constructor: `CClientMask(number)` or `CClientMask(string)`
---
--- Example: `CClientMask(0)` creates a CClientMask with all bits set to false
---@class CClientMask
local CClientMask = {}

--- Set all bits to true
function CClientMask:SetAll() end

--- Set all bits to false
function CClientMask:UnsetAll() end

--- Set the bit at position `pos` to the value `value`
---@param pos number
---@param value boolean
function CClientMask:Set(pos, value) end

---@param pos number
---@return boolean
function CClientMask:At(pos) end

---@return number
function CClientMask:Size() end

---@class CPlayer
---@field Team number
---@field CID number
---@field PlayerFlags number
---@field SpectatorID number
---@field RespawnTick number
---@field DieTick number
local CPlayer = {}

function CPlayer:Respawn() end

---@param team number Consts.Team
---@param do_chat_msg? boolean @default true
function CPlayer:SetTeam(team, do_chat_msg) end

---@param weapon number Consts.KillWeapon
function CPlayer:KillCharacter(weapon) end

---@return boolean
function CPlayer:IsBot() end

---@class CIcPlayer : CPlayer
---@field Class string
---@field MaxHP number
---@field Tag string
---@field Score number
---@field Deaths number
local CIcPlayer = {}

---@return boolean
function CIcPlayer:HookProtectionEnabled() end

---@param value boolean
---@param automatic? boolean @default true
function CIcPlayer:SetHookProtection(value, automatic) end

---@return boolean
function CIcPlayer:SpecialCameraIsActive() end

function CIcPlayer:ResetSpecialCamera() end

---@return number
function CIcPlayer:GetSpecialCameraTargetCid() end

---@param client_id number
---@param duration number
function CIcPlayer:SetSpecialCameraTargetCid(client_id, duration) end

---@class CBaseBotPlayer : CIcPlayer
---@field Lives number
---@field MaxLives number
---@field RespawnInterval number
---@field LastSeenTargetAtTick number
local CBaseBotPlayer = {}

function CBaseBotPlayer:UpdateControls() end

---@class CHiveMind
local CHiveMind = {}

---@class CIcEntity
---@field Position vec2
---@field Velocity vec2
---@field Lifespan number
---@field ProximityRadius number
local CIcEntity = {}

---@param position vec2
function CIcEntity:MoveTo(position) end

function CIcEntity:Destroy() end

function CIcEntity:MarkForDestroy() end

---@class CPlacedObject : CIcEntity
---@field SecondPosition vec2
---@field MaxLength number
local CPlacedObject = {}

---@return boolean
function CPlacedObject:HasSecondPosition() end

---@class CControlPoint : CPlacedObject
local CControlPoint = {}

---@return boolean
function CControlPoint:IsTaken() end

---@return boolean
function CControlPoint:IsInfected() end

---@param seconds number
function CControlPoint:SetNextEffectTime(seconds) end

---@class CDoor : CPlacedObject
local CDoor = {}

---@param open boolean
function CDoor:SetOpen(open) end

---@class CScientistMine : CPlacedObject
local CScientistMine = {}

---@param tiles number
function CScientistMine:SetExplosionRadius(tiles) end

---@class CTurret : CPlacedObject
---@field ReloadDuration number
---@field Damage number
---@field Destructable boolean
local CTurret = {}

---@class CCharacter
---@field CID number
---@field Position vec2
---@field Health number
---@field Armor number
---@field MaxArmor number
---@field TeleCheckpoint number
local CCharacter = {}

---@return vec2
function CCharacter:GetPosition() end

---@param position vec2
function CCharacter:SetPosition(position) end

---@param relative_position vec2
function CCharacter:Move(relative_position) end

function CCharacter:ResetVelocity() end

---@param new_velocity vec2
function CCharacter:SetVelocity(new_velocity) end

---@param addition vec2
function CCharacter:AddVelocity(addition) end

---@return number
function CCharacter:GetHealthArmorSum() end

---@param health_amount number
---@param armor_amount number
function CCharacter:SetHealthArmor(health_amount, armor_amount) end

---@param weapon number
---@param ammo number
function CCharacter:AddAmmo(weapon, ammo) end

---@param weapon number
function CCharacter:GetAmmo(weapon) end

---@param weapon number
---@param ammo? number @default -1
function CCharacter:GiveWeapon(weapon, ammo) end

function CCharacter:TakeAllWeapons() end

---@param amount number
---@return boolean
function CCharacter:IncreaseOverallHp(amount) end

---@param emote number
---@param tick number
function CCharacter:SetEmote(emote, tick) end

---@return boolean
function CCharacter:IsAlive() end

---@param tele_number number
---@param tele_type number
function CCharacter:TeleportToTeleId(tele_number, tele_type) end

---@class CIcCharacter : CCharacter
---@field FreezeStartTick number
local CIcCharacter = {}

---@return boolean
function CIcCharacter:IsInfected() end

---@param hit_points number
---@param from? number
---@return boolean
function CIcCharacter:Heal(hit_points, from) end

---@param amount number
---@param from number Give the armor on behalf of player 'from' (use -1 as 'no player')
function CIcCharacter:GiveArmor(amount, from) end

---@param amount number
---@param from number Give the health on behalf of player 'from' (use -1 as 'no player')
function CIcCharacter:GiveHealth(amount, from) end

---@return boolean
function CIcCharacter:GetDropLevel() end

---@param level number
function CIcCharacter:SetDropLevel(level) end

---@return boolean
function CIcCharacter:IsPassenger() end

---@return boolean
function CIcCharacter:HasPassenger() end

---@return CIcCharacter
function CIcCharacter:GetPassenger() end

---@return CIcCharacter
function CIcCharacter:GetTaxi() end

--- Driver is the last Taxi in a chain
---@return CIcCharacter
function CIcCharacter:GetTaxiDriver() end

---@return boolean
function CIcCharacter:IsInvisible() end

function CIcCharacter:MakeVisible() end

---@param duration number
function CIcCharacter:GrantInvisibility(duration) end

---@param invincible number
function CIcCharacter:SetInvincible(invincible) end

---@param active boolean
function CIcCharacter:SetDeepDefence(active) end

---@return boolean
function CIcCharacter:IsSleeping() end

---@param duration number Sleep duration in seconds
---@param from number Put to sleep on behalf of player 'from' (use -1 as 'no player')
function CIcCharacter:PutToSleep(duration, from) end

function CIcCharacter:CancelSleep() end

---@return number
function CIcCharacter:AwakenedBy() end

---@param duration number The effect duration in seconds
---@param from number Make blind on behalf of player 'from' (use -1 as 'no player')
function CIcCharacter:MakeBlind(duration, from) end

function CIcCharacter:ResetBlindness() end

---@return boolean
function CIcCharacter:IsBlind() end

---@return boolean
function CIcCharacter:IsFrozen() end

function CIcCharacter:Unfreeze() end

---@param from number Try to unfreeze on behalf of player 'from' (use -1 as 'no player')
function CIcCharacter:TryUnfreeze(from) end

---@return boolean
function CIcCharacter:IsInSlowMotion() end

---@param duration number
---@param from? number
---@return number
function CIcCharacter:MakeSlow(duration, from) end

---@return boolean
function CIcCharacter:IsPoisoned() end

function CIcCharacter:ResetPoisonEffect() end

---@param time number
function CIcCharacter:LoveEffect(time) end

---@return boolean
function CIcCharacter:IsInLove() end

function CIcCharacter:ResetLoveEffect() end

---@return boolean
function CIcCharacter:IsDead() end

---@param duration number
function CIcCharacter:SetDeadForDuration(duration) end

---@param seconds number
function CIcCharacter:SetAntiFireDuration(seconds) end

infclass.Config = Config

local Game = {}
infclass.Game = Game

---@class IServer
---@field Tick number The current tick
---@field TickSpeed number The number of ticks per second
local Server = {}
Game.Server = Server

---@param client_id number
---@return number Consts.AuthLevel
function Server:GetAuthedState(client_id) end

---@param client_id number
---@param reason string
function Server:Kick(client_id, reason) end

---@param client_id number
---@return string
function Server:GetClientName(client_id) end

---@class CCollision
---@field Width number
---@field Height number
local CCollision = {}

---@param pos vec2
---@return boolean
function CCollision:CheckPoint(pos) end

---@class CGameServer
---@field Collision CCollision
---@field Paused boolean Whether the game is paused
local Context = {}
Game.Context = Context

---@param index number The vote index (starts with 0)
---@param text string The vote description text
---@param command string The vote command to execute on voted yes
function Context:InsertVote(index, text, command) end

---@param text string The vote description text
---@param command string The vote command to execute on voted yes
function Context:AddVote(text, command) end

---@param vote string The vote description or command
function Context:RemoveVote(vote) end

function Context:ClearVotes() end

---@param text string The vote description text
---@param command string The vote command to execute on voted yes
---@param reason string The vote reasons
function Context:StartVote(text, command, reason) end

function Context:EndVote() end

---@param pos vec2
---@param angel_mod number
---@param amount number
---@param mask? CClientMask
function Context:CreateDamageInd(pos, angel_mod, amount, mask) end

---@param pos vec2
---@param owner number
---@param weapon number Consts.KillWeapon
---@param mask? CClientMask
function Context:CreateExplosion(pos, owner, weapon, mask) end

---@param pos vec2
---@param mask? CClientMask
function Context:CreateHammerHit(pos, mask) end

---@param pos vec2
---@param mask? CClientMask
function Context:CreatePlayerSpawn(pos, mask) end

---@param pos vec2
---@param who number
---@param mask? CClientMask
function Context:CreateDeath(pos, who, mask) end

---@param pos vec2
---@param sound number Consts.Sound
---@param mask? CClientMask
function Context:CreateSound(pos, sound, mask) end

---@param sound number Consts.Sound
---@param target? number @default -1
function Context:CreateSoundGlobal(sound, target) end

---@param targetId number The current tick
---@param text string The vote description text
function Context:SendChatTarget(targetId, text) end

---@param client_id number
---@param team number Consts.Team
---@param text string
---@param spam_proetction_client_id? number @default -1
function Context:SendChat(client_id, team, text, spam_proetction_client_id) end

---@param client_id number
---@param emoticon number Consts.Emoticon
function Context:SendEmoticon(client_id, emoticon) end

---@param to number
---@param text string
---@param priority number Consts.BroadcastPriority
---@param lifespan number
function Context:SendBroadcast(to, text, priority, lifespan) end

---@class MapInfo
---@field MinimumPlayers number
---@field MaximumPlayers number
---@field Enabled boolean
---@field Name string
local MapInfo = {}

---@class IGameController
---@field RaceEnabled boolean
---@field HealthArmorHudEnabled boolean
---@field AmmoHudEnabled boolean
local IGameController = {}

---@return boolean
function IGameController:IsGameOver() end

---@class CIcGameController : IGameController
---@field GameType string The server gametype
---@field RoundType string The type of the round
---@field RoundStartTick number
---@field InfectionTick number
---@field InfectionStartTick number
---@field RoundTick number
---@field TimeLimitSeconds number
---@field InfectionDelaySeconds number
---@field WinCheckEnabled boolean
---@field VotesEnabled boolean
---@field RoundMinimumPlayers number
---@field RoundMinimumInfected number
local CIcGameController = {}

---@param map_name string
---@return MapInfo
function CIcGameController:AddMapInfo(map_name) end

---@param map_name string
---@return MapInfo
function CIcGameController:GetMapInfo(map_name) end

---@param player_id number
---@return CIcPlayer
function CIcGameController:GetPlayer(player_id) end

---@param player_id number
---@return CIcCharacter
function CIcGameController:GetCharacter(player_id) end

---@return number
function CIcGameController:GetSecondsAfterInfection() end

---@return number
function CIcGameController:GetSecondsElapsed() end

---@return number
function CIcGameController:GetSecondsRemaining() end

---@param include_bots? boolean @default false
---@return PlayersNumber
function CIcGameController:GetPlayersNumber(include_bots) end

---@return ArrayVec2
function CIcGameController:GetHeroFlagPositions() end

---@return ArrayVec2
function CIcGameController:GetHumanSpawns() end

---@return ArrayVec2
function CIcGameController:GetInfectedSpawns() end

---@param index number
---@param enabled boolean
function CIcGameController:SetHumanSpawnEnabled(index, enabled) end

---@param index number
---@param enabled boolean
function CIcGameController:SetInfectedSpawnEnabled(index, enabled) end

---@param flag_position vec2
---@return boolean
function CIcGameController:IsPositionAvailableForHumans(flag_position) end

function CIcGameController:UpdateHeroFlags() end

function CIcGameController:StartRound() end

function CIcGameController:FinishRound() end

function CIcGameController:CancelRound() end

---@param round_type string The type of the round
function CIcGameController:QueueRound(round_type) end

---@param seconds number Starts a warmup for the given number of seconds
function CIcGameController:DoWarmup(seconds) end

function CIcGameController:PrepareSurvival() end

---@param class_name string
---@return boolean
function CIcGameController:GetPlayerClassEnabled(class_name) end

---@param class_name string
---@param enabled boolean
function CIcGameController:SetPlayerClassEnabled(class_name, enabled) end

---@param class_name string
---@return boolean
function CIcGameController:ResetPlayerClassEnabled(class_name) end

function CIcGameController:ResetPlayerClassesEnablement() end

---@param at vec2
---@return CControlPoint
function CIcGameController:AddControlPoint(at) end

---@param from vec2
---@param to vec2
---@return CDoor
function CIcGameController:AddDoor(from, to) end

---@return CScientistMine
function CIcGameController:AddSciMine() end

---@return CPlacedObject
function CIcGameController:AddLaserWall() end

---@return CPlacedObject
function CIcGameController:AddLooperWall() end

---@return CTurret
function CIcGameController:AddTurret() end

---@param class_name string
---@return CBaseBotPlayer
function CIcGameController:AddBot(class_name) end

---@param client_id number
---@return CBaseBotPlayer
function CIcGameController:GetBot(client_id) end

---@param client_id number
---@return boolean
function CIcGameController:RemoveBot(client_id) end

function CIcGameController:RemoveAllBots() end

---@return SurvivalGameConfiguration
function CIcGameController:SurvivalGetGameConfiguration() end

---@param wave number The wave number (starts with 1)
---@param wave_name string The wave name (title)
function CIcGameController:SurvivalAddWave(wave, wave_name) end

---@param wave number The wave number (starts with 1)
---@param class_name string The bot class name
---@return SurvivalBotConfiguration
function CIcGameController:SurvivalAddBot(wave, class_name) end

-- SURVIVAL STUFF
---@class TweaksArray
local TweaksArray = {}

---@param tweak_name string
function TweaksArray:Add(tweak_name) end

---@param tweak_name string
function TweaksArray:Remove(tweak_name) end

---@param tweak_name string
---@return boolean
function TweaksArray:Contains(tweak_name) end

---@param index number
---@return string
function TweaksArray:At(index) end

---@return number
function TweaksArray:Size() end

---@class SurvivalBotConfiguration
---@field Class string The class name
---@field Tag string
---@field SpawnSecond number Spawn second
---@field SpawnPointId number Spawn point ID
---@field SpawnWitchId number Spawn witch ID
---@field ScriptedSpawn boolean Makes the game using lua Get_character_spawn_position() if set
---@field Lives number Lives
---@field HP number MaxHP
---@field DropLevel number Drop level
---@field RespawnInterval number Respawn interval in seconds
local SurvivalBotConfiguration = {}

---@return TweaksArray
function SurvivalBotConfiguration:GetTweaks() end

---@class SurvivalGameConfiguration
---@field Hardmode boolean
local SurvivalGameConfiguration = {}

function SurvivalGameConfiguration:Reset() end

Game.Controller = CIcGameController

return infclass
