#!/usr/bin/env bash
set -euo pipefail
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
flags=(-std=c11 -Wall -Wextra -Werror -Icommon -Itests/peers -Ihitbox-nrf52840/src)
cc "${flags[@]}" tests/protocol_test.c -o "$build/wire"
cc "${flags[@]}" hitbox-nrf52840/src/{engine,config}.c tests/engine_test.c -o "$build/engine"
common=(common/{transmitter,source}.c tests/peers/receiver.c)
cc "${flags[@]}" "${common[@]}" tests/full_test.c -o "$build/pc"
cc "${flags[@]}" "${common[@]}" tests/peers/{console_pad,console_usb,link}.c tests/console_test.c -o "$build/console"
cc "${flags[@]}" "${common[@]}" common/uplink.c hitbox-nrf52840/src/engine.c tests/peers/{link,console_pad}.c tests/single_test.c -o "$build/single"
for test in wire engine pc console single; do "$build/$test"; done
node tests/web_test.mjs
