.global _startup
_startup:
    ldr x30, =stack_top	    /* Retrieve initial stack address */
    mov sp, x30             /* Set stack address */
    bl main                 /* Branch to main() */
system_off:
    ldr x29, =0x10000000    /* MMIO Address */
    ldr x0, =0x84000002     /* CPU_OFF function ID */
    hvc #0                  /* Hypervisor call */
    //str x0, [x29]           /* Return hypervisor call result via MMIO */
    ldr x0, =0x84000008     /* SYSTEM_OFF function ID */
    hvc #0                  /* Hypervisor call */
    //str x0, [x29]           /* Return hypervisor call result via MMIO */
    ldr w0, =0xaaaaaaaa     /* Let host know that we are done */
    str w0, [x29]           /* MMIO */
sleep:
    wfi                     /* Wait for interrupt */
    b sleep                 /* Endless loop */
