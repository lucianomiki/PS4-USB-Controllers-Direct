#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "c4f_ds3_report.h"
static void test_neutral(void) {
    uint8_t d[49]={1}; d[6]=d[7]=d[8]=d[9]=128;
    C4fDs3Report r; assert(c4fDs3Parse(d,sizeof(d),&r));
    assert(r.buttons==0 && r.lx==128 && r.ly==128 && r.rx==128 && r.ry==128);
    assert(r.l2==0 && r.r2==0);
}
static void test_buttons(void) {
    uint8_t d[49]={1}; d[2]=0x19; d[3]=0xfc; d[4]=1;
    d[6]=5;d[7]=240;d[8]=155;d[9]=100;
    C4fDs3Report r; assert(c4fDs3Parse(d,49,&r));
    assert((r.buttons & 0x10000) && (r.buttons & 0x4000) && (r.buttons & 0x2000));
    assert((r.buttons & 0x00100000) && !(r.buttons & 0x0001));
    assert((r.buttons & 0x0008) && (r.buttons & 0x0010));
    assert(r.lx==5 && r.ly==240 && r.rx==155 && r.ry==100);
}
static void test_triggers(void) {
    uint8_t d[64]={1}; d[3]=0x03; d[18]=37; d[19]=203;
    C4fDs3Report r; assert(c4fDs3Parse(d,64,&r));
    assert(r.l2==37 && r.r2==203 && (r.buttons & 0x0300)==0x0300);
    d[18]=d[19]=0; assert(c4fDs3Parse(d,49,&r));
    assert(r.l2==255 && r.r2==255);
    assert(!c4fDs3Parse(d,48,&r));
    d[0]=2; assert(!c4fDs3Parse(d,64,&r));
}
int main(void) {
    test_neutral(); test_buttons(); test_triggers();
    puts("OK: DS3 49/64 byte mapping, button bits, PS, sticks, triggers and validation");
    return 0;
}
