/* SPDX-License-Identifier: MIT */
#include "board.h"
#include "imu.h"
#include "dxl.h"
static imu_t sensor;
static dxl_device_t device;
static dxl_parser_t parser;
static dxl_packet_t packet;
static uint8_t tx[DXL_MAX_FRAME];
static void le32(unsigned a,uint32_t v) {
    for(unsigned i=0;i<4;i++)device.table[a+i]=(uint8_t)(v>>(8*i));
}
static void update_table(uint32_t now) {
    device.healthy=imu_healthy(&sensor,now) && !device.freeze;
    for(unsigned i=0;i<12;i++)device.table[124+i]=sensor.core[i];
    uint8_t flags=(sensor.configured?1:0)|(sensor.gyro_seq?2:0)|(sensor.quat_seq?4:0)|
        (device.healthy?8:0)|(device.freeze?16:0)|(sensor.quat_valid?32:0)|(sensor.init_error?64:0);
    device.table[0x106]=flags;device.table[0x107]=0;
    le32(0x108,now);le32(0x10c,sensor.gyro_seq);le32(0x110,sensor.quat_seq);
    le32(0x114,sensor.gyro_seq?now-sensor.gyro_ms:0xffffffffu);
    le32(0x118,sensor.quat_seq?now-sensor.quat_ms:0xffffffffu);
    le32(0x11c,sensor.spi_errors);le32(0x120,sensor.fifo_overruns);
    le32(0x124,board_stats.uart_errors);le32(0x128,board_stats.rx_drops);
    le32(0x12c,parser.crc_errors);le32(0x130,parser.frame_errors);
    le32(0x134,device.sync_timeouts);le32(0x138,board_stats.tx_errors);
    le32(0x13c,sensor.invalid_quats);
    for(unsigned i=0;i<6;i++)device.table[0x140+i]=sensor.accel[i];
    for(unsigned i=0;i<2;i++)device.table[0x146+i]=sensor.temperature[i];
    device.table[0x148]=sensor.who;device.table[0x149]=board_pins();
    device.table[0x14a]=board_stats.clock_flags;device.table[0x14b]=UART1_BRIDGE;
    le32(0x14c,board_stats.clock_hz);
    for(unsigned i=0;i<8;i++)device.table[0x150+i]=sensor.config[i];
    le32(0x158,board_stats.reset_flags);
    device.table[0x15c]=WATCHDOG;device.table[0x15d]=ENABLE_FAULTS;
    device.table[0x160]='D';device.table[0x161]='B';device.table[0x162]='G';device.table[0x163]='2';
    device.table[0x164]=sensor.init_stage;device.table[0x165]=sensor.init_error;
    device.table[0x166]=sensor.who_history[0];device.table[0x167]=sensor.who;
    device.table[0x168]=(uint8_t)sensor.who_reads;device.table[0x169]=(uint8_t)(sensor.who_reads>>8);
    device.table[0x16a]=(uint8_t)sensor.who_valid_mask;device.table[0x16b]=(uint8_t)(sensor.who_valid_mask>>8);
    le32(0x16c,sensor.who_mismatches);
    for(unsigned i=0;i<8;i++)device.table[0x170+i]=sensor.who_history[i];
    le32(0x178,SPI_MHZ*1000000u);
    device.table[0x17c]=sensor.last_error_reg;device.table[0x17d]=sensor.last_error_op;
    device.table[0x17f]=2;
}
int main(void) {
    board_init();dxl_device_init(&device);
    imu_io_t io={board_spi_read,board_spi_write,board_delay_ms};
    (void)imu_init(&sensor,io); /* bad IMU must not disable Ping / diagnostics */
    uint32_t last_poll=0,last_log=0;
    for(;;) {
        board_kick();uint32_t now=board_ms();dxl_expire(&device,now);
        uint8_t b;uint32_t when;
        while(board_rx(&b,&when)) {
            if(dxl_feed(&parser,b,when,&packet)==1) {
                update_table(board_ms());
                size_t n=dxl_handle(&device,&packet,board_ms(),tx,sizeof(tx));
                if(n) {
                    bool sent=board_send(tx,n);
                    if(device.reboot && sent){board_delay_ms(2);board_reset();}
                }
            }
        }
        /* Incoming bytes / predecessor replies take priority over SPI / console. */
        if(!device.waiting && (uint32_t)(now-last_poll)>=1) {
            last_poll=now;if(!device.freeze)imu_poll(&sensor,now);
        }
        if((uint32_t)(now-last_log)>=1000) {
            last_log=now;update_table(now);
            board_console_status(sensor.who,sensor.gyro_seq,sensor.quat_seq,sensor.spi_errors,device.table[0x106]);
            board_console_diag(sensor.init_stage,sensor.init_error,sensor.who_reads,
                               sensor.who_valid_mask,sensor.who_mismatches,sensor.who_history);
        }
        board_console_poll();
    }
}
