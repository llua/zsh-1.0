/*

	test.c - the test builtin

	This file is part of zsh, the Z shell.

   zsh is free software; no one can prevent you from reading the source
   code, or giving it to someone else.
   This file is copyrighted under the GNU General Public License, which
   can be found in the file called COPYING.

   Copyright (C) 1990 Paul Falstad

   zsh is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY.  No author or distributor accepts
   responsibility to anyone for the consequences of using it or for
   whether it serves any particular purpose or works at all, unless he
   says so in writing.  Refer to the GNU General Public License
   for full details.

   Everyone is granted permission to copy, modify and redistribute
   zsh, but only under the conditions described in the GNU General Public
   License.   A copy of this license is supposed to have been given to you
   along with zsh so you can know your rights and responsibilities.
   It should be in a file named COPYING.

   Among other things, the copyright notice and this notice must be
   preserved on all copies.

*/

#include "zsh.h"

#ifndef F_OK
#define F_OK 00
#define R_OK 04
#define W_OK 02
#define X_OK 01
#endif

#include "test.pro"
#include "table.pro"

static char **arg;
static int efg;

void die(char *str)
{
	if (!efg)
		zerrnam("test",str);
	efg = 1;
}

int test(comm comm)
{
Node n;
int t0;
char **av,**ap;

	for (n = comm->args->first, t0 = 0; n; n = n->next,t0++);
	ap = av = (char **) zalloc((sizeof(char *))*(t0+1));
	for (n = comm->args->first; n; n = n->next)
		*ap++ = n->dat;
	*ap = NULL;
	t0 = testmain(av);
	free(av);
	return t0;
}

int testmain(char **argv)
{
int ret,isbrack;
	
	efg = 0;
	arg = argv+1;
	ret = testexpr();
	if (efg)
		return 1;
	isbrack = !strcmp(*argv,"[");
	if (isbrack)
		{
		if (*arg && !strcmp(*arg,"]") && !arg[1])
			return !ret;
		}
	else
		if (!*arg)
			return !ret;
	die("bad test format");
	return 1;
}

int testexpr(void)
{
int ret = testexpr2(),ret2;

	if (*arg && !strcmp(*arg,"-o"))
		{
		arg++;
		ret2 = testexpr2();
		if (efg)
			return 0;
		ret = ret || ret2;
		}
	return ret;
}

int testexpr2(void)
{
int ret = testexpr3(),ret2;

	if (*arg && !strcmp(*arg,"-a"))
		{
		arg++;
		ret2 = testexpr2();
		if (efg)
			return 0;
		ret = ret && ret2;
		}
	return ret;
}

int testexpr3(void)
{
	if (*arg && !strcmp(*arg,"!"))
		{
		arg++;
		return !testexpr3();
		}
	return testexpr4();
}

