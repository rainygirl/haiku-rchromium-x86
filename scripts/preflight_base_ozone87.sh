#!/bin/sh
set -u

checkout=${1:-/boot/home/rchromium-chromium87-fast}
overlay=${2:-/boot/home/rchromium-native}
out="$checkout/chromium/out/rchromium_native"
phase_file="$overlay/base-ozone-preflight.phase"
status_file="$overlay/base-ozone-preflight.status"

PKG_CONFIG_PATH=/boot/system/develop/lib/x86/pkgconfig
export PKG_CONFIG_PATH

: > "$status_file"
printf 'base-jumbo\n' > "$phase_file"
ninja -C "$out" -j1 \
  obj/base/base/base_jumbo_2.o \
  obj/base/base/base_jumbo_3.o \
  obj/base/base/base_jumbo_4.o \
  obj/base/base/base_jumbo_5.o \
  obj/base/base/base_jumbo_6.o \
  obj/base/base/base_jumbo_7.o \
  obj/base/base/base_jumbo_8.o
code=$?
if [ "$code" -ne 0 ]; then
  printf 'failed base-jumbo %s\n' "$code" > "$status_file"
  exit "$code"
fi

printf 'haiku-ozone\n' > "$phase_file"
ninja -C "$out" -j1 \
  obj/haiku_port/ozone/ozone/client_native_pixmap_factory_haiku.o \
  obj/haiku_port/ozone/ozone/haiku_application.o \
  obj/haiku_port/ozone/ozone/haiku_surface_factory.o \
  obj/haiku_port/ozone/ozone/haiku_window.o \
  obj/haiku_port/ozone/ozone/haiku_window_manager.o \
  obj/haiku_port/ozone/ozone/ozone_platform_haiku.o \
  obj/haiku_port/ozone/ozone/headless_screen.o
code=$?
if [ "$code" -ne 0 ]; then
  printf 'failed haiku-ozone %s\n' "$code" > "$status_file"
  exit "$code"
fi

printf 'complete\n' > "$phase_file"
printf 'complete 0\n' > "$status_file"
