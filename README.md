# Microduck IMU — field diary and diagnostic firmware

A bench project for the **STM32G031F8P6 + LSM6DSV16XTR** replica IMU, exposed as **Dynamixel ID 200**. Hardware: [fanhao375's replica PCB](https://github.com/fanhao375/microduck-replica/tree/3599731c250a06648e18ca9fb1453b8a88410769/hardware/imu_to_dxl), with J4/J5 unused.

**Engineering status:** one board responding normally and other boards reporting WHO=00/FF are owner-reported observations. No raw field logs, measured voltages or scope captures have been supplied. This is not a hardware-qualified replacement or a complete DYNAMIXEL product.

## Delivery scope

This repository contains MCU source and tests, the standalone DXL probe, a read-only controller-config checker, wiring guides and the diary below. **The connected tool blocked publication of the Web-service source, so a fresh clone does not contain a runnable 3D server.**

The full local macOS workbench, its source/tests, labeled capture utilities and prebuilt ELF/BIN/HEX/MAP files are in the companion download **`Microduck_IMU_macOS_Diagnostics_20260929.zip`**, supplied in the delivery conversation. Web and `capture_board.py` commands in the guides apply to that complete package. Firmware/CLI commands work from either distribution.

## Guides

|Task|Start here|
|---|---|
|Diagnose WHO=00/FF and compare boards|[Fault isolation (中文)](docs/FAULTS_00_FF.zh-CN.md)|
|Install U2D2 and run tests on a Mac|[macOS guide (中文)](docs/MACOS_U2D2.zh-CN.md)|
|Flash diagnostic firmware v2|[New diagnostics and images](docs/FIRMWARE_DIAGNOSTICS.md)|
|Connect to the robot's main controller|[Compatibility gate](docs/MICRODUCK_COMPATIBILITY.md)|
|Review software evidence|[Validation](reports/VALIDATION.md), [build manifest](reports/build-manifest.json)|

## Critical compatibility setting

Pinned upstream: **daemon-v0.15.0**, commit **`a9ec4b2079ef8ee7904014089c885bb07d57d63c`**.

That release defaults to **Fast Sync Read, opcode 0x8A**. This firmware implements **ordinary Sync Read, opcode 0x82**, not Fast Sync Read. In the controller's **existing `[bus]` section**, explicitly set:

```toml
fast_sync_read = false
```

Retain the real port and all other settings. Do not duplicate `[bus]` or replace the entire robot configuration with the merge fragment. Check the actual configuration used by the service and restart only after making the robot mechanically safe. The repository's checker is read-only:

```bash
python3 scripts/check_robot_config.py /etc/robot/robotd.toml
```

With that setting and a healthy, correctly mounted board, the interface matches **ID 200 / 1 Mbps / Protocol 2.0 / address 124 / length 12**. This is a source-and-protocol compatibility statement, not an executed robot test. The unchanged Fast-enabled default is unsupported. This configuration cannot repair WHO=00/FF.

## macOS CLI quick start

Use Python 3.11+. External regulated 5V goes to J1.2; supply ground goes to J1.1. U2D2 **TTL 3P** DATA goes to J2.3 and GND to J2.1. U2D2 does not supply target power. Use only one ID-200 board and one serial master at a time.

```bash
git clone https://github.com/superobk/microduck-imu.git
cd microduck-imu
bash scripts/setup_macos.sh
.venv/bin/python -m serial.tools.list_ports -v
PORT=/dev/cu.usbserial-REPLACE_WITH_REAL_DEVICE
.venv/bin/python src/firmware/tools/imu_probe.py --port "$PORT" ping
.venv/bin/python src/firmware/tools/imu_probe.py --port "$PORT" diag
```

The ordinary TTL console is separate: adapter RX to J3.4, GND to J3.1, **115200/8N1**, adapter TX/VCC unconnected. The DXL path is **1000000/8N1**. Do not join an ordinary UART's TX/RX to the single-wire DXL bus.

For the complete companion package, enter its extracted `microduck-imu` directory and run:

```bash
bash scripts/setup_macos.sh
bash scripts/start_macos.sh
open http://127.0.0.1:8765
```

The 3D workbench is local and read-only. Select an actual serial port, enter board label A/B, select DXL and ordinary Sync Read, and connect live. DEMO is explicitly simulated and never replaces a failed real connection. No Node/npm/CDN is needed at runtime. Stop Web acquisition before opening that port from a CLI.

## Field diary — 29 September 2026

### Observation: programming success did not prove sensor bring-up

The owner completed electrical checks and programming. One board returns normally; the others show all-zero fields or WHO=FF followed by zero counters. We do not have exact failing-board counts, original terminal captures or electrical measurements. These observations are recorded without converting them into assistant-operated hardware results.

The legacy initializer clears its state, reads WHO_AM_I and exits unless it receives 0x70. When configuration has failed, the polling loop does not acquire samples. Therefore, zero GSEQ/QSEQ can be a consequence of failing before sampling starts, rather than two independent counter faults.

### Lesson: SPIERR=0 does not establish an electrical connection

An SPI master can complete a transaction and sample 00/FF without reporting a transport error. There is no per-device acknowledgement here. A stuck or floating MISO, incorrect chip select, missing sensor supply or open LGA joint can produce this pattern. It prioritizes the MCU-to-IMU power/SPI/assembly path; it does not identify a damaged component.

The next experiment is **A → B → A** under identical power, harness, adapter and firmware conditions. Keep good board A unchanged. Measure both sensor supply rails and compare CS/SCK/MOSI/MISO at both ends. A WHO read sends 0x8F followed by a dummy byte in one 16-clock CS-low transaction; the expected 0x70 is the second MISO byte. Record evidence before reflowing or replacing components.

### Change: preserve the failed startup instead of retrying until lucky

Diagnostic firmware v2 records four WHO reads before reset and four afterward, a completed-transfer mask, mismatch count, failed stage and operation. Every read in its group must be correct to advance. A successful SPI transfer is not relabeled as a successful identity check.

Core address 124/12 and the existing 96-byte IMU1 diagnostic ABI are preserved. A DBG2 extension at 0x160 adds startup evidence; the original five console fields remain, followed by a debug line. An optional **1 MHz SPI** image supports a controlled margin comparison; external DXL stays at **1 Mbps**. Lower SPI speed is an experiment, not an asserted repair.

### Integration finding: bench success could still fail on robotd

The newly reviewed 0.15.0 controller enables Fast Sync Read by default. We added the explicit upstream `fast_sync_read=false` gate rather than pretending to implement the different Fast response format. This incompatibility and WHO failure are separate problems.

### Results actually obtained

|Check|Actual result and limitation|
|---|---|
|Firmware native/simulated-register tests|30 passed, including 10,000 packet round-trips and WHO/post-reset/readback cases|
|Companion platform/configuration tests|31 passed, including 5 native-C/POSIX pseudo-serial cases|
|ASan/UBSan protocol stress|100,000 cases completed without sanitizer reports|
|3D mathematics|1,000 rotations plus coordinate checks passed|
|ARM compilation|Four Clang/LLD configurations compiled and linked; vectors/static memory checked|
|Browser|Offline DOM/in-process DEMO and Canvas 3D passed; localhost navigation was blocked by the execution environment|
|Real Mac/U2D2/PCB/installed robotd|Not executed here; field acceptance remains required|

Pseudo-serial tests run the C protocol core on the host, not ARM machine code on a physical MCU. HTTP tests and offline browser interaction are separate evidence, not a complete browser-to-USB-to-PCB test. Native WebGL2 was unavailable; the software 3D fallback was exercised.

### Next field gates

Save 60 seconds per board with board label, firmware hash, production revision, actual supply voltages/current and original logs. Verify new gyro/quaternion samples and mounting direction. Next run ordinary 50 Hz reads with one torque-disabled XL330, then scale the bus. Finally verify the running controller configuration and fault-to-stop behavior on a safe rig.

A ten-leg-motor build still needs correct handling of the missing head/neck/mouth joints. Do not fabricate extra devices. Keep stock XL330 supplies within their rating, normally 5V, and do not tie a HAT's raw battery positive into the regulated 5V branch.

## Rebuild and provenance

```bash
cd src/firmware
make test
make TOOLCHAIN=clang CLOCK=hsi TRANSPORT=dxl SPI_MHZ=4
make TOOLCHAIN=clang CLOCK=hsi TRANSPORT=dxl SPI_MHZ=1
```

The companion package contains prebuilt images. ELF/HEX contain addresses; BIN programming starts at **0x08000000**. Supplied images have fault injection and watchdog disabled for supervised debugging, not production qualification. See the [firmware contract](src/firmware/README.md).

MIT licensing is retained for project code. The PCB remains the replica author's design. [Primary sources](docs/SOURCES.md) and [provenance](PROVENANCE.json) separate manufacturer facts, software checks and owner reports.

**No unconditional plug-and-play, complete DYNAMIXEL implementation, confirmed physical root cause or whole-robot certification is claimed.**
