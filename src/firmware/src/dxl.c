/* SPDX-License-Identifier: MIT
 * Protocol 2.0 sensor subset. See README: no broadcast Ping, Fast/Bulk Read or EEPROM writes.
 */
#include "dxl.h"
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
uint16_t dxl_crc(const uint8_t *p, size_t n) {
    uint16_t crc=0;
    while(n--) {
        crc ^= (uint16_t)*p++ << 8;
        for(unsigned i=0;i<8;i++) crc=(uint16_t)((crc<<1) ^ ((crc&0x8000u)?0x8005u:0u));
    }
    return crc;
}
size_t dxl_encode(uint8_t *out, size_t cap, uint8_t id, uint8_t inst,
                  const uint8_t *p, size_t n) {
    if(cap<10 || n>DXL_MAX_FRAME-16) return 0;
    out[0]=0xff;out[1]=0xff;out[2]=0xfd;out[3]=0;out[4]=id;
    size_t k=7;
    for(size_t i=0;i<=n;i++) {
        if(k+3>cap) return 0;
        out[k++]=(i==0)?inst:p[i-1];
        if(k>=10 && out[k-3]==0xff && out[k-2]==0xff && out[k-1]==0xfd) {
            if(k+3>cap) return 0;
            out[k++]=0xfd;
        }
    }
    put16(out+5,(uint16_t)(k-7+2));
    uint16_t crc=dxl_crc(out,k);put16(out+k,crc);return k+2;
}
static void restart(dxl_parser_t *s) { s->used=0;s->expected=0; }
int dxl_feed(dxl_parser_t *s,uint8_t byte,uint32_t us,dxl_packet_t *out) {
    static const uint8_t hdr[4]={0xff,0xff,0xfd,0};
    if(s->used && (uint32_t)(us-s->last_us)>1500u) { s->frame_errors++; restart(s); }
    s->last_us=us;
    if(s->used<4) {
        if(byte==hdr[s->used]) s->bytes[s->used++]=byte;
        else if(byte==0xff) { /* also recover FF FF FF FD 00 */
            s->used=(s->used>=2)?2:1;s->bytes[0]=s->bytes[1]=0xff;
        } else restart(s);
        return 0;
    }
    if(s->used>=DXL_MAX_FRAME) { s->frame_errors++;restart(s);return -1; }
    s->bytes[s->used++]=byte;
    if(s->used==7) {
        uint16_t len=get16(s->bytes+5);
        if(len<3 || len>DXL_MAX_FRAME-7 || s->bytes[4]>0xfe || s->bytes[4]==0xfd) {
            s->frame_errors++;restart(s);return -1;
        }
        s->expected=(uint16_t)(7+len);
    }
    if(s->expected==0 || s->used<s->expected) return 0;
    size_t end=s->used-2;
    if(dxl_crc(s->bytes,end)!=get16(s->bytes+end)) { s->crc_errors++;restart(s);return -1; }
    out->id=s->bytes[4];
    size_t j=0;
    for(size_t i=7;i<end;i++) {
        out->params[j++]=s->bytes[i];
        if(j>=3 && out->params[j-3]==0xff && out->params[j-2]==0xff && out->params[j-1]==0xfd) {
            if(i+1>=end || s->bytes[i+1]!=0xfd) {
                s->frame_errors++;restart(s);return -1;
            }
            i++;
        }
    }
    out->instruction=out->params[0];out->size=(uint16_t)(j-1);
    for(size_t i=1;i<j;i++) out->params[i-1]=out->params[i];
    restart(s);return 1;
}
void dxl_device_init(dxl_device_t *d) {
    for(size_t i=0;i<sizeof(*d);i++) ((uint8_t*)d)[i]=0;
    put16(d->table,DXL_MODEL);d->table[6]=DXL_FW;d->table[7]=DXL_ID;
    d->table[8]=3;d->table[9]=10; /* minimum 20 us turnaround; no EEPROM */
    d->table[13]=2;d->table[68]=2;
    d->table[DXL_DIAG]='I';d->table[DXL_DIAG+1]='M';
    d->table[DXL_DIAG+2]='U';d->table[DXL_DIAG+3]='1';
    put16(d->table+DXL_DIAG+4,1);
}
static size_t status(uint8_t error,const uint8_t *p,size_t n,uint8_t *out,size_t cap) {
    uint8_t body[DXL_MAX_READ+1];
    if(n>DXL_MAX_READ) return 0;
    body[0]=error;for(size_t i=0;i<n;i++) body[i+1]=p[i];
    return dxl_encode(out,cap,DXL_ID,0x55,body,n+1);
}
static size_t read_reply(dxl_device_t *d,uint16_t a,uint16_t n,uint8_t *out,size_t cap) {
    if(!n || n>DXL_MAX_READ || (uint32_t)a+n>DXL_TABLE_SIZE)
        return status(4,0,0,out,cap);
    /* Never present unavailable/stale orientation as a healthy measurement. */
    uint8_t error=(!d->healthy && a<136 && (uint32_t)a+n>124)?0x80:0;
    return status(error,d->table+a,n,out,cap);
}
void dxl_expire(dxl_device_t *d,uint32_t now) {
    if(d->waiting && (int32_t)(now-d->deadline_ms)>=0) {
        d->waiting=0;d->sync_timeouts++;
    }
}
size_t dxl_handle(dxl_device_t *d,const dxl_packet_t *p,uint32_t now,uint8_t *out,size_t cap) {
    dxl_expire(d,now);
    if(p->instruction==0x55) {
        if(d->waiting && p->size>=1 && p->id==d->predecessors[d->seen]) {
            if(++d->seen==d->waiting) { d->waiting=0;return read_reply(d,d->address,d->length,out,cap); }
        }
        return 0;
    }
    d->waiting=0; /* a new instruction cancels an old pending Sync Read */
    if(p->id!=DXL_ID && p->id!=0xfe) return 0;
    const uint8_t *v=p->params;uint16_t n=p->size;
    if(p->id==0xfe) {
        if(p->instruction!=0x82 || n<5 || n>4+DXL_MAX_IDS) return 0;
        unsigned count=n-4,ours=count;
        for(unsigned i=0;i<count;i++) {
            if(v[4+i]>=0xfd) return 0;
            for(unsigned j=0;j<i;j++) if(v[4+i]==v[4+j]) return 0;
            if(v[4+i]==DXL_ID) ours=i;
        }
        if(ours==count) return 0;
        d->address=get16(v);d->length=get16(v+2);
        if(ours==0) return read_reply(d,d->address,d->length,out,cap);
        for(unsigned i=0;i<ours;i++) d->predecessors[i]=v[4+i];
        d->waiting=(uint8_t)ours;d->seen=0;d->deadline_ms=now+20;
        return 0;
    }
    switch(p->instruction) {
    case 1: {
        if(n) return status(5,0,0,out,cap);
        uint8_t pinfo[3]={(uint8_t)DXL_MODEL,(uint8_t)(DXL_MODEL>>8),DXL_FW};
        return status(0,pinfo,3,out,cap);
    }
    case 2:
        if(n!=4) return status(5,0,0,out,cap);
        return read_reply(d,get16(v),get16(v+2),out,cap);
    case 3: {
        if(n<3) return status(5,0,0,out,cap);
        uint16_t a=get16(v);uint16_t len=(uint16_t)(n-2);
        if(a>=DXL_SCRATCH && (uint32_t)a+len<=DXL_SCRATCH+DXL_SCRATCH_SIZE) {
            for(unsigned i=0;i<len;i++) d->table[a+i]=v[i+2];
            return status(0,0,0,out,cap);
        }
#if ENABLE_FAULTS
        if(a==DXL_FAULT_REG && len==3 && v[2]==0xDE && v[3]==0xAD && v[4]<=1) {
            d->freeze=(v[4]!=0);return status(0,0,0,out,cap);
        }
#endif
        return status(7,0,0,out,cap);
    }
    case 8:
        if(n) return status(5,0,0,out,cap);
        d->reboot=true;return status(0,0,0,out,cap);
    default: return status(2,0,0,out,cap);
    }
}
