#
#	NMH's Simple C Compiler, 2011--2022
#	C runtime module for Linux/armv6
#

# Calling conventions: r0,r1,r2,stack, return in r0
#                      64-bit values in r0/r1, r2/r3, never in r1/r2
#                      (observe register alignment!)
# System call: r7=call#, arguments r0,r1,r2,r3,r4,r5
#              carry indicates error,
#              return/error value in r0

# https://github.com/aligrudi/neatlibc/blob/master/arm/syscall.s
# https://gist.github.com/yamnikov-oleg/454f48c3c45b735631f2
# https://syscalls.w3challs.com/?arch=arm_strong

	.data
	.align	2
	.globl	Cenviron
Cenviron:
	.long	0

	.text
	.align	2
	.globl	_start
_start:
	bl	C_init		@INIT
	add	r2,sp,#4	@ argv
	ldr	r1,[sp]		@ argc
	mov	r0,r1		@ environ = &argv[argc+1]
	add	r0,r0,#1
	lsl	r0,r0,#2
	add	r0,r0,r2
	ldr	r3,Lenv
	b	env
Lenv:	.long	Cenviron
env:	str	r0,[r3]
	push	{r2}
	push	{r1}
 	bl	Cmain
	add	sp,sp,#8
	push	{r0}
x:	bl	Cexit		@EXIT
	mov	r0,#6		@ SIGABRT
	push	{r0}
	bl	Craise
	add	sp,sp,#4
	b	x

# unsigned integer divide
# inner loop code taken from http://me.henri.net/fp-div.html
# in:  r0 = num,  r1 = den
# out: r0 = quot, r1 = rem

	.globl	udiv
	.align	2
udiv:	rsb     r2,r1,#0
	mov     r1,#0
	adds    r0,r0,r0
	.rept   32
	adcs    r1,r2,r1,lsl #1
	subcc   r1,r1,r2
	adcs    r0,r0,r0
	.endr
	mov     pc,lr

# signed integer divide
# in:  r0 = num,  r1 = den
# out: r0 = quot

	.globl	sdiv
	.align	2
sdiv:	push	{lr}
	eor	r3,r0,r1	@ r3 = sign
	asr	r3,r3,#31
	cmp	r1,#0
	beq	divz
	rsbmi	r1,r1,#0
	cmp	r0,#0
	rsbmi	r0,r0,#0
	bl	udiv
	cmp	r3,#0
	rsbne	r0,r0,#0
	pop	{pc}
divz:	mov	r0,#8		@ SIGFPE
	push	{r0}
	mov	r0,#1
	push	{r0}
	bl	Craise
	mov	r0,#0		@ if raise(SIGFPE) failed, return 0
	pop	{pc}

# signed integer modulo
# in:  r0 = num,  r1 = den
# out: r0 = rem

	.globl	srem
	.align	2
srem:	push	{lr}
	asr	r4,r0,#31		@ r4 = sign
	bl	sdiv
	mov	r0,r1
	cmp	r4,#0
	rsbne	r0,r0,#0
	pop	{pc}

# internal switch(expr) routine
# r1 = switch table, r0 = expr

	.globl	switch
	.align	2
switch:	ldr	r2,[r1]		@ # of non-default cases
	add	r1,r1,#4	@ first case
next:	ldr	r3,[r1]
	cmp	r0,r3
	beq	match
	add	r1,r1,#8
	subs	r2,r2,#1
	bne	next
	ldr	r0,[r1]
	blx	r0
match:	add	r1,r1,#4
	ldr	r0,[r1]
	blx	r0

# int setjmp(jmp_buf env);

	.globl	Csetjmp
	.align	2
