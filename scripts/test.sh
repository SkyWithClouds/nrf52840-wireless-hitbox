#!/usr/bin/env bash
set -euo pipefail
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
flags=(-std=c11 -Wall -Wextra -Werror -Icommon -Itests/peers -Ihitbox-nrf52840/src)
cc "${flags[@]}" tests/protocol_test.c -o "$build/wire"
cc "${flags[@]}" hitbox-nrf52840/src/{engine,config,effects}.c tests/engine_test.c -o "$build/engine"
cc "${flags[@]}" hitbox-nrf52840/src/{engine,config,effects,display}.c tests/effects_test.c -o "$build/effects"
cc "${flags[@]}" -Itests/fake_flash hitbox-nrf52840/src/{engine,effects,store}.c tests/store_test.c -o "$build/store"
common=(common/{transmitter,source}.c tests/peers/receiver.c)
cc "${flags[@]}" "${common[@]}" tests/full_test.c -o "$build/pc"
cc "${flags[@]}" "${common[@]}" tests/peers/{console_pad,console_usb,link}.c tests/console_test.c -o "$build/console"
cc "${flags[@]}" "${common[@]}" common/uplink.c hitbox-nrf52840/src/{engine,effects}.c tests/peers/{link,console_pad}.c tests/single_test.c -o "$build/single"
for test in wire engine effects store pc console single; do "$build/$test"; done
node tests/web_test.mjs

cc "${flags[@]}" tests/survey_test.c -o "$build/survey"
"$build/survey"
cc "${flags[@]}" tests/controls_test.c hitbox-nrf52840/src/{effects,engine}.c -o "$build/controls"
"$build/controls"
