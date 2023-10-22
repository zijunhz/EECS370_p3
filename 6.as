	beq	0	0	start
start	lw	0	1	1
	beq	0	1	end
	beq	1	1	fuxk
back	add	1	1	2
	add	2	2	4
	add	1	2	3
	beq	3	3	end
fuxk	beq	0	0	back
end	halt
one	.fill	1
