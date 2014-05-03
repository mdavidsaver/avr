/** Modbus RTU server for AVR8
 * Copyright (C) 2013 Michael Davidsaver <mdavidsaver@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "mbus.h"

// testing infrastructure

static size_t npass, nfail, nplan;

void testDiag(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("# ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void testPassV(const char* fmt, va_list args)
{
    npass++;
    printf("ok - ");
    vprintf(fmt, args);
    printf("\n");
}

void testFailV(const char* fmt, va_list args)
{
    nfail++;
    printf("fail - ");
    vprintf(fmt, args);
    printf("\n");
}

void testPass(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    testPassV(fmt, args);
    va_end(args);
}

void testFail(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    testFailV(fmt, args);
    va_end(args);
}

int testOk(int v, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if(v)
        testPassV(fmt, args);
    else
        testFailV(fmt, args);
    return v;
}

#define testOk1(X) testOk(X, #X)

static void testPlan(size_t N)
{
    nplan = N;
}

static int testDone(void)
{
    printf("\n");
    if(nplan && npass+nfail!=nplan) {
        printf("Planned %lu tests but ran %lu\n",
               (unsigned long)nplan,
               (unsigned long)(npass+nfail));
    }

    printf("%lu test pass\n",
           (unsigned long)(npass));

    if(nfail) {
        printf("%lu tests failed!\n",
               (unsigned long)nfail);
    }

    return nfail!=0;
}

uint16_t _crc16_update(uint16_t sum, uint8_t next)
{
    int n;
    sum ^= next;
    for(n=0; n<8; n++) {
        if(sum&1)
            sum = (sum>>1) ^ 0xa001;
        else
            sum = sum>>1;
    }
    //testDiag("Calculate CRC16 %04x", sum);
    return sum;
}

// helpers

static
int modbus_out_all(uint8_t* data, unsigned int maxdata)
{
    uint8_t *end=data+maxdata;
    while(data!=end && mbus_status&MBUS_TX_READY) {
        *data++ = mbus_out_byte;
        mbus_status &= ~MBUS_TX_READY;
        mbus_process();
    }
    if(mbus_status&MBUS_TX_READY)
        return -1;
    return data-(end-maxdata);
}

static
int modbus_in_all(uint8_t* data, size_t dsize)
{
    uint8_t *end=data+dsize;
    while(data!=end && !(mbus_status&(MBUS_RX_READY|MBUS_RX_ERROR)) ) {
        mbus_in_byte = *data++;
        mbus_status |= MBUS_RX_READY;
        mbus_process();
    }
    return data!=end || mbus_status&(MBUS_RX_READY|MBUS_RX_ERROR);
}

static int read_fail;
static size_t read_counter;
static uint16_t read_addr;
static uint8_t read_count;

void mbus_read_holding(uint16_t addr, uint8_t count, uint16_t * restrict result)
{
    int i;
    read_counter++;
    read_addr = addr;
    read_count = count;

    for(i=0; i<count; i++) {
        result[i]=(2*i+1)|(2*i<<8);
    }

    if(read_fail)
        mbus_exception(2);
}

static int write_fail;
static size_t write_counter;
static uint16_t write_addr, write_data;

void mbus_write_holding(uint16_t addr, uint16_t value)
{
    write_counter++;
    write_addr = addr;
    write_data = value;

    if(write_fail)
        mbus_exception(3);
}

// tests

static void testRead(void)
{
    static uint8_t cmd[] = {0x1, 0x3, 0x12, 0x34, 0x00, 0x4, 0xBF, 0x00};
    static uint8_t rep[20];
    static uint8_t expect[] = {0x1, 0x3, 0x8, 0x0, 0x1, 0x2, 0x3,
                               0x4, 0x5, 0x6, 0x7, 0xA6, 0x93};

    testDiag("Testing read holding registers (command 3)");

    testOk1(modbus_in_all(cmd, sizeof(cmd))==0);

    testOk1(read_counter==1);
    testOk1(write_counter==0);
    testOk1(read_addr=0x1234);
    testOk1(read_count==4);

    if(testOk1(modbus_out_all(rep, sizeof(rep))==sizeof(expect))) {
        testOk1(memcmp(rep, expect, sizeof(expect))==0);
    } else
        testFail("Sizes don't match");
}

static void testWrite(void)
{
    static uint8_t cmd[] = {0x1, 0x6, 0x21, 0x43, 0x56, 0x78, 0xA0, 0x4D};
    static uint8_t rep[20];

    testDiag("Testing write single holding register (command 6)");

    testOk1(modbus_in_all(cmd, sizeof(cmd))==0);

    testOk1(read_counter==0);
    testOk1(write_counter==1);
    testOk1(write_addr=0x2143);
    testOk1(write_data==0x5678);

    if(testOk1(modbus_out_all(rep, sizeof(rep))==sizeof(cmd))) {
        testOk1(memcmp(rep, cmd, sizeof(cmd))==0);
    } else
        testFail("Sizes don't match");
}

static void testInvalidFunc(void)
{
    static uint8_t cmd[] = {0x1, 0x08};
    static uint8_t rep[20];
    static uint8_t expect[] = {0x1, 0x88, 0x1, 0x76};

    testDiag("Testing reception of an invalid function code");

    testOk1(modbus_in_all(cmd, sizeof(cmd))==1);

    testOk1(mbus_status==(MBUS_RX_ERROR|MBUS_TX_READY));

    testOk1(read_counter==0);
    testOk1(write_counter==0);

    if(testOk1(modbus_out_all(rep, sizeof(rep))==sizeof(expect))) {
        testOk1(memcmp(rep, expect, sizeof(expect))==0);
    } else
        testFail("Sizes don't match");

    testDiag("Make sure we can still process a valid message");

    mbus_status &= ~MBUS_RX_ERROR;
    testOk1(mbus_status==0);

    testWrite();
}

static void testBadCRC(void)
{
    static uint8_t cmd[] = {0x1, 0x3, 0x12, 0x34, 0x00, 0x4, 0xFF, 0xFF};
    static uint8_t rep[20];
    static uint8_t expect[] = {0x1, 0x83, 0x4, 0x78};

    testDiag("Testing reception of an message with bad CRC");

    testOk1(modbus_in_all(cmd, sizeof(cmd))==1);

    testOk1(mbus_status==(MBUS_RX_ERROR|MBUS_TX_READY));

    testOk1(read_counter==0);
    testOk1(write_counter==0);

    if(testOk1(modbus_out_all(rep, sizeof(rep))==sizeof(expect))) {
        testOk1(memcmp(rep, expect, sizeof(expect))==0);
    } else
        testFail("Sizes don't match");

    testDiag("Make sure we can still process a valid message");

    mbus_status &= ~MBUS_RX_ERROR;
    testOk1(mbus_status==0);

    testWrite();
}

static void testUserReadError(void)
{
    static uint8_t cmd[] = {0x1, 0x3, 0x12, 0x34, 0x00, 0x4, 0xBF, 0x00};
    static uint8_t rep[20];
    static uint8_t expect[] = {0x1, 0x83, 0x2, 0x0};

    read_fail = 1;

    testDiag("Testing user error on read");

    testOk1(modbus_in_all(cmd, sizeof(cmd))==1);

    read_fail = 0;

    testOk1(mbus_status==(MBUS_RX_ERROR|MBUS_TX_READY));

    testOk1(read_counter==1);
    testOk1(write_counter==0);

    if(testOk1(modbus_out_all(rep, sizeof(rep))==sizeof(expect))) {
        testOk1(memcmp(rep, expect, sizeof(expect))==0);
    } else
        testFail("Sizes don't match");

    mbus_status &= ~MBUS_RX_ERROR;
    testOk1(mbus_status==0);
}

static void testUserWriteError(void)
{
    static uint8_t cmd[] = {0x1, 0x6, 0x21, 0x43, 0x56, 0x78, 0xA0, 0x4D};
    static uint8_t rep[20];
    static uint8_t expect[] = {0x1, 0x86, 0x3, 0x76};

    write_fail = 1;

    testDiag("Testing user error on write");

    testOk1(modbus_in_all(cmd, sizeof(cmd))==1);

    write_fail = 0;

    testOk1(mbus_status==(MBUS_RX_ERROR|MBUS_TX_READY));

    testOk1(read_counter==0);
    testOk1(write_counter==1);

    if(testOk1(modbus_out_all(rep, sizeof(rep))==sizeof(expect))) {
        testOk1(memcmp(rep, expect, sizeof(expect))==0);
    } else
        testFail("Sizes don't match");

    mbus_status &= ~MBUS_RX_ERROR;
    testOk1(mbus_status==0);
}

static void runtests(int reset)
{

    mbus_reset();
    testOk1(mbus_status==0);

    read_counter = write_counter = 0;

    testRead();

    if(reset) {mbus_reset(); testOk1(mbus_status==0);}
    read_counter = write_counter = 0;

    testWrite();

    if(reset) {mbus_reset(); testOk1(mbus_status==0);}
    read_counter = write_counter = 0;

    testInvalidFunc();

    if(reset) {mbus_reset(); testOk1(mbus_status==0);}
    read_counter = write_counter = 0;

    testBadCRC();

    if(reset) {mbus_reset(); testOk1(mbus_status==0);}
    read_counter = write_counter = 0;

    testUserReadError();

    if(reset) {mbus_reset(); testOk1(mbus_status==0);}
    read_counter = write_counter = 0;

    testUserWriteError();
}

int main(int argc, char** argv)
{
    testPlan(119);

    testDiag("run and reset state between tests");
    runtests(1);
    testDiag("run tests w/o resets");
    runtests(0);

    return testDone();
}
