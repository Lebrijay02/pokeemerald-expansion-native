// Options-style debug page opened from the start menu (DEBUG_QUICK_MENU).
// Left/Right picks a value (L/R jump on the GIVE row), A applies the selected row, B closes.
#include "global.h"
#include "debug_quick_menu.h"
#include "event_object_lock.h"
#include "event_object_movement.h"
#include "field_screen_effect.h"
#include "heal_location.h"
#include "main.h"
#include "map_name_popup.h"
#include "menu.h"
#include "overworld.h"
#include "pokemon.h"
#include "region_map.h"
#include "rtc.h"
#include "script.h"
#include "script_pokemon_util.h"
#include "sound.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/heal_locations.h"
#include "constants/pokemon.h"
#include "constants/species.h"
#include "constants/rtc.h"
#include "constants/songs.h"

enum QuickDebugRow
{
    ROW_TIME,
    ROW_TELEPORT,
    ROW_GIVE_MON,
    ROW_CLOSE,
    ROW_COUNT,
};

#define LINE_HEIGHT  16
#define CURSOR_X     0
#define LABEL_X      10
#define VALUE_X      88

#define tWindowId    data[0]
#define tRow         data[1]
#define tTime        data[2]
#define tDestination data[3]
#define tSpecies     data[4]
#define tStatus      data[5]

#define GIVE_MON_LEVEL       50
#define SPECIES_FAST_STEP    50 // L/R jump on the GIVE row

enum GiveStatus
{
    GIVE_STATUS_NONE,
    GIVE_STATUS_PARTY,
    GIVE_STATUS_PC,
    GIVE_STATUS_FAILED,
};

// HEAL_LOCATION_NONE stands for the player's house, which depends on gender
static const u8 sDestinations[] =
{
#if IS_FRLG
    HEAL_LOCATION_PALLET_TOWN,
    HEAL_LOCATION_VIRIDIAN_CITY,
    HEAL_LOCATION_PEWTER_CITY,
    HEAL_LOCATION_ROUTE4,
    HEAL_LOCATION_CERULEAN_CITY,
    HEAL_LOCATION_VERMILION_CITY,
    HEAL_LOCATION_ROUTE10,
    HEAL_LOCATION_LAVENDER_TOWN,
    HEAL_LOCATION_CELADON_CITY,
    HEAL_LOCATION_SAFFRON_CITY,
    HEAL_LOCATION_FUCHSIA_CITY,
    HEAL_LOCATION_CINNABAR_ISLAND,
    HEAL_LOCATION_INDIGO_PLATEAU,
    HEAL_LOCATION_ONE_ISLAND,
    HEAL_LOCATION_TWO_ISLAND,
    HEAL_LOCATION_THREE_ISLAND,
    HEAL_LOCATION_FOUR_ISLAND,
    HEAL_LOCATION_FIVE_ISLAND,
    HEAL_LOCATION_SIX_ISLAND,
    HEAL_LOCATION_SEVEN_ISLAND,
#else
    HEAL_LOCATION_NONE, // Littleroot Town
    HEAL_LOCATION_OLDALE_TOWN,
    HEAL_LOCATION_PETALBURG_CITY,
    HEAL_LOCATION_RUSTBORO_CITY,
    HEAL_LOCATION_DEWFORD_TOWN,
    HEAL_LOCATION_SLATEPORT_CITY,
    HEAL_LOCATION_MAUVILLE_CITY,
    HEAL_LOCATION_VERDANTURF_TOWN,
    HEAL_LOCATION_FALLARBOR_TOWN,
    HEAL_LOCATION_LAVARIDGE_TOWN,
    HEAL_LOCATION_FORTREE_CITY,
    HEAL_LOCATION_LILYCOVE_CITY,
    HEAL_LOCATION_MOSSDEEP_CITY,
    HEAL_LOCATION_SOOTOPOLIS_CITY,
    HEAL_LOCATION_PACIFIDLOG_TOWN,
    HEAL_LOCATION_EVER_GRANDE_CITY,
    HEAL_LOCATION_BATTLE_FRONTIER_OUTSIDE_EAST,
    HEAL_LOCATION_SOUTHERN_ISLAND_EXTERIOR,
#endif
};

static const u8 *const sTimeNames[TIMES_OF_DAY_COUNT] =
{
    [TIME_MORNING] = COMPOUND_STRING("MORNING"),
    [TIME_DAY]     = COMPOUND_STRING("DAY"),
    [TIME_EVENING] = COMPOUND_STRING("EVENING"),
    [TIME_NIGHT]   = COMPOUND_STRING("NIGHT"),
};