int testexpr4(void)
{
int ret,t0,t1;
struct stat *st;
char buf[16],*op;

	if (!*arg)
		{
		die("expression expected");
		return 0;
		}
	if (!strcmp(*arg,"("))
		{
		arg++;
		ret = testexpr();
		if (!*arg || strcmp(*arg,")"))
			{
			die("')' expected");
			return 0;
			}
		arg++;
		return ret;
		}
	if (**arg == '-' && !(*arg)[2])
		{
		switch((*arg++)[1])
			{
			case 'a': return(doaccess(F_OK));
			case 'b': return(S_ISBLK(dostat()));
			case 'c': return(S_ISCHR(dostat()));
			case 'd': return(S_ISDIR(dostat()));
			case 'f': return(S_ISREG(dostat()));
			case 'g': return(!!(dostat() & S_ISGID));
			case 'k': return(!!(dostat() & S_ISVTX));
			case 'L': return(S_ISLNK(dostat()));
			case 'p': return(S_ISFIFO(dostat()));
			case 'r': return(doaccess(R_OK));
			case 's': return((st = getstat()) && !!(st->st_size));
			case 'S': return(S_ISSOCK(dostat()));
			case 'u': return(!!(dostat() & S_ISUID));
			case 'w': return(doaccess(W_OK));
			case 'x': return(doaccess(X_OK));
			case 'O': return((st = getstat()) && st->st_uid == geteuid());
			case 'G': return((st = getstat()) && st->st_gid == getegid());
			case 't': {
				int t0 = 1;

				if (*arg && isdigit(**arg))
					t0 = atoi(*arg++);
				return isatty(t0);
				}
			case 'z':
				if (!*arg)
					{
					die("string expected");
					return 0;
					}
				return !strlen(*arg++);
			case 'n':
				if (!*arg)
					{
					die("string expected");
					return 0;
					}
				return !!strlen(*arg++);
			case 'l':
				sprintf(buf,"%d",strlen(*arg));
				*arg = buf;
				break;
			}
		}
	if (!arg[1] || !strcmp(arg[1],"-o") || !strcmp(arg[1],"-a") ||
			!strcmp(arg[1],"]") || !strcmp(arg[1],")"))
		return(!!strlen(*arg++));
	if (!arg[2])
		{
		die("bad expression");
		return 0;
		}
	if (!strcmp(arg[1],"-nt"))
		{
		time_t a;

		if (!(st = getstat()))
			{
			arg += 2;
			return 0;
			}
		a = st->st_mtime;
		arg++;
		if (!(st = getstat()))
			{
			arg += 2;
			return 0;
			}
		return a > st->st_mtime;
		}
	if (!strcmp(arg[1],"-ot"))
		{
		time_t a;

		if (!(st = getstat()))
			{
			arg += 2;
			return 0;
			}
		a = st->st_mtime;
		arg++;
		if (!(st = getstat()))
			{
			arg += 2;
			return 0;
			}
		return a < st->st_mtime;
		}
	if (!strcmp(arg[1],"-ef"))
		{
		dev_t d;
		ino_t i;

		if (!(st = getstat()))
			{
			arg += 2;
			return 0;
			}
		d = st->st_dev;
		i = st->st_ino;
		arg++;
		if (!(st = getstat()))
			{
			arg += 2;
			return 0;
			}
		return d == st->st_dev && i == st->st_ino;
		}
	if (!strcmp(arg[1],"~="))
		{
		arg += 3;
		return patmatch(arg[-3],arg[-1]);
		}
	if (!strcmp(arg[1],"="))
		{
		arg += 3;
		return !strcmp(arg[-3],arg[-1]);
		}
	if (!strcmp(arg[1],"!="))
		{
		arg += 3;
		return !!strcmp(arg[-3],arg[-1]);
		}
	t0 = atoi(arg[0]);
	op = arg[1];
	arg += 2;
	if (!strcmp(*arg,"-l"))
		{
		if (!arg[1])
			{
			die("string expected");
			return 0;
			}
		t1 = strlen(arg[1]);
		arg += 2;
		}
	else
		t1 = atoi(*arg++);
	if (!strcmp(op,"-eq"))
		return t0 == t1;
	if (!strcmp(op,"-ne"))
		return t0 != t1;
	if (!strcmp(op,"-lt"))
		return t0 < t1;
	if (!strcmp(op,"-le"))
		return t0 <= t1;
	if (!strcmp(op,"-gt"))
		return t0 > t1;
	if (!strcmp(op,"-ge"))
		return t0 >= t1;
	if (!efg)
		zerrnam("test","unrecognized operator: %s",op);
	efg = 1;
	return 0;
}

int doaccess(int c)
{
	if (!*arg)
		{
		die("filename expected");
		return 0;
		}
	return !access(*arg++,c);
}

struct stat *getstat(void)
{
static struct stat st;

	if (!*arg)
		{
		die("filename expected");
		return NULL;
		}
	if (!strncmp(*arg,"/dev/fd/",8))
		{
		if (fstat(atoi((*arg++)+8),&st))
			return NULL;
		}
	else if (lstat(*arg++,&st))
		return NULL;
	return &st;
}

unsigned short dostat(void)
{
struct stat *st;

	if (!(st = getstat()))
		return 0;
	return st->st_mode;
}

