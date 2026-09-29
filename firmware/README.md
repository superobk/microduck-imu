# Prebuilt diagnostic firmware

The complete companion download `Microduck_IMU_macOS_Diagnostics_20260929.zip`
contains four compiled ELF/BIN/HEX/MAP configurations, listed in
[the build manifest](../reports/build-manifest.json). This repository contains
reproducible source and the hashes, not the binary payloads.

Use HSI-DXL-SPI4 for a suspect-board diagnostic test. Preserve the good board on
its known baseline. HSI-DXL-SPI1 is a controlled signal-margin comparison, not a
claimed hardware repair. DXL stays at 1 Mbps in both. Do not use the UART1-only
image for J1/J2 or the robot controller. BIN address: 0x08000000.

All provided images have FAULTS=0 and WATCHDOG=0 for supervised bench diagnosis.
No version is declared hardware-qualified. See docs/FIRMWARE_DIAGNOSTICS.md.
