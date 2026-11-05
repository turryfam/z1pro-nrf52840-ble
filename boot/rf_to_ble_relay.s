/* Called by a 4-byte B.W patch at factory USB dispatcher 0x2234c.
 * Preserve the original B4 command and every unsupported-command path.
 * New command19, length4, BT01 sets only volatile GPREGRET2 then resets.
 */
.syntax unified
.cpu cortex-m4
.thumb
start:
    cmp r1, #0xb4
    beq original_b4
    cmp r1, #0x19
    bne original_other
    push {r0,r2,r3,lr}
    ldrb.w r2, [r8, #5]
    cmp r2, #4
    bne restore_other
    ldrb.w r2, [r8, #6]
    cmp r2, #0x42
    bne restore_other
    ldrb.w r2, [r8, #7]
    cmp r2, #0x54
    bne restore_other
    ldrb.w r2, [r8, #8]
    cmp r2, #0x30
    bne restore_other
    ldrb.w r2, [r8, #9]
    cmp r2, #0x31
    bne restore_other
    ldr r2, =0x40000520
    movs r3, #0x4d
    str r3, [r2]
    dsb
    ldr r2, =0xe000ed0c
    ldr r3, =0x05fa0004
    str r3, [r2]
    dsb
stop:
    b stop
restore_other:
    pop {r0,r2,r3,lr}
original_other:
    b.w 0x223f0
original_b4:
    b.w 0x223ea
.align 2
.ltorg
