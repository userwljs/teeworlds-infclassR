#ifndef INFCLASS_BOT_CONFIG_PARSER_H
#define INFCLASS_BOT_CONFIG_PARSER_H

#include "bot-player.h"

int ParseSpawnTime(const char *pStr, bool *pOk);
int ParseLives(const char *pStr, bool *pOk);
int ParseHP(const char *pStr, bool *pOk);
int ParseDropLevel(const char *pStr, bool *pOk);
float ParseRespawn(const char *pStr, bool *pOk);
bool ParseTweaks(const char *pStr, TweaksArray *pTweaks);

#endif // INFCLASS_BOT_CONFIG_PARSER_H
