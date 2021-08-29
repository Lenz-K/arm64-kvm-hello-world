.global _startup
_startup:
    ldr x30, =stack_top	/* setup stack */
    mov sp, x30
    bl main
system_off:
    ldr x29, =0x10000000
    ldr x0, =0x84000002 /* CPU_OFF */
    str x0, [x29]
    hvc #0
    str x0, [x29]
    ldr x0, =0x84000008 /* SYSTEM_OFF */
    hvc #0
    str x0, [x29]
sleep:
    wfi
    b sleep
