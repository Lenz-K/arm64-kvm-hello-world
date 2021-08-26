.global _startup
_startup:
    ldr x30, =stack_top	/* setup stack */
    mov sp, x30
    mov x0, #0x10000000
    str x30, [x0]
    bl main
sleep:
    wfi
    b sleep
