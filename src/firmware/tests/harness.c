/* SPDX-License-Identifier: MIT; simulated memory/peripherals, NOT physical validation. */
#include <string.h>
#include "dxl.h"
#include "imu.h"
static dxl_parser_t parser;
static dxl_device_t device;
static dxl_packet_t packet;
static imu_t sensor;
static uint8_t regs[2][128],bank,fifo[7];
static int io_fail,who_index,who_length,corrupt_register;
static uint8_t who_script[8];
void test_reset(void){memset(&parser,0,sizeof(parser));dxl_device_init(&device);device.healthy=true;}
int test_feed(uint8_t b,uint32_t us){return dxl_feed(&parser,b,us,&packet);}
unsigned test_size(void){return packet.size;}
unsigned test_id(void){return packet.id;}
unsigned test_inst(void){return packet.instruction;}
unsigned test_param(unsigned i){return i<packet.size?packet.params[i]:0;}
unsigned test_handle(uint32_t ms,uint8_t *out){return (unsigned)dxl_handle(&device,&packet,ms,out,384);}
unsigned test_counter(unsigned i){return i==0?parser.crc_errors:i==1?parser.frame_errors:device.sync_timeouts;}
void test_expire(uint32_t ms){dxl_expire(&device,ms);}
void test_health(int yes){device.healthy=yes!=0;}
int test_freeze(void){return device.freeze;}
int test_reboot(void){return device.reboot;}
static int mock_read(uint8_t a,uint8_t *p,size_t n){
 if(io_fail)return -1;
 if(!bank && a==0x0f && n==1 && who_length){*p=who_script[who_index<who_length?who_index++:who_length-1];return 0;}
 if(!bank && a==(uint8_t)corrupt_register && corrupt_register>0){memset(p,0,n);return 0;}
 if(!bank && a==0x78 && n==7){memcpy(p,fifo,7);regs[0][0x1b]=0;return 0;}
 if((unsigned)a+n>128)return -1;
 memcpy(p,regs[bank]+a,n);return 0;
}
static int mock_write(uint8_t a,const uint8_t *p,size_t n){
 if(io_fail)return -1;
 if(a==1 && n==1){bank=(p[0]&0x80)?1:0;return 0;}
 if((unsigned)a+n>128)return -1;
 memcpy(regs[bank]+a,p,n);return 0;
}
static void mock_delay(uint32_t ms){(void)ms;}
int test_sensor_init(int fail,int who){
 memset(regs,0,sizeof(regs));bank=0;who_index=who_length=corrupt_register=0;io_fail=fail;regs[0][0xf]=(uint8_t)who;regs[1][0x5e]=0x43;
 imu_io_t io={mock_read,mock_write,mock_delay};return imu_init(&sensor,io);
}
unsigned test_reg(unsigned b,unsigned r){return regs[b&1][r&127];}
void test_sensor_sample(uint32_t now,const uint8_t *raw,const uint8_t *q){
 regs[0][0x1e]=2;memcpy(regs[0]+0x20,raw,14);regs[0][0x1b]=1;regs[0][0x1c]=0;
 fifo[0]=0x13<<3;memcpy(fifo+1,q,6);imu_poll(&sensor,now);
}
void test_sensor_overrun(uint32_t now){regs[0][0x1c]=0x40;imu_poll(&sensor,now);}
int test_sensor_health(uint32_t now){return imu_healthy(&sensor,now);}
unsigned test_sensor_counter(unsigned i){return i==0?sensor.gyro_seq:i==1?sensor.quat_seq:i==2?sensor.spi_errors:i==3?sensor.fifo_overruns:sensor.invalid_quats;}
unsigned test_core(unsigned i){return sensor.core[i%12];}

unsigned test_sensor_debug(unsigned which){
 switch(which){case 0:return sensor.init_stage;case 1:return sensor.init_error;
 case 2:return sensor.who_reads;case 3:return sensor.who_valid_mask;
 case 4:return sensor.who_mismatches;case 5:return sensor.who;
 default:return sensor.who_history[(which-6)%8];}
}
int test_sensor_script(const uint8_t *values,unsigned count,int corrupt){
 memset(regs,0,sizeof(regs));bank=0;io_fail=0;who_index=0;who_length=(int)count;
 if(count>8)return 0;
 memcpy(who_script,values,count);regs[0][0xf]=0x70;regs[1][0x5e]=0x43;
 corrupt_register=corrupt;imu_io_t io={mock_read,mock_write,mock_delay};return imu_init(&sensor,io);
}

void test_publish_core(const uint8_t *p){memcpy(device.table+124,p,12);}
