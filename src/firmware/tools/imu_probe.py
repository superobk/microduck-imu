#!/usr/bin/env python3
"""Safe IMU bring-up client: no servo torque-enable / goal-position writes.
Requires Python >=3.10 and pyserial==3.5 only for real ports. Offline tests use stdlib.
"""
from __future__ import annotations
import argparse
import json
import math
import struct
import sys
import time
from pathlib import Path

HEADER = b"\xff\xff\xfd\x00"
MODEL = 0x7D00
DIAG = 0x100
SCRATCH = 0x180


def crc16(data: bytes) -> int:
    crc = 0
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x8005 if crc & 0x8000 else 0)) & 0xFFFF
    return crc


def encode(device: int, instruction: int, params: bytes = b"") -> bytes:
    if not (0 <= device <= 252 or device == 254) or not 0 <= instruction <= 255:
        raise ValueError("Invalid ID/instruction")
    body = bytes([instruction]) + params
    stuffed = body.replace(b"\xff\xff\xfd", b"\xff\xff\xfd\xfd")
    if len(stuffed) > 65533:
        raise ValueError("Packet too long")
    packet = HEADER + bytes([device]) + struct.pack("<H", len(stuffed) + 2) + stuffed
    return packet + struct.pack("<H", crc16(packet))


def decode(raw: bytes) -> tuple[int, int, bytes]:
    if len(raw) < 10 or raw[:4] != HEADER:
        raise ValueError("Bad header/short packet")
    if struct.unpack_from("<H", raw, 5)[0] + 7 != len(raw):
        raise ValueError("Wrong packet length")
    if crc16(raw[:-2]) != struct.unpack_from("<H", raw, len(raw) - 2)[0]:
        raise ValueError("CRC mismatch")
    body = raw[7:-2]
    unstuffed = body.replace(b"\xff\xff\xfd\xfd", b"\xff\xff\xfd")
    if unstuffed.replace(b"\xff\xff\xfd", b"\xff\xff\xfd\xfd") != body:
        raise ValueError("Malformed byte stuffing")
    return raw[4], unstuffed[0], unstuffed[1:]


class Link:
    def __init__(self, port: str, baud: int = 1_000_000, timeout: float = .1):
        try:
            import serial
        except ImportError as exc:
            raise RuntimeError("Install pyserial==3.5 in a separate firmware environment") from exc
        self.port = serial.Serial(port, baudrate=baud, timeout=.001, write_timeout=.2)
        self.timeout = timeout
        self.bad_frames = 0
        self.last_trace: list[str] = []

    def close(self) -> None:
        self.port.close()

    def exchange(self, device: int, instruction: int, params: bytes = b"",
                 expected: list[int] | None = None, raw: bytes | None = None) -> list[tuple[int, int, bytes]]:
        expected = [device] if expected is None else expected
        self.port.reset_input_buffer()
        request = encode(device, instruction, params) if raw is None else raw
        self.last_trace = ["TX " + request.hex()]
        self.port.write(request)
        self.port.flush()
        result: list[tuple[int, int, bytes]] = []
        buffer = bytearray()
        end = time.monotonic() + self.timeout
        while time.monotonic() < end:
            buffer.extend(self.port.read(max(1, self.port.in_waiting)))
            while True:
                start = buffer.find(HEADER)
                if start < 0:
                    if len(buffer) > 3:
                        del buffer[:-3]
                    break
                if start:
                    del buffer[:start]
                if len(buffer) < 7:
                    break
                length = int.from_bytes(buffer[5:7], "little") + 7
                if not 10 <= length <= 4096:
                    self.bad_frames += 1
                    del buffer[0]
                    continue
                if len(buffer) < length:
                    break
                frame = bytes(buffer[:length]); del buffer[:length]
                self.last_trace.append("RX " + frame.hex())
                try:
                    ident, inst, data = decode(frame)
                except ValueError:
                    self.bad_frames += 1
                    continue
                if inst != 0x55:  # adapter echo is not a Status Packet
                    continue
                if not data:
                    raise RuntimeError("Status packet lacks error byte")
                result.append((ident, data[0], data[1:]))
                order = [item[0] for item in result]
                if order != expected[:len(order)]:
                    raise RuntimeError(f"Unexpected response order/ID: {order}, expected {expected}")
                if len(result) == len(expected):
                    return result
        if not expected and not result:
            return []
        raise TimeoutError(f"Expected {expected}, received {[v[0] for v in result]}; trace={self.last_trace}")

    def read(self, device: int, address: int, length: int) -> bytes:
        _, error, data = self.exchange(device, 2, struct.pack("<HH", address, length))[0]
        if error:
            raise RuntimeError(f"Device {device} status=0x{error:02X}, raw={data.hex()}")
        if len(data) != length:
            raise RuntimeError(f"Short read: {len(data)} != {length}")
        return data

    def require_imu(self) -> None:
        _, error, data = self.exchange(200, 1)[0]
        if error or len(data) != 3 or int.from_bytes(data[:2], "little") != MODEL:
            raise RuntimeError("Refusing write: ID200 is not this project's IMU firmware")


