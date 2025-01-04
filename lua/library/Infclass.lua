---@meta

local infclass = {}

---@class Vec2
---@field x number
---@field y number
local Vec2 = {}

---@return number
function Vec2:Length() end

---@return Vec2
function Vec2:Normalized() end

---@class ArrayVec2
local ArrayVec2 = {}

---@return number
function ArrayVec2:Size() end

---@param index number
---@return Vec2
function ArrayVec2:At(index) end


---@class CLuaPlayersNumber
---@field Humans number
---@field Infected number
---@field Spectators number


---@class CIcPlayer
---@field Class string
---@field MaxHP number
---@field Tag string
local CIcPlayer = {}


---@class CIcEntity
---@field Position Vec2
---@field Velocity Vec2
---@field Lifespan number
---@field ProximityRadius number
local CIcEntity = {}

---@param position Vec2
function CIcEntity:MoveTo(position) end


---@class CIcCharacter
---@field CID number
---@field Health number
---@field Armor number
local CIcCharacter = {}

---@return boolean
function CIcCharacter:IsInfected() end

---@return boolean
function CIcCharacter:IsAlive() end

---@param amount number
---@param from number Give the armor on behalf of player 'from' (use -1 as 'no player')
function CIcCharacter:GiveArmor(amount, from) end

---@param amount number
---@param from number Give the health on behalf of player 'from' (use -1 as 'no player')
function CIcCharacter:GiveHealth(amount, from) end

---@param amount number
function CIcCharacter:IncreaseOverallHp(amount) end

---@return boolean
function CIcCharacter:IsInSlowMotion() end

---@return boolean
function CIcCharacter:IsInvisible() end

function CIcCharacter:MakeVisible() end

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
function CIcCharacter:IsFrozen() end

---@return number
function CIcCharacter:FreezeStartTick() end

function CIcCharacter:Unfreeze() end

---@param from number Try to unfreeze on behalf of player 'from' (use -1 as 'no player')
function CIcCharacter:TryUnfreeze(from) end

---@return boolean
function CIcCharacter:IsPoisoned() end

function CIcCharacter:ResetPoisonEffect() end

