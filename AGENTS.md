# AGENTS.md

Personal fork of **uehlbran/pokeemerald-expansion-native**, worked on from an
Apple-silicon Mac. That project is a native PC port of rh-hideout's
**pokeemerald-expansion**, pinned at **expansion/1.17.1**
(`include/constants/expansion.h`). Here it is built as a **GBA ROM** and
played in **mGBA**.

## 0. Defaults

- **Always build the GBA ROM** (`make`) unless the user explicitly asks for
  something else (the native PC build, a test run, a release ROM, FRLG).
  Don't start the native build (`Makefile.native`, `tools/build-native.sh`)
  on your own.
- **Play and test in mGBA.** After a build the user wants to try, open it:
  `open -a mGBA pokeemerald.gba`.
- Before editing, check the branch (`git branch --show-current`; work goes on
  `personal`) and the expansion version.

## 1. This machine

| What | Value |
|---|---|
| Mac | Apple silicon (arm64). No Rosetta 2, no x86_64 Homebrew |
| ARM toolchain | devkitARM release 66 (GCC 15.1) at `/opt/devkitpro/devkitARM`; `DEVKITPRO`/`DEVKITARM` exported in `~/.zshrc` |
| Host tools | Xcode Command Line Tools, Homebrew (`/opt/homebrew`) `libpng`, `pkg-config`, `coreutils` 9.12 (`g`-prefixed names; not on PATH unprefixed) |
| Emulator | mGBA 0.10.5, Homebrew cask `mgba-app`, at `/Applications/mGBA.app` |
| Debugger | `arm-none-eabi-gdb` (ships with devkitARM) |

## 2. Git and GitHub

| Remote | Repo | Use |
|---|---|---|
| `origin` | `uehlbran/pokeemerald-expansion-native` | upstream of this fork; pull only |
| `mine` | `Lebrijay02/pokeemerald-expansion-native` | the user's fork; `personal` is its default branch |

- Local `personal` tracks `mine/personal`. `main` stays equal to `origin/main`.
- **Pushing:** git gets its GitHub login from `gh`'s *active* account, and the
  default is `JuanLebrijaSofttek`, which cannot push to the fork. Push like this:
  ```sh
  gh auth switch -u Lebrijay02 && git push; gh auth switch -u JuanLebrijaSofttek
  ```
  Don't add credential helpers to the git config; the user declined that.
- **Pull fork updates:** `git fetch origin && git merge origin/main` (on `personal`).
- **Moving to a new expansion tag:** follow `docs/UPGRADING.md` only. Never
  `git pull` an rh-hideout tag or branch straight into this tree; see §5.
- Commit or push only when the user asks. Never commit `.gba`, `.elf`, `.sav`,
  ROM or BIOS files (`*.gba`/`*.elf` are gitignored). `.claude/` and
  `.tokensave/` are local-only (listed in `.git/info/exclude`).
- Don't open PRs or issues on `uehlbran` or `rh-hideout` unless the user asks.
  If something is offered back to `uehlbran`, it targets `main` and follows
  `CONTRIBUTING.md` and `.github/pull_request_template.md`.

## 3. Building and running

```sh
make -j"$(sysctl -n hw.ncpu)"            # pokeemerald.gba (+ .elf, .map); the default
make -j"$(sysctl -n hw.ncpu)" release    # pokeemerald-release.gba: no debug menus, LTO
make debug                               # pokeemerald.elf with debug info for GDB
make clean                               # needed when switching Emerald <-> FRLG
make tidy                                # remove build output, keep built tools
make firered                             # or leafgreen; see docs/tutorials/how_to_frlg.md
open -a mGBA pokeemerald.gba             # run it
```

`nproc` does not exist on macOS; use `sysctl -n hw.ncpu`. `make compare` is an
alias for `all`, because this repo doesn't build a byte-matching retail ROM.
agbcc is not used.

**What a good build looks like** (first build, 2026-11-08):

- It ends with `gbafix pokeemerald.gba -p` and a memory report: EWRAM about
  86.4%, IWRAM about 86.6%, ROM about 79.7%. Watch these after every change.
- Expected noise: about 209 `bKGD: invalid index` warnings (PNG metadata),
  two `-Wdeprecated-literal-operator` warnings in a host tool, and
  `pokeemerald.elf has a LOAD segment with RWX permissions`.
