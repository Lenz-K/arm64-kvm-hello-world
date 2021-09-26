.global _start
_start:
    ldr x30, =stack_top	    /* Retrieve initial stack address */
    mov sp, x30             /* Set stack address */
    bl main                 /* Branch to main() */

.global system_off
system_off:
    ldr x0, =0x84000008     /* SYSTEM_OFF function ID */
    hvc #0                  /* Hypervisor call */

sleep:                      /* This point should not be reached */
    wfi                     /* Wait for interrupt */
    b sleep                 /* Endless loop */

