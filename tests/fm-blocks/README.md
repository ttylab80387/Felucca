# FM BLOCKS tests

Headless checks of FM BLOCKS (`firmware/src/ui_game.c`, `firmware/src/game_snd.c`) in the browser build of the
emulator, plus two tools for the device. Node runs the tests; each plays the game through the emulator's exports
(`web_keys`, `web_buttons`, the `web_test_game_*` hooks in `web/emu/felucca_web.c`).

```sh
sh web/emu/build.sh          # build/emu/felucca.wasm (the game boots: FELUCCA_GAME_BOOT=1)
sh tests/fm-blocks/run.sh    # every test below
```

| test | checks |
|---|---|
| `game_test.mjs` | title, moves and turns, the key LEDs, a clear's effect, hard drop, GAME OVER turning gray, the saver |
| `best_test.mjs` | BEST survives a power-off (the flash kept, a fresh boot) |
| `best_bad_test.mjs` | BEST bytes no BEST wrote read as 0 (the panel's buffer) |
| `lvup_test.mjs` | the level-up fanfare plays after the thump, the line sound kept, also with a quick next drop |
| `lockdelay_test.mjs` | lock delay, the 15 restarts, turning on the floor still locks, soft drop held locks at once, a hold kept from the last piece does not |
| `das_test.mjs`, `clear_input_test.mjs` | left / right pressed during a clear or with a hard drop carries to the next piece; soft drop held over a clear |
| `transport_test.mjs` | MENU > GAME stops a playing song (BEST is written only when stopped) |
| `saver_knob_test.mjs` | any knob wakes the saver |
| `scenes_test.mjs` | the eight background scenes differ, one for every two levels; `node scenes_test.mjs <wasm> <dir>` also writes their pictures |

Device tools (not run by `run.sh`):

- `check_boot.sh build/felucca.dis`: after `./build.sh`, that `fm1_main` calls `game_open` (the emulator has its own boot
  path, so only the disassembly shows the device's)
- `python3 fm1_console.py /dev/cu.usbmodemXXXX status crash`: the FM-1's serial console, read-only commands only
