.global fib_iter_a
    .func fib_iter_a
    
    /* r0 - int n */
    /* r1 - int prev */
    /* r2 - int curr */
    /* r3 - int total */
    /* r12 - int i */

fib_iter_a:
    cmp r0, #0
    beq zero
    mov r1, #0
    mov r2, #0
    mov r3, #1
    mov r12, #1

loop:
    cmp r0, r12
    beq endloop
    mov r1, r2
    mov r2, r3
    add r3, r2, r1
    add r12, r12, #1
    b loop

endloop:
    mov r0, r3
    bx lr

zero:
    bx lr
