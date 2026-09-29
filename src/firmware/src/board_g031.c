/* SPDX-License-Identifier: MIT
 * STM32G031F8P6 only. Register positions cross-checked against ST cmsis-device-g0
 * f576c24e... Include/stm32g031xx.h. Engineering bring-up, not hardware certified.
 */
#include "board.h"
#define R(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define B(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define BIT(n) (1u<<(n))
#define A 0x50000000u
#define G 0x50000400u
#define C 0x50000800u
#define RCC 0x40021000u
#define U1 0x40013800u
#define U2 0x40004400u
#define SPI 0x40013000u
#define TIM 0x40000000u
#if UART1_BRIDGE
#define BUS U1
#else
#define BUS U2
#endif
volatile board_stats_t board_stats;
static volatile uint32_t milliseconds;
/* Arrival timestamps preserve the inter-byte timeout even when the main loop is busy. */
static volatile uint16_t head,tail;
static uint8_t rx_bytes[256];
static uint32_t rx_times[256];
static volatile bool transmitting;
#if !UART1_BRIDGE
static char console[300];
static uint16_t console_len,console_pos;
#endif
static void mode(uint32_t port,unsigned pin,unsigned value) {
    R(port)=(R(port)&~(3u<<(2*pin)))|(value<<(2*pin));
}
static void pull(uint32_t port,unsigned pin,unsigned value) {
    R(port+0x0c)=(R(port+0x0c)&~(3u<<(2*pin)))|(value<<(2*pin));
}
static void af(uint32_t port,unsigned pin,unsigned value) {
    unsigned reg=0x20+(pin/8)*4,shift=(pin%8)*4;
    R(port+reg)=(R(port+reg)&~(15u<<shift))|(value<<shift);
    R(port+8)|=3u<<(pin*2);mode(port,pin,2);
}
static void high(uint32_t port,unsigned pin) {R(port+0x18)=BIT(pin);}
static void low(uint32_t port,unsigned pin) {R(port+0x18)=BIT(pin+16);}
static bool wait_boot(uint32_t addr,uint32_t mask,uint32_t value) {
    for(volatile uint32_t k=0;k<1000000u;k++) if((R(addr)&mask)==value) return true;
    return false;
}
static bool wait_us(uint32_t addr,uint32_t mask,uint32_t value,uint32_t max_us) {
    uint32_t t=board_us();
    do {if((R(addr)&mask)==value)return true;} while((uint32_t)(board_us()-t)<max_us);
    return false;
}
uint32_t board_us(void) {return R(TIM+0x24);}
uint32_t board_ms(void) {return milliseconds;}
void SysTick_Handler(void) {milliseconds++;}
void board_delay_us(uint32_t us) {uint32_t t=board_us();while((uint32_t)(board_us()-t)<us){} }
void board_delay_ms(uint32_t ms) {uint32_t t=board_ms();while((uint32_t)(board_ms()-t)<ms){} }
static void clock_init(void) {
    R(RCC+0x3c)|=BIT(28); /* PWR */
    R(0x40007000)=(R(0x40007000)&~(3u<<9))|(1u<<9); /* voltage range 1 */
    if(!wait_boot(0x40007014,BIT(10),0))board_reset();
    R(0x40022000)=(R(0x40022000)&~7u)|2u; /* Flash 2 wait states before 64 MHz */
    if(!wait_boot(0x40022000,7u,2u))board_reset();
    R(RCC)|=BIT(8);if(!wait_boot(RCC,BIT(10),BIT(10)))board_reset();
    R(RCC)&=~(7u<<11); /* HSI16 /1 */
    R(RCC+8)=0;if(!wait_boot(RCC+8,0x38,0))board_reset();
    R(RCC)&=~BIT(24);if(!wait_boot(RCC,BIT(25),0))board_reset();
    uint32_t source=2; /* HSI16 -> PLL */
#if HSE_BYPASS
    board_stats.clock_flags|=1; /* requested */
    R(RCC)&=~BIT(16);R(RCC)|=BIT(18);R(RCC)|=BIT(16);
    if(wait_boot(RCC,BIT(17),BIT(17))) {source=3;board_stats.clock_flags|=2;}
    else {R(RCC)&=~BIT(16);board_stats.clock_flags|=4;} /* explicit HSI fallback */
#endif
    R(RCC+0x0c)=source|(8u<<8)|BIT(28)|(1u<<29); /* M=1,N=8,R=2 =>64 MHz */
    R(RCC)|=BIT(24);if(!wait_boot(RCC,BIT(25),BIT(25)))board_reset();
    R(RCC+8)=2;if(!wait_boot(RCC+8,0x38,0x10))board_reset();
    board_stats.clock_hz=64000000u;
}
static void uart_init(uint32_t u,uint32_t brr,bool interrupts) {
    R(u)=0;R(u+4)=0;R(u+8)=0;R(u+0x0c)=brr;
    R(u+0x20)=0x1f;R(u)=BIT(0)|BIT(2)|BIT(3)|(interrupts?BIT(5):0);
    if(!wait_boot(u+0x1c,BIT(21)|BIT(22),BIT(21)|BIT(22)))board_reset();
}
void board_init(void) {
    R(RCC+0x34)|=BIT(0)|BIT(1)|BIT(2);
    (void)R(RCC+0x34);
    /* Keep the unused die pads sharing a physical pin in analog mode. Keep SWD intact. */
    mode(G,8,3);mode(G,9,3);mode(G,1,3);mode(G,2,3);mode(A,8,3);mode(A,15,3);
    high(A,1);mode(A,1,1); /* /OE HIGH first, safe during all initialization */
    high(G,7);mode(G,7,1);high(A,4);mode(A,4,1);
    mode(C,14,3);mode(A,0,0);mode(G,0,0);pull(A,0,0);pull(G,0,0);
    board_stats.reset_flags=R(RCC+0x60);R(RCC+0x60)|=BIT(23);
    clock_init();
    R(RCC+0x3c)|=BIT(0)|BIT(17);
    R(RCC+0x40)|=BIT(0)|BIT(12)|BIT(14);
    (void)R(RCC+0x40);
    R(TIM+0x28)=63;R(TIM+0x2c)=0xffffffffu;R(TIM+0x14)=1;R(TIM)=1;
    R(0xe000e014)=63999;R(0xe000e018)=0;R(0xe000e010)=7;
    af(A,5,0);af(A,6,0);af(A,7,0);
    R(SPI+4)=(7u<<8)|BIT(12); /* 8-bit data, byte RX threshold */
    R(SPI)=BIT(2)|BIT(8)|BIT(9)|((SPI_MHZ==4?3u:5u)<<3)|BIT(6); /* SPI0, PCLK /16 or /64 */
    /* PA11/12 package pads remap to PA9/10: SYSCFG CFGR1 PA11_RMP/PA12_RMP. */
    R(0x40010000)|=BIT(3)|BIT(4);af(A,9,1);af(A,10,1);pull(A,10,1);
#if UART1_BRIDGE
    uart_init(U1,64,true); /* full duplex J3, NO DXL driver */
    mode(A,2,3);mode(A,3,3);
    R(0xe000e100)=BIT(27);
#else
    af(A,2,1);af(A,3,1);pull(A,3,1);
    uart_init(U1,556,false); /* 115200 console, polled, not DXL */
    uart_init(U2,64,true);
    R(U2)&=~BIT(0);
    R(U2+8)=BIT(14)|BIT(15); /* DEM, DEP: hardware DE LOW active; HDSEL=0 */
    af(A,1,1);R(U2)|=BIT(0);
    R(0xe000e100)=BIT(28);
#endif
#if WATCHDOG
    R(0x40003000)=0xcccc;R(0x40003000)=0x5555;
    R(0x40003004)=4;R(0x40003008)=500;
    if(!wait_us(0x4000300c,7,0,100000))board_reset();
    board_kick();
#endif
}
static void irq(uint32_t u) {
    uint32_t s=R(u+0x1c);
    if(s&15u) {board_stats.uart_errors++;R(u+0x20)=s&15u;}
    if(s&BIT(5)) {
        uint8_t b=(uint8_t)R(u+0x24);
        if(transmitting || (s&15u))return;
        uint16_t next=(head+1)&255;
        if(next==tail) {board_stats.rx_drops++;return;}
        rx_bytes[head]=b;rx_times[head]=board_us();
        __asm volatile("dmb":::"memory");head=next;
    }
}
void USART1_IRQHandler(void) {irq(U1);}
void USART2_IRQHandler(void) {irq(U2);}
bool board_rx(uint8_t *b,uint32_t *t) {
    if(tail==head)return false;
    *b=rx_bytes[tail];*t=rx_times[tail];
    __asm volatile("dmb":::"memory");tail=(tail+1)&255;return true;
}
bool board_send(const uint8_t *p,size_t n) {
    board_delay_us(20); /* >=2*return_delay_time, starts after request fully decoded */
    transmitting=true;
#if !UART1_BRIDGE
    low(G,7); /* disable receiver; PA3 pull-up prevents floating RX */
#endif
    R(BUS+0x20)=BIT(6);
    bool ok=true;
    for(size_t i=0;i<n;i++) {
        if(!wait_us(BUS+0x1c,BIT(7),BIT(7),2000)) {ok=false;break;}
        R(BUS+0x28)=p[i];
    }
    if(ok)ok=wait_us(BUS+0x1c,BIT(6),BIT(6),2000); /* TC, never DMA complete */
    board_delay_us(2);
#if !UART1_BRIDGE
    if(!ok) {high(A,1);mode(A,1,1);} /* fail released, requires reset to recover */
    high(G,7);
#endif
    transmitting=false;
    if(!ok)board_stats.tx_errors++;
    return ok;
}
static int exchange(uint8_t tx,uint8_t *rx) {
    if(!wait_us(SPI+8,BIT(1),BIT(1),500))return -1;
    B(SPI+0x0c)=tx;
    if(!wait_us(SPI+8,BIT(0),BIT(0),500))return -1;
    *rx=B(SPI+0x0c);return 0;
}
static int transfer(uint8_t a,uint8_t *rd,const uint8_t *wr,size_t n) {
    uint8_t ignored;int error=0;low(A,4);
    if(exchange(a,&ignored))error=-1;
    for(size_t i=0;i<n && !error;i++) {
        uint8_t value=0;
        if(exchange(wr?wr[i]:0,&value))error=-1;
        if(rd)rd[i]=value;
    }
    if(!wait_us(SPI+8,BIT(7),0,500))error=-1;
    high(A,4);return error;
}
int board_spi_read(uint8_t a,uint8_t *p,size_t n) {return transfer(a|0x80,p,0,n);}
int board_spi_write(uint8_t a,const uint8_t *p,size_t n) {return transfer(a&0x7f,0,p,n);}
uint8_t board_pins(void) {
    return (uint8_t)((R(A+0x10)&1u)|((R(G+0x10)&1u)<<1)|(((R(A+0x10)>>1)&1u)<<2)|(((R(G+0x10)>>7)&1u)<<3));
}
void board_console_poll(void) {
#if !UART1_BRIDGE
    if(console_pos<console_len && (R(U1+0x1c)&BIT(7)))R(U1+0x28)=(uint8_t)console[console_pos++];
    if(R(U1+0x1c)&BIT(5))(void)R(U1+0x24);
    if(R(U1+0x1c)&15u)R(U1+0x20)=15;
#endif
}
void board_console_status(uint8_t who,uint32_t gs,uint32_t qs,uint32_t errors,uint8_t flags) {
#if !UART1_BRIDGE
    if(console_pos<console_len)return;
    const char *prefix="IMU1 WHO GSEQ QSEQ SPIERR FLAGS (hex): ";unsigned k=0;
    while(*prefix)console[k++]=*prefix++;
    uint32_t v[5]={who,gs,qs,errors,flags};
    for(unsigned j=0;j<5;j++) {
        for(int shift=28;shift>=0;shift-=4)console[k++]="0123456789ABCDEF"[(v[j]>>shift)&15];
        console[k++]=' ';
    }
    console[k++]='\r';console[k++]='\n';console_pos=0;console_len=(uint16_t)k;
#else
    (void)who;(void)gs;(void)qs;(void)errors;(void)flags;
#endif
}
void board_console_diag(uint8_t stage,uint8_t error,uint16_t reads,uint16_t mask,
                        uint32_t mismatches,const uint8_t history[8]) {
#if !UART1_BRIDGE
    if(console_pos!=0 || console_len>160)return;
    unsigned k=console_len;
    const char *label="DBG2 STAGE ERROR READS VALID_MASK MISMATCH SPI_HZ HISTORY (hex): ";
    while(*label)console[k++]=*label++;
    uint32_t values[6]={stage,error,reads,mask,mismatches,SPI_MHZ*1000000u};
    for(unsigned j=0;j<6;j++) {
        for(int shift=28;shift>=0;shift-=4)console[k++]="0123456789ABCDEF"[(values[j]>>shift)&15];
        console[k++]=' ';
    }
    for(unsigned j=0;j<8;j++) {
        console[k++]="0123456789ABCDEF"[history[j]>>4];
        console[k++]="0123456789ABCDEF"[history[j]&15];console[k++]=' ';
    }
    console[k++]='\r';console[k++]='\n';console_len=(uint16_t)k;
#else
    (void)stage;(void)error;(void)reads;(void)mask;(void)mismatches;(void)history;
#endif
}
void board_kick(void) {
#if WATCHDOG
    R(0x40003000)=0xaaaa;
#endif
}
void board_reset(void) {__asm volatile("cpsid i");R(0xe000ed0c)=0x05fa0004;for(;;){} }
/* Freestanding compiler support; no heap/newlib needed. */
void *memcpy(void *d,const void *s,size_t n) {uint8_t *a=d;const uint8_t *b=s;while(n--)*a++=*b++;return d;}
void *memset(void *d,int c,size_t n) {uint8_t *a=d;while(n--)*a++=(uint8_t)c;return d;}
void __aeabi_memcpy(void *d,const void *s,size_t n) {(void)memcpy(d,s,n);}
void __aeabi_memcpy4(void *d,const void *s,size_t n) {(void)memcpy(d,s,n);}
void __aeabi_memclr(void *d,size_t n) {(void)memset(d,0,n);}
void __aeabi_memclr4(void *d,size_t n) {(void)memset(d,0,n);}
