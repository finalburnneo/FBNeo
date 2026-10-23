# CPS2+ platform tests

Build the existing CPS2 core with the optional headless test executable in a
dedicated CPS2-only build:

```sh
meson setup build-cps2plus -Dfrontend=sdl2 -Ddriver_list=capcom/d_cps2.cpp -Dcps2plus_tests=true
meson compile -C build-cps2plus cps2plus_test
meson test -C build-cps2plus --no-rebuild --print-errorlogs
```

For Windows, use `-Dfrontend=win32` and a separate CPS2-only build with the
same test option and target.

The synthetic test supplies generated ROM bytes through `BurnExtLoadRom` and
substitutes ROM metadata inside the test process. It uses the production
loader, 68000/Z80 cores, memory handlers, graphics decode/drawing, QSound and
state scanner. No test game is added to the emulator's driver list.

Coverage includes upper program data/fetch/execution and write protection,
object mirrors and byte lanes, both physical object banks, eight graphics
banks, sprite compaction/latching/row overflow/flips, all three scroll
selectors (including native address wrapping and row scroll), blend-table
bounds, PCM/ADPCM sample address bits, all sample decoder profiles, both sound
ROM sizes, Z80 bank/status boundaries, state restoration, resets, invalid ROM
geometry (including partially populated graphics), native ROM-loading error
policy, single frontend-owned exit after a failed init, and stock/enhanced
board transitions. Palette checks cover all 64
DMA page masks, RGB32/RGB565 state conversion and the existing XMvsSF palette
state layout. Generated file fixtures check truncated, overlong and corrupt ROM
inputs; the released-set gate rejects every failed load even where the common
loader permits degraded execution.

For address-sanitized checks, configure another build with `-Db_sanitize=address`.
The test support replaces frontend services only; it does not replace the CPU,
renderer or sound engine. EEPROM persistence is disabled by an absent
`cps2plus_test_nvram` directory; do not create that directory in the test's
working directory. The test rejects an existing path to prevent persistent
EEPROM data from affecting the fixtures.

The optional real-game test requires the player's released `vsav2s05` RC3 ROM
members, extracted into a directory. It verifies every declared member's size
and CRC before loading it:

```sh
cd build-cps2plus
./cps2plus_test --rom-dir /path/to/extracted/vsav2s05
```

This runs 5,400 frames with coin/start/selection inputs, writes bounded PPM
captures at boot, select, options and combat, then checks exact machine/audio/
video replay after a save/load. A replay with drawing disabled must retain
the same machine and audio state. It runs without a window or audio device.
The ROMs and captures are not part of the source distribution. This is focused
platform validation; it does not certify all Arrange gameplay or netplay.