- **The SHA1 will not equal CI's `e2d4600d4e7b1269ac50c28bc96ece2b758ded9e`.**
  That reference comes from Ubuntu's `gcc-arm-none-eabi` (GCC 13), and
  devkitARM's GCC 15 gives different bytes. This is expected, not a regression.
  A byte-match check needs Ubuntu 24.04 with its toolchain. `rom.sha1` is the
  vanilla Emerald hash and does not apply.

**Clean environment.** If `CPATH`, `C_INCLUDE_PATH`, `LIBRARY_PATH`, `LDFLAGS`,
`CFLAGS` or `CPPFLAGS` are set, they can leak macOS headers into the ARM build
(link errors on `__getreent`, `_kill_r` and the like in `make check`/`make debug`).
None are set today. If one appears, build with
`env -u CPATH -u C_INCLUDE_PATH -u LIBRARY_PATH -u LDFLAGS -u CFLAGS -u CPPFLAGS make ...`.

**Tests.** `make check` builds a test ROM and runs it headlessly with
`tools/mgba-rom-test-hydra`. On macOS it needs Homebrew `coreutils`
(`docs/install/mac/MAC_OS.md`, step 3), which is installed. The hydra calls
plain `stdbuf`, which resolves to macOS's `/usr/bin/stdbuf`. If a run hangs
without output, retry with GNU's:
`PATH="/opt/homebrew/opt/coreutils/libexec/gnubin:$PATH" make check ...`.
Verified 2026-11-08: `make check TESTS="Spikes"` passed 4/4 with macOS's
`stdbuf`. The first run took about 3.5 minutes, nearly all of it compiling the
test ROM (`pokeemerald-test.elf`); later runs reuse that build.
**The full suite has not been run on this Mac yet**, so there's no local
baseline. Until there is one, compare a full run's totals with the 1.17.1
reference below, and record the first full run's totals and time here.

```sh
make -j"$(sysctl -n hw.ncpu)" check
make check TESTS="Spikes"                          # by name (double quotes)
make check TESTS="test/battle/ability/stamina.c"   # one file
```

The reference totals for 1.17.1 (`docs/BUILDING.md`, gate (c)): 5862 total,
5414 passed, 18 known failing, 9 expected failing, 421 to-do, rc 0. Building the
ROM alone does not compile `test/`. If a run stops making progress, interrupt
it and narrow it down with `TESTS=`.

**mGBA.** Default keys: arrows for the D-pad, X for A, Z for B, A for L, S for R,
Enter for Start, Backspace for Select (change them under Settings → Controllers).
For debugging, run `make debug`, start mGBA's Tools → Start GDB server, then run
`arm-none-eabi-gdb pokeemerald.elf` and enter `target remote localhost:2345`.

**Common errors**

| Symptom | Fix |
|---|---|
| `arm-none-eabi-gcc: command not found`, `[libagbsyscall] Error 2` | `DEVKITARM` unset in this shell: `source ~/.zshrc` |
| `png.h` not found / gbagfx fails to build | `brew install libpng pkg-config`; check `pkg-config --cflags libpng` |
| Link errors only in `make check`/`make debug` | clear the env vars above |
| Odd results after switching branch, game or toolchain | `make clean` |
| Files that differ only by case | APFS is case-insensitive; never create them |

## 4. Repository layout

- `src/`: game C code. `src/data/` holds species (`src/data/pokemon/`), moves,
  items, `trainers.party`, `battle_partners.party` and `wild_encounters.json`.
- `include/`: headers. `include/constants/` has IDs and enums, and
  `include/config/` has the feature toggles (`ai.h`, `battle.h`, `general.h`
  with `GEN_LATEST`, `pokemon.h` with `P_FAMILY_*`, `debug.h`, `overworld.h`,
  `species_enabled.h`, …; run `ls include/config` for the full set).
- `data/`: scripts and map data (`data/maps/*/map.json` and `scripts.inc`,
  `data/layouts/layouts.json`, `data/maps/map_groups.json`).
- `graphics/`, `sound/`: assets, converted at build time.
- `tools/`: host tools, built automatically (`gbagfx`, `gbafix`, `mapjson`,
  `jsonproc`, `preproc`, `scaninc`, `trainerproc`, `mgba-rom-test-hydra`,
  `find_func/find_func.py`).
- `test/`: expansion's test suite. `migration_scripts/`, `dev_scripts/`: helper scripts.
- `docs/`: mdBook sources (`docs/SUMMARY.md`; add every new page there),
  `docs/changelogs/`, `docs/tutorials/`, `docs/install/`, `docs/team_procedures/`.