static const u8 *const sRowLabels[ROW_COUNT] =
{
    [ROW_TIME]     = COMPOUND_STRING("TIME"),
    [ROW_TELEPORT] = COMPOUND_STRING("TELEPORT"),
    [ROW_GIVE_MON] = COMPOUND_STRING("GIVE Lv50"),
    [ROW_CLOSE]    = COMPOUND_STRING("CLOSE"),
};

static const u8 sText_Title[] = _("DEBUG");
static const u8 sText_Help[] = _("{DPAD_LEFTRIGHT} CHANGE  {A_BUTTON} APPLY  {B_BUTTON} CLOSE");
static const u8 sText_HelpGive[] = _("{DPAD_LEFTRIGHT} STEP 1  {L_BUTTON}{R_BUTTON} STEP 50  {A_BUTTON} GIVE  {B_BUTTON} CLOSE");
static const u8 sText_GivenParty[] = _("{STR_VAR_1} joined your party!");
static const u8 sText_GivenPC[] = _("{STR_VAR_1} was sent to the PC.");
static const u8 sText_GiveFailed[] = _("No room for {STR_VAR_1}!");

static const u8 sTextColor_Normal[] = {TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};
static const u8 sTextColor_Selected[] = {TEXT_COLOR_WHITE, TEXT_COLOR_RED, TEXT_COLOR_LIGHT_RED};

static const struct WindowTemplate sQuickDebugWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 1,
    .tilemapTop = 1,
    .width = 28,
    .height = 2 * (ROW_COUNT + 2), // title + rows + help line
    .paletteNum = 15,
    .baseBlock = 1,
};

static void Task_QuickDebugMenu(u8 taskId);

// Species that can be given: enabled and not a battle-only form
static bool32 IsGivableSpecies(u32 species)
{
    if (species == SPECIES_NONE || species >= NUM_SPECIES || !IsSpeciesEnabled(species))
        return FALSE;
    return !(gSpeciesInfo[species].isMegaEvolution
          || gSpeciesInfo[species].isPrimalReversion
          || gSpeciesInfo[species].isUltraBurst
          || gSpeciesInfo[species].isGigantamax
          || gSpeciesInfo[species].isTeraForm
          || gSpeciesInfo[species].isTotem);
}

// Steps by delta, wrapping around, then keeps going one at a time in the
// same direction until it lands on a givable species
static u32 CycleSpecies(s32 species, s32 delta)
{
    s32 count = NUM_SPECIES - 1; // valid species are 1..NUM_SPECIES - 1
    s32 step = (delta < 0) ? -1 : 1;
    u32 tries;

    species = ((species - 1 + delta) % count + count) % count + 1;
    for (tries = 0; tries < NUM_SPECIES && !IsGivableSpecies(species); tries++)
        species = ((species - 1 + step) % count + count) % count + 1;
    return species;
}

static u32 GetDestinationHealLocation(u32 index)
{
    u32 healLocation = sDestinations[index];

    if (healLocation == HEAL_LOCATION_NONE)
        healLocation = (gSaveBlock2Ptr->playerGender == MALE) ? HEAL_LOCATION_LITTLEROOT_TOWN_BRENDANS_HOUSE
                                                              : HEAL_LOCATION_LITTLEROOT_TOWN_MAYS_HOUSE;
    return healLocation;
}

// Generations without a morning or evening period leave those hours empty
static bool32 IsTimeOfDayAvailable(enum TimeOfDay time)
{
    switch (time)
    {
    case TIME_MORNING:
        return MORNING_HOUR_BEGIN != MORNING_HOUR_END;
    case TIME_EVENING:
        return EVENING_HOUR_BEGIN != EVENING_HOUR_END;
    default:
        return TRUE;
    }
}

static s32 CycleTimeOfDay(s32 time, s32 delta)
{
    do
    {
        time = (time + delta + TIMES_OF_DAY_COUNT) % TIMES_OF_DAY_COUNT;
    } while (!IsTimeOfDayAvailable(time));
    return time;
}

// Picks a clock time well inside each period, past the fades between them
static void GetTargetTime(enum TimeOfDay time, s32 *hours, s32 *minutes)
{
    *minutes = 0;
    switch (time)
    {
    case TIME_MORNING:
        *hours = MORNING_HOUR_BEGIN + (MORNING_HOUR_END - MORNING_HOUR_BEGIN) / 2;
        break;
    case TIME_DAY:
        *hours = DAY_HOUR_BEGIN + (DAY_HOUR_END - DAY_HOUR_BEGIN) / 2;
        break;
    case TIME_EVENING:
        *hours = EVENING_HOUR_END - 1;
        *minutes = 45;
        break;
    case TIME_NIGHT:
    default:
        *hours = NIGHT_HOUR_BEGIN + 1;
        break;
    }
    *hours = (*hours + HOURS_PER_DAY) % HOURS_PER_DAY;
}

