/* SPDX-License-Identifier: MIT */
#ifndef IMU_DXL_H
#define IMU_DXL_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define DXL_ID 200u
#define DXL_MODEL 0x7D00u /* Project-local test model, NOT an assigned ROBOTIS model. */
#define DXL_FW 2u
#define DXL_MAX_FRAME 384u
#define DXL_MAX_READ 128u
#define DXL_MAX_IDS 32u
#define DXL_TABLE_SIZE 512u
#define DXL_SCRATCH 0x180u
#define DXL_SCRATCH_SIZE 32u
#define DXL_DIAG 0x100u
#define DXL_FAULT_REG 0x1F0u
#ifndef ENABLE_FAULTS
#define ENABLE_FAULTS 0
#endif

typedef struct {
    uint8_t id, instruction;
    uint16_t size;
    uint8_t params[DXL_MAX_FRAME];
} dxl_packet_t;
typedef struct {
    uint8_t bytes[DXL_MAX_FRAME];
    uint16_t used, expected;
    uint32_t last_us, crc_errors, frame_errors;
} dxl_parser_t;
typedef struct {
    uint8_t table[DXL_TABLE_SIZE];
    uint8_t predecessors[DXL_MAX_IDS], waiting, seen;
    uint16_t address, length;
    uint32_t deadline_ms, sync_timeouts;
    bool reboot, freeze, healthy;
} dxl_device_t;
uint16_t dxl_crc(const uint8_t *p, size_t n);
size_t dxl_encode(uint8_t *out, size_t cap, uint8_t id,
                  uint8_t instruction, const uint8_t *params, size_t n);
/* Returns 1 complete, 0 partial, -1 discarded; timeout uses wrap-safe microseconds. */
int dxl_feed(dxl_parser_t *s, uint8_t byte, uint32_t now_us, dxl_packet_t *out);
void dxl_device_init(dxl_device_t *d);
/* Reply frame length, or 0 for silence. No physical TX and no motor instructions here. */
size_t dxl_handle(dxl_device_t *d, const dxl_packet_t *p, uint32_t now_ms,
                  uint8_t *out, size_t cap);
void dxl_expire(dxl_device_t *d, uint32_t now_ms);
#endif
