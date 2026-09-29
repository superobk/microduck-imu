/* SPDX-License-Identifier: MIT; deterministic native parser/handler stress. */
#include <assert.h>
#include <stdio.h>
#include "dxl.h"
static uint32_t state=200;
static uint32_t random32(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(void){
 dxl_parser_t s={0};dxl_device_t d;dxl_packet_t p;dxl_device_init(&d);
 uint8_t payload[128],wire[384],reply[384];uint32_t us=0;
 for(unsigned test=0;test<100000;test++){
  unsigned n=random32()%129;
  for(unsigned j=0;j<n;j++)payload[j]=(uint8_t)random32();
  unsigned size=(unsigned)dxl_encode(wire,sizeof(wire),(test&1)?200:254,(uint8_t)random32(),payload,n);
  assert(size && size<=sizeof(wire));
  if(test%3==0)wire[random32()%size]^=0x80;
  if(test%5==0)size=random32()%size;
  us+=2000;
  for(unsigned j=0;j<size;j++){
   us+=10;
   if(dxl_feed(&s,wire[j],us,&p)==1){
    size_t r=dxl_handle(&d,&p,us/1000,reply,sizeof(reply));assert(r<=sizeof(reply));
   }
  }
 }
 puts("100000 deterministic parser/handler cases completed");return 0;
}
