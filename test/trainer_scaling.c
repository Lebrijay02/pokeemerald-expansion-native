#include "global.h"
#include "battle.h"
#include "battle_partner.h"
#include "battle_setup.h"
#include "caps.h"
#include "data.h"
#include "difficulty.h"
#include "event_data.h"
#include "pokemon.h"
#include "string_util.h"
#include "trainer_scaling.h"
#include "test/test.h"
#include "constants/abilities.h"
#include "constants/battle.h"
#include "constants/battle_partner.h"
#include "constants/moves.h"
#include "constants/opponents.h"

// Caps are the player's level limit per badge count (src/caps.c), so the scaling curve is
// derived from it rather than from a second table.
TEST("Trainer scaling: the curve follows the level cap table")
{
    u32 i;

    EXPECT_EQ(GetBadgeCountLevelCap(0), 15);
    EXPECT_EQ(GetBadgeCountLevelCap(3), 29);
    EXPECT_EQ(GetBadgeCountLevelCap(7), 46);
    EXPECT_EQ(GetBadgeCountLevelCap(8), 58);
    // More badges than exist behaves like the last entry.
    EXPECT_EQ(GetBadgeCountLevelCap(20), 58);
    for (i = 1; i <= NUM_BADGES; i++)
        EXPECT_GE(GetBadgeCountLevelCap(i), GetBadgeCountLevelCap(i - 1));
}

TEST("Trainer scaling: the curve agrees with GetCurrentLevelCap when badges are earned in order")
{
    u32 i, j;

    ASSUME(B_LEVEL_CAP_TYPE == LEVEL_CAP_FLAG_LIST);
    for (i = 0; i < NUM_BADGES; i++)
        FlagClear(FLAG_BADGE01_GET + i);
    FlagClear(FLAG_IS_CHAMPION);
    for (i = 0; i <= NUM_BADGES; i++)
    {
        for (j = 0; j < NUM_BADGES; j++)
        {
            if (j < i)
                FlagSet(FLAG_BADGE01_GET + j);
            else
                FlagClear(FLAG_BADGE01_GET + j);
        }
        EXPECT_EQ(GetCurrentLevelCap(), GetBadgeCountLevelCap(i));
    }
    for (i = 0; i < NUM_BADGES; i++)
        FlagClear(FLAG_BADGE01_GET + i);
}

TEST("Trainer scaling: badges are counted by flag, in any order")
{
    u32 i;

    for (i = 0; i < NUM_BADGES; i++)
        FlagClear(FLAG_BADGE01_GET + i);
    EXPECT_EQ(CountPlayerBadges(), 0);
    FlagSet(FLAG_BADGE02_GET);
    FlagSet(FLAG_BADGE05_GET);
    FlagSet(FLAG_BADGE08_GET);
    EXPECT_EQ(CountPlayerBadges(), 3);
    for (i = 0; i < NUM_BADGES; i++)
        FlagClear(FLAG_BADGE01_GET + i);
}

TEST("Trainer scaling: link, recorded, Frontier and Trainer Hill battles are not scaled")
{
    ASSUME(B_TRAINER_BADGE_SCALING);

    gBattleTypeFlags = 0;
    EXPECT(IsTrainerScalingActive());
    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    EXPECT(IsTrainerScalingActive());
    gBattleTypeFlags = BATTLE_TYPE_LINK;
    EXPECT(!IsTrainerScalingActive());
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_RECORDED;
    EXPECT(!IsTrainerScalingActive());
    gBattleTypeFlags = BATTLE_TYPE_TRAINER_HILL;
    EXPECT(!IsTrainerScalingActive());
    gBattleTypeFlags = BATTLE_TYPE_BATTLE_TOWER;
    EXPECT(!IsTrainerScalingActive());
    gBattleTypeFlags = 0;
}

TEST("Trainer scaling: levels only go up")
{
    EXPECT_EQ(GetTrainerScalingShift(15, 12), 3);
    EXPECT_EQ(GetTrainerScalingShift(15, 15), 0);
    EXPECT_EQ(GetTrainerScalingShift(10, 40), 0);
}

