# Diagnostic firmware v2 (engineering build)

This update makes WHO00/FF diagnosable. It is **not an experimentally proven repair** of the user's boards. Keep the known-good board unchanged as a control; record/backup a failing board before reflashing. Use ST-LINK SWD and verify writes. No new PCB is required just to read these diagnostics.

## Images

The following prebuilt directories are in the companion download, not the source-only repository. The repository contains build sources and the manifest.

|Directory|Use|
|---|---|
|firmware/diag-v2-hsi-dxl-spi4|First diagnostic image; internal HSI clock, external DXL1Mbps, SPI4MHz|
|firmware/diag-v2-hsi-dxl-spi1|Controlled margin comparison on the same suspect board; SPI1MHz, DXL still1Mbps|
|firmware/diag-v2-hse-dxl-spi4|Only after verifying external16MHz active oscillator; check fallback flag|
|firmware/diag-v2-hsi-uart1-spi4|J3 full-duplex1Mbps diagnostic path; disables DXL driver, not a U3 qualification|

All supplied builds: FAULTS=0, WATCHDOG=0, private model0x7D00, firmware version2. Files: ELF preferred or HEX (both contain addresses); BIN uses0x08000000. Select one format. Compiler and hashes are in reports/build-manifest.json. All are bench images, not hardware-approved production firmware. Watchdog remains disabled for debugging and must be separately qualified before operational use.

## Flash wiring (specific user ST-LINK label)

Probe2 SWCLK→J3.2, probe4 SWDIO↔J3.3, probe5/6 GND→J3.1. Board supplied throughJ1 at current-limited5V. Probe3SWIM, probe1unknownRST and probe7/8/9/10power outputs are not connected. J3.4/.5 are UART, .6 is the target3V3 rail; do not connect it to an unverified probe power output. Confirm real board net continuity first.

CubeProgrammer on macOS: ST-LINK / SWD / low SWD clock / Normal / Software reset. Program ELF/HEX and Verify, disconnect, remove probeUSB, switch target off, disconnectSWD then independently power the target again. Do not blindly update the third-party probe or alter RDP/BOOT options. No claim of actual Mac/USB flashing in this repository's software validation.

## Changes

Four WHO samples are collected before sensor reset and four after it, spaced5ms; every sample in its group must be0x70. Mismatch and MCU transport errors are separate. No periodic retry-until-lucky or fake quaternion. Init stage/error is latched. Existing five hex console fields retained; new second `DBG2` line includes stage/error/reads/valid mask/mismatch count/SPI Hz/history. `FLAGS` bit6 indicates an initialization error (badinit often0x40, not legacy0x00).

Stage:0notstarted,1bootwait,2WHO-before,3reset,4WHO-after,5baseconfig,6SFLPconfig,7readback,8configured. Error:0none,1WHO mismatch,2SPI transport,3configurationreadback. Stage8 does NOT prove SFLP is producing fresh valid data.

Existing96-byte IMU1 diagnostic layout/ABI1 and core124/12 remain. Request128 bytes at0x100 to include extension:

|Address|Bytes|Meaning|
|---|---|---|
|0x160|4|ASCII DBG2|
|0x164|1|init_stage|
|0x165|1|init_error|
|0x166|1|first sampledWHO|
|0x167|1|latest successfully transferredWHO|
|0x168|2|number of WHO attempts|
|0x16A|2|bitmask: corresponding SPI transfer returned success, **not proof WHO was0x70**|
|0x16C|4|non0x70 count from successful transfers|
|0x170|8|WHO samples; only indices below who_reads are meaningful|
|0x178|4|configuredSPIfrequencyHz|
|0x17C|1|last transport-error register|
|0x17D|1|last transport-error operation:1read/2write|
|0x17E|1|reserved|
|0x17F|1|extensionversion2|

All multibyte integers little-endian. Startup failure leaveshealthfalse, no samplesequences. Default configuredSPIHz is not an oscilloscope measurement. Host parser accepts old96bytes and old firmware's zero extension; reports startup_debug=false instead of inventing stages. A history with final70 but earlier00/FF remains failed by design.

## Rebuild

```bash
cd src/firmware
make test
make TOOLCHAIN=clang CLOCK=hsi TRANSPORT=dxl SPI_MHZ=4
make TOOLCHAIN=clang CLOCK=hsi TRANSPORT=dxl SPI_MHZ=1
make TOOLCHAIN=clang CLOCK=hse TRANSPORT=dxl SPI_MHZ=4
make TOOLCHAIN=clang CLOCK=hsi TRANSPORT=uart1 SPI_MHZ=4
```

GNU Arm GCC path is supported by Makefile but only the recordedClang/LLD was executed for this delivery. Switching compiler:makeclean. The expanded directory name ends in `-spi4`/`-spi1`. No sensor driver download at build time, no heap, no floating-point library in MCU. UpstreamST register sources/provenance remain documented.
