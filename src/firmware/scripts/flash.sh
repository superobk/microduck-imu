#!/usr/bin/env bash
# Erases/programs the IMU application only. No RDP/option-byte/servo operation.
set -euo pipefail
if [[ $# != 2 || "$1" != "--confirmed-j3" ]]; then
  echo "Usage: bash scripts/flash.sh --confirmed-j3 build/hsi-dxl-f0-w0-spi4/imu.elf" >&2
  echo "Power=5V current-limited; J3 pin4=TX, pin6=3V3 confirmed by continuity; motors disconnected." >&2
  exit 2
fi
image="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
[[ -f "$image" && "$image" == *.elf ]] || { echo 'Expected existing ELF' >&2; exit 2; }
command -v openocd >/dev/null || { echo 'Install OpenOCD with STM32G0 support' >&2; exit 2; }
# Pictured 10-pin probe: pin2 -> J3.2, pin4 -> J3.3, pin5/6 -> J3.1.
# Leave probe power pins7-10 and target J3.6 disconnected: no confirmed VTref.
# Target remains independently powered at J1 with current-limited5V.
# Do NOT assume the pictured RST pin1 is a STM32 reset output.
# NRST=1 is ONLY for a separately verified true NRST probe connection to U1.6.
reset='reset_config none'
if [[ "${NRST:-0}" == 1 ]]; then reset='reset_config srst_only srst_nogate connect_assert_srst'; fi
# OpenOCD paths vary: INTERFACE=interface/cmsis-dap.cfg is supported.
openocd -f "${INTERFACE:-interface/stlink.cfg}" -f target/stm32g0x.cfg \
  -c "adapter speed 100; $reset" \
  -c "program {$image} verify reset exit"