TEST("Trainer scaling: bosses sit on the cap, ordinary trainers below it")
{
    // Gym leader and rival: on the cap.
    EXPECT_EQ(GetTrainerClassScalingTargetLevel(TRAINER_CLASS_LEADER, 2), 24);
    EXPECT_EQ(GetTrainerClassScalingTargetLevel(TRAINER_CLASS_RIVAL, 0), 15);
    // Magma and Aqua leaders: cap + 2.
    EXPECT_EQ(GetTrainerClassScalingTargetLevel(TRAINER_CLASS_MAGMA_LEADER, 4), 33);
    EXPECT_EQ(GetTrainerClassScalingTargetLevel(TRAINER_CLASS_AQUA_LEADER, 4), 33);
    // Ordinary trainers: cap - 3 by default, class offsets on top.
    EXPECT_EQ(GetTrainerClassScalingTargetLevel(TRAINER_CLASS_HIKER, 3), 26);
    EXPECT_EQ(GetTrainerClassScalingTargetLevel(TRAINER_CLASS_YOUNGSTER, 3), 24);
    EXPECT_EQ(GetTrainerClassScalingTargetLevel(TRAINER_CLASS_LASS, 3), 24);
    EXPECT_EQ(GetTrainerClassScalingTargetLevel(TRAINER_CLASS_BUG_CATCHER, 3), 24);
    EXPECT_EQ(GetTrainerClassScalingTargetLevel(TRAINER_CLASS_COOLTRAINER, 3), 28);
    EXPECT_EQ(GetTrainerClassScalingTargetLevel(TRAINER_CLASS_TEAM_MAGMA, 3), 27);
    // The Elite Four and Champion classes are never scaled.
    EXPECT_EQ(GetTrainerClassScalingTargetLevel(TRAINER_CLASS_ELITE_FOUR, 8), 0);
    EXPECT_EQ(GetTrainerClassScalingTargetLevel(TRAINER_CLASS_CHAMPION, 8), 0);
}

TEST("Trainer scaling: Elite Four, Champion, Steven and gym leader rematches are never scaled")
{
    u32 id;

    EXPECT_EQ(GetTrainerScalingTargetLevel(TRAINER_SIDNEY, 8), 0);
    EXPECT_EQ(GetTrainerScalingTargetLevel(TRAINER_DRAKE, 8), 0);
    EXPECT_EQ(GetTrainerScalingTargetLevel(TRAINER_WALLACE, 8), 0);
    EXPECT_EQ(GetTrainerScalingTargetLevel(TRAINER_STEVEN, 8), 0);
    for (id = TRAINER_ROXANNE_2; id <= TRAINER_JUAN_5; id++)
        EXPECT_EQ(GetTrainerScalingTargetLevel(id, 4), 0);
}

TEST("Trainer scaling: boss tiers are looked up by base ID only")
{
    EXPECT_EQ(GetBossTierTeam(TRAINER_ROXANNE_1, 0), &gBossTierTeams[DIFFICULTY_NORMAL][BOSS_TIER_ROXANNE_EARLY]);
    EXPECT_EQ(GetBossTierTeam(TRAINER_ROXANNE_1, 2), &gBossTierTeams[DIFFICULTY_NORMAL][BOSS_TIER_ROXANNE_EARLY]);
    EXPECT_EQ(GetBossTierTeam(TRAINER_ROXANNE_1, 3), &gBossTierTeams[DIFFICULTY_NORMAL][BOSS_TIER_ROXANNE_MID]);
    EXPECT_EQ(GetBossTierTeam(TRAINER_ROXANNE_1, 5), &gBossTierTeams[DIFFICULTY_NORMAL][BOSS_TIER_ROXANNE_MID]);
    EXPECT_EQ(GetBossTierTeam(TRAINER_ROXANNE_1, 6), &gBossTierTeams[DIFFICULTY_NORMAL][BOSS_TIER_ROXANNE_LATE]);
    EXPECT_EQ(GetBossTierTeam(TRAINER_ROXANNE_1, 8), &gBossTierTeams[DIFFICULTY_NORMAL][BOSS_TIER_ROXANNE_LATE]);
    // Rematches and ordinary trainers have no tiers.
    EXPECT_EQ(GetBossTierTeam(TRAINER_ROXANNE_2, 4), NULL);
    EXPECT_EQ(GetBossTierTeam(TRAINER_CALVIN_1, 4), NULL);
}

TEST("Trainer scaling: tier teams use no trainer IDs")
{
    // The trainer table, and the flags that go with it, did not grow.
    EXPECT_EQ(TRAINERS_COUNT_EMERALD, 855);
}

