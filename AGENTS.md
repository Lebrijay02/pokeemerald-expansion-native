# AGENTS.md for rh-hideout/pokeemerald-expansion (macOS edition, verified against expansion/1.17.2)

The latest tagged release is **expansion/1.17.2**, published 31 October 2026, and the 1.17.3 cycle has started on `master`. A macOS build needs Xcode Command Line Tools, Homebrew `libpng` and `pkg-config`, and an `arm-none-eabi` toolchain (devkitARM through devkitPro pacman is the documented path).\[1\] After that, `make -j$(sysctl -n hw.ncpu)` produces `pokeemerald.gba`.\[2\]

## TL;DR

- **Version check:** 1.17.2 is the current tag. 1.17.1 shipped 29 Sep 2026 via release PR #10851 ("Expansion 1.17.1 Release" by grintoul1), and 1.17.0 shipped 31 Aug 2026. The 1.17.0 release changed `GEN_LATEST` (it can now be set to `GEN_CHAMPIONS` in `include/config/general.h`) and removed the `gEnemyParty`/`gEnemyPartyCount` globals. I found no new toolchain requirement in the 1.17.x release notes. The macOS install text has moved out of `INSTALL.md` into `docs/install/mac/MAC_OS.md`, which I could not retrieve directly, so the setup commands below are reconstructed from earlier official macOS instructions.
- **Biggest macOS risk in 2026:** the toolchain is not the main problem. Homebrew 7.0.0 (announced 13 September 2026 on brew.sh) moved Intel macOS to Tier 3: "no new bottles", with Homebrew planned to stop running on Intel after 2027-09-01. On top of that, host `CPATH`/`LDFLAGS` exported by a macOS shell can leak into the ARM cross-build and break `make check`/`make debug` linking. Agents should build in a clean environment.
- **Agent policy:** run `make check` before proposing changes. Send bug fixes, docs-only and tests-only PRs to `master` and everything else to `upcoming`. Don't hand-edit generated files. Disclose AI use in the PR template's "Large Language Models (AI)" field. Maintainers appear wary of AI-generated contributions, so a human must review and own every change.\[3\]\[4\]

## Key Findings (what changed and what is uncertain)

- **Latest version:** expansion/1.17.2. GitHub Releases shows "@grintoul1 released this 31 Oct 13:34" (tag expansion/1.17.2, commit 79dd436); the release PR #11004 also opened the "Start of 1.17.3 cycle". The README credit string now reads "Based off RHH's pokeemerald-expansion 1.17.2".\[5\]
- **Install docs moved:** `INSTALL.md` now just links to per-OS guides under `docs/install/`. The Mac section says only that "Some extra considerations exist to get the testing system working" and links to `docs/install/mac/MAC_OS.md`.\[2\] I could not fetch that file. The macOS commands below come from the earlier official macOS instructions (Xcode CLT, `brew install libpng`, `brew install pkg-config`, devkitPro pacman with `gba-dev` and `devkitarm-rules`, and `DEVKITPRO`/`DEVKITARM` exports).\[1\] **Diff them against MAC_OS.md before trusting them.**
- **Toolchain flexibility:** the Makefile finds devkitARM through `DEVKITARM`. Any other toolchain can be supplied with `make TOOLCHAIN="/path"`, where the path must contain `bin` (plus `lib`, `include`, `arm-none-eabi` for the modern target).\[2\]\[6\]
- **agbcc is not needed:** the expansion builds only with a modern GCC (`build/modern`, `ld_script_modern.ld`). The agbcc steps in old forks' INSTALL.md files are pret-era leftovers.\[1\]\[2\]\[7\]
- **GNU make 3.81 on macOS:** I found no repo document that requires a newer GNU make. Treat `brew install make` (`gmake`) as a fallback for unexplained Makefile errors, not a requirement. This is unverified.

---

## AGENTS.md (drop-in content for the repository root)

### 0. Read this first

