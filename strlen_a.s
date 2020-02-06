.global strlen_a
    .func strlen_a

    /* r0 - char* s */
    /* r1 - int i */

strlen_a:
    mov r1, #0

loop:
    ldrb r2, [r0]
    cmp r2, #0
    beq endloop
    add r1, r1, #1
    add r0, r0, #1
    b loop

endloop:
    mov r0, r1
    bx lr
