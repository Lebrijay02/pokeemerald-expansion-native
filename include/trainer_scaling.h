#ifndef GUARD_TRAINER_SCALING_H
#define GUARD_TRAINER_SCALING_H

#include "data.h"
#include "constants/boss_tiers.h"

extern const struct Trainer gBossTierTeams[DIFFICULTY_COUNT][BOSS_TIER_COUNT];

// Badge-based trainer scaling. See B_TRAINER_BADGE_SCALING in include/config/battle.h.

u32 CountPlayerBadges(void);

// FALSE when the config is off or the current battle type must keep its data as written
// (link, recorded, Battle Frontier, Trainer Hill).
bool32 IsTrainerScalingActive(void);

// Level the strongest mon of the trainer should reach with `badgeCount` badges,
// or 0 if the trainer is never scaled (Elite Four, Champion, Steven, gym leader rematches).
u32 GetTrainerScalingTargetLevel(u16 trainerId, u32 badgeCount);

// Same, from the trainer class alone. 0 for the Elite Four and Champion classes.
u32 GetTrainerClassScalingTargetLevel(u32 trainerClass, u32 badgeCount);

// How many levels to add to every mon so the strongest one reaches the target.
// Raise only: never negative.
u32 GetTrainerScalingShift(u32 targetLevel, u32 highestDataLevel);

// Boss team (src/data/boss_tiers.party) for the player's badge tier, or NULL if `trainerId` is not a
// boss with tiers. Only base boss IDs have tiers; rematch IDs and anything else return NULL.
const struct Trainer *GetBossTierTeam(u16 trainerId, u32 badgeCount);

// Evolves `mon` while it is past a plain level-up evolution (including the personality and
// gender splits such as Wurmple), then optionally re-rolls the level-up moveset for the new
// species, then recalculates stats. Returns TRUE if the species changed.
bool32 EvolveTrainerMonForLevel(struct Pokemon *mon, bool32 rerollMoves);

#endif // GUARD_TRAINER_SCALING_H