- This repo is **pokeemerald-expansion**, a GBA ROM-hack *base* built on top of pret's `pokeemerald` decompilation. "It is not a playable Pokémon game on its own."\[5\]
- Current release: **expansion/1.17.2** (31 Oct 2026). Check `include/constants/expansion.h` (`EXPANSION_VERSION_MAJOR/MINOR/PATCH`, `EXPANSION_TAGGED_RELEASE`) or the debug menu's Utilities → Expansion Version to see what the working tree is on.\[8\]\[9\]
- The repo does **not** build a byte-matching retail ROM. Don't use `make compare` or pret-style matching workflows, and don't install agbcc.
- Primary docs to consult before acting: `INSTALL.md`, `docs/install/mac/MAC_OS.md`, `CONTRIBUTING.md`, `docs/STYLEGUIDE.md`, `docs/team_procedures/scope.md`, `docs/team_procedures/merge_checklist.md`, `FEATURES.md`, `docs/CHANGELOG.md`, `docs/tutorials/`, and the mdBook site at https://rh-hideout.github.io/pokeemerald-expansion/.

### 1. Project overview

- **Base:** pret/pokeemerald, with pret merges pulled in regularly (most recently "Pret merge (30th of July 2026)" in 1.17.0).\[5\]\[10\] Expansion keeps pret's documentation and naming, so vanilla symbol names are usually still valid.\[5\]\[11\] Vanilla *tutorials*, however, often target data layouts that no longer exist.
- **Outputs (repo root):**
  - `make` → `pokeemerald.gba` plus `pokeemerald.elf` and a `.map` file.\[12\] This is a development build with the debug menus enabled.\[13\]\[14\]\[15\]
  - `make release` → `pokeemerald-release.gba`.\[15\] It builds into `build/modern-release` (directory name may differ by version), disables `DEBUG_OVERWORLD_MENU`, `DEBUG_BATTLE_MENU` and `DEBUG_POKEMON_SPRITE_VISUALIZER`, and enables LTO by default (configurable in `config.mk`).\[13\]\[16\]
  - `make check` → builds a test ELF and runs the test suite headlessly.\[6\]
- **Supported games:** Emerald is the default and primary target. **FireRed/LeafGreen** can be built with `make firered` / `make leafgreen` (see `docs/tutorials/how_to_frlg.md`). Run `make clean` whenever you switch between Emerald and FRLG.\[17\] FRLG maps need a `region` custom attribute (`REGION_KANTO`/`REGION_HOENN`) and a `layout_version` in `layouts.json`.\[17\] Treat FRLG as newer and less mature than Emerald.
- **Compatibility:** the ROM is not save-compatible with vanilla Emerald. Link features work only with other expansion-based games.\[18\]

### 2. macOS setup and building

**2.1 Prerequisites (run once)**

```sh
# 1. Apple toolchain (host compiler for tools/, git, make 3.81)
xcode-select --install

# 2. Homebrew (Apple Silicon installs to /opt/homebrew)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
eval "$(/opt/homebrew/bin/brew shellenv)"

# 3. Host libraries for gbagfx etc.
brew install libpng pkg-config python git
```

**2.2 ARM toolchain: choose ONE**

*Option A, documented: devkitARM through devkitPro pacman.*
1. Download `devkitpro-pacman-installer.pkg` from https://github.com/devkitPro/pacman/releases and open it. If Gatekeeper blocks it, allow it in System Settings → Privacy & Security.\[19\]
2. Then run:
```sh
sudo dkp-pacman -Sy
sudo dkp-pacman -S gba-dev          # accept default selection
sudo dkp-pacman -S devkitarm-rules
echo 'export DEVKITPRO=/opt/devkitpro' >> ~/.zshrc
echo 'export DEVKITARM=$DEVKITPRO/devkitARM' >> ~/.zshrc
source ~/.zshrc
"$DEVKITARM/bin/arm-none-eabi-gcc" --version
```