- **Native-port files (§5):** `platform/`, `Makefile.native`, `tools/build-native.sh`,
  `tools/patchelf/`, `tests/`, and docs `BUILDING.md`, `RUNNING.md`,
  `ARCHITECTURE.md`, `DECISIONS.md`, `UPGRADING.md`, `UPSTREAM_MODIFICATIONS.md`.

## 5. The native port: leave it alone unless asked

This fork exists to run expansion as a native Linux/Windows executable. Its
rules protect that work even when only the ROM is being built:

- Expansion's own code is changed only under `PLATFORM_NATIVE` guards, and every
  such change is logged in `docs/UPSTREAM_MODIFICATIONS.md`. Never delete or
  "simplify" a `PLATFORM_NATIVE` block, and keep the GBA side of each guard
  behaving exactly as upstream.
- The GBA ROM is the reference that the native build is checked against, so a
  change to game code affects both. If a change touches a guarded file, say so.
- Don't edit `platform/`, `Makefile.native` or the native docs unless the user
  asks. Their gates are in `docs/BUILDING.md` under "Gates".
- The macOS native build lives on the unmerged branch `origin/work/macos`. It is
  x86_64-only (Rosetta), has never run on Apple silicon, and needs an x86_64
  Homebrew this Mac doesn't have.

## 6. Tools and how-tos

- **Debug menus:** hold R + Start in the overworld; Select in battle; Select on
  the summary screen opens the sprite visualizer. `make release` removes them.
- **trainerproc** compiles `src/data/*.party` (Showdown-style blocks with `Pic`,
  `Name`, `Level`, `IVs`, moves, items, `Ball:`) into C.
- **find_func:** `python3 tools/find_func/find_func.py pokeemerald.elf 0x08YYYYYY`
  maps a crash or assert address to a function.
- **Add a trainer:** edit `src/data/trainers.party`, then `make`.
- **Add a move/ability/item:** constant in `include/constants/`, data entry
  (`src/data/moves_info.h`, `abilities.h`, `items.h`), the effect or script, and a
  test in `test/battle/`.
- **Add a species:** follow the mdBook tutorial for this version (species info,
  constants, learnset, graphics, `P_FAMILY_*` toggle).
- **External editors** (not installed): Porymap (maps), Porytiles (tilesets),
  poryscript (scripting).

## 7. Limits

- Memory: ROM 32 MB, EWRAM 256 KB, IWRAM 32 KB, and both RAM regions are already
  about 86% full. Any new static or global state needs a reason. To free ROM,
  disable `P_FAMILY_*` families or debug features.
- SaveBlock1/2 sizes are fixed by flash sectors. Adding to them can corrupt saves
  and breaks saves from earlier builds.
- Saves and link play are not compatible with retail Emerald.
- GBA graphics: 4bpp, 16-colour palettes with index 0 transparent, limited
  VRAM/OAM and palette slots.
- Emulators hide lag. Keep heavy work out of per-frame, VBlank and IRQ code.
- Old pret or PokéCommunity tutorials often target removed structures. Check
  them against the current source.
- Don't distribute ROMs or ripped assets; share hacks as patches.

## 8. Agent rules

**Do**
1. Build the ROM after every change and read the memory report. Don't add new warnings.
2. For battle, AI or data changes, add or update a test in `test/` and run it
   with `make check TESTS=...`.
3. Write tests that hold across generation configs (assert the config, or use
   moves whose data doesn't change between generations).
4. Follow `docs/STYLEGUIDE.md`: 4-space indents in `.c`/`.h`, tabs in `.s`;
   `PascalCase` functions and structs, `camelCase` locals and fields, `g` and `s`
   prefixes for globals and statics, `UPPER_SNAKE` macros; braces on their own
   line; prefer enums over `#define` lists.
5. Put changes to vanilla or generation behaviour behind a config in
   `include/config/*.h`.

**Don't**
- Hand-edit generated files: `src/data/trainers.h`, `battle_partners.h`,
  `mapjson`/`jsonproc` output, converted graphics (`*.4bpp`, `*.gbapal`, `*.lz`,
  `*.smol`), or anything under `build/`.
- Remove or rename script commands, config defines or data fields without a migration note.
- Install toolchains or system packages, or change the git or `gh` config, without asking.
