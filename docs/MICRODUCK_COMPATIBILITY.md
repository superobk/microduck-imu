# Microduck controller compatibility: a configuration requirement, not a sensor repair

Reviewed release: **daemon-v0.15.0**, commit `a9ec4b2079ef8ee7904014089c885bb07d57d63c`, released 2026-09-23. Sources: upstream `deploy/robotd.toml`, `duck-control/src/bus.rs`, `imu.rs`, `model.rs`. This is a pinned compatibility statement, not a promise about future HEAD.

## Decision

The board firmware exposes the required **ID200 / 1Mbps / Protocol2 / ordinary Sync Read0x82 / address124 / length12** IMU interface. It is **NOT compatible with the unmodified v0.15.0 default fast-read setting**: that release enables **0x8A Fast Sync Read** by default. Use the upstream-supported `fast_sync_read=false` switch. No controller source-code patch or fake XL330 identity is required for this ordinary-read path. Physical timing, sensor validity, and robot safety still require bench verification.

This issue is independent of WHO00/FF: changing host configuration cannot repair MCU↔IMU SPI communication or soldering. The firmware intentionally does not guess Fast Sync Read response framing. Ordinary and Fast replies are different; never alias opcode0x8A to the0x82 handler.

## Safe configuration procedure on the Linux main controller

Make the robot mechanically safe and disable motor motion using your validated procedure BEFORE stopping/restarting daemons. `--no-policy` alone is not treated here as a guarantee of no motor output. No script in this repository edits root files, restarts robotd or enables torque.

```bash
# These commands belong on the Radxa/Linux controller, not the Mac.
systemctl cat robotd
sudo cp -a /etc/robot/robotd.toml /etc/robot/robotd.toml.imu-backup-$(date +%Y%m%dT%H%M%S)
sudoedit /etc/robot/robotd.toml
```

Inside the **existing** `[bus]` section, retain the real serial port and add/change an uncommented line:

```toml
[bus]
port = "/dev/ttyS2"  # retain the actual existing hardware port; do not blindly replace
fast_sync_read = false
```

Do not add a duplicate `[bus]` section, replace the entire configuration, or leave the line commented out (omitted uses true). `integration/robotd-bus.toml` is a MERGE FRAGMENT, not a complete robot config.

With this repository available on the controller and Python3.11+:

```bash
python3 scripts/check_robot_config.py /etc/robot/robotd.toml
# Operator action only after physical safety gate and confirming actual service configuration:
sudo systemctl restart robotd
robotctl health
sudo journalctl -u robotd -n 100 --no-pager
```

The checker only parses configuration. `configuration_compatible=true` is NOT a hardware pass. Check that `systemctl cat` really references the config you edited. Expect the on-wire request to use0x82, not0x8A. Verify actual state gravity/gyro and errors using the controller's existing telemetry; do not infer safety from a green process indicator.

For an IMU-only read test on the controller, stop other bus masters after making the robot safe; run the included probe against the real serial port as an authorized serial user. Do not run U2D2/Mac and HAT/robotd as two masters on the same bus. U2D2 is for bench commissioning; the installed board can be served by the existing buffered HAT UART.

## Contract retained by version2 diagnostic firmware

|Property|Required / implementation|
|---|---|
|ID, baud|200,1,000,000bps,8N1|
|Data|124–129 gyro xyz `int16` LE, ±500dps =17.5mdps/LSB|
|Quaternion|130–135 SFLP xyz IEEE binary16 LE; host reconstructs nonnegative w|
|Mount|Host applies `trunk=[+sensor_z,+sensor_y,-sensor_x]`; MCU does not rotate twice|
|Ordinary Sync Read|0x82; IMU first in the official request list; other placements tested by predecessor logic|
|Official complete list|200,20,21,22,23,24,30,31,32,33,34,10,11,12,13,14|
|Failure|Invalid/stale sensor core read has Alert0x80; Ping/diagnostics remain available|
|Local identity|Private test model0x7D00, FW2; controller raw IMU read does not require spoofing a motor|
|Extensions|Project diagnostics at0x100 and DBG2 at0x160; official core bytes unchanged|

The stock daemon still expects15 servos. A build with10 leg motors and no head joints needs the separate correct model/runtime adaptation. Do not fabricate responses for missing joints. Stock XL330-M288-T stays within3.7–6.0V, normally5V. Do not connect a HAT's raw battery pin2 to the protected5V servo/IMU branch; preserve common GND/DATA and separate incompatible positive power rails using verified harnesses.

## Evidence and remaining gates

Repository regression tests exercise the same request layout and data decoding, including16IDs, normal ordered response, reserved Fast instruction silence and config false/true/omitted. They do **not** execute the actual Rust robotd binary or real 16-device hardware. No Rust/Cargo runtime integration result is claimed.

Before controller operation: (1) stableWHO70 + freshSFLP; (2) actual50Hz shared-bus test; (3) correct mounting axes; (4) explicitfast=false read from the running config; (5) actual fault/stop behavior tested on a safe rig. No unconditional plug-and-play or whole-robot certification is claimed.