*Option B, community path: Arm GNU toolchain from Homebrew.*
```sh
brew install --cask gcc-arm-embedded      # Arm's prebuilt arm-none-eabi toolchain
# then build with an explicit toolchain root, e.g.:
make TOOLCHAIN="/Applications/ArmGNUToolchain/<version>/arm-none-eabi"   # path is version-dependent; verify
```
Option B is not covered by the official docs. One forum user fixed `arm-none-eabi-gcc: command not found` / `libagbsyscall Error 2` with `brew install armmbed/formulae/arm-none-eabi-gcc` (that tap is old).\[20\] Prefer Option A unless you have a reason not to.

**2.3 Apple Silicon vs Intel**

- Apple Silicon (arm64) is the well-supported path. devkitARM and the Homebrew bottles run natively, and I found no source that says Rosetta 2 is needed for this repo.\[21\]
- Intel Macs: Homebrew 7.0.0 (13 Sept 2026; brew.sh release post by Mike McQuaid) moved Intel macOS to Tier 3 with "no new bottles", and the install script now refuses new Intel installs. Intel users should use MacPorts, an existing Homebrew install, or Linux in a VM/container. devkitPro pacman has its own macOS packages and does not depend on Homebrew.

**2.4 Build commands**

```sh
git clone https://github.com/rh-hideout/pokeemerald-expansion   # never use "Download ZIP" (no history → can't update)
cd pokeemerald-expansion
make -j"$(sysctl -n hw.ncpu)"            # nproc does not exist on macOS
make -j"$(sysctl -n hw.ncpu)" release    # shippable ROM without debug menus
make debug                               # pokeemerald.elf with -Og/-g debug info for GDB
make -j"$(sysctl -n hw.ncpu)" check      # full test suite
make check TESTS="Spikes"                # filter by test name (use DOUBLE quotes)
make check TESTS="test/battle/ability/stamina.c"   # one file
make check TESTS="*Illuminate*"          # infix match
make clean                               # remove build artefacts (needed when switching Emerald↔FRLG)
make tidy                                # purge build output but keep built tools (used by CI before `make release`)
make firered -j"$(sysctl -n hw.ncpu)"    # or: make leafgreen
```

A successful link prints a memory report. The INSTALL.md sample shows **EWRAM 256 KB at 92.83%, IWRAM 32 KB at 93.05%, ROM 32 MB at 77.70%**.\[2\] Check these numbers after every change. The warning `pokeemerald.elf has a LOAD segment with RWX permissions` is expected.\[2\]

**2.5 Clean-environment build (recommended for agents)**

Shell-level `CPATH`/`LDFLAGS`/`CFLAGS` pointing at the macOS SDK or Homebrew can be picked up by `arm-none-eabi-cpp`. That makes GBA sources compile against Darwin libc headers and causes `make check`/`make debug` to fail to link with undefined `__getreent`, `_getpid_r`, `_kill_r` and `_write_r`. This was reported on a downstream fork, but the mechanism is generic.\[22\] Build like this:

```sh
env -u CPATH -u C_INCLUDE_PATH -u LIBRARY_PATH -u LDFLAGS -u CFLAGS -u CPPFLAGS \
  make -j"$(sysctl -n hw.ncpu)"
```

**2.6 Common macOS errors**

| Symptom | Fix |
|---|---|
| `arm-none-eabi-gcc: command not found`, `[libagbsyscall] Error 2` | `DEVKITARM` is unset in this shell. Run `source ~/.zshrc`, or pass `TOOLCHAIN=` |
| `png.h` not found / gbagfx fails to build | `brew install libpng pkg-config`; check `pkg-config --cflags libpng` |
| `nproc: command not found` | use `sysctl -n hw.ncpu` |
| Link errors in `make check`/`make debug` only | clear `CPATH`/`LDFLAGS` (§2.5) |
| Odd results after switching branch, game or toolchain | `make clean` (or `make clean-tools` if host tools were built by a different compiler) |
| Python script errors | Python 3 has been required since 1.8.0. Use Homebrew `python3` |
| Makefile syntax errors with `/usr/bin/make` 3.81 | unverified edge case: try `brew install make` and run `gmake` |
| Duplicate-looking files differing only by case | APFS is case-insensitive by default. Never create files that differ only in case |

