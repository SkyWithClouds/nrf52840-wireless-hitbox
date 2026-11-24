cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nologo_hitbox)
target_include_directories(app PRIVATE ../common src ${ZEPHYR_BASE}/subsys/usb/device)
target_sources(app PRIVATE ../common/uplink.c ../common/air.c ../common/source.c ../common/transmitter.c
  src/main.c src/engine.c src/effects.c src/display.c src/accessories.c src/config.c src/store.c src/board.c src/usb.c src/radio.c)

target_sources(app PRIVATE ../common/match.c)
