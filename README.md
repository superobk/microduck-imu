# Microduck IMU

Board bring-up, fault diagnosis and a local read-only 3D workbench for the STM32G031F8P6 / LSM6DSV16X replica IMU.

## Field diary — 2026-09-29

The owner reports that electrical checks and firmware flashing were completed. One board returns the expected console response; other boards show zero fields, or a first WHO field of FF followed by zeros. Raw per-board logs and oscilloscope captures have not yet been supplied. These are user-reported observations, not measurements made by this repository's software tests.

The legacy firmware exits sensor initialization when WHO_AM_I is not 0x70. The remaining zero counters are therefore consistent with initialization not reaching sampling. SPIERR=0 does not prove a working sensor: an MCU SPI peripheral can clock in 00 or FF without reporting an electrical connection error.

A separate integration issue was found in the official daemon-v0.15.0 source: fast_sync_read defaults to true (instruction 0x8A). The existing firmware implements ordinary Sync Read (0x82), so use the upstream-supported [bus] fast_sync_read=false setting. This setting does not fix WHO_AM_I failures.

Source, macOS guides, diagnostic firmware and regression evidence are being assembled in this repository. No whole-robot or hardware certification is claimed.