### 3. Repository layout

- `src/`: C game code (battle engine `battle_*.c`, overworld, menus). `src/data/` holds data tables: species (`src/data/pokemon/`), moves, items, `trainers.party`, `battle_partners.party`, and `wild_encounters.json`.
- `include/`: headers. `include/constants/` holds IDs and enums. **`include/config/`** holds the feature toggles: `ai.h`, `battle.h`, `caps.h`, `debug.h`, `dexnav.h`, `general.h` (incl. `GEN_LATEST`), `pokedex_plus_hgss.h`, `item.h`, `follower_npc.h`, `overworld.h`, `pokemon.h` (incl. `P_FAMILY_*` toggles), `save.h`, `species_enabled.h`, `summary_screen.h`.\[23\]\[24\] Exact filenames vary between versions, so run `ls include/config` first.
- `data/`: assembly/script data. `data/maps/` holds per-map `map.json` + `scripts.inc`, `data/layouts/layouts.json`, `data/maps/map_groups.json`, and `data/scripts/` (e.g. `debug.inc`).
- `graphics/`, `sound/`: assets (PNG/.pal/.bin, songs, samples). Converted at build time.
- `tools/`: host tools built automatically: `gbagfx`, `gbafix`, `mapjson`, `jsonproc`, `preproc`, `scaninc`, `trainerproc`, `mgba-rom-test-hydra` (test runner), and `find_func/find_func.py`.
- `test/`: test suite (`test/battle/ability/…`, `test/battle/move_effect/…`, AI tests, etc.).
- `migration_scripts/`: Python scripts referenced by release notes (📜 marks a migration), e.g. `convert_parties.py`.\[25\]
- `dev_scripts/`: maintainer helper scripts.
- `docs/`: mdBook sources (`docs/SUMMARY.md`; CI checks that every docs `.md` is listed there), changelogs, tutorials, install guides, team procedures.
- `Makefile`, `config.mk` (build options such as release LTO), `make_tools.mk`, `map_data_rules.mk`, `ld_script_modern.ld`.\[13\]

### 4. Tools

**In-repo**
- **Debug menu:** hold **R + Start** in the overworld (`DEBUG_OVERWORLD_MENU`). **Select** opens it in battle (`DEBUG_BATTLE_MENU`), and **Select** on the summary screen opens the sprite visualizer.\[26\] These menus are removed by `make release`.\[13\] 1.17.0 refactored the menu: it supports undo mid-selection, gender choice in Give Mon, and mass-outbreak actions.\[10\]
- **Test runner:** `make check` runs tests headlessly through `mgba-rom-test-hydra`, in parallel with `-j`. One downstream fork reported that a test hanging inside an IRQ handler can stall `make check` indefinitely.\[27\] If the run stops making progress, interrupt it and narrow down with `TESTS=`.
- **trainerproc:** compiles `src/data/*.party` (Showdown/"competitive" syntax) into C. Pragmas: `#pragma trainerproc ivs explicit`, `#pragma trainerproc level explicit`, etc.\[25\]\[28\]
- **find_func:** `python3 tools/find_func/find_func.py pokeemerald.elf 0x08YYYYYY` maps an assert/crash address to a function.\[29\]
- **mGBA logging:** `DebugPrintf` defaults to `MGBA_LOG_WARN` (since 1.13.0).\[30\]