def diagnostic(data: bytes) -> dict:
    if len(data) not in (96, 128) or data[:4] != b"IMU1" or data[4:6] != b"\x01\x00":
        raise ValueError("Wrong diagnostic magic/ABI/length")
    names = ["uptime_ms", "gyro_seq", "quat_seq", "gyro_age_ms", "quat_age_ms",
             "spi_errors", "fifo_overruns", "uart_errors", "rx_drops", "crc_errors",
             "frame_errors", "sync_timeouts", "tx_errors", "invalid_quats"]
    out = {name: struct.unpack_from("<I", data, 8 + 4*i)[0] for i, name in enumerate(names)}
    out.update(flags=int.from_bytes(data[6:8], "little"), healthy=bool(data[6] & 8),
               who_am_i=data[72], gpio_bits=data[73], clock_flags=data[74], uart1_bridge=data[75],
               clock_hz=struct.unpack_from("<I", data, 76)[0],
               sensor_config=data[80:88].hex(), reset_flags=struct.unpack_from("<I", data, 88)[0],
               watchdog=bool(data[92]), fault_build=bool(data[93]),
               accel_g=[v*.000122 for v in struct.unpack_from("<hhh", data, 64)],
               temperature_c=25 + struct.unpack_from("<h", data, 70)[0]/256)
    out['startup_debug'] = False
    if len(data) == 128 and data[96:100] == b'DBG2':
        if data[127] != 2:
            raise ValueError('Unsupported DBG2 extension revision')
        stages = {0:'not_started',1:'boot_wait',2:'who_before_reset',3:'sensor_reset',
                  4:'who_after_reset',5:'base_configuration',6:'sflp_configuration',
                  7:'configuration_readback',8:'sampling_configured'}
        errors = {0:'none',1:'who_mismatch',2:'spi_transport',3:'config_readback'}
        reads = struct.unpack_from('<H', data, 104)[0]
        out.update(startup_debug=True, init_stage=data[100], init_error=data[101],
                   init_stage_name=stages.get(data[100], 'unknown'),
                   init_error_name=errors.get(data[101], 'unknown'),
                   who_first=data[102], who_last=data[103], who_reads=reads,
                   who_valid_mask=struct.unpack_from('<H', data, 106)[0],
                   who_mismatches=struct.unpack_from('<I', data, 108)[0],
                   who_history=list(data[112:112+min(reads,8)]),
                   spi_hz=struct.unpack_from('<I', data, 120)[0],
                   last_spi_error_register=data[124], last_spi_error_operation=data[125])
    return out


def orientation(data: bytes) -> dict:
    if len(data) != 12:
        raise ValueError("IMU block must be 12 bytes")
    raw_g = struct.unpack_from("<hhh", data)
    xyz = struct.unpack_from("<eee", data, 6)
    norm = sum(v*v for v in xyz)
    if data[6:] == b"\0"*6 or not all(math.isfinite(v) for v in xyz) or norm > 1.02:
        raise ValueError("Uninitialized/invalid SFLP quaternion")
    q = [math.sqrt(max(0., 1 - norm)), *xyz]
    size = math.sqrt(sum(v*v for v in q)); q = [v/size for v in q]
    # q_trunk_to_world = q_sensor_to_world * inverse(mount +90deg Y)
    a = math.sqrt(.5); w, x, y, z = q
    qw, qx, qy, qz = a*(w+y), a*(x+z), a*(y-w), a*(z-x)
    gravity = [2*(qw*qy-qx*qz), -2*(qy*qz+qw*qx), -(1-2*(qx*qx+qy*qy))]
    g = [v * .0175 * math.pi/180 for v in raw_g]
    return {"raw_gyro": list(raw_g), "gyro_rad_s_trunk": [g[2], g[1], -g[0]],
            "quat_wxyz_trunk_world": [qw, qx, qy, qz], "gravity_trunk": gravity}


