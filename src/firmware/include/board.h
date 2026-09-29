/* SPDX-License-Identifier: MIT */
#ifndef IMU_BOARD_H
#define IMU_BOARD_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifndef SPI_MHZ
#define SPI_MHZ 4
#endif
#if SPI_MHZ != 1 && SPI_MHZ != 4
#error SPI_MHZ_must_be_1_or_4
#endif
#ifndef HSE_BYPASS
#define HSE_BYPASS 0
#endif
#ifndef UART1_BRIDGE
#define UART1_BRIDGE 0
#endif
#ifndef WATCHDOG
#define WATCHDOG 0
#endif
typedef struct {uint32_t uart_errors,rx_drops,tx_errors,clock_hz,reset_flags;uint8_t clock_flags;} board_stats_t;
extern volatile board_stats_t board_stats;
void board_init(void);
uint32_t board_us(void);
uint32_t board_ms(void);
void board_delay_ms(uint32_t ms);
void board_delay_us(uint32_t us);
bool board_rx(uint8_t *b, uint32_t *arrival_us);
bool board_send(const uint8_t *p,size_t n);
void board_console_poll(void);
void board_console_status(uint8_t who,uint32_t gs,uint32_t qs,uint32_t errors,uint8_t flags);
int board_spi_read(uint8_t addr,uint8_t *p,size_t n);
int board_spi_write(uint8_t addr,const uint8_t *p,size_t n);
uint8_t board_pins(void);
void board_console_diag(uint8_t stage,uint8_t error,uint16_t reads,uint16_t mask,
                        uint32_t mismatches,const uint8_t history[8]);
void board_kick(void);
void board_reset(void) __attribute__((noreturn));
void USART1_IRQHandler(void);
void USART2_IRQHandler(void);
void SysTick_Handler(void);
#endif
