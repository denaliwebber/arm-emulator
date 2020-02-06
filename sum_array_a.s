	.global sum_array_a
	.func sum_array_a

/* r0 - int *array */ 
/* r1 - len of array */ 
/* r2 - int counter */ 
/* r3 - sum of array */ 

sum_array_a: 
	mov r2, #0 
	mov r3, #0
	
loop: 
	cmp r1, r2
	beq endloop
	ldr r12, [r0]
	add r3, r12, r3	//adds to sum 
	add r2, #1	//adds to counter
	cmp r1, r2
	bne next
	
next: 
	add r0, r0, #4
	b loop
		

endloop:
	mov r0, r3	//returns the sum 
	bx lr
