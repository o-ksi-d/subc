/*
 *	NMH's Simple C Compiler, 2012--2021
 *	setjmp.h
 */

typedef struct {
	void	*sp, *fp, *ip;
} jmp_buf[1];

void	longjmp(jmp_buf env, int v);
int	setjmp(jmp_buf env);