**External (macOS install)**
- **Porymap** (map editor; reads `map.json`/`layouts.json`): download the macOS release from https://github.com/huderlem/porymap/releases. The repo ships Porymap default settings (1.14.0).\[31\] FRLG needs project-setting tweaks, described in `how_to_frlg.md`.\[17\]
- **Porytiles** (metatile/tileset compiler): https://github.com/grunt-lucas/porytiles releases.\[2\]
- **poryscript** (higher-level scripting language → `.inc`): https://github.com/huderlem/poryscript releases, plus the VS Code extension `karathan.poryscript`.\[2\]
- **Tilemap Studio**: https://github.com/Rangi42/tilemap-studio. Mainly a Windows/Linux tool, so expect to build it yourself or run it under a compatibility layer on macOS (unverified).
- **mGBA**: `brew install --cask mgba`. Use Tools → Start GDB server and `arm-none-eabi-gdb pokeemerald.elf` (or `gdb-multiarch`) → `target remote localhost:2345`. Build with `make debug` for usable symbols.

### 5. Capabilities and how-tos

- **Features** (see `FEATURES.md`): species and forms through Gen 9; moves and abilities through Scarlet/Violet, with an opt-in Pokémon Champions generation (`GEN_CHAMPIONS`); per-generation mechanic toggles (`B_*` in `battle.h`, using `GEN_LATEST` / `GEN_X`); Mega Evolution, Primal Reversion, Ultra Burst, Z-Moves, Dynamax/Gigantamax, Terastallization; configurable battle AI flags; follower Pokémon and follower NPCs; DexNav; HGSS-style Pokédex; day/night; mass outbreaks (1.17.0); the debug menus; and a test framework.\[8\]\[24\]
- **Add a trainer:** edit `src/data/trainers.party` (Showdown-style blocks with `Pic`, `Name`, `Level`, `IVs`, moves, held items, and `Ball:` expecting `enum Pokeball` since 1.15.0).\[32\]\[33\] Then `make`. trainerproc regenerates `src/data/trainers.h`, which you should **never** hand-edit.
- **Add a species:** per FEATURES.md this takes about 5 files: species info in `src/data/pokemon/species_info/…`, constants, learnsets, graphics and the `P_FAMILY_*` toggle.\[23\] Follow the mdBook "how to add a new Pokémon" tutorial for your version.
- **Add a move/ability/item:** add the constant (an enum in `include/constants/…`), the data entry (`src/data/moves_info.h`, `abilities.h`, `items.h`), the battle effect or script, and a test in `test/battle/…`.
- **Update the expansion:**
```sh
git remote add RHH https://github.com/rh-hideout/pokeemerald-expansion
git pull RHH expansion/1.17.2        # or: master (bugfixes) / upcoming (unstable features)
```
  Upgrade one version at a time through 1.6.2 → 1.7.4 → 1.8.3 → 1.9.4 → 1.10.3 → … and read each release's 📜 migration notes.\[2\] Run `make clean` after big refactors (e.g. the 1.15.0 `enum TrainerPicID` change required it).\[32\]

### 6. Limitations

- **Hard memory budgets:** ROM 32 MB maximum, EWRAM 256 KB and IWRAM 32 KB. The stock development build already uses about 93% of both RAM regions. New `static`/global state must be justified. Shrink the ROM by disabling species families (`P_FAMILY_*`), bard phonemes, or debug features (use `make release`).\[24\]
- **Save data:** SaveBlock1/2 sizes are fixed by flash sectors. Any struct added to a SaveBlock can push it over the limit and corrupt existing saves. Changing SaveBlock layout breaks saves made by earlier builds of the same hack.
- **Not vanilla-compatible:** saves, link battles and trades with retail Emerald do not work.
- **Graphics:** GBA 4bpp, 16-colour palettes (index 0 transparent), and limited VRAM/OAM and palette slots. Overworld sprites and followers compete for palette slots.
- **Hardware performance:** emulators hide lag. Test on real hardware or an accurate core when you touch per-frame code, and avoid heavy loops in VBlank/IRQ.
- **Vanilla tutorials:** pret or old PokéCommunity tutorials often target removed structures (e.g. `trainer_parties.h`, `gEnemyParty`).\[34\] Check them against the current source.
- **Legal:** don't commit or distribute ROMs, BIOS files or ripped commercial assets. Distribute hacks only as patches.
- **Scope:** features outside `docs/team_procedures/scope.md` will be rejected upstream.\[35\]

