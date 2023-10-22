	lw	0	1	one	# reg1=1
	lw	0	2	two	# reg2=2
	lw	0	3	three	# reg3=4
	noop
	add	1	2	4
	add	4	0	4
	nor	2	3	5
	add	5	0	5
	lw	0	6	arrPtr
	noop
	sw	6	4	0
	sw	6	5	1
	halt
one	.fill	1
two	.fill	2
three	.fill	4
eight	.fill	8
arrPtr	.fill	arr
arr	.fill	0
	.fill	0
