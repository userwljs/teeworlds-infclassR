#ifndef GAME_SERVER_ENTITIES_FILTER_H
#define GAME_SERVER_ENTITIES_FILTER_H

class CEntity;

// `true` means "accept this entity"
// `false` means "ignore this entity"
using EntityFilter = bool (*)(const CEntity *);

#endif // GAME_SERVER_ENTITIES_FILTER_H