---@class CConfig
---@field about_source_url string The server source code URL
---@field about_translation_url string The translation site URL
---@field about_contacts_discord string Discord server invite URL
---@field about_contacts_telegram string Telegram URL or ID
---@field about_contacts_matrix string Matrix room URL
---@field sv_lua_runtime string Lua runtime file
---@field inf_converter_id string Map converter version id
---@field inf_converter_force_regeneration number Always (re)generate client map (regardless of cache)
---@field sv_maps_base_url string Client maps base URL (HTTPS)
---@field sv_show_open_doors number Show open doors (0 = no, 1 = yes)
---@field sv_timelimit_in_seconds number Time limit in seconds (0 means 'fallback to sv_timelimit')
---@field sv_max_ddnet_version number Automatically kick clients with DDNet version higher than specified
---@field sv_use_ddnet_skins number Enable DDNet skins for DDNet clients (can cause issues on certain client configs)
---@field inf_smart_maprotation number Enable smart map rotation algorhythm
---@field inf_inactive_humans_kick_time number How many seconds to wait before taking care of inactive humans
---@field inf_inactive_infected_kick_time number How many seconds to wait before taking care of inactive infected
---@field inf_initial_infection_delay number The number of seconds until the game infect the first humans in the round
---@field inf_training_mode number Enable the training mode (with commands)
---@field inf_event string Special event
---@field inf_default_round_type string Default round type
---@field inf_min_players number Minimum number of players to start the round
---@field inf_teambalance_seconds number How many seconds to wait before autobalancing teams
---@field inf_challenge number Enable challenges
---@field inf_accusation_threshold number Number of accusations needed to start a banvote
---@field inf_leaver_ban_time number How long an infected gets banned (in minutes), when leaving and leaving causes a human to get infected
---@field inf_show_score_time number Number of seconds the score will be shown at the end of a round
---@field inf_maprotation_random number When enabled, next map in rotation will be chosen randomly
---@field inf_first_infected_limit number The number of initially infected players
---@field inf_min_rounds_map_vote number Minimum number of rounds before a new map can be voted
---@field inf_min_player_percent_map_vote number Minimum percentage of players that are needed to start a map vote
---@field inf_min_player_number_map_vote number Minimum number of players that are needed to start a map vote
---@field inf_con_waiting_time number Number of seconds to wait before enter the game
---@field inf_captcha number Enable captcha
---@field inf_shock_wave_affect_humans number Explosion shockwave affect humans
---@field inf_spawn_protection_time number Time zombies stay invincible while spawning
---@field inf_anti_fire_time number Time players can't attack after spawning (in ms)
---@field inf_class_chooser number Enable the class chooser
---@field inf_allow_picking_same_class number Allow a player to pick the same class again
---@field inf_taxi number Toggle taxi mode (disabled, enabled (without passengers ammo regen), enabled
---@field inf_taxi_collisions number Set taxi collision flags (1 for attach collisions, 2 for move collisions
---@field inf_defender_limit number Maximum number of defenders in game
---@field inf_medic_limit number Maximum number of medics in game
---@field inf_hero_limit number Maximum number of heros in game
---@field inf_support_limit number Maximum number of supports in game
---@field inf_witch_limit number Maximum number of witches in game
---@field inf_soldier_bombs number Number of bombs for the soldier
---@field inf_merc_bombs number Number of the mercenary bomb upgrades
---@field inf_merc_bomb_max_damage number The max damage of a fully upgraded mercenary bomb
---@field inf_merc_love number Enables love bombs for the mercenary (hammer)
---@field inf_barrier_lifespan number Barrier lifespan
---@field inf_voodoo_alive_time number How long a voodoo keeps staying alive after being killed (in ms)
---@field inf_barrier_timereduce number Time to remove from a barrier lifespan when an infected dies (centisec)
---@field inf_bio_mine_lasers number Radius of mines
---@field inf_human_invisibility_time number Humans invisibility effect duration in seconds
---@field inf_mine_radius number Radius of mines
---@field inf_mine_limit number Maximum number of mines per player
---@field inf_ninja_jump number Maximum number of katana attacks
---@field inf_ninja_min_infected number Minimum number of infected to activate the target system
---@field inf_ninja_target_afk_time number How long in minutes before an afk zombie in the infection zone does not count as target anymore
---@field inf_poison_damage number Damage deals by the poison grenades
---@field inf_poison_duration number The poison grenades effect duration (int ms)
---@field inf_ghoul_digestion number Time for a ghoul to digest an infected (sec)
---@field inf_ghoul_stomach_size number Number of dead that the ghoul can eat
---@field inf_ghoul_threshold number Ghouls will only be created when the number of infected has reached this threshold
---@field inf_slime_duration number How long Slug-Slime will stay in game (in seconds)
---@field inf_slime_poison_damage number The total damage from Slug-Slime
---@field inf_slime_poison_interval number The interval between slug slime poison 1HP damage
---@field inf_slime_heal_rate number Slug-Slime heals infected for X hearts every second
---@field inf_slime_max_heal number The maximum total HP that can be gained from Slug Slime (reasonable max value is 20)
---@field inf_infzone_heal_rate number Infection zone heals infected for X hearts every second
---@field inf_sleeper_take_damage_ratio number The ratio of damage taken by a sleeping tee (1.0 to 4.0)
---@field inf_scientist_tp_selfharm number Self damage on each teleportation
---@field inf_sci_portal_lifespan number aaa
---@field inf_bat_airjump_limit number Max number of extra airjumps
---@field inf_bat_damage number Damage taken by bat
---@field inf_bat_life_steal number Amount of HP given to a bat per hammer hit
---@field inf_bat_hook_time number For how long bat will be able to hook humans (in seconds)
---@field inf_spider_hook_time number For how long spiders will be able to hook humans (in seconds)
---@field inf_spider_web_hook_length number The maximum length of spider web hook
---@field inf_smoker_hook_damage number Damage taken by smoker (hook)
---@field inf_spider_catch_humans number Always catch humans with hook
---@field inf_undead_freeze_duration number For how long Undead death will freeze the character (in seconds) (0 = disable)
---@field inf_stunning_hammer_force number (Infected) stunning hammer force
---@field inf_infzone_freeze_duration number For how long infection zone will freeze humans (in seconds) (0 = disable)
---@field inf_last_enforcer_time_ms number For how long the last hooker will be forced as the char indirect killer (in ms)
---@field inf_double_click_filter_ms number Filter out probably undesired 2nd clicks during given ms (affects soldier bomb)
---@field inf_proba_smoker number Probability for an infected to be a smoker
---@field inf_proba_hunter number Probability for an infected to be a hunter
---@field inf_proba_bat number Probability for an infected to be a bat
---@field inf_proba_boomer number Probability for an infected to be a boomer
---@field inf_proba_ghost number Probability for an infected to be a ghost
---@field inf_proba_spider number Probability for an infected to be a spider
---@field inf_proba_ghoul number Probability for an infected to be a ghoul
---@field inf_proba_slug number Probability for an infected to be a slug
---@field inf_proba_voodoo number Probability for an infected to be a voodoo
---@field inf_proba_witch number Probability for an infected to be a witch
---@field inf_proba_undead number Probability for an infected to be an undead
---@field inf_enable_engineer number Makes the engineer class available
---@field inf_enable_soldier number Makes the soldier class available
---@field inf_enable_scientist number Makes the scientist class available
---@field inf_enable_biologist number Makes the biologist class available
---@field inf_enable_looper number Makes the looper class available
---@field inf_enable_mercenary number Makes the mercenary class available
---@field inf_enable_sniper number Makes the sniper class available
---@field inf_enable_ninja number Makes the ninja class available
---@field inf_enable_medic number Makes the medic class available
---@field inf_enable_hero number Makes the hero class available
---@field inf_enable_following_camera number Enable the camera following a teammate in some cases (bf, kill, etc)
---@field inf_min_players_for_engineer number Minimum number of players that are needed to enable Engineer class
---@field inf_proba_spawn_near_witch number Probability for an infected to spawn near a witch
---@field inf_enable_tranquilizer_rifle number Replace revival laser with tranquilizer rifle (for medic)
---@field inf_revival_damage number The number of total HP taken from the medic
---@field inf_revival_min_infected number The minimum number of infected to allow revival
---@field inf_hero_flag_indicator number Shows the heros in which direction the next flag is
---@field inf_hero_flag_indicator_time number How many seconds the hero has to stand still until the indicator is shown
---@field funround_title string Fun round title
---@field funround_limit number Number of possible fun rounds per map
---@field funround_duration number Fun round duration (min)
---@field funround_ghoul_stomach_size number Number of dead that the ghoul can eat
---@field tips_interval number Interval between tip messages (minutes, 0 to disable)
---@field inf_slow_motion_wall_duration number How long looper wall slow motion effect will slow down zombies (in centiSec)
---@field inf_slow_motion_gun_duration number How long looper gun slow motion effect will slow down zombies (in centiSec)
---@field inf_looper_barrier_life_span number How long looper barrier will last (in seconds)
---@field inf_looper_barrier_timereduce number Time to remove from a barrier lifespan when an infected dies (centisec)
---@field inf_slow_motion_percent number Factor that manipulates the slowmotion intensity
---@field inf_slow_motion_hook_speed number Factor that manipulates the slowmotion hook speed
---@field inf_slow_motion_hook_accel number Factor that manipulates the slowmotion hook acceleration
---@field inf_slow_motion_max_speed number Create a speed limit while in slowmotion, make it 0 to disable it
---@field inf_slow_motion_gravity number Modify gravity while in slowmotion
---@field inf_cp_caption_radius number Control Point inner (proximity) radius
---@field inf_cp_visual_radius number Control Point outer (effect) radius
---@field inf_cp_global_effect_interval number Control Point global effect rate (1HP every N seconds)
---@field inf_min_players_for_turrets number Minimum number of players that are needed to enable turrets
---@field inf_turret_enable number If turrets are available
---@field inf_turret_give number Gives hero extra turrets
---@field inf_turret_duration number turret life span
---@field inf_turret_self_destruct_dmg number damage taken by zombie if turret is destroyed
---@field inf_turret_radar_range number turret radar range
---@field inf_turret_enable_laser number enable turret laser ammunition
---@field inf_turret_enable_plasma number enable turret plasma ammunition
---@field inf_turret_plasma_reload_duration number plasma ammo reload duration
---@field inf_turret_laser_reload_duration number laser ammo reload duration
---@field inf_turret_plasma_life_span number plasma life span
---@field inf_turret_warm_up_duration number turret warm up duration
---@field inf_turret_dmg_factor number how much damage a plasma turret does, 10 == grenade explosion
---@field inf_turret_dmg_health_laser number how much damage in life points a laser turret does
---@field inf_turret_ammunition number number of projectiles per multi-shot (not per zombie)
---@field inf_turret_max_per_player number maximal number of turrets per player
---@field inf_blindness_duration number The duration of blindness in seconds
---@field inf_min_players_for_white_hole number Minimal number of players that are needed to enable white hole
---@field inf_white_hole_minimal_kills number Minimal number of kills before white hole become available
---@field inf_white_hole_probability number Probability of super weapon being available after MinimalKill requirement
---@field inf_white_hole_life_span number White hole life span
---@field inf_white_hole_radius number Radius of white holes
---@field inf_white_hole_affects_humans number Makes white holes suck in humans
---@field inf_white_hole_num_particles number Number of particles that will be used for a white hole animation
---@field inf_white_hole_pull_strength number How strong a white hole sucks players in
---@field inf_bot_lives number The number of bot lives (for survive rounds)
---@field inf_debug_bot number Filter the bot debug by one bot Id (-1 to unset)
---@field inf_bot_debug_level number Set the bots debug level (0 = off, 4 = max)
---@field inf_bot_remove_delay number Delay the bots removal (on lives==0) for X seconds
---@field inf_bot_backjump number Enable backjumps
---@field inf_bot_check_pos number Enable 'checked pos' logic for bots
---@field inf_survival_infected_spawning_delay number The number of seconds for humans to pull themselves together
---@field inf_hive_hooks number Max concurrent hooks per player for Survival rounds
---@field inf_survival_mode number Survival mode (0 = off, 1 = kill-based, 2 = time-based)
---@field inf_survival_hardmode number Survival hard mode (another way of difficulty leveling)
---@field inf_survival_autostart number Automatically start the last choosen survival scenario
---@field inf_stun_grenade_minimal_kills number Deprecated (has no effect now)
---@field inf_stun_grenade_probability number Deprecated (has no effect now)
---@field inf_slime_poison_duration number Deprecated (has no effect now, use inf_slime_poison_damage instead)
---@field inf_fast_download number Deprecated (use sv_fast_download instead)
---@field inf_map_window number Deprecated (use sv_map_window instead)
local Config = {}

infclass.Config = Config

local Game = {}
infclass.Game = Game

---@class IServer
---@field Tick number The current tick
---@field TickSpeed number The number of ticks per second
local Server = {}
Game.Server = Server

---@class CGameServer
---@field Paused boolean Whether the game is paused
local Context = {}
Game.Context = Context


---@param text string The vote description text
---@param command string The vote command to execute on voted yes
function Context:AddVote(text, command) end

---@param index number The vote index (starts with 0)
---@param text string The vote description text
---@param command string The vote command to execute on voted yes
function Context:InsertVote(index, text, command) end

---@param vote string The vote description or command
function Context:RemoveVote(vote) end

---@param text string The vote description text
---@param command string The vote command to execute on voted yes
---@param reason string The vote reasons
function Context:StartVote(text, command, reason) end

function Context:EndVote() end

---@param targetId number The current tick
---@param text string The vote description text
function Context:SendChatTarget(targetId, text) end

---@class CIcGameController
---@field GameType string The server gametype
---@field RoundType string The type of the round
---@field RoundStartTick number
---@field InfectionStartTick number
---@field RoundTick number
---@field TimeLimitSeconds number
local CIcGameController = {}

---@param player_id number
---@return CIcPlayer
function CIcGameController:GetPlayer(player_id) end

---@param player_id number
---@return CIcCharacter
function CIcGameController:GetCharacter(player_id) end

---@return CLuaPlayersNumber
function CIcGameController:GetPlayersNumber() end

---@return number
function CIcGameController:GetSecondsElapsed() end

---@return number
function CIcGameController:GetSecondsAfterInfection() end

---@return number
function CIcGameController:GetSecondsRemaining() end

---@param seconds number Starts a warmup for the given number of seconds
function CIcGameController:DoWarmup(seconds) end

---@param round_type string The type of the round
function CIcGameController:QueueRound(round_type) end

function CIcGameController:StartRound() end
function CIcGameController:FinishRound() end
function CIcGameController:CancelRound() end

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

-- SURVIVAL STUFF
---@class TweaksArray
local TweaksArray = {}

---@param tweak_name string
function TweaksArray:Add(tweak_name) end

---@class SurvivalBotConfiguration
---@field Class string The class name
---@field SpawnSecond number Spawn second
---@field SpawnPointId number Spawn point ID
---@field SpawnWitchId number Spawn witch ID
---@field Lives number Lives
---@field HP number MaxHP
---@field DropLevel number Drop level
---@field RespawnInterval number Respawn interval in seconds
---@field Tag string
local SurvivalBotConfiguration = {}

---@return TweaksArray
function SurvivalBotConfiguration:GetTweaks() end

---@class SurvivalGameConfiguration
---@field MaxPlayers number
---@field Hardmode boolean
local SurvivalGameConfiguration = {}

function SurvivalGameConfiguration:Reset() end

---@return SurvivalGameConfiguration
function CIcGameController:SurvivalGetGameConfiguration() end

---@param wave number The wave number (starts with 1)
---@param wave_name string The wave name (title)
function CIcGameController:SurvivalAddWave(wave, wave_name) end

---@param wave number The wave number (starts with 1)
---@param class_name string The bot class name
---@return SurvivalBotConfiguration
function CIcGameController:SurvivalAddBot(wave, class_name) end

function CIcGameController:PrepareSurvival() end

Game.Controller = CIcGameController

return infclass