Csetjmp:
	ldr	r1,[sp]		@ env
	mov	r2,sp
	add	r2,r2,#4
	str	sp,[r1]
	str	r11,[r1,#4]
	str	lr,[r1,#8]
	mov	r0,#0
	mov	pc,lr

# void longjmp(jmp_buf env, int v);

	.globl	Clongjmp
	.align	2
Clongjmp:
	ldr	r0,[sp,#4]	@ v
	cmp	r0,#0
	moveq	r0,#1
	ldr	r1,[sp]		@ env
	ldr	sp,[r1]
	ldr	r11,[r1,#4]
	ldr	lr,[r1,#8]
	mov	pc,lr

# void _exit(int rc);

	.globl	C_exit
	.align	2
C_exit:	push	{lr}
	ldr	r0,[sp,#4]	@ rc
	mov	r7,#1		@ SYS_exit
	svc	0
	pop	{pc}

# int _sbrk(int size);

	.data
curbrk:	.long 0	

	.text
	.globl	C_sbrk
	.align	2
cbaddr:	.long	curbrk
C_sbrk:	push	{lr}
	ldr	r3,cbaddr
	ldr	r0,[r3]
	cmp	r0,#0
	bne	sbrk

	eor	r0,r0,r0	@ get break
	mov	r7,#45		@ SYS_break
	svc	0
	ldr	r3,cbaddr
	str	r0,[r3]
	
sbrk:
	ldr	r1,[sp,#4]	@ size
	cmp	r1,#0
	bne	setbrk
	
	ldr	r0,[r3]
	pop 	{pc}

setbrk:
	ldr	r0,[r3]
	ldr	r1,[sp,#4]	@ size
	add	r0,r0,r1
	mov	r7,#45		@ SYS_break
	svc	0
	ldr	r3,cbaddr
	ldr	r1,[r3]
	cmp 	r0,r1
	bne 	sbrkok

	mov	r0,#-1
	pop	{pc}

sbrkok:	ldr	r2,[r3]
	str	r0,[r3]
	mov	r0,r2
	pop	{pc}

# int _write(int fd, void *buf, int len);

	.globl	C_write
	.align	2
C_write:
	push	{lr}
	ldr	r2,[sp,#12]	@ len
	ldr	r1,[sp,#8]	@ buf
	ldr	r0,[sp,#4]	@ fd
	mov	r7,#4		@ SYS_write
	svc	0
wrtok:	pop	{pc}

# int _read(int fd, void *buf, int len);

	.globl	C_read
	.align	2
C_read:	push	{lr}
	ldr	r2,[sp,#12]	@ len
	ldr	r1,[sp,#8]	@ buf
	ldr	r0,[sp,#4]	@ fd
	mov	r7,#3		@ SYS_read
	svc	0
redok:	pop	{pc}

# int _lseek(int fd, int pos, int how);

	.globl	C_lseek
	.align	2
C_lseek:
	push	{lr}
	ldr	r2,[sp,#12]	@ how
	ldr	r1,[sp,#8]	@ off_t
	ldr	r0,[sp,#4]	@ fd
	mov	r7,#19
	svc	0
lskok:	
	pop	{pc}

# int _creat(char *path, int mode);

	.globl	C_creat
	.align	2
C_creat:
	push	{lr}
	ldr	r1,[sp,#8]	@ mode
	ldr	r0,[sp,#4]	@ path
	mov	r7,#8		@ SYS_creat
	svc	0
crtok:	pop	{pc}

# int _open(char *path, int flags);

	.globl	C_open
	.align	2
C_open:	push	{lr}
	mov	r2,#0x1A4	@ 0644
	ldr	r1,[sp,#8]	@ flags
	ldr	r0,[sp,#4]	@ path
	mov	r7,#5		@ SYS_open
	svc	0
opnok:	pop	{pc}

# int _close(int fd);

	.globl	C_close
	.align	2
C_close:
	push	{lr}
	ldr	r0,[sp,#4]	@ fd
	mov	r7,#6		@ SYS_close
	svc	0
clsok:	pop	{pc}

# int _unlink(char *path);

	.globl	C_unlink
	.align	2
C_unlink:
	push	{lr}
	ldr	r0,[sp,#4]	@ path
	mov	r7,#10		@ SYS_unlink
	svc	0
unlok:	pop	{pc}

# int _rename(char *old, char *new);

	.globl	C_rename
	.align	2
C_rename:
	push	{lr}
	ldr	r1,[sp,#8]	@ new
	ldr	r0,[sp,#4]	@ old
	mov	r7,#0x26	@ SYS_rename
	svc	0
renok:	pop	{pc}

# int _fork(void);

	.globl	C_fork
	.align	2
C_fork:	push	{lr}
	mov	r7,#2		@ SYS_fork
	svc	0
frkok:	pop	{pc}

# int _wait(int *rc);

	.globl	C_wait
	.align	2
C_wait:	push	{lr}
	mov	r3,#0		@ rusage
	mov	r2,#0		@ options
	ldr	r1,[sp,#4]	@ rc
	mov	r0,#-1		@ wpid
	mov	r7,#0x72	@ SYS_wait4
	svc	0
watok:	pop	{pc}

# int _execve(char *path, char *argv[], char *envp[]);

	.globl	C_execve
	.align	2
C_execve:
	push	{lr}
	ldr	r2,[sp,#12]	@ envp
	ldr	r1,[sp,#8]	@ argv
	ldr	r0,[sp,#4]	@ path
	mov	r7,#11		@ SYS_execve
	svc	0
excok:	pop	{pc}

# int _time(void);

	.globl	C_time
	.align	2
C_time:	push	{lr}
	sub	sp,sp,#16	@ struct timespec
	mov	r1,sp
	mov	r0,#0		@ CLOCK_REALTIME
	mov	r7,#0x7		@ SYS_clock_gettime 0x107
	add	r7,r7,#0x100	
	svc	0
timok:	ldr	r0,[sp]
	add	sp,sp,#16
	pop	{pc}

# int raise(int sig);

	.globl	Craise
	.align	2
Craise:	push	{lr}
	mov	r7,#20		@ SYS_getpid
	svc	0
	ldr	r1,[sp,#4]	@ sig
	mov	r7,#37		@ SYS_kill
	svc	0
rasok:	pop	{pc}

# int signal(int sig, int (*fn)());

	.globl	Csignal
	.align	2
Csignal:
	push	{lr}
	ldr	r0,[sp,#8]	@ fn / act
	ldr	r3,[sp,#4]	@ sig
	sub	sp,sp,#24	@ struct sigaction oact
	sub	sp,sp,#24	@ struct sigaction act
	str	r0,[sp]		@ act.sa_handler / sa_action
	mov	r0,#0
	str	r0,[sp,#4]	@ act.sa_flags
	str	r0,[sp,#8]	@ act.sa_mask
	str	r0,[sp,#12]
	str	r0,[sp,#16]
	str	r0,[sp,#20]
	mov	r2,sp		@ oact
	add	r2,r2,#24
	mov	r1,sp		@ act
	mov	r0,r3		@ sig
	ldr	r7,L_sigaction
	svc	0
sacok:	ldr	r0,[sp,#24]
	add	sp,sp,#48
	pop	{pc}
L_sigaction:
	.long	0x43		@ SYS_sigaction6
