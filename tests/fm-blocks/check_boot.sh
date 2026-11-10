#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 ttylab80387
# prints "boot: game_open called in fm1_main" when the call is inside fm1_main's body in the disassembly ($1)
awk '/^fm1_main:$/{f=1;next} f&&/^[A-Za-z_][A-Za-z0-9_.$]*:$/{exit} f&&/call.*<game_open /{print "boot: game_open called in fm1_main"; found=1; exit} END{if(!found) print "boot: game_open NOT FOUND in fm1_main"}' "$1"
