#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 ttylab80387
# FM BLOCKS' emulator tests, each against the browser build (sh web/emu/build.sh first): sh tests/fm-blocks/run.sh
# [wasm]. Prints each test's last line; exits 1 if any fails.
here=$(cd "$(dirname "$0")" && pwd)
wasm=${1:-$here/../../build/emu/felucca.wasm}
[ -f "$wasm" ] || { echo "no $wasm: run sh web/emu/build.sh first"; exit 1; }
fail=0
for t in game best lvup transport das clear_input saver_knob best_bad lockdelay scenes; do
    out=$(node "$here/${t}_test.mjs" "$wasm" 2>&1 | tail -1)
    echo "$out"
    case "$out" in *FAIL* | *fail* | "") fail=1 ;; esac
done
[ $fail = 0 ] && echo "fm-blocks: all ok" || { echo "fm-blocks: FAILED"; exit 1; }
