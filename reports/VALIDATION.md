# Validation record — 2026-09-29

Executed in an isolated Linux container, not the user's Mac or GPU workstation. No PCB, ST-LINK, U2D2, USB-UART, XL330 or robot controller attached. Owner-reported one-good/several00-orFF observations are not reproduced hardware measurements.

|Check|Actual evidence|
|---|---|
|Unchanged baseline23 firmware tests|baseline-unit-tests.log|
|Updated30 firmware tests, including10,000 packet round-trips|firmware-tests.log|
|31 platform/config tests, including5 native-C/POSIXPTY integration cases|platform-tests.log|
|100,000 ASan+UBSan parser cases|fuzz.log|
|1,000 rotations plus identity/axis/math|math-tests.log|
|Four ARMClang17/LLD builds|build-*.log, build-manifest.json|
|Python syntax/shell syntax|py_compile and bash -n executed successfully|
|Browser throughlocalhost|FAILED environment reachability:ERR_BLOCKED_BY_ADMINISTRATOR; browser-attempt.log and browser-check.json|
|Offline DOM/in-processDEMO interaction|browser-offline-check.json and browser-offline.log; Canvas3D, pose updates, board/sensor transform, stale gray state, exports and mobile layout|

Firmware build paths: HSI-DXL-SPI4, HSI-DXL-SPI1, HSE-DXL-SPI4, HSI-UART1-SPI4. AllFAULTS0/WATCHDOG0. First diagnostic BIN7416bytes, BSS3452bytes, data0; HSE-DXL BIN7544bytes. All link with at least2KiB reserved stack inside8KiBRAM. Dynamic stack high-water is not measured. ConfiguredHz is not a waveform measurement.

Software reproduced the logical pattern: a successful SPI transfer with00/FF yields init failure and zero sampling counters without a SPI transport error. It did NOT reproduce or prove the physical defect of a user's board. Version2 separates these outcomes in DBG2; it is not an asserted soldering/power fix.

Compatibility regression uses the pinned0.15.0 ordinary-read request layout, core encoding and config gate. The actual Rust robotd binary and its library were NOT compiled/executed; no robotd hardware-in-loop claim. Real fault-to-motor-stop behavior remains unverified. GNU Arm GCC, macOS driver installation, Safari/ChromeUSB acquisition and physical flashing were not run here.

Browser tests did not bypass the localhost restriction. HTTP server endpoints were tested by Python; DOM was loaded offline with in-processDEMO data. NativeWebGL2 was unavailable and Canvas software3D was used. Screenshots/logs are DEMO, not field evidence. The companion bundle contains the full Web source/tests and illustrative captures. Upload of the Web-service source was blocked by the connected tool. The source repository retains MCU tests and this evidence summary, not a self-contained Web application.

Reproduce from the **complete companion package** root (the firmware tests/build also run from a repository clone):

```bash
(cd src/firmware && make test)
python3 -m unittest discover -s tests -p 'test_*.py' -v
node tests/test_math.mjs
(cd src/firmware && make TOOLCHAIN=clang CLOCK=hsi TRANSPORT=dxl SPI_MHZ=4)
(cd src/firmware && make TOOLCHAIN=clang CLOCK=hsi TRANSPORT=dxl SPI_MHZ=1)
(cd src/firmware && make TOOLCHAIN=clang CLOCK=hse TRANSPORT=dxl SPI_MHZ=4)
(cd src/firmware && make TOOLCHAIN=clang CLOCK=hsi TRANSPORT=uart1 SPI_MHZ=4)
```

Host-side C tests require a native compiler. Application operation requires Python3.11+ and pyserial3.5, not a compiler. Version2 images differ from the prior version1 binary. Keep prior known-good board/image as the A/B control; do not overwrite the raw evidence.
