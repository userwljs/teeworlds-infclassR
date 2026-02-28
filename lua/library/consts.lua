local Consts = {}

Consts.Team = {}
Consts.Team.ALL = -2
Consts.Team.SPECTATORS = -1
Consts.Team.RED = 0
Consts.Team.BLUE = 1

Consts.PlayerFlag = {}
Consts.PlayerFlag.PLAYING = 1 << 0
Consts.PlayerFlag.IN_MENU = 1 << 1
Consts.PlayerFlag.CHATTING = 1 << 2
Consts.PlayerFlag.SCOREBOARD = 1 << 3
Consts.PlayerFlag.AIM = 1 << 4
Consts.PlayerFlag.SPEC_CAM = 1 << 5

Consts.KillWeapon = {}
--- team switching etc
Consts.KillWeapon.GAME = -3
--- console kill command
Consts.KillWeapon.SELF = -2
--- death tiles etc
Consts.KillWeapon.WORLD = -1

Consts.AuthLevel = {}
Consts.AuthLevel.NO = 0
Consts.AuthLevel.HELPER = 1
Consts.AuthLevel.MOD = 2
Consts.AuthLevel.ADMIN = 3

Consts.Sound = {}
Consts.Sound.GUN_FIRE = 0
Consts.Sound.SHOTGUN_FIRE = 1
Consts.Sound.GRENADE_FIRE = 2
Consts.Sound.HAMMER_FIRE = 3
Consts.Sound.HAMMER_HIT = 4
Consts.Sound.NINJA_FIRE = 5
Consts.Sound.GRENADE_EXPLODE = 6
Consts.Sound.NINJA_HIT = 7
Consts.Sound.LASER_FIRE = 8
Consts.Sound.LASER_BOUNCE = 9
Consts.Sound.WEAPON_SWITCH = 10
Consts.Sound.PLAYER_PAIN_SHORT = 11
Consts.Sound.PLAYER_PAIN_LONG = 12
Consts.Sound.BODY_LAND = 13
Consts.Sound.PLAYER_AIRJUMP = 14
Consts.Sound.PLAYER_JUMP = 15
Consts.Sound.PLAYER_DIE = 16
Consts.Sound.PLAYER_SPAWN = 17
Consts.Sound.PLAYER_SKID = 18
Consts.Sound.TEE_CRY = 19
Consts.Sound.HOOK_LOOP = 20
Consts.Sound.HOOK_ATTACH_GROUND = 21
Consts.Sound.HOOK_ATTACH_PLAYER = 22
Consts.Sound.HOOK_NOATTACH = 23
Consts.Sound.PICKUP_HEALTH = 24
Consts.Sound.PICKUP_ARMOR = 25
Consts.Sound.PICKUP_GRENADE = 26
Consts.Sound.PICKUP_SHOTGUN = 27
Consts.Sound.PICKUP_NINJA = 28
Consts.Sound.WEAPON_SPAWN = 29
Consts.Sound.WEAPON_NOAMMO = 30
Consts.Sound.HIT = 31
Consts.Sound.CHAT_SERVER = 32
Consts.Sound.CHAT_CLIENT = 33
Consts.Sound.CHAT_HIGHLIGHT = 34
Consts.Sound.CTF_DROP = 35
Consts.Sound.CTF_RETURN = 36
Consts.Sound.CTF_GRAB_PL = 37
Consts.Sound.CTF_GRAB_EN = 38
Consts.Sound.CTF_CAPTURE = 39
Consts.Sound.MENU = 40

Consts.Emoticon = {}
Consts.Emoticon.OOP = 0
Consts.Emoticon.EXCLAMATION = 1
Consts.Emoticon.HEARTS = 2
Consts.Emoticon.DROP = 3
Consts.Emoticon.DOTDOT = 4
Consts.Emoticon.MUSIC = 5
Consts.Emoticon.SORRY = 6
Consts.Emoticon.GHOST = 7
Consts.Emoticon.SUSHI = 8
Consts.Emoticon.SPLATTEE = 9
Consts.Emoticon.DEVILTEE = 10
Consts.Emoticon.ZOMG = 11
Consts.Emoticon.ZZZ = 12
Consts.Emoticon.WTF = 13
Consts.Emoticon.EYES = 14
Consts.Emoticon.QUESTION = 15

Consts.BroadcastPriority = {}
Consts.BroadcastPriority.LOWEST = 0
Consts.BroadcastPriority.WEAPONSTATE = 1
Consts.BroadcastPriority.EFFECTSTATE = 2
Consts.BroadcastPriority.GAMEANNOUNCE = 3
Consts.BroadcastPriority.SERVERANNOUNCE = 4
Consts.BroadcastPriority.INTERFACE = 5

return Consts
