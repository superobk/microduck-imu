/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include "board.h"
extern uint32_t _estack,_sidata,_sdata,_edata,_sbss,_ebss;
extern int main(void);
void Reset_Handler(void);
static void fault(void) {board_reset();}
__attribute__((section(".isr_vector"),used))
void (* const vectors[48])(void)={
    [0]=(void(*)(void))&_estack,[1]=Reset_Handler,[2]=fault,[3]=fault,
    [11]=fault,[14]=fault,[15]=SysTick_Handler,
    [16+27]=USART1_IRQHandler,[16+28]=USART2_IRQHandler
};
void Reset_Handler(void) {
    uint32_t *d=&_sdata,*s=&_sidata;
    while(d<&_edata)*d++=*s++;
    for(d=&_sbss;d<&_ebss;d++)*d=0;
    (void)main();for(;;){}
}