TEST("Trainer scaling: tier team sizes and aces match their badge tier")
{
    u32 i, j, tier = 0, size = 0, lowestBadges = 0;

    PARAMETRIZE { tier = BOSS_TIER_ROXANNE_EARLY; size = 3; lowestBadges = 0; }
    PARAMETRIZE { tier = BOSS_TIER_ROXANNE_MID;   size = 4; lowestBadges = 3; }
    PARAMETRIZE { tier = BOSS_TIER_ROXANNE_LATE;  size = 5; lowestBadges = 6; }

    const struct Trainer *team = &gBossTierTeams[DIFFICULTY_NORMAL][tier];
    u32 highest = 0;

    EXPECT_EQ((u32)team->partySize, size);
    for (i = 0, j = 0; i < team->partySize; i++)
    {
        if (team->party[i].lvl > highest)
            highest = team->party[i].lvl;
        j++;
    }
    EXPECT_EQ(j, size);
    // Written at the cap of the tier's lowest badge count, since scaling only raises levels.
    EXPECT_EQ(highest, GetBadgeCountLevelCap(lowestBadges));
}

#define TEST_MON(_species, _lvl, ...) { .species = _species, .lvl = _lvl, .gender = TRAINER_MON_RANDOM_GENDER, __VA_ARGS__ }

static const struct TrainerMon sPartyRock[] =
{
    TEST_MON(SPECIES_GEODUDE, 13),
    TEST_MON(SPECIES_ARON, 14),
    TEST_MON(SPECIES_NOSEPASS, 15),
};

static const struct Trainer sRockTrainer =
{
    .trainerClass = TRAINER_CLASS_LEADER,
    .partySize = ARRAY_COUNT(sPartyRock),
    .party = sPartyRock,
};

static u32 GetPartyLevel(struct Pokemon *party, u32 slot)
{
    return GetMonData(&party[slot], MON_DATA_LEVEL);
}

TEST("Trainer scaling: every mon is raised by the same amount so the ace reaches the target")
{
    struct Pokemon party[PARTY_SIZE];

    CreateNPCTrainerPartyScaled(party, &sRockTrainer, 24);
    EXPECT_EQ(GetPartyLevel(party, 0), 22);
    EXPECT_EQ(GetPartyLevel(party, 1), 23);
    EXPECT_EQ(GetPartyLevel(party, 2), 24);
    EXPECT_EQ(GetMonData(&party[3], MON_DATA_SPECIES), SPECIES_NONE);
}

TEST("Trainer scaling: a team already at or above the target is left as written")
{
    struct Pokemon party[PARTY_SIZE];

    CreateNPCTrainerPartyScaled(party, &sRockTrainer, 15);
    EXPECT_EQ(GetPartyLevel(party, 0), 13);
    EXPECT_EQ(GetPartyLevel(party, 2), 15);
    CreateNPCTrainerPartyScaled(party, &sRockTrainer, 10);
    EXPECT_EQ(GetPartyLevel(party, 0), 13);
    EXPECT_EQ(GetPartyLevel(party, 2), 15);
}

TEST("Trainer scaling: target 0 builds the team exactly as written")
{
    struct Pokemon party[PARTY_SIZE];

    CreateNPCTrainerPartyScaled(party, &sRockTrainer, 0);
    EXPECT_EQ(GetPartyLevel(party, 0), 13);
    EXPECT_EQ(GetPartyLevel(party, 1), 14);
    EXPECT_EQ(GetPartyLevel(party, 2), 15);
}

TEST("Trainer scaling: levels never go past 100")
{
    struct Pokemon party[PARTY_SIZE];

    CreateNPCTrainerPartyScaled(party, &sRockTrainer, 100);
    EXPECT_EQ(GetPartyLevel(party, 2), 100);
    EXPECT_EQ(GetPartyLevel(party, 0), 98);
}

static const struct TrainerMon sPartyWurmple[] =
{
    TEST_MON(SPECIES_WURMPLE, 5),
    TEST_MON(SPECIES_WURMPLE, 5, .moves = { MOVE_SPLASH }),
};

static const struct Trainer sWurmpleTrainer =
{
    .trainerClass = TRAINER_CLASS_YOUNGSTER,
    .partySize = ARRAY_COUNT(sPartyWurmple),
    .party = sPartyWurmple,
};