### 7. Agent rules

**Do**
1. Before editing, identify the branch (`git branch --show-current`) and version (`include/constants/expansion.h`).
2. After every change, build cleanly: `make -j"$(sysctl -n hw.ncpu)"`. Read the EWRAM/IWRAM/ROM usage lines, and keep the build free of new warnings.
3. Run tests: start with a targeted `make check TESTS="<name or file>"`, then the full `make -j"$(sysctl -n hw.ncpu)" check`. Any battle, AI or data change needs a new or updated test in `test/`.
4. Write tests that stay robust across generation configs. For example, assert the config or use moves whose data doesn't change by generation; PR #10982 ("fix: Tackle breaks tests with certain generational configs", Oct 2026) "replaces Tackle with Pound in a battle test to avoid it breaking with certain gen configs."
5. Follow `docs/STYLEGUIDE.md`: 4-space indentation in `.c`/`.h` and tabs in `.s`; `PascalCase` functions and structs, `camelCase` locals and fields, `g` prefix for globals, `s` for statics, `UPPER_SNAKE` for macros, enums and defines; braces on their own line; prefer enums over `#define` lists (an ongoing refactor theme).\[36\]
6. Put new behaviour behind a config in `include/config/*.h` when it changes vanilla or generation behaviour, and document it.
7. Target `master` for fixes to bugs present on master, documentation-only PRs and test-only PRs. Target `upcoming` for everything else. Branch from the latest target branch (`git reset --hard RHH/upcoming`).\[35\]
8. Fill in the PR template completely, including **"Large Language Models (AI)"** disclosure, issues fixed, and changelog notes.\[37\] Maintainers squash merges.\[35\]
9. Add new docs pages to `docs/SUMMARY.md`.\[38\]

**Don't**
- Don't hand-edit generated files: `src/data/trainers.h`/`battle_partners.h` (from `.party`), map/layout headers and `.inc` produced by `mapjson`, `jsonproc` outputs, converted graphics (`*.4bpp`, `*.gbapal`, `*.lz`, `*.smol`), and anything under `build/`.
- Don't force-push after review has started.\[35\]
- Don't commit `.gba`, `.elf`, `.sav` or ROM/BIOS files.
- Don't reintroduce agbcc or matching-build assumptions.
- Don't remove or rename public script commands, config defines or data fields without a migration note or script.
- Don't open PRs or issues autonomously. Maintainers have warned contributors off LLM use, so a human must understand, test and submit every change.\[3\]

---

## Caveats

- I could not fetch `docs/install/mac/MAC_OS.md`, the Makefile or the `include/config/` directory listing directly. The macOS commands, the `make tidy`/`clean-tools` targets and the config filenames come from earlier official instructions, release notes, CI snippets and `FEATURES.md`. Verify them on your checkout with `ls include/config` and `grep -E '^[a-z-]+:' Makefile`.
- The CPATH/LDFLAGS leak, the hung-test behaviour and the trainerproc range-check gaps were reported on downstream forks, not upstream.\[39\] The mechanisms are general, but I have not confirmed them on upstream master.
- The AI-policy reading is an inference: the PR template asks for disclosure, and in PR #10844 ("Safari tests (WIP)") the 1.17.x release manager grintoul1 answered the AI field with "I'd like to say no, but this was started before I was warned off its use." I found no formal written policy.
- Homebrew's Intel deprecation timeline (full removal planned for 2027-09-01) is Homebrew's announcement, not an expansion requirement.\[40\]

## Sources

