.data

msg: .ascii "Hello World!\n"
len = . - msg

.text          /* start of the text (code) section */
.global _start /* entry point */

_start:
    mov     x0, #1
    ldr     x1, =msg
    ldr     x2, =len
    mov     w8, #64
    svc     #0
    
    mov     x0, #0
    mov     w8, #93
    svc     #0