static void PrintRow(u32 windowId, u32 row, bool32 selected, const u8 *value)
{
    u32 y = LINE_HEIGHT * (row + 1);
    const u8 *color = selected ? sTextColor_Selected : sTextColor_Normal;
    u8 text[40];

    if (selected)
        AddTextPrinterParameterized3(windowId, FONT_NORMAL, CURSOR_X, y, sTextColor_Normal, TEXT_SKIP_DRAW, gText_SelectorArrow);
    AddTextPrinterParameterized3(windowId, FONT_NORMAL, LABEL_X, y, color, TEXT_SKIP_DRAW, sRowLabels[row]);
    if (value != NULL)
    {
        StringCopy(text, COMPOUND_STRING("{LEFT_ARROW} "));
        StringAppend(text, value);
        StringAppend(text, COMPOUND_STRING(" {RIGHT_ARROW}"));
        AddTextPrinterParameterized3(windowId, FONT_NORMAL, VALUE_X, y, color, TEXT_SKIP_DRAW, text);
    }
}

static void DrawQuickDebugMenu(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    u32 windowId = task->tWindowId;
    const struct HealLocation *loc = GetHealLocation(GetDestinationHealLocation(task->tDestination));
    u8 mapName[32];
    u8 monText[24];
    const u8 *helpText;

    GetMapName(mapName, Overworld_GetMapHeaderByGroupAndId(loc->mapGroup, loc->mapNum)->regionMapSectionId, 0);

    FillWindowPixelBuffer(windowId, PIXEL_FILL(TEXT_COLOR_WHITE));
    AddTextPrinterParameterized3(windowId, FONT_NORMAL, 0, 0, sTextColor_Normal, TEXT_SKIP_DRAW, sText_Title);
    PrintRow(windowId, ROW_TIME, task->tRow == ROW_TIME, sTimeNames[task->tTime]);
    PrintRow(windowId, ROW_TELEPORT, task->tRow == ROW_TELEPORT, mapName);

    // "025 PIKACHU": national dex number, then name
    ConvertIntToDecimalStringN(monText, SpeciesToNationalPokedexNum(task->tSpecies), STR_CONV_MODE_LEADING_ZEROS, 4);
    StringAppend(monText, COMPOUND_STRING(" "));
    StringAppend(monText, GetSpeciesName(task->tSpecies));
    PrintRow(windowId, ROW_GIVE_MON, task->tRow == ROW_GIVE_MON, monText);

    PrintRow(windowId, ROW_CLOSE, task->tRow == ROW_CLOSE, NULL);

    // Bottom line: result of the last give, otherwise the controls
    if (task->tStatus != GIVE_STATUS_NONE)
    {
        StringCopy(gStringVar1, GetSpeciesName(task->tSpecies));
        if (task->tStatus == GIVE_STATUS_PARTY)
            StringExpandPlaceholders(gStringVar4, sText_GivenParty);
        else if (task->tStatus == GIVE_STATUS_PC)
            StringExpandPlaceholders(gStringVar4, sText_GivenPC);
        else
            StringExpandPlaceholders(gStringVar4, sText_GiveFailed);
        helpText = gStringVar4;
    }
    else
    {
        helpText = (task->tRow == ROW_GIVE_MON) ? sText_HelpGive : sText_Help;
    }
    AddTextPrinterParameterized3(windowId, FONT_SMALL, 0, LINE_HEIGHT * (ROW_COUNT + 1), sTextColor_Normal, TEXT_SKIP_DRAW, helpText);
    CopyWindowToVram(windowId, COPYWIN_FULL);
}

static void DestroyQuickDebugMenu(u8 taskId)
{
    ClearStdWindowAndFrame(gTasks[taskId].tWindowId, TRUE);
    RemoveWindow(gTasks[taskId].tWindowId);
    DestroyTask(taskId);
}

static void CloseQuickDebugMenu(u8 taskId)
{
    PlaySE(SE_SELECT);
    DestroyQuickDebugMenu(taskId);
    ScriptUnfreezeObjectEvents();
    UnlockPlayerFieldControls();
}

