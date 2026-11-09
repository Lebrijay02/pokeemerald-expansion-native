#include "global.h"
#include "battle.h"
#include "caps.h"
#include "data.h"
#include "event_data.h"
#include "pokemon.h"
#include "trainer_scaling.h"
#include "trainer_util.h"
#include "constants/abilities.h"
#include "constants/battle.h"
#include "constants/battle_ai.h"
#include "constants/flags.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/opponents.h"
#include "constants/pokemon.h"
#include "constants/trainers.h"

// Battle types whose parties must stay exactly as written in the data.
#define BATTLE_TYPES_NOT_SCALED (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK \
                               | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_FRONTIER)

// Boss teams per badge tier. Tier 0: 0-2 badges, tier 1: 3-5, tier 2: 6-8.
// The strongest mon of each tier team is written at the level cap of the lowest badge
// count of its tier, because scaling only raises levels.
#define BADGE_TIER_COUNT 3

// Compiled by trainerproc from src/data/boss_tiers.party. Only DIFFICULTY_NORMAL is filled in.
const struct Trainer gBossTierTeams[DIFFICULTY_COUNT][BOSS_TIER_COUNT] =
{
#include "data/boss_tiers.h"
};

static const struct BossTiers
{
    u16 baseTrainer;
    u8 tierTeam[BADGE_TIER_COUNT];
} sBossTiers[] =
{
    { TRAINER_ROXANNE_1, { BOSS_TIER_ROXANNE_EARLY, BOSS_TIER_ROXANNE_MID, BOSS_TIER_ROXANNE_LATE } },
};

// Gym leader rematches (_2 to _5) stay as written, so Match Call rematches are untouched.
STATIC_ASSERT(TRAINER_JUAN_5 - TRAINER_ROXANNE_2 + 1 == 8 * 4, GymLeaderRematchIdsNotContiguous);

static bool32 IsNeverScaled(u16 trainerId)
{
    if (trainerId >= TRAINER_ROXANNE_2 && trainerId <= TRAINER_JUAN_5)
        return TRUE;
    if (trainerId >= TRAINER_SIDNEY && trainerId <= TRAINER_DRAKE)
        return TRUE;
    return trainerId == TRAINER_WALLACE || trainerId == TRAINER_STEVEN;
}

// Offset from the level cap. Bosses sit on the cap; everyone else sits below it.
static s32 GetClassLevelOffset(u32 trainerClass)
{
    switch (trainerClass)
    {
    case TRAINER_CLASS_LEADER:
    case TRAINER_CLASS_RIVAL:
        return 0;
    case TRAINER_CLASS_MAGMA_LEADER:
    case TRAINER_CLASS_AQUA_LEADER:
        return 2;
    case TRAINER_CLASS_YOUNGSTER:
    case TRAINER_CLASS_LASS:
    case TRAINER_CLASS_BUG_CATCHER:
        return -5;
    case TRAINER_CLASS_TEAM_MAGMA:
    case TRAINER_CLASS_TEAM_AQUA:
        return -2;
    case TRAINER_CLASS_MAGMA_ADMIN:
    case TRAINER_CLASS_AQUA_ADMIN:
    case TRAINER_CLASS_COOLTRAINER:
    case TRAINER_CLASS_COOLTRAINER_2:
        return -1;
    default:
        return -3;
    }
}

u32 CountPlayerBadges(void)
{
    u32 i, count = 0;

    for (i = 0; i < NUM_BADGES; i++)
    {
        if (FlagGet(FLAG_BADGE01_GET + i))
            count++;
    }
    return count;
}

bool32 IsTrainerScalingActive(void)
{
    // The trainer IDs below are Emerald's; FRLG has a different trainer table.
    if (!B_TRAINER_BADGE_SCALING || IS_FRLG)
        return FALSE;
    return !(gBattleTypeFlags & BATTLE_TYPES_NOT_SCALED);
}

u32 GetTrainerClassScalingTargetLevel(u32 trainerClass, u32 badgeCount)
{
    s32 target;

    if (trainerClass == TRAINER_CLASS_ELITE_FOUR || trainerClass == TRAINER_CLASS_CHAMPION)
        return 0;

    target = GetBadgeCountLevelCap(badgeCount) + GetClassLevelOffset(trainerClass);
    if (target < 1)
        target = 1;
    if (target > MAX_LEVEL)
        target = MAX_LEVEL;
    return target;
}

u32 GetTrainerScalingTargetLevel(u16 trainerId, u32 badgeCount)
{
    if (IsNeverScaled(trainerId))
        return 0;
    return GetTrainerClassScalingTargetLevel(GetTrainerStructFromId(trainerId)->trainerClass, badgeCount);
}

u32 GetTrainerScalingShift(u32 targetLevel, u32 highestDataLevel)
{
    if (targetLevel <= highestDataLevel)
        return 0;
    return targetLevel - highestDataLevel;
}

