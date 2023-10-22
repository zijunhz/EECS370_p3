	beq	0	0	start
end	halt
start	lw	0	1	one	# reg1=1
	lw	0	2	two	# reg2=2
	lw	0	3	three	# reg3=4
	add	1	2	4
	add	4	0	4
	lw	0	7	th
	beq	4	7	jump
	lw	0	7	eight
jump	nor	2	3	5
	add	5	0	5
	beq	5	0	end
	lw	0	6	arrPtr
	sw	6	4	0
	sw	6	5	1
	beq	0	0	end
one	.fill	1
two	.fill	2
three	.fill	4
th	.fill	3
eight	.fill	8
arrPtr	.fill	arr
arr	.fill	0
	.fill	0
