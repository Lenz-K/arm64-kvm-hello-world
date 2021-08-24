.data
.text          /* start of the text (code) section */
.global _start /* entry point */ 

_start:
    mov     x1, #20
    mov     x2, #22
    add     x3, x1, x2
    
    mov     x0, #0
    mov     w8, #93
    svc     #0

