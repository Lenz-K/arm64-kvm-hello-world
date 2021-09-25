.global _startup
_startup:
    ldr x30, =stack_top	    /* setup stack */
    mov sp, x30
    bl main                 /* Branch to main() */

system_off:
    ldr x0, =0x84000008     /* SYSTEM_OFF function ID */
    hvc #0                  /* Hypervisor call */

sleep:                      /* This point should not be reached */
    wfi                     /* Wait for interrupt */
    b sleep                 /* Endless loop */
