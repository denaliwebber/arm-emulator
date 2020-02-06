	.global find_max_a
	.func find_max_a

/*
    r0 - takes in the array
    r1 - holds the len of array
    r2 - counter for elem
    r3 - holds the max
    r4 holds max
*/
 

find_max_a:
    mov r2, #1
    mov r12, #0

loop :

    //compares length and counter
    cmp r1, r2
    beq endloop
    //loop
    ldr r3, [r0]    //max
    ldr r12, [r0, #4]

    cmp r12, r3
    bgt r12_max
    add r2, r2, #1
    add r0, r0, #4

    str r3, [r0]

    b loop


 r12_max :
    
    mov r3, r12
    add r2, r2, #1
    add r0, r0, #4

    b loop

endloop:

    mov r0, r3
    bx lr

