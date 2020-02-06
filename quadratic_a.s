main:
    mov r0, #2
    mov r1, #3
    mov r2, #4
    mov r3, #5
    bl quadratic_a

end:
    b end

quadratic_a: 
    mul r2, r0, r2
    mul r1, r0, r1
    mul r1, r0, r1
    add r3, r3, r1
    add r3, r3, r2
    mov r0, r3
    bx lr