TEST("Trainer scaling: scaled mons evolve, re-roll unlisted moves and keep listed ones")
{
    u32 i;
    enum Species species;
    struct Pokemon party[PARTY_SIZE], reference;

    CreateNPCTrainerPartyScaled(party, &sWurmpleTrainer, 52);

    for (i = 0; i < 2; i++)
    {
        species = GetMonData(&party[i], MON_DATA_SPECIES);
        EXPECT_EQ(GetPartyLevel(party, i), 52);
        EXPECT(species == SPECIES_BEAUTIFLY || species == SPECIES_DUSTOX);
    }

    // No moves listed: the moveset is the new species' level-up moves at the new level.
    species = GetMonData(&party[0], MON_DATA_SPECIES);
    CreateMon(&reference, species, 52, 0, OTID_STRUCT_PLAYER_ID);
    GiveMonInitialMoveset(&reference);
    for (i = 0; i < MAX_MON_MOVES; i++)
        EXPECT_EQ(GetMonData(&party[0], MON_DATA_MOVE1 + i), GetMonData(&reference, MON_DATA_MOVE1 + i));
    EXPECT_NE(GetMonData(&party[0], MON_DATA_MOVE1), MOVE_NONE);

    // Moves listed in the data are kept as written.
    EXPECT_EQ(GetMonData(&party[1], MON_DATA_MOVE1), MOVE_SPLASH);
    EXPECT_EQ(GetMonData(&party[1], MON_DATA_MOVE2), MOVE_NONE);
}

static const struct TrainerMon sPartyListedAbility[] =
{
    TEST_MON(SPECIES_WURMPLE, 5, .ability = ABILITY_SHIELD_DUST),
};

static const struct Trainer sListedAbilityTrainer =
{
    .trainerClass = TRAINER_CLASS_YOUNGSTER,
    .partySize = ARRAY_COUNT(sPartyListedAbility),
    .party = sPartyListedAbility,
};

TEST("Trainer scaling: a mon with a listed ability is raised but does not evolve")
{
    struct Pokemon party[PARTY_SIZE];

    CreateNPCTrainerPartyScaled(party, &sListedAbilityTrainer, 52);
    EXPECT_EQ(GetPartyLevel(party, 0), 52);
    EXPECT_EQ(GetMonData(&party[0], MON_DATA_SPECIES), SPECIES_WURMPLE);
}

TEST("Trainer scaling: Wurmple follows its personality split to a fully evolved species")
{
    u32 personality = 0;
    enum Species expected = SPECIES_NONE;
    struct Pokemon mon;

    PARAMETRIZE { personality = 5u << 16; expected = SPECIES_BEAUTIFLY; }
    PARAMETRIZE { personality = 0; expected = SPECIES_DUSTOX; }

    CreateMon(&mon, SPECIES_WURMPLE, 52, personality, OTID_STRUCT_PLAYER_ID);
    EXPECT(EvolveTrainerMonForLevel(&mon, TRUE));
    EXPECT_EQ(GetMonData(&mon, MON_DATA_SPECIES), expected);
}

TEST("Trainer scaling: a mon below its evolution level does not evolve")
{
    struct Pokemon mon;

    CreateMon(&mon, SPECIES_WURMPLE, 5, 0, OTID_STRUCT_PLAYER_ID);
    EXPECT(!EvolveTrainerMonForLevel(&mon, TRUE));
    EXPECT_EQ(GetMonData(&mon, MON_DATA_SPECIES), SPECIES_WURMPLE);
}

TEST("Trainer scaling: evolutions that need an item, trade or the player's state are skipped")
{
    struct Pokemon mon;

    CreateMon(&mon, SPECIES_EEVEE, 60, 0, OTID_STRUCT_PLAYER_ID);
    EXPECT(!EvolveTrainerMonForLevel(&mon, TRUE));
    EXPECT_EQ(GetMonData(&mon, MON_DATA_SPECIES), SPECIES_EEVEE);
    CreateMon(&mon, SPECIES_KADABRA, 60, 0, OTID_STRUCT_PLAYER_ID);
    EXPECT(!EvolveTrainerMonForLevel(&mon, TRUE));
    EXPECT_EQ(GetMonData(&mon, MON_DATA_SPECIES), SPECIES_KADABRA);
    CreateMon(&mon, SPECIES_POLIWHIRL, 60, 0, OTID_STRUCT_PLAYER_ID);
    EXPECT(!EvolveTrainerMonForLevel(&mon, TRUE));
    EXPECT_EQ(GetMonData(&mon, MON_DATA_SPECIES), SPECIES_POLIWHIRL);
}

TEST("Trainer scaling: an evolved mon gets the new species' moves, name and full HP")
{
    u32 i;
    u8 name[POKEMON_NAME_LENGTH + 1];
    struct Pokemon mon, reference;

    CreateMon(&mon, SPECIES_WURMPLE, 52, 5u << 16, OTID_STRUCT_PLAYER_ID);
    CreateMon(&reference, SPECIES_BEAUTIFLY, 52, 5u << 16, OTID_STRUCT_PLAYER_ID);
    GiveMonInitialMoveset(&reference);
    EXPECT(EvolveTrainerMonForLevel(&mon, TRUE));

    for (i = 0; i < MAX_MON_MOVES; i++)
        EXPECT_EQ(GetMonData(&mon, MON_DATA_MOVE1 + i), GetMonData(&reference, MON_DATA_MOVE1 + i));
    GetMonData(&mon, MON_DATA_NICKNAME, name);
    EXPECT(!StringCompare(name, GetSpeciesName(SPECIES_BEAUTIFLY)));
    EXPECT_GT(GetMonData(&mon, MON_DATA_MAX_HP), 0);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_HP), GetMonData(&mon, MON_DATA_MAX_HP));
}

