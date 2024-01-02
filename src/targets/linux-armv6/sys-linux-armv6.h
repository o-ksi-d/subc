/*
 *	NMH's Simple C Compiler, 2013,2014
 *	Linux/armv6 environment
 */

#define OS		"Linux"
#ifndef LDCMD
#define LDCMD		"ld -o %s %s/lib/%scrt0.o"
#endif
#ifndef ASCMD
#define ASCMD		"as -o %s %s"
#endif

#define SYSLIBC		""
