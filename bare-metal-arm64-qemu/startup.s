.global _startup
_startup:
    ldr x30, =stack_top	/* setup stack */
    mov sp, x30
    bl main
sleep:
    wfi
    b sleep