TEST("Trainer scaling: moves are left alone when the caller does not ask for a re-roll")
{
    u32 i;
    enum Move before[MAX_MON_MOVES];
    struct Pokemon mon;

    CreateMon(&mon, SPECIES_WURMPLE, 52, 5u << 16, OTID_STRUCT_PLAYER_ID);
    GiveMonInitialMoveset(&mon);
    EXPECT_NE(GetMonData(&mon, MON_DATA_MOVE1), MOVE_NONE);
    for (i = 0; i < MAX_MON_MOVES; i++)
        before[i] = GetMonData(&mon, MON_DATA_MOVE1 + i);
    EXPECT(EvolveTrainerMonForLevel(&mon, FALSE));
    for (i = 0; i < MAX_MON_MOVES; i++)
        EXPECT_EQ(GetMonData(&mon, MON_DATA_MOVE1 + i), before[i]);
}


// In-game partners. The test build's PARTNER_STEVEN slot is Steven with Metang 42, Skarmory 43 and Aggron 44.
static void SetBadges(u32 count)
{
    u32 i;

    // The test build has one Steven per difficulty, and other tests leave the difficulty variable set.
    SetCurrentDifficultyLevel(DIFFICULTY_NORMAL);

    for (i = 0; i < NUM_BADGES; i++)
    {
        if (i < count)
            FlagSet(FLAG_BADGE01_GET + i);
        else
            FlagClear(FLAG_BADGE01_GET + i);
    }
}

TEST("Trainer scaling: Steven's partner team reaches the cap at 8 badges and Metang evolves")
{
    ASSUME(B_TRAINER_BADGE_SCALING);

    SetBadges(8);
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_TWO_OPPONENTS | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    FillPartnerPartyScaled(TRAINER_PARTNER(PARTNER_STEVEN));
    gBattleTypeFlags = 0;
    SetBadges(0);

    // Shift of 58 - 44 = 14 for the whole team.
    EXPECT_EQ(GetPartyLevel(gParties[B_TRAINER_PARTNER], 0), 56);
    EXPECT_EQ(GetPartyLevel(gParties[B_TRAINER_PARTNER], 1), 57);
    EXPECT_EQ(GetPartyLevel(gParties[B_TRAINER_PARTNER], 2), 58);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PARTNER][0], MON_DATA_SPECIES), SPECIES_METAGROSS);
    // Listed moves are kept as written.
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PARTNER][0], MON_DATA_MOVE1), MOVE_LIGHT_SCREEN);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PARTNER][0], MON_DATA_MOVE4), MOVE_METAL_CLAW);
}

TEST("Trainer scaling: Steven's partner team stays as written at 0 badges")
{
    ASSUME(B_TRAINER_BADGE_SCALING);

    SetBadges(0);
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    FillPartnerPartyScaled(TRAINER_PARTNER(PARTNER_STEVEN));
    gBattleTypeFlags = 0;

    EXPECT_EQ(GetPartyLevel(gParties[B_TRAINER_PARTNER], 0), 42);
    EXPECT_EQ(GetPartyLevel(gParties[B_TRAINER_PARTNER], 2), 44);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PARTNER][0], MON_DATA_SPECIES), SPECIES_METANG);
}

TEST("Trainer scaling: the unscaled FillPartnerParty never scales, whatever the badges")
{
    SetBadges(8);
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    FillPartnerParty(TRAINER_PARTNER(PARTNER_STEVEN));
    gBattleTypeFlags = 0;
    SetBadges(0);

    EXPECT_EQ(GetPartyLevel(gParties[B_TRAINER_PARTNER], 0), 42);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PARTNER][0], MON_DATA_SPECIES), SPECIES_METANG);
}

TEST("Trainer scaling: a partner in a Frontier battle is not scaled")
{
    SetBadges(8);
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_BATTLE_TOWER;
    FillPartnerPartyScaled(TRAINER_PARTNER(PARTNER_STEVEN));
    gBattleTypeFlags = 0;
    SetBadges(0);

    EXPECT_EQ(GetPartyLevel(gParties[B_TRAINER_PARTNER], 0), 42);
}
