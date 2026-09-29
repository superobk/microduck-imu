/* SPDX-License-Identifier: MIT
 * Narrow, independent register driver. Register map verified against ST DS13510
 * Rev4 and lsm6dsv16x-pid 2808e5cd6b85f91b66758e1dd0faab5f043aba07.
 * Main-bank diagnostics never write arbitrary sensor registers from the host.
 */
#include "imu.h"
static int rd(imu_t *s,uint8_t a,uint8_t *p,size_t n) {
    int e=s->io.read(a,p,n);
    if(e){s->spi_errors++;s->last_error_reg=a;s->last_error_op=1;if(!s->configured)s->init_error=2;}
    return e;
}
static int wr(imu_t *s,uint8_t a,uint8_t v) {
    int e=s->io.write(a,&v,1);
    if(e){s->spi_errors++;s->last_error_reg=a;s->last_error_op=2;if(!s->configured)s->init_error=2;}
    return e;
}
static int modify(imu_t *s,uint8_t a,uint8_t mask,uint8_t v) {
    uint8_t old;if(rd(s,a,&old,1))return -1;
    return wr(s,a,(uint8_t)((old&~mask)|(v&mask)));
}
static bool fail(imu_t *s) {
    s->configured=false;
    if(!s->init_error)s->init_error=3; /* configuration readback mismatch */
    /* Do not issue additional speculative writes to an unidentified/broken chip. */
    return false;
}
static bool check_who(imu_t *s) {
    bool ok=true;
    for(unsigned j=0;j<4;j++) {
        uint8_t value=0; unsigned i=s->who_reads++;
        int error=rd(s,0x0f,&value,1);
        if(i<8) {
            s->who_history[i]=value;
            if(!error)s->who_valid_mask|=(uint16_t)(1u<<i);
        }
        if(!error) {
            s->who=value;
            if(value!=0x70){s->who_mismatches++;if(!s->init_error)s->init_error=1;ok=false;}
        } else ok=false;
        s->io.delay_ms(5);
    }
    /* Repetition gathers evidence, not a retry-until-one-good policy. */
    return ok;
}
bool imu_init(imu_t *s,imu_io_t io) {
    for(size_t i=0;i<sizeof(*s);i++)((uint8_t*)s)[i]=0;
    s->io=io;s->init_stage=1;io.delay_ms(20);
    s->init_stage=2;
    if(!check_who(s))return false;
    /* Full sensor power-on reset, then bounded settle; default bank restored. */
    s->init_stage=3;
    if(wr(s,0x01,0x04))return fail(s);
    io.delay_ms(30);
    s->init_stage=4;
    if(!check_who(s))return false;
    s->init_stage=5;
    if(wr(s,0x12,0x44) || modify(s,0x03,1,1) || /* BDU, auto increment, SPI only */
       modify(s,0x15,0x0f,2) || modify(s,0x17,3,1) || /* +/-500 dps, +/-4g */
       wr(s,0x07,1) || wr(s,0x09,0) || wr(s,0x0a,0) ||
       wr(s,0x0d,2) || wr(s,0x0e,1) || /* optional INT1 gyro, INT2 accel */
       wr(s,0x10,6) || wr(s,0x11,6))return fail(s); /* high performance 120Hz */
    s->init_stage=6;
    if(wr(s,0x01,0x80))return fail(s);
    /* Preserve reserved SFLP_ODR bits (Rev4: b6,b1,b0 = 1); ODR120Hz code=3. */
    if(modify(s,0x5e,0x38,3u<<3) || modify(s,0x44,0x32,2) ||
       modify(s,0x04,2,2))return fail(s);
    uint8_t e=0,f=0;
    if(rd(s,0x5e,s->config+6,1) || rd(s,0x44,&f,1) || rd(s,0x04,&e,1))return fail(s);
    if((s->config[6]&0x38)!=0x18 || !(f&2) || !(e&2))return fail(s);
    if(wr(s,0x01,0) || wr(s,0x0a,6))return fail(s); /* continuous FIFO */
    s->init_stage=7;
    const uint8_t addresses[6]={0x10,0x11,0x12,0x15,0x17,0x0a};
    for(unsigned i=0;i<6;i++)if(rd(s,addresses[i],s->config+i,1))return fail(s);
    s->config[7]=f;
    if(s->config[0]!=6 || s->config[1]!=6 || (s->config[2]&0x44)!=0x44 ||
       (s->config[3]&15)!=2 || (s->config[4]&3)!=1 || (s->config[5]&7)!=6)return fail(s);
    s->init_stage=8;s->configured=true;return true;
}
/* Validate binary16 without an FPU/libm. Q14 squared norm <=1.02, finite xyz.
 * All +zero remains invalid because the current host interprets it as uninitialized.
 * Do NOT fabricate epsilon/identity to force host readiness. */
bool imu_quat_valid(const uint8_t p[6]) {
    uint32_t sum=0;bool any=false;
    for(unsigned i=0;i<3;i++) {
        uint16_t h=(uint16_t)(p[2*i]|((uint16_t)p[2*i+1]<<8));any|=(h!=0);
        unsigned a=h&0x7fff,exp=a>>10,frac=a&1023;
        if(exp==31 || a>0x3c14)return false; /* no NaN/Inf, no component >~1.02 */
        uint32_t q;
        if(exp==0)q=frac>>10;
        else if(exp>=11)q=(1024u+frac)<<(exp-11);
        else q=(1024u+frac)>>(11-exp);
        sum+=q*q;
    }
    return any && sum<=273804165u;
}
void imu_poll(imu_t *s,uint32_t now) {
    if(!s->configured)return;
    uint8_t st=0;
    if(rd(s,0x1e,&st,1))return;
    if(st&2) {
        uint8_t p[14];
        if(rd(s,0x20,p,14))return;
        for(unsigned i=0;i<2;i++)s->temperature[i]=p[i];
        for(unsigned i=0;i<6;i++){s->core[i]=p[i+2];s->accel[i]=p[i+8];}
        s->gyro_seq++;s->gyro_ms=now;
    }
    uint8_t fs[2];if(rd(s,0x1b,fs,2))return;
    unsigned n=fs[0]|((fs[1]&1u)<<8); /* FIFO level is 9 bits on LSM6DSV16X */
    if(fs[1]&0x48) {
        s->fifo_overruns++;s->quat_valid=false;
        if(wr(s,0x0a,0) || wr(s,0x0a,6))s->configured=false;
        return;
    }
    if(n>8)n=8; /* bounded drain, do not starve protocol parsing */
    for(unsigned j=0;j<n;j++) {
        uint8_t f[7];if(rd(s,0x78,f,7))return;
        if((f[0]>>3)==0x13) {
            bool good=imu_quat_valid(f+1);
            if(!good){s->invalid_quats++;s->quat_valid=false;continue;}
            for(unsigned i=0;i<6;i++)s->core[6+i]=f[1+i];
            s->quat_seq++;s->quat_ms=now;s->quat_valid=true;
        }
    }
}
bool imu_healthy(const imu_t *s,uint32_t now) {
    return s->configured && s->who==0x70 && s->gyro_seq && s->quat_seq && s->quat_valid &&
      (uint32_t)(now-s->gyro_ms)<=100u && (uint32_t)(now-s->quat_ms)<=100u;
}
