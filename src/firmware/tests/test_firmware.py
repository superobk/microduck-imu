# SPDX-License-Identifier: MIT
from __future__ import annotations
import ctypes as C
import importlib.util
import random
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('probe',ROOT/'tools/imu_probe.py')
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)

class Firmware(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();cls.addClassCleanup(cls.tmp.cleanup)
  lib=Path(cls.tmp.name)/'firmware.so'
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-shared','-fPIC','-DENABLE_FAULTS=1','-I'+str(ROOT/'include'),str(ROOT/'src/dxl.c'),str(ROOT/'src/imu.c'),str(ROOT/'tests/harness.c'),'-o',str(lib)],check=True)
  cls.c=C.CDLL(str(lib));cls.c.dxl_crc.argtypes=[C.c_char_p,C.c_size_t];cls.c.dxl_crc.restype=C.c_uint16
  cls.c.dxl_encode.argtypes=[C.c_void_p,C.c_size_t,C.c_uint8,C.c_uint8,C.c_char_p,C.c_size_t];cls.c.dxl_encode.restype=C.c_size_t
  cls.c.test_feed.argtypes=[C.c_uint8,C.c_uint32]
  cls.c.test_handle.argtypes=[C.c_uint32,C.c_void_p]
  cls.c.test_sensor_sample.argtypes=[C.c_uint32,C.c_char_p,C.c_char_p]
  cls.c.imu_quat_valid.argtypes=[C.c_char_p];cls.c.imu_quat_valid.restype=C.c_bool
 def setUp(self):self.c.test_reset();self.us=0
 def feed(self,raw):
  result=0
  for b in raw:
   self.us+=10;r=self.c.test_feed(b,self.us)
   if r:result=r
  return result
 def call(self,inst,params=b'',ident=200):
  self.assertEqual(self.feed(p.encode(ident,inst,params)),1)
  out=C.create_string_buffer(384);n=self.c.test_handle(self.us//1000,out)
  return p.decode(out.raw[:n]) if n else None
 def test_official_crc_vector(self):
  # ROBOTIS Protocol2 unicast Ping ID1 example FF FF FD 00 01 03 00 01 19 4E
  self.assertEqual(p.encode(1,1).hex(),'fffffd0001030001194e')
  self.assertEqual(self.c.dxl_crc(bytes.fromhex('fffffd0001030001'),8),0x4e19)
 def test_ping_model(self):self.assertEqual(self.call(1),(200,85,b'\0\0}\x02'))
 def test_stuffing_random_roundtrips(self):
  rng=random.Random(200)
  for i in range(10000):
   data=rng.randbytes(rng.randrange(241))
   if i%2==0:data=data[:20]+b'\xff\xff\xfd\xfd\xff\xff\xfd'+data[20:]
   raw=p.encode(200,3,data);out=C.create_string_buffer(384)
   n=self.c.dxl_encode(out,384,200,3,data,len(data));self.assertEqual(out.raw[:n],raw)
   self.assertEqual(self.feed(raw),1);self.assertEqual(self.c.test_size(),len(data))
   self.assertEqual(bytes(self.c.test_param(j) for j in range(len(data))),data)
 def test_crc_reject_and_recover(self):
  raw=bytearray(p.encode(200,1));raw[-1]^=1;self.assertEqual(self.feed(raw),-1)
  self.assertEqual(self.c.test_counter(0),1);self.assertIsNotNone(self.call(1))
 def test_timeout_and_header_recovery(self):
  self.feed(p.encode(200,1)[:8]);self.us+=2000
  self.assertEqual(self.feed(b'\xff'+p.encode(200,1)),1)
  self.assertGreater(self.c.test_counter(1),0)
 def test_huge_length_recovery(self):
  self.assertEqual(self.feed(bytes.fromhex('fffffd00c8ffff')),-1);self.assertIsNotNone(self.call(1))
 def test_invalid_stuffing(self):
  raw=bytearray(bytes.fromhex('fffffd00c8060003fffffd'));raw+=struct.pack('<H',p.crc16(raw))
  self.assertEqual(self.feed(raw),-1)
 def test_other_ids_and_broadcasts_silent(self):
  for ident in [0,1,10,20,252,254]:self.assertIsNone(self.call(1,ident=ident))
  for inst in [3,8,0x83,0x92]:self.assertIsNone(self.call(inst,ident=254))
 def test_scratch_boundaries(self):
  data=bytes.fromhex('00fffffdfffffdfd55aa')
  self.assertEqual(self.call(3,struct.pack('<H',0x180)+data)[2],b'\0')
  self.assertEqual(self.call(2,struct.pack('<HH',0x180,len(data)))[2],b'\0'+data)
  self.assertEqual(self.call(3,b'\x9f\x01\x00\x00')[2],b'\x07')
 def test_access_lengths_errors(self):
  for a,n in [(65535,65535),(500,20),(124,0),(0,129)]:self.assertEqual(self.call(2,struct.pack('<HH',a,n))[2],b'\4')
  self.assertEqual(self.call(2,b'\0')[2],b'\5');self.assertEqual(self.call(0x7f)[2],b'\2')
  self.assertEqual(self.call(3,b'\7\0\1')[2],b'\7')
 def test_stale_alert_only_sensor_range(self):
  self.c.test_health(0)
  self.assertEqual(self.call(2,struct.pack('<HH',124,12))[2][0],0x80)
  self.assertEqual(self.call(2,struct.pack('<HH',256,96))[2][0],0)
 def test_sync_first_and_missing(self):
  self.assertEqual(len(self.call(0x82,struct.pack('<HH',124,12)+bytes([200,10]),254)[2]),13)
  self.assertIsNone(self.call(0x82,struct.pack('<HH',124,12)+bytes([10,20]),254))
 def test_sync_predecessors(self):
  self.assertIsNone(self.call(0x82,struct.pack('<HH',124,12)+bytes([10,20,200]),254))
  self.assertIsNone(self.call(0x55,b'\0',20));self.assertIsNone(self.call(0x55,b'\0',10))
  self.assertIsNotNone(self.call(0x55,b'\0',20))
 def test_sync_cancel_timeout_duplicate(self):
  for ids in [bytes([200,200]),bytes([10,253,200])]:self.assertIsNone(self.call(0x82,struct.pack('<HH',124,12)+ids,254))
  self.call(0x82,struct.pack('<HH',124,12)+bytes([10,200]),254)
  self.c.test_expire(100);self.assertEqual(self.c.test_counter(2),1)
  self.assertIsNone(self.call(0x55,b'\0',10))
  self.call(0x82,struct.pack('<HH',124,12)+bytes([10,200]),254)
  self.call(1,ident=20);self.assertIsNone(self.call(0x55,b'\0',10))
 def test_reboot_and_bench_freeze(self):
  self.call(3,b'\xf0\x01\xde\xad\1');self.assertEqual(self.c.test_freeze(),1)
  self.call(3,b'\xf0\x01\xde\xad\0');self.assertEqual(self.c.test_freeze(),0)
  self.call(8);self.assertEqual(self.c.test_reboot(),1)
 def test_sensor_init_and_reserved_bits(self):
  self.assertEqual(self.c.test_sensor_init(0,0x70),1)
  for reg,v in [(0x10,6),(0x11,6),(0x15,2),(0x17,1),(0xa,6),(0xd,2),(0xe,1)]:self.assertEqual(self.c.test_reg(0,reg),v)
  self.assertEqual(self.c.test_reg(1,0x5e),0x5b);self.assertEqual(self.c.test_reg(1,0x44),2)
 def test_sensor_faults(self):
  self.assertEqual(self.c.test_sensor_init(1,0x70),0)
  self.assertEqual(self.c.test_sensor_init(0,0xff),0)
  self.assertEqual(self.c.test_sensor_health(0),0)
 def test_sensor_samples_age_and_overrun(self):
  self.c.test_sensor_init(0,0x70)
  raw=struct.pack('<hhhhhhh',0,10,-20,30,0,0,8192);q=struct.pack('<eee',0,.7071,0)
  self.c.test_sensor_sample(100,raw,q)
  self.assertEqual(bytes(self.c.test_core(i) for i in range(12)),raw[2:8]+q)
  self.assertEqual(self.c.test_sensor_health(150),1);self.assertEqual(self.c.test_sensor_health(201),0)
  self.c.test_sensor_overrun(202);self.assertEqual(self.c.test_sensor_counter(3),1);self.assertEqual(self.c.test_sensor_health(202),0)
 def test_invalid_quaternions(self):
  for q in [b'\0'*6,struct.pack('<eee',float('nan'),0,0),struct.pack('<eee',1,1,1),struct.pack('<eee',float('inf'),0,0)]:self.assertFalse(self.c.imu_quat_valid(q))
  self.assertTrue(self.c.imu_quat_valid(struct.pack('<eee',0,.7071,0)))
 def test_host_mount_and_gravity(self):
  result=p.orientation(struct.pack('<hhh',1,2,3)+struct.pack('<eee',0,.7071,0))
  self.assertAlmostEqual(result['gravity_trunk'][2],-1,places=5)
  self.assertAlmostEqual(result['gyro_rad_s_trunk'][0],3*.0175*3.141592653589793/180)


 def test_host_diagnostic_layout(self):
  raw=bytearray(96);raw[:6]=b'IMU1\x01\x00';raw[6]=0x2f;raw[72]=0x70
  struct.pack_into('<I',raw,12,120);struct.pack_into('<I',raw,16,121)
  struct.pack_into('<I',raw,76,64000000);struct.pack_into('<h',raw,70,256)
  d=p.diagnostic(bytes(raw));self.assertEqual(d['gyro_seq'],120);self.assertEqual(d['quat_seq'],121)
  self.assertEqual(d['temperature_c'],26);self.assertTrue(d['healthy'])
 def test_servo_preflight_is_read_only(self):
  class Servo:
   def __init__(self):self.torque=0
   def read(self,ident,a,n):
    self.assert_id=ident
    return {0:struct.pack('<H',1200),64:bytes([self.torque]),8:b'\3',9:b'\0',144:struct.pack('<H',50),70:b'\0'}[a]
  servo=Servo();self.assertEqual(p.servo_preflight(servo,[200,10])['10']['volts'],5)
  servo.torque=1
  with self.assertRaises(RuntimeError):p.servo_preflight(servo,[200,10])
 def test_host_link_echo_crc_and_order(self):
  class Serial:
   def __init__(self,response):self.response=response;self.buffer=bytearray()
   @property
   def in_waiting(self):return min(len(self.buffer),3)
   def reset_input_buffer(self):self.buffer.clear()
   def write(self,data):self.buffer.extend(data+self.response);return len(data)
   def flush(self):pass
   def read(self,n):out=bytes(self.buffer[:n]);del self.buffer[:n];return out
  def link(response):
   l=p.Link.__new__(p.Link);l.port=Serial(response);l.timeout=.01;l.bad_frames=0;l.last_trace=[];return l
  valid=p.encode(200,85,b'\0abc')
  l=link(valid);self.assertEqual(l.exchange(200,2,b'1234'),[(200,0,b'abc')])
  with self.assertRaises(RuntimeError):link(p.encode(10,85,b'\0')).exchange(200,1)
  bad=bytearray(valid);bad[-1]^=1;l=link(bytes(bad)+valid)
  self.assertEqual(l.exchange(200,1)[0][0],200);self.assertEqual(l.bad_frames,1)

 def test_who_00_ff_not_spi_timeout(self):
  for who in [0,255]:
   self.assertEqual(self.c.test_sensor_init(0,who),0)
   self.assertEqual([self.c.test_sensor_counter(i) for i in range(3)],[0,0,0])
   self.assertEqual([self.c.test_sensor_debug(i) for i in range(6)],[2,1,4,15,4,who])
   self.assertEqual([self.c.test_sensor_debug(i+6) for i in range(4)],[who]*4)
 def test_transport_errors_are_not_valid_who(self):
  self.assertEqual(self.c.test_sensor_init(1,0x70),0)
  self.assertEqual(self.c.test_sensor_debug(1),2)
  self.assertEqual(self.c.test_sensor_debug(3),0)
  self.assertEqual(self.c.test_sensor_counter(2),4)
 def test_post_reset_failure_and_intermittent_who(self):
  self.c.test_sensor_script.argtypes=[C.c_char_p,C.c_uint,C.c_int]
  self.assertEqual(self.c.test_sensor_script(bytes([0x70]*4+[255]*4),8,0),0)
  self.assertEqual(self.c.test_sensor_debug(0),4)
  self.assertEqual(self.c.test_sensor_debug(4),4)
  self.assertEqual(self.c.test_sensor_script(bytes([0x70,0,0x70,0x70]),4,0),0)
  self.assertEqual(self.c.test_sensor_debug(4),1) # never accept only one good identity
 def test_config_failure_stage(self):
  self.c.test_sensor_script.argtypes=[C.c_char_p,C.c_uint,C.c_int]
  self.assertEqual(self.c.test_sensor_script(bytes([0x70]*8),8,0x10),0)
  self.assertEqual(self.c.test_sensor_debug(0),7)
  self.assertEqual(self.c.test_sensor_debug(1),3)
 def test_debug_extension_and_legacy_compatibility(self):
  raw=bytearray(128);raw[:6]=b'IMU1\x01\x00';raw[96:100]=b'DBG2';raw[100:104]=bytes([2,1,255,255]);raw[127]=2
  struct.pack_into('<HHI',raw,104,4,15,4);raw[112:116]=bytes([255]*4);struct.pack_into('<I',raw,120,1000000)
  d=p.diagnostic(bytes(raw));self.assertTrue(d['startup_debug']);self.assertEqual(d['init_stage_name'],'who_before_reset')
  self.assertEqual(d['who_history'],[255]*4);self.assertEqual(d['spi_hz'],1000000)
  self.assertFalse(p.diagnostic(bytes(raw[:96]))['startup_debug'])
 def test_fast_sync_read_explicitly_unsupported(self):
  self.assertIsNone(self.call(0x8a,struct.pack('<HH',124,12)+bytes([200]),254))

 def test_official_16_device_standard_request_core(self):
  ids=bytes([200,20,21,22,23,24,30,31,32,33,34,10,11,12,13,14])
  core=struct.pack('<hhh',100,-200,300)+struct.pack('<eee',0,.7071,0)
  self.c.test_publish_core.argtypes=[C.c_char_p];self.c.test_publish_core(core)
  response=self.call(0x82,struct.pack('<HH',124,12)+ids,254)
  self.assertEqual(response,(200,0x55,b'\0'+core))
  pose=p.orientation(response[2][1:]);self.assertAlmostEqual(pose['gravity_trunk'][2],-1,places=5)
  self.assertAlmostEqual(pose['gyro_rad_s_trunk'][2],-100*.0175*3.141592653589793/180)

if __name__=='__main__':unittest.main()
