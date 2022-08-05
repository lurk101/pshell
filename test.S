.syntax unified
.cpu cortex-m0plus
.thumb

.section .text.test_func
.global test_func
.type test_func,%function
.thumb_func

.extern __wrap_printf
.extern __wrap_sprintf

// int test_func(void* stk, int wrds);

		.balign 4
test_func:
		push {lr}
		pop  {pc}
		blx r2
		mov	r0,r1
		cmp  r0, r1
        push {r4-r7, lr}
		mov  r7, sp
		sub  sp, #0
		ldr	 r1, i
		add	 sp, r1
		ldr  r1, [r7]
		add	 r0,r7

		ldr  r6, i
		push {r0}
		pop {r1}
		str r0,[r1]
		strb r0,[r1]

l2:		mov  sp, r7
        pop  {r4-r7,pc}  
		.balign 4
i:		.word 0