const struct Trainer *GetBossTierTeam(u16 trainerId, u32 badgeCount)
{
    u32 i, tier;

    for (i = 0; i < ARRAY_COUNT(sBossTiers); i++)
    {
        if (sBossTiers[i].baseTrainer != trainerId)
            continue;
        tier = badgeCount / 3;
        if (tier >= BADGE_TIER_COUNT)
            tier = BADGE_TIER_COUNT - 1;
        return &gBossTierTeams[DIFFICULTY_NORMAL][sBossTiers[i].tierTeam[tier]];
    }
    return NULL;
}

// Only conditions that depend on the mon alone are accepted. Anything that reads the player
// (party, bag, friendship, time of day, location) makes the evolution ineligible.
static bool32 EvolutionConditionsAllowed(struct Pokemon *mon, const struct EvolutionParam *params)
{
    u32 i;
    u32 personality = GetMonData(mon, MON_DATA_PERSONALITY);
    u32 upperPersonality = personality >> 16;

    for (i = 0; params != NULL && params[i].condition != CONDITIONS_END; i++)
    {
        bool32 met;

        switch (params[i].condition)
        {
        case IF_GENDER:
            met = GetMonGender(mon) == params[i].arg1;
            break;
        case IF_PID_UPPER_MODULO_10_GT:
            met = (upperPersonality % 10) > params[i].arg1;
            break;
        case IF_PID_UPPER_MODULO_10_EQ:
            met = (upperPersonality % 10) == params[i].arg1;
            break;
        case IF_PID_UPPER_MODULO_10_LT:
            met = (upperPersonality % 10) < params[i].arg1;
            break;
        case IF_PID_MODULO_100_GT:
            met = (personality % 100) > params[i].arg1;
            break;
        case IF_PID_MODULO_100_EQ:
            met = (personality % 100) == params[i].arg1;
            break;
        case IF_PID_MODULO_100_LT:
            met = (personality % 100) < params[i].arg1;
            break;
        default:
            return FALSE;
        }
        if (!met)
            return FALSE;
    }
    return TRUE;
}

static enum Species GetLevelEvolutionTarget(struct Pokemon *mon, enum Species species, u32 level)
{
    u32 i;
    const struct Evolution *evolutions = GetSpeciesEvolutions(species);

    if (evolutions == NULL)
        return SPECIES_NONE;
    for (i = 0; evolutions[i].method != EVOLUTIONS_END; i++)
    {
        if (evolutions[i].method != EVO_LEVEL || evolutions[i].param == 0 || level < evolutions[i].param)
            continue;
        // A target that is disabled in include/config/species_enabled.h sanitizes to SPECIES_NONE.
        if (SanitizeSpeciesId(evolutions[i].targetSpecies) == SPECIES_NONE || !EvolutionConditionsAllowed(mon, evolutions[i].params))
            continue;
        return evolutions[i].targetSpecies;
    }
    return SPECIES_NONE;
}

bool32 EvolveTrainerMonForLevel(struct Pokemon *mon, bool32 rerollMoves)
{
    u32 i;
    u32 level = GetMonData(mon, MON_DATA_LEVEL);
    bool32 evolved = FALSE;

    // Evolution lines are short; the bound only guards against a data loop.
    for (i = 0; i < 4; i++)
    {
        enum Species species = GetMonData(mon, MON_DATA_SPECIES);
        enum Species target = GetLevelEvolutionTarget(mon, species, level);

        if (target == SPECIES_NONE)
            break;
        EvolutionRenameMon(mon, species, target);
        SetMonData(mon, MON_DATA_SPECIES, &target);
        evolved = TRUE;
    }
    if (!evolved)
        return FALSE;

    // The new species has its own learnset, so the moves come after the evolution.
    if (rerollMoves)
    {
        u32 j;
        enum Move none = MOVE_NONE;
        u32 noPp = 0;

        for (j = 0; j < MAX_MON_MOVES; j++)
        {
            SetMonData(mon, MON_DATA_MOVE1 + j, &none);
            SetMonData(mon, MON_DATA_PP1 + j, &noPp);
        }
        GiveMonInitialMoveset(mon);
    }
    CalculateMonStats(mon);
    return TRUE;
}

void GenerateScaledTrainerMon(struct Pokemon *mon, const struct TrainerMon *trainerMon, struct TrainerGenerator *generator, u32 shift)
{
    struct TrainerMon scaledMon;
    u32 level, i;
    bool32 hasListedMoves = FALSE;

    if (shift == 0)
    {
        GenerateMonFromTrainerMon(mon, trainerMon, generator);
        return;
    }

    scaledMon = *trainerMon;
    level = scaledMon.lvl + shift;
    scaledMon.lvl = level > MAX_LEVEL ? MAX_LEVEL : level;
    GenerateMonFromTrainerMon(mon, &scaledMon, generator);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (trainerMon->moves[i] != MOVE_NONE)
            hasListedMoves = TRUE;
    }
    if (trainerMon->ability == ABILITY_NONE)
        EvolveTrainerMonForLevel(mon, !hasListedMoves);
}
