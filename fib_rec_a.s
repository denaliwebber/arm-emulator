.global fib_rec_a
    .func fib_rec_a

    /* r0 - n */

fib_rec_a:
    sub sp, sp, #16     /* allocates space for four words on the stack */
        
    /* base case */
    cmp r0, #1
    bgt fib_rec_step    /* if r0 is greater than 1 branch to recursive steps */
    b end               /* else end and return r0 */

fib_rec_step:
    str lr, [sp]        /* store lr on stack */
    str r0, [sp, #4]    /* store r0 on stack */

    sub r0, r0, #1      /* n = n - 1 */
    bl fib_rec_a        /* call fib_rec_a with (n - 1) */

    str r0, [sp, #8]    /* store r0 (n-1) on stack */
    ldr r0, [sp, #4]    /* load original n into r0 */

    sub r0, r0, #2      /* n = n - 2 */
    bl fib_rec_a        /* call fib_rec_a with (n - 2) */
    mov r1, r0          /* move current n into r1 */

    ldr r0, [sp, #8]    /* load (n-1) back into r0 */ 
    add r0, r0, r1      /* add (n-1) result with the (n-2) result */
    ldr lr, [sp]        /* restore original lr */

end:
    add sp, sp, #16     /* deallocates four words on the stack (sp is restored) */
    bx lr               /* return fib in r0 */
