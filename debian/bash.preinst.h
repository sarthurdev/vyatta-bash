#ifndef BASH_PREINST_H
#define BASH_PREINST_H

/*
 * This file is in the public domain.
 * You may freely use, modify, distribute, and relicense it.
 */

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>

#if !defined(__GNUC__) && !defined(__attribute__)
# define __attribute__(x)
#endif
#define NORETURN __attribute__((__noreturn__))
#define PRINTFLIKE __attribute__((format(printf, 1, 2)))

extern NORETURN PRINTFLIKE void die_errno(const char *fmt, ...);
extern NORETURN PRINTFLIKE void die(const char *fmt, ...);

extern int exists(const char *path);

extern void run(const char * const cmd[]);	/* spawn and wait */

#endif