// Moves the clock forward to the chosen time of day and reloads the map to show it
static void ApplyTime(u8 taskId)
{
    s32 hours, minutes, days;

    GetTargetTime(gTasks[taskId].tTime, &hours, &minutes);
    RtcCalcLocalTime();
    days = gLocalTime.days;
    if (hours * MINUTES_PER_HOUR + minutes <= gLocalTime.hours * MINUTES_PER_HOUR + gLocalTime.minutes)
        days++;
    RtcCalcLocalTimeOffset(days, hours, minutes, 0);
    UpdateTimeOfDay(TRUE);

    PlaySE(SE_SELECT);
    DestroyQuickDebugMenu(taskId);
    SetMainCallback2(CB2_LoadMap);
}

// Gives the selected species at GIVE_MON_LEVEL; the menu stays open
static void ApplyGiveMon(u8 taskId)
{
    struct Task *task = &gTasks[taskId];

    switch (ScriptGiveMon(task->tSpecies, GIVE_MON_LEVEL, ITEM_NONE))
    {
    case MON_GIVEN_TO_PARTY:
        task->tStatus = GIVE_STATUS_PARTY;
        PlaySE(SE_SELECT);
        break;
    case MON_GIVEN_TO_PC:
        task->tStatus = GIVE_STATUS_PC;
        PlaySE(SE_SELECT);
        break;
    default:
        task->tStatus = GIVE_STATUS_FAILED;
        PlaySE(SE_FAILURE);
        break;
    }
    DrawQuickDebugMenu(taskId);
}

static void ApplyTeleport(u8 taskId)
{
    SetWarpDestinationToHealLocation(GetDestinationHealLocation(gTasks[taskId].tDestination));
    DestroyQuickDebugMenu(taskId);
    ResetInitialPlayerAvatarState();
    DoWarp();
}

void ShowQuickDebugMenu(void)
{
    u8 taskId = CreateTask(Task_QuickDebugMenu, 80);
    struct Task *task = &gTasks[taskId];

    FreezeObjectEvents();
    HideMapNamePopUpWindow();
    LoadMessageBoxAndBorderGfx();
    task->tWindowId = AddWindow(&sQuickDebugWindowTemplate);
    task->tRow = ROW_TIME;
    task->tTime = IsTimeOfDayAvailable(gTimeOfDay) ? gTimeOfDay : CycleTimeOfDay(gTimeOfDay, 1);
    task->tDestination = 0;
    task->tSpecies = CycleSpecies(SPECIES_NONE, 1);
    task->tStatus = GIVE_STATUS_NONE;
    DrawStdWindowFrame(task->tWindowId, FALSE);
    DrawQuickDebugMenu(taskId);
}

static void Task_QuickDebugMenu(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    s32 delta = 0;

    if (JOY_NEW(B_BUTTON))
    {
        CloseQuickDebugMenu(taskId);
        return;
    }

    if (JOY_NEW(A_BUTTON))
    {
        switch (task->tRow)
        {
        case ROW_TIME:
            ApplyTime(taskId);
            break;
        case ROW_TELEPORT:
            ApplyTeleport(taskId);
            break;
        case ROW_GIVE_MON:
            ApplyGiveMon(taskId);
            break;
        case ROW_CLOSE:
            CloseQuickDebugMenu(taskId);
            break;
        }
        return;
    }

    if (JOY_REPEAT(DPAD_UP))
    {
        task->tRow = (task->tRow + ROW_COUNT - 1) % ROW_COUNT;
    }
    else if (JOY_REPEAT(DPAD_DOWN))
    {
        task->tRow = (task->tRow + 1) % ROW_COUNT;
    }
    else if (JOY_REPEAT(DPAD_LEFT))
    {
        delta = -1;
    }
    else if (JOY_REPEAT(DPAD_RIGHT))
    {
        delta = 1;
    }
    else if (JOY_REPEAT(L_BUTTON) && task->tRow == ROW_GIVE_MON)
    {
        delta = -SPECIES_FAST_STEP;
    }
    else if (JOY_REPEAT(R_BUTTON) && task->tRow == ROW_GIVE_MON)
    {
        delta = SPECIES_FAST_STEP;
    }
    else
    {
        return;
    }
    task->tStatus = GIVE_STATUS_NONE;

    if (delta != 0)
    {
        if (task->tRow == ROW_TIME)
            task->tTime = CycleTimeOfDay(task->tTime, delta);
        else if (task->tRow == ROW_TELEPORT)
            task->tDestination = (task->tDestination + delta + ARRAY_COUNT(sDestinations)) % ARRAY_COUNT(sDestinations);
        else if (task->tRow == ROW_GIVE_MON)
            task->tSpecies = CycleSpecies(task->tSpecies, delta);
        else
            return;
    }

    PlaySE(SE_SELECT);
    DrawQuickDebugMenu(taskId);
}

#undef tWindowId
#undef tRow
#undef tTime
#undef tDestination
#undef tSpecies
#undef tStatus