1. [Installation - pokeemerald-expansion](https://leo60228.github.io/pokeemerald/INSTALL.html)
2. [pokeemerald-expansion/INSTALL.md at master · rh-hideout/pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion/blob/master/INSTALL.md)
3. [Safari tests (WIP) by grintoul1 · Pull Request #10844 · rh-hideout/pokeemerald-expansion · GitHub](https://github.com/rh-hideout/pokeemerald-expansion/pull/10844)
4. [fix: Tackle breaks tests with certain generational configs by purrfectdoodle · Pull Request #10982 · rh-hideout/pokeemerald-expansion · GitHub](https://github.com/rh-hideout/pokeemerald-expansion/pull/10982)
5. [GitHub - rh-hideout/pokeemerald-expansion: Pokémon Romhack Base, based off pret's pokeemerald decompilation. See the wiki for more info. · GitHub](https://github.com/rh-hideout/pokeemerald-expansion)
6. [Getting Started](https://deepwiki.com/rh-hideout/pokeemerald-expansion/1.1-getting-started)
7. [pokeemerald-expansion/INSTALL.md at master · MelonSpeedruns/pokeemerald-expansion · GitHub](https://github.com/MelonSpeedruns/pokeemerald-expansion/blob/master/INSTALL.md)
8. [cybermanhao/pokeemerald-expansion](https://deepwiki.com/cybermanhao/pokeemerald-expansion)
9. [GitHub - bowbahdoe/pokeemerald-expansion: Feature branche for the pokeemerald decompilation. See the wiki for more info. · GitHub](https://github.com/bowbahdoe/pokeemerald-expansion)
10. [rh-hideout/pokeemerald-expansion expansion/1.17.0 on GitHub](https://newreleases.io/project/github/rh-hideout/pokeemerald-expansion/release/expansion/1.17.0)
11. [GitHub - Pawkkie/pokeemerald-expansion: Expanded hack base for the pokeemerald decompilation. See the wiki for more info. · GitHub](https://github.com/Pawkkie/pokeemerald-expansion)
12. [github.com](https://github.com/rh-hideout-chinese/pokeemerald-expansion/blob/master/INSTALL.md)
13. [Add make release target by jschoeny · Pull Request #7296 · rh-hideout/pokeemerald-expansion · GitHub](https://github.com/rh-hideout/pokeemerald-expansion/pull/7296)
14. [Project Setup and Installation](https://deepwiki.com/iriv24/pokeemerald-expansion/1.1-project-setup-and-installation)
15. [GitHub - Andyism1014/Poke-fusion · GitHub](https://github.com/Andyism1014/Poke-fusion)
16. [Instalador: la partida de verdad con make release by Mike360mine · Pull Request #38 · Jorgemori06/pokeemerald-expansion · GitHub](https://github.com/Jorgemori06/pokeemerald-expansion/pull/38)
17. [pokeemerald-expansion/docs/tutorials/how\_to\_frlg.md at master · rh-hideout/pokeemerald-expansion · GitHub](https://github.com/rh-hideout/pokeemerald-expansion/blob/master/docs/tutorials/how_to_frlg.md)
18. [pokeemerald-expansion/README.md at master · rh-hideout-chinese/pokeemerald-expansion · GitHub](https://github.com/rh-hideout-chinese/pokeemerald-expansion/blob/master/README.md?plain=1)
19. [Setting Up Pokémon Emerald Development on macOS](https://octopus-app-ukjl6.ondigitalocean.app/notes/pokemon-emerald-development/)
20. [\[Pokeemerald\] - Building pokeemerald on Mac - The PokéCommunity Forums](https://www.pokecommunity.com/threads/building-pokeemerald-on-mac.542210/)
21. [How to Install Homebrew on Mac (Apple Silicon) — The Missing Package Manager — tech.raatools/](https://tech.raatools.net/homebrew-mac-silicon-setup)
22. [Host CPATH/LDFLAGS leak into devkitARM builds: the cause of #27, the documented \`make check\` fails, and the ROM bytes depend on the shell · Issue #42 · trey-gonsoulin/pokeemerald-expansion · GitHub](https://github.com/trey-gonsoulin/pokeemerald-expansion/issues/42)
23. [pokeemerald-expansion/FEATURES.md at upcoming · grunt-lucas/pokeemerald-expansion · GitHub](https://github.com/grunt-lucas/pokeemerald-expansion/blob/upcoming/FEATURES.md)
24. [iriv24/pokeemerald-expansion](https://deepwiki.com/iriv24/pokeemerald-expansion)
25. [pokeemerald-expansion/docs/changelogs/1.9.x/1.9.0.md at master · rh-hideout/pokeemerald-expansion · GitHub](https://github.com/rh-hideout/pokeemerald-expansion/blob/master/docs/changelogs/1.9.x/1.9.0.md)
26. [pokeemerald-expansion/include/config/debug.h at master · rh-hideout/pokeemerald-expansion · GitHub](https://github.com/rh-hideout/pokeemerald-expansion/blob/master/include/config/debug.h)
27. [\[upstream\] mgba-rom-test-hydra has no wall-clock timeout, and the in-ROM TIMEOUT can't fire inside an IRQ handler, so a hung test hangs make check — from #73 · Issue #124 · trey-gonsoulin/pokeemerald-expansion · GitHub](https://github.com/trey-gonsoulin/pokeemerald-expansion/issues/124)
28. [Debug Tools](https://deepwiki.com/iriv24/pokeemerald-expansion/7.2-debug-tools)
29. [Releases · rh-hideout/pokeemerald-expansion · GitHub](https://github.com/rh-hideout/pokeemerald-expansion/releases)
30. [Release Expansion 1.13.0 · rh-hideout/pokeemerald-expansion · GitHub](https://github.com/rh-hideout/pokeemerald-expansion/releases/tag/expansion/1.13.0)
31. [rh-hideout/pokeemerald-expansion expansion/1.14.0 on GitHub](https://newreleases.io/project/github/rh-hideout/pokeemerald-expansion/release/expansion/1.14.0)
32. [rh-hideout/pokeemerald-expansion expansion/1.15.0 on GitHub](https://newreleases.io/project/github/rh-hideout/pokeemerald-expansion/release/expansion/1.15.0)
33. [(help) how do i make a trainer in pokeemerald-expansion](https://www.pokecommunity.com/threads/help-how-do-i-make-a-trainer-in-pokeemerald-expansion.535549/)
34. [\[Pokeemerald\] - Choose between 2 pokémon to decide trainer party](https://www.pokecommunity.com/threads/choose-between-2-pok%C3%A9mon-to-decide-trainer-party.486395/)
35. [pokeemerald-expansion/CONTRIBUTING.md at master · rh-hideout/pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion/blob/master/CONTRIBUTING.md)
36. [pokeruby/pokeemerald style guide · GitHub](https://gist.github.com/huderlem/9a29f8e0dd055fb03a915030ace61caf)
37. [Docs status cleanup by despairarc000 · Pull Request #1 · despairarc000/pokeemerald-expansion · GitHub](https://github.com/despairarc000/pokeemerald-expansion/pull/1)
38. [rh-hideout/pokeemerald-expansion expansion/1.14.2 on GitHub](https://newreleases.io/project/github/rh-hideout/pokeemerald-expansion/release/expansion/1.14.2)
39. [\[upstream\] trainerproc accepts out-of-range IVs, levels, EVs and party sizes, and overflows on too many items or tags — from #76 · Issue #255 · trey-gonsoulin/pokeemerald-expansion · GitHub](https://github.com/trey-gonsoulin/pokeemerald-expansion/issues/255)
40. [Homebrew: 7.0.0](https://brew.sh/2026/09/13/homebrew-7.0.0/)
