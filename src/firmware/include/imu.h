/* SPDX-License-Identifier: MIT */
#ifndef IMU_SENSOR_H
#define IMU_SENSOR_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef struct {
    int (*read)(uint8_t,uint8_t*,size_t);
    int (*write)(uint8_t,const uint8_t*,size_t);
    void (*delay_ms)(uint32_t);
} imu_io_t;
typedef struct {
    imu_io_t io;
    uint8_t core[12],accel[6],temperature[2],who,config[8];
    uint32_t gyro_seq,quat_seq,gyro_ms,quat_ms,spi_errors,fifo_overruns,invalid_quats;
    /* Startup evidence, retained on failure; never interpreted as sensor samples. */
    uint8_t init_stage,init_error,who_history[8],last_error_reg,last_error_op;
    uint16_t who_reads,who_valid_mask;
    uint32_t who_mismatches;
    bool configured,quat_valid;
} imu_t;
bool imu_init(imu_t *s,imu_io_t io);
void imu_poll(imu_t *s,uint32_t now);
bool imu_healthy(const imu_t *s,uint32_t now);
bool imu_quat_valid(const uint8_t p[6]);
#endif
