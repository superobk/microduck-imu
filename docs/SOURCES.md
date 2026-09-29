# Primary sources and scope (reviewed 2026-09-29)

The field observation is the owner's conversation report, not a measurement generated here. No raw board logs or scope captures were supplied. The project starts from the actual prior conversation archive `Microduck_IMU_STLINK_Web3D_20260926.zip`, not an inferred current microduck-lab directory. Hashes and modifications are in PROVENANCE.json.

1. [Replica PCB and wiring](https://github.com/fanhao375/microduck-replica/tree/3599731c250a06648e18ca9fb1453b8a88410769/hardware/imu_to_dxl): J1/J2/J3, U1–U4, CS/SPI and historical J3.4 conflict. Actual fabricated batch must be checked separately.
2. [Microduck release0.15.0](https://github.com/pollen-robotics/microduck/releases/tag/daemon-v0.15.0), commit a9ec4b2079ef8ee7904014089c885bb07d57d63c.
3. [Upstream robotd.toml](https://github.com/pollen-robotics/microduck/blob/a9ec4b2079ef8ee7904014089c885bb07d57d63c/deploy/robotd.toml): defaultfast=true, explicitoff supported, /etc/robot configuration and restart. Git blob1c41492c63261724525b1341f5d5c6052a342380.
4. [Upstream bus.rs](https://github.com/pollen-robotics/microduck/blob/a9ec4b2079ef8ee7904014089c885bb07d57d63c/duck-control/src/bus.rs):0x8A default, ordinaryfallback,124/12,IMUfirst. Blob083ab4821681cfc027584d6a97e74c51d3b30195.
5. [Upstream imu.rs](https://github.com/pollen-robotics/microduck/blob/a9ec4b2079ef8ee7904014089c885bb07d57d63c/duck-control/src/imu.rs): gyro500dps, binary16xyz, +90Ymount,25acceptedblocks before ready. Blob4ed8d7093af4db0f16593927aa56049e7ed4834b.
6. [ST LSM6DSV16X DS13510](https://www.st.com/resource/en/datasheet/lsm6dsv16x.pdf), Rev4: pin table p11, SPI timing/modes p16, WHO registerp64=0x70. Mode0/3 supported, maxSPI10MHz. TR denotes packaging, not a different identity.
7. [ST sensor register driver reference](https://github.com/STMicroelectronics/lsm6dsv16x-pid/tree/2808e5cd6b85f91b66758e1dd0faab5f043aba07) and [ST G031 CMSIS definitions](https://github.com/STMicroelectronics/cmsis-device-g0/blob/f576c24e123edf3332988ecd49512c0f35f85186/Include/stm32g031xx.h). The MCU code is the project's independent narrow driver; no entireST driver is relicensed here.
8. [ROBOTIS U2D2](https://emanual.robotis.com/docs/en/parts/interface/u2d2/): TTL/RS485, externalpower, supportedbaud, FTDI driver. [Actual TTL connector diagram](https://emanual.robotis.com/assets/images/parts/interface/u2d2_08.png) labels Pin2N/C. It is not a5VUSBpoweroutput.
9. [ROBOTIS Protocol2](https://emanual.robotis.com/docs/en/dxl/protocol2/): CRC,stuffing,orderedordinarySyncRead and distinctFast framing.
10. [XL330-M288](https://emanual.robotis.com/docs/en/dxl/x/xl330-m288/): stockvoltage3.7–6V/recommended5V, connector/model/controltable.
11. [FTDI VCP drivers](https://ftdichip.com/drivers/vcp-drivers/): use OS/architecture-supported signeddriver; currentDriverKit support varies bymacOSversion. No WindowsLatencyTimer instruction is presented as aMacmenu.
12. [Apple USB accessory permission](https://support.apple.com/en-us/102282): unlockandallow onlyexpectedaccessory; do not bypasssystemprotection.
13. [DYNAMIXEL Wizard2](https://emanual.robotis.com/docs/en/software/dynamixel/dynamixel_wizard2/): optionalMacGUI, verifycurrentOSrequirements. Does not turn this customtestmodel into aROBOTISproduct.
14. [ST CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html), [Python venv](https://docs.python.org/3/library/venv.html), [pySerial](https://pyserial.readthedocs.io/en/latest/tools.html).

Electrical screening thresholds, board-ID labels and A/B schedule are recommendations. No independentEDA/DRC, electricalcertification, actualMac/FTDIUSBtest or installedrobotd hardware test was performed in this build environment. The repository records supported config and a tested software contract, not futureupstream compatibility or mechanical safety.
