/* Verified Z1 PRO V2.17 cave 0x1c100..0x1c1ff. No Flash writes.
 * Normal boot: ZMK. DPI+back, or one-shot GPREGRET2=0x52: factory RF/USB.
 * GPREGRET2=0x4d forces ZMK. Invalid secondary manifest/CRC falls back to RF.
 */
.syntax unified
.cpu cortex-m4
.thumb
start:
    mrs r12, primask
    cpsid i
    ldr r0, =0x40000520
    ldr r1, [r0]
    movs r2, #0
    str r2, [r0]
    cmp r1, #0x52
    beq factory
    cmp r1, #0x4d
    beq validate
    ldr r6, =0x50000708
    ldr r7, =0x50000a3c
    ldr r4, [r6]
    ldr r5, [r7]
    movs r0, #12
    str r0, [r6]
    str r0, [r7]
    movw r0, #8192
settle:
    subs r0, #1
    bne settle
    ldr r0, =0x50000510
    ldr r1, [r0]
    ldr r0, =0x50000810
    ldr r2, [r0]
    str r4, [r6]
    str r5, [r7]
    tst r1, #4
    bne validate
    tst r2, #0x8000
    bne validate
    b factory
validate:
    ldr r4, =0x7fff0
    ldm r4, {r0,r1,r2,r3}
    ldr r5, =0x31454c42
    cmp r0, r5
    bne factory
    ldr r6, =0x34000
    cmp r1, r6
    bne factory
    cmp r2, #256
    blo factory
    ldr r5, =0x4b000
    cmp r2, r5
    bhi factory
    mov r4, r6
    movs r0, #0
    mvns r0, r0
    ldr r7, =0xedb88320
crc_byte:
    ldrb r1, [r4], #1
    eors r0, r1
    movs r5, #8
crc_bit:
    lsrs r0, r0, #1
    it cs
    eorcs r0, r0, r7
    subs r5, #1
    bne crc_bit
    subs r2, #1
    bne crc_byte
    mvns r0, r0
    cmp r0, r3
    bne factory
    ldr r0, [r6]
    tst r0, #7
    bne factory
    ldr r1, =0x20000000
    cmp r0, r1
    bls factory
    ldr r1, =0x20040000
    cmp r0, r1
    bhi factory
    ldr r3, [r6, #4]
    tst r3, #1
    beq factory
    cmp r3, r6
    bls factory
    cmp r3, r4
    bhs factory
    ldr r1, =0xe000ed08
    str r6, [r1]
    dsb
    isb
    msr msp, r0
    bx r3
factory:
    ldr r3, =0x1c4a5
    msr primask, r12
    bx r3
.align 2
.ltorg