def servo_preflight(link: Link, ids: list[int]) -> dict:
    report = {}
    for ident in ids:
        if ident == 200:
            continue
        model = int.from_bytes(link.read(ident, 0, 2), "little")
        torque = link.read(ident, 64, 1)[0]
        baud = link.read(ident, 8, 1)[0]
        delay = link.read(ident, 9, 1)[0]
        volts = int.from_bytes(link.read(ident, 144, 2), "little") * .1
        hw_error = link.read(ident, 70, 1)[0]
        report[str(ident)] = {"model": model, "torque": torque, "baud": baud, "return_delay": delay, "volts":volts, "hardware_error":hw_error}
        if model != 1200 or torque != 0 or baud != 3 or delay != 0 or not 3.7 <= volts <= 6.0 or hw_error:
            raise RuntimeError(f"Servo {ident} fails torque-OFF preflight: {report[str(ident)]}. Configure separately in Wizard; no automatic writes.")
    return report


def soak(link: Link, args: argparse.Namespace) -> int:
    ids = [int(v) for v in args.ids.split(",")]
    if not ids or len(ids) != len(set(ids)) or 200 not in ids or any(not 0 <= v <= 252 for v in ids):
        raise ValueError("Provide unique IDs including 200")
    if len(ids) > 32 or args.count < 1 or not 0 < args.hz <= 100:
        raise ValueError("At most32 IDs, positive count, Hz in (0,100]")
    link.require_imu()
    preflight = servo_preflight(link, ids)
    initial = diagnostic(link.read(200, DIAG, 128))
    if not initial["healthy"]:
        raise RuntimeError(f"IMU not ready. Mount/tilt board, then retry: {initial}")
    path = Path(args.log); path.parent.mkdir(parents=True, exist_ok=True)
    failures = deadlines = fresh_failures = 0
    latencies = []
    last_diag = initial
    next_diag = time.monotonic() + 1
    target = time.monotonic()
    with path.open("x", encoding="utf-8") as f:
        f.write(json.dumps({"kind":"start", "wall_time":time.time(), "ids":ids, "preflight":preflight,
                            "diagnostic":initial, "port":args.port, "baud":args.baud}) + "\n")
        for i in range(args.count):
            if target > time.monotonic():
                time.sleep(target-time.monotonic())
            t = time.monotonic(); row = {"kind":"sample", "index":i, "t":t}
            try:
                values = link.exchange(254, 0x82, struct.pack("<HH",124,12)+bytes(ids), expected=ids)
                row["raw"] = {str(ident):data.hex() for ident, _, data in values}
                row["order"] = [ident for ident, _, _ in values]
                row["status"] = [error for _, error, _ in values]
                if any(error or len(data) != 12 for _, error, data in values):
                    raise RuntimeError(f"Status/length failure: {row['status']}")
                row.update(orientation(next(data for ident, _, data in values if ident==200)))
            except (ValueError, RuntimeError, TimeoutError) as exc:
                failures += 1; row["error"] = str(exc); row["trace"] = link.last_trace
            elapsed = (time.monotonic()-t)*1000
            row["round_trip_ms"] = elapsed; latencies.append(elapsed)
            if elapsed > 20:
                deadlines += 1
            f.write(json.dumps(row, allow_nan=False) + "\n")
            if time.monotonic() >= next_diag:
                try:
                    d = diagnostic(link.read(200, DIAG, 128))
                    if (not d["healthy"] or d["gyro_seq"] == last_diag["gyro_seq"] or
                            d["quat_seq"] == last_diag["quat_seq"] or
                            d["uptime_ms"] < last_diag["uptime_ms"]):
                        fresh_failures += 1
                    last_diag = d
                    f.write(json.dumps({"kind":"diagnostic", "data":d})+"\n")
                except (ValueError, RuntimeError, TimeoutError) as exc:
                    fresh_failures += 1; f.write(json.dumps({"kind":"diagnostic_error", "error":str(exc)})+"\n")
                next_diag = time.monotonic()+1; f.flush()
            target += 1/args.hz
            if target < time.monotonic()-1/args.hz:
                target = time.monotonic()  # never issue catch-up bursts
        final = diagnostic(link.read(200, DIAG, 128))
        counters = ["spi_errors","fifo_overruns","uart_errors","rx_drops","crc_errors","frame_errors","sync_timeouts","tx_errors"]
        deltas = {k:final[k]-initial[k] for k in counters}
        reset_or_stale = final["uptime_ms"] < initial["uptime_ms"] or not final["healthy"]
        latencies.sort()
        summary = {"kind":"summary", "samples":args.count, "failures":failures,
                   "over_20ms":deadlines,"freshness_failures":fresh_failures,
                   "host_bad_frames":link.bad_frames,"counter_deltas":deltas,
                   "p50_ms":latencies[len(latencies)//2], "p99_ms":latencies[min(len(latencies)-1,int(len(latencies)*.99))],
                   "hardware_certified":False,
                   "passed":not(failures or deadlines or fresh_failures or link.bad_frames or any(deltas.values()) or reset_or_stale)}
        f.write(json.dumps({"kind":"end_diagnostic", "data":final})+"\n")
        f.write(json.dumps(summary)+"\n")
    print(json.dumps(summary, indent=2));return 0 if summary["passed"] else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port")
    parser.add_argument("--baud",type=int,default=1_000_000)
    sub = parser.add_subparsers(dest="command",required=True)
    sub.add_parser("ports"); sub.add_parser("diag")
    p=sub.add_parser("ping");p.add_argument("--id",type=int,default=200)
    p=sub.add_parser("read");p.add_argument("--id",type=int,default=200);p.add_argument("--addr",type=lambda v:int(v,0),default=124);p.add_argument("--length",type=int,default=12)
    p=sub.add_parser("scratch");p.add_argument("--hex",default="00 ff ff fd 00 aa 55 ff ff fd fd")
    p=sub.add_parser("sample");p.add_argument("--ids",default="200");p.add_argument("--count",type=int,default=30000);p.add_argument("--hz",type=float,default=50);p.add_argument("--log",required=True)
    p=sub.add_parser("negative");p.add_argument("--isolated",action="store_true",required=True)
    p=sub.add_parser("reboot");p.add_argument("--confirm",action="store_true",required=True)
    p=sub.add_parser("freeze");p.add_argument("state",choices=["on","off"]);p.add_argument("--isolated",action="store_true",required=True)
    args=parser.parse_args()
    if args.command=="ports":
        from serial.tools.list_ports import comports
        for p in comports(): print(p.device,p.description)
        return 0
    if not args.port:parser.error("--port is required")
    link=Link(args.port,args.baud)
    try:
        if args.command=="sample":return soak(link,args)
        if args.command=="diag": print(json.dumps(diagnostic(link.read(200,DIAG,128)),indent=2))
        elif args.command=="ping": print([(i,e,d.hex()) for i,e,d in link.exchange(args.id,1)])
        elif args.command=="read":
            data=link.read(args.id,args.addr,args.length);print(data.hex(" "))
            if args.id==200 and args.addr==124 and args.length==12:print(json.dumps(orientation(data),indent=2))
        else:
            link.require_imu()
            if args.command=="scratch":
                value=bytes.fromhex(args.hex)
                if not 1<=len(value)<=32:raise ValueError("Scratch length must be1..32")
                response=link.exchange(200,3,struct.pack("<H",SCRATCH)+value)[0]
                if response[1] or link.read(200,SCRATCH,len(value))!=value:raise RuntimeError("Scratch round trip failed")
                print("Scratch write/read including stuffing PASS; no sensor/servo configuration changed")
            elif args.command=="reboot":
                print(link.exchange(200,8));time.sleep(.5);print(link.exchange(200,1))
            elif args.command=="freeze":
                d=diagnostic(link.read(200,DIAG,128))
                if not d["fault_build"]:raise RuntimeError("Flash FAULTS=1 bench image first; remove all servos")
                r=link.exchange(200,3,b"\xf0\x01\xde\xad"+bytes([args.state=="on"]))
                if r[0][1]:raise RuntimeError(str(r))
                print("Sampling freeze",args.state,". Resume with freeze off or power cycle.")
            elif args.command=="negative":
                bad=bytearray(encode(200,1));bad[-1]^=1
                link.exchange(200,1,raw=bytes(bad),expected=[])
                link.exchange(252,1,expected=[])
                for inst,params,error in [(2,b"\xff\xff\xff\xff",4),(3,b"\x07\x00\x01",7),(0x7f,b"",2)]:
                    r=link.exchange(200,inst,params)
                    if r[0][1]!=error:raise RuntimeError(f"Expected error{error}, got{r}")
                link.port.write(encode(200,2,b"\x7c\x00\x0c\x00")[:9]);link.port.flush();time.sleep(.01)
                link.require_imu();print("Negative cases PASS. Reboot before a clean soak; error counters intentionally increased.")
    finally:
        link.close()
    return 0


if __name__ == "__main__":
    try:sys.exit(main())
    except (Exception,KeyboardInterrupt) as exc:
        print(f"FAIL: {exc}",file=sys.stderr);sys.exit(1)
