/*

	utils.c - miscellaneous utilities

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
#include "funcs.h"
#include <pwd.h>
#include <stdarg.h> /* had to change this to stdarg.h.old on one machine */
#include <errno.h>
#include <sys/dir.h>
#include <fcntl.h>

/* add vars to the parm hash table */

void addvars(table vars)
{
char *s1,*s2;
Node node;

	for (node = vars->first; node; node = node->next)
		{
		s1 = node->dat;
		untokenize(s1);
		node = node->next;
		s2 = node->dat;
		dovarsubs(&s2);
		if (errflag)
			break;
		untokenize(s2);
		setparm(s1,s2,0,0);
		}
	free(vars);
}

/* set a parameter to an integer value */

void setiparm(char *s,long v,int isint)
{
struct pmnode *pmn,*pmo;

	if (!strcmp(s,"RANDOM"))
		{
		srand((unsigned long) v);
		return;
		}
	if (!strcmp(s,"SECONDS"))
		{
		shtimer = v+time(NULL);
		return;
		}
	if (pmo = gethnode(s,parmhtab))
		{
		char buf[12];

		pmn = alloc(sizeof *pmn);
		if (pmn->isint = pmo->isint | isint)
			pmn->u.val = v;
		else
			{
			sprintf(buf,"%ld",v);
			pmn->u.str = strdup(buf);
			}
		addhnode(s,pmn,parmhtab,freepm);
		}
	else if (getenv(s) || (opts[ALLEXPORT] == OPT_SET))
		{
		char buf[12];

		sprintf(buf,"%ld",v);
		putenv(tricat(s,"=",buf));
		}
	else
		{
		char buf[12];

		pmn = alloc(sizeof *pmn);
		if (pmn->isint = isint)
			pmn->u.val = v;
		else
			{
			sprintf(buf,"%ld",v);
			pmn->u.str = strdup(buf);
			}
		addhnode(s,pmn,parmhtab,freepm);
		addlocal(s);
		}
	if (!strcmp(s,"PERIOD"))
		{
		period = v*60;
		lastperiod = time(NULL)+period;
		}
	if (!strcmp(s,"HISTSIZE"))
		{
		tevs = v;
		if (tevs <= 2)
			tevs = 2;
		}
}

/* set a parameter to a string value */

void setparm(char *s,char *t,int ex,int isint)
{
struct pmnode *pmn,*pmo;

	if (!strcmp(s,"RANDOM"))
		{
		srand((unsigned long) atol(t));
		return;
		}
	if (!strcmp(s,"SECONDS"))
		{
		shtimer = atol(t)+time(NULL);
		return;
		}
	if (ex && gethnode(s,parmhtab))
		freepm(remhnode(s,parmhtab));
	if (pmo = gethnode(s,parmhtab))
		{
		pmn = alloc(sizeof *pmn);
		if (pmn->isint = pmo->isint | isint)
			{
			pmn->u.val = matheval(t);
			free(t);
			t = NULL;
			}
		else
			pmn->u.str = t;
		addhnode(s,pmn,parmhtab,freepm);
		}
	else if (ex || getenv(s) || (opts[ALLEXPORT] == OPT_SET))
		putenv(tricat(s,"=",t));
	else
		{
		pmn = alloc(sizeof *pmn);
		if (pmn->isint = isint)
			{
			pmn->u.val = matheval(t);
			free(t);
			t = NULL;
			}
		else
			pmn->u.str = t;
		addhnode(s,pmn,parmhtab,freepm);
#if 0
		addlocal(s);
#endif
		}
	if (!t)
		return;
	if (!strcmp(s,"PATH"))
		parsepath();
	if (!strcmp(s,"CDPATH"))
		parsecdpath();
	if (!strcmp(s,"IFS"))
		{
		free(ifs);
		ifs = strdup(t);
		}
	if (!strcmp(s,"PERIOD"))
		{
		period = atoi(t)*60;
		lastperiod = time(NULL)+period;
		}
	if (!strcmp(s,"HISTSIZE"))
		{
		tevs = atoi(t);
		if (tevs <= 2)
			tevs = 2;
		}
	if (!strcmp(s,"HOME"))
		{
		free(home);
		home = xsymlink(t);
		}
	if (!strcmp(s,"MAIL") || !strcmp(s,"MAILCHECK") || !strcmp(s,"MAILPATH"))
		lastmailcheck = 0;
}

void unsetparm(char *s)
{
char **pp;

	if (!strcmp(s,"HOME"))
		return;
	if (!strcmp(s,"PERIOD"))
		period = 0;
	if (!strcmp(s,"HISTSIZE"))
		tevs = 1;
	if (gethnode(s,parmhtab))
		{
		freepm(remhnode(s,parmhtab));
		return;
		}
	for (pp = environ; *pp; pp++)
		if (!strncmp(*pp,s,strlen(s)) && (*pp)[strlen(s)] == '=')
			{
			while (pp[0] = pp[1])
				pp++;
			return;
			}
}

/* get the integer value of a parameter */

long getiparm(char *s)
{
struct pmnode *pmn;
char *t;

	if (!isalpha(*s) && !s[1])
		{
		t = getsparmval(s,1);
		return (t) ? atoi(t) : 0;
		}
	if (s[0] == 'T' && s[1] == 'C' && !s[4])	/* TCxx */
		return tgetnum(s+2);
	if (!strcmp(s,"RANDOM"))
		return rand() & 0x7fff;
	if (!strcmp(s,"LINENO"))
		return lineno;
	if (!strcmp(s,"SECONDS"))
		return time(NULL)-shtimer;
	if (pmn = gethnode(s,parmhtab))
		{
		if (pmn->isint)
			return pmn->u.val;
		return atol(pmn->u.str);
		}
	return atol(getenv(s));
}

/* get the string value of a parameter */

char *getparm(char *s)
{
struct pmnode *pmn;

	if (!isalpha(*s) && !s[1])
		return getsparmval(s,1);
	if (s[0] == 'T' && s[1] == 'C' && !s[4])	/* TCxx */
		{
		static char buf[1024];
		char *ss = buf;
		int t0;

		if (tgetstr(s+2,&ss))
			return buf;
		if ((t0 = tgetnum(s+2)) != -1)
			{
			sprintf(buf,"%d",t0);
			return buf;
			}
		return NULL;
		}
	if (!strcmp(s,"LINENO"))
		{
		static char buf[8];

		sprintf(buf,"%d",lineno);
		return buf;
		}
	if (!strcmp(s,"RANDOM"))
		{
		static char buf[8];

		sprintf(buf,"%d",rand() & 0x7fff);
		return buf;
		}
	if (!strcmp(s,"SECONDS"))
		{
		static char buf[12];

		sprintf(buf,"%ld",time(NULL)-shtimer);
		return buf;
		}
	if (pmn = gethnode(s,parmhtab))
		{
		static char buf[12];

		if (pmn->isint)
			{
			sprintf(buf,"%ld",pmn->u.val);
			return buf;
			}
		return pmn->u.str;
		}
	return getenv(s);
}

/* parse the PATH parameter into directory names in a array of
	strings and create the command hash table */

void parsepath(void)
{
char *pptr = getparm("PATH"),*ptr;

	if (path)
		{
		while(pathct)
			free(path[--pathct]);
		free(path);
		}
	for (pathct = 1, ptr = pptr; *ptr; ptr++)
		if (*ptr == ':')
			pathct++;
	path = (char **) zalloc(pathct*sizeof(char *));
	pathct = 0;
	ptr = pptr;
	while(pptr)
		{
		ptr = strchr(pptr,':');
		if (ptr)
			*ptr = '\0';
		path[pathct] = strdup(pptr);
		if (ptr)
			{
			*ptr = ':';
			pptr = ptr+1;
			}
		else
			pptr = NULL;
		if (!*path[pathct])
			{
			free(path[pathct]);
			path[pathct] = strdup(".");
			}
		pathsub(&path[pathct]);
		if (*path[pathct] != '/' && strcmp(path[pathct],"."))
			{
#ifdef PATH_WARNINGS
			zerr("PATH component not absolute pathname: %s",path[pathct]);
#endif
			free(path[pathct--]);
			}
		pathct++;
		}
	createchtab();
}

void parsecdpath(void)
{
char *pptr = getparm("CDPATH"),*ptr;

	if (cdpath)
		{
		while(cdpathct)
			free(cdpath[--cdpathct]);
		free(cdpath);
		}
	if (pptr == NULL)
		{
		cdpath = (char **) zalloc(sizeof(char *));
		cdpath[0] = strdup(".");
		cdpathct = 1;
		return;
		}
	for (cdpathct = 2, ptr = pptr; *ptr; ptr++)
		if (*ptr == ':')
			cdpathct++;
	cdpath = (char **) zalloc(cdpathct*sizeof(char *));
	cdpath[0] = strdup(".");
	cdpathct = 1;
	ptr = pptr;
	while (pptr)
		{
		ptr = strchr(pptr,':');
		if (ptr)
			*ptr = '\0';
		cdpath[cdpathct] = strdup(pptr);
		if (ptr)
			{
			*ptr = ':';
			pptr = ptr+1;
			}
		else
			pptr = NULL;
		pathsub(&cdpath[cdpathct]);
		if (*cdpath[cdpathct] != '/')
			{
#ifdef PATH_WARNINGS
			zerr("CDPATH component not absolute pathname: %s",cdpath[cdpathct]);
#endif
			free(cdpath[cdpathct--]);
			}
		cdpathct++;
		}
}

/* source a file */

int source(char *s)
{
int fd,cj = curjob,iact = opts[INTERACTIVE];
FILE *obshin = bshin;

	fd = SHIN;
	opts[INTERACTIVE] = OPT_UNSET;
	if ((SHIN = movefd(open(s,O_RDONLY))) == -1)
		{
		SHIN = fd;
		curjob = cj;
		opts[INTERACTIVE] = iact;
		return 1;
		}
	bshin = fdopen(SHIN,"r");
	loop();
	fclose(bshin);
	opts[INTERACTIVE] = iact;
	bshin = obshin;
	SHIN = fd;
	peek = EMPTY;
	curjob = cj;
	errflag = 0;
	retflag = 0;
	return 0;
}

/* try to source a file in our home directory */

void sourcehome(char *s)
{
char buf[MAXPATHLEN];

	sprintf(buf,"%s/%s",getparm("HOME"),s);
	(void) source(buf);
}

/* print an error */

void zerrnam(char *cmd, char *fmt, ...)
{
va_list ap;
char *str;
int num;

	va_start(ap,fmt);
	fputs(cmd,stderr);
	putc(':',stderr);
	putc(' ',stderr);
	while (*fmt)
		if (*fmt == '%')
			{
			fmt++;
			switch(*fmt++)
				{
				case 's':	/* string */
					str = va_arg(ap,char *);
					while (*str)
						niceputc(*str++,stderr);
					break;
				case 'l':	/* string with a length */
					num = va_arg(ap,int);
					str = va_arg(ap,char *);
					while (num--)
						niceputc(*str++,stderr);
					break;
				case 'd':	/* number */
					num = va_arg(ap,int);
					fprintf(stderr,"%d",num);
					break;
				case '%':
					putc('%',stderr);
					break;
				case 'c':	/* char */
					num = va_arg(ap,int);
					niceputc(num,stderr);
					break;
				case 'e':	/* system error */
					num = va_arg(ap,int);
					if (num == EINTR)
						{
						fputs("interrupt\n",stderr);
						errflag = 1;
						return;
						}
					fputc(tolower(sys_errlist[num][0]),stderr);
					fputs(sys_errlist[num]+1,stderr);
					break;
				}
			}
		else
			putc(*fmt++,stderr);
	putc('\n',stderr);
	va_end(ap);
}

void zerr(char *fmt,...)
{
va_list ap;
char *str;
int num;

	va_start(ap,fmt);
	fputs("zsh: ",stderr);
	while (*fmt)
		if (*fmt == '%')
			{
			fmt++;
			switch(*fmt++)
				{
				case 's':
					str = va_arg(ap,char *);
					while (*str)
						niceputc(*str++,stderr);
					break;
				case 'l':
					num = va_arg(ap,int);
					str = va_arg(ap,char *);
					while (num--)
						niceputc(*str++,stderr);
					break;
				case 'd':
					num = va_arg(ap,int);
					fprintf(stderr,"%d",num);
					break;
				case '%':
					putc('%',stderr);
					break;
				case 'c':
					num = va_arg(ap,int);
					niceputc(num,stderr);
					break;
				case 'e':
					num = va_arg(ap,int);
					if (num == EINTR)
						{
						fputs("interrupt\n",stderr);
						errflag = 1;
						return;
						}
					fputc(tolower(sys_errlist[num][0]),stderr);
					fputs(sys_errlist[num]+1,stderr);
					break;
				}
			}
		else
			putc(*fmt++,stderr);
	putc('\n',stderr);
	va_end(ap);
}

void niceputc(int c,FILE *f)
{
	if (istok(c))
		{
		if (c >= Pound && c <= Qtick)
			putc(tokens[c-Pound],f);
		return;
		}
	c &= 0x7f;
	if (c >= ' ' && c < '\x7f')
		putc(c,f);
	else if (c == '\n')
		{
		putc('\\',f);
		putc('n',f);
		}
	else
		{
		putc('^',f);
		putc(c|'A',f);
		}
}

/* enable ^C interrupts */

void intr(void)
{
struct sigvec vec = { handler,sigmask(SIGINT),SV_INTERRUPT };
	
	if (interact)
		sigvec(SIGINT,&vec,NULL);
	sigsetmask(0);
}

void noholdintr(void)
{
	intr();
}

void holdintr(void)
{
struct sigvec vec = { handler,sigmask(SIGINT),0 };

	if (interact)
		{
		sigvec(SIGINT,&vec,NULL);
		sigsetmask(0);
		}
}

char *fgetline(char *buf,int len,FILE *in)
{
	if (!fgets(buf,len,in))
		return NULL;
	buf[len] = '\0';
	buf[strlen(buf)-1] = '\0';
	return buf;
}

/* get a symlink-free pathname for s relative to PWD */

char *findcwd(char *s)
{
char *t;

	if (*s == '/')
		return xsymlink(s);
	s = tricat((cwd[1]) ? cwd : "","/",s);
	t = xsymlink(s);
	free(s);
	return t;
}

static char xbuf[MAXPATHLEN];

/* expand symlinks in s, and remove other weird things */

char *xsymlink(char *s)
{
	if (*s != '/')
		return NULL;
	strcpy(xbuf,"");
	if (xsymlinks(s+1))
		return strdup(s);
	if (!*xbuf)
		return strdup("/");
	return strdup(xbuf);
}

char **slashsplit(char *s)
{
char *t,**r,**q;
int t0;

	if (!*s)
		return (char **) calloc(sizeof(char **),1);
	for (t = s, t0 = 0; *t; t++)
		if (*t == '/')
			t0++;
	q  = r = (char **) zalloc(sizeof(char **)*(t0+2));
	while (t = strchr(s,'/'))
		{
		*t = '\0';
		*q++ = strdup(s);
		*t = '/';
		while (*t == '/')
			t++;
		if (!*t)
			{
			*q = NULL;
			return r;
			}
		s = t;
		}
	*q++ = strdup(s);
	*q = NULL;
	return r;
}

int islink(char *s)
{
char xbuf[MAXPATHLEN];

	if (readlink(s,xbuf,1) == -1 && errno == EINVAL)
		return 0;
	return 1;
}

int xsymlinks(char *s)
{
char **pp,**opp;
char xbuf2[MAXPATHLEN],xbuf3[MAXPATHLEN];
int t0;

	opp = pp = slashsplit(s);
	for (; *pp; pp++)
		{
		if (!strcmp(*pp,"."))
			{
			free(*pp);
			continue;
			}
		if (!strcmp(*pp,".."))
			{
			char *p;

			free(*pp);
			if (!strcmp(xbuf,"/"))
				continue;
			p = xbuf+strlen(xbuf);
			while (*--p != '/');
			*p = '\0';
			continue;
			}
		sprintf(xbuf2,"%s/%s",xbuf,*pp);
		t0 = readlink(xbuf2,xbuf3,MAXPATHLEN);
		if (t0 == -1)
			{
			if (errno != EINVAL)
				{
				while (*pp)
					free(*pp++);
				free(opp);
				return 1;
				}
			strcat(xbuf,"/");
			strcat(xbuf,*pp);
			free(*pp);
			}
		else
			{
			xbuf3[t0] = '\0'; /* STUPID */
			if (*xbuf3 == '/')
				{
				strcpy(xbuf,"");
				if (xsymlinks(xbuf3+1))
					return 1;
				}
			else
				if (xsymlinks(xbuf3))
					return 1;
			free(*pp);
			}
		}
	free(opp);
	return 0;
}

void printdir(char *s)
{
int t0;

	if (!strncmp(s,home,t0 = strlen(home)))
		{
		putchar('~');
		fputs(s+t0,stdout);
		}
	else
		fputs(s,stdout);
}


int ddifftime(time_t t1,time_t t2)
{
	return ((long) t2-(long) t1);
}

/* see if jobs need printing */

void scanjobs(void)
{
int t0;

	for (t0 = 1; t0 != MAXJOB; t0++)
		if (jobtab[t0].stat & STAT_CHANGED)
			printjob(jobtab+t0,0);
}

/* do pre-prompt stuff */

void preprompt(void)
{
int diff;
list list;
char *mc = getparm("MAILCHECK"),*wc = getparm("LOGCHECK");
struct schnode *sch,*schl;

	if (unset(NOTIFY))
		scanjobs();
	if (errflag)
		return;
	if (list = gethnode("precmd",shfunchtab))
		newrunlist(list);
	if (errflag)
		return;
	if (period && (time(NULL) > lastperiod+period) &&
			(list = gethnode("periodic",shfunchtab)))
		{
		newrunlist(list);
		lastperiod = time(NULL);
		}
	if (errflag)
		return;
	if (getparm("WATCH"))
		{
		diff = (int) ddifftime(lastwatch,time(NULL));
		if (diff > ((wc) ? atoi(wc)*60 : 300))
			{
			lastwatch = time(NULL);
			watch();
			}
		}
	if (errflag)
		return;
	diff = (int) ddifftime(lastmailcheck,time(NULL));
	if (diff > ((mc) ? atoi(mc) : 60))
		{
		lastmailcheck = time(NULL);
		if (getparm("MAILPATH"))
			checkmailpath();
		else
			checkmail();
		}
	for (schl = (struct schnode *) &scheds, sch = scheds; sch;
			sch = (schl = sch)->next)
		{
		if (sch->time < time(NULL))
			{
			execstring(sch->cmd);
			schl->next = sch->next;
			free(sch);
			}
		if (errflag)
			return;
		}
}
 
void checkmail(void)
{
struct stat st;
char *s;

	if (!(s = getparm("MAIL")))
		return;
	if (stat(s,&st) == -1)
		{
		if (errno != ENOENT)
			zerr("%e: %s",errno,getparm("MAIL"));
		lastmailval = 0;
		lastmailsize = 0;
		return;
		}
	else
		if (lastmailval != -1 && lastmailval < st.st_mtime &&
				lastmailsize < st.st_size)
			zerr("you have new mail.");
	lastmailval = st.st_mtime;
	lastmailsize = st.st_size;
}

void checkfirstmail(void)
{
struct stat st;
char *s;

	if (!(s = getparm("MAIL")))
		return;
	if (stat(s,&st) == -1)
		{
		if (errno != ENOENT)
			zerr("%e: %s",errno,getparm("MAIL"));
		lastmailval = 0;
		lastmailsize = 0;
		return;
		}
	lastmailval = st.st_mtime;
	lastmailsize = st.st_size;
	zerr("you have mail.");
}
 
void checkmailpath(void)
{
struct stat st;
char *s = getparm("MAILPATH"),*v,*u,c,d;

	for (;;)
		{
		for (v = s; *v && *v != '?' && *v != ':'; v++);
		c = *v;
		*v = '\0';
		if (c != '?')
			u = NULL;
		else
			{
			for (u = v+1; *u && *u != ':'; u++);
			d = *u;
			*u = '\0';
			}
		if (stat(s,&st) == -1)
			{
			if (errno != ENOENT)
				zerr("%e: %s",errno,getparm("MAIL"));
			}
		else
			if (lastmailval != -1 && lastmailval < st.st_mtime &&
					lastmailsize < st.st_size)
				if (!u)
					fprintf(stderr,"You have new mail.\n");
				else
					{
					char *z = u;

					while (*z)
						if (*z == '$' && z[1] == '_')
							{
							fprintf(stderr,"%s",s);
							z += 2;
							}
						else
							fputc(*z++,stderr);
					fputc('\n',stderr);
					}
		lastmailval = st.st_mtime;
		lastmailsize = st.st_size;
		*v = c;
		if (u)
			*u = d;
		if (!c || (u && !d))
			break;
		v = (u) ? u+1 : v+1;
		}
}

/* create command hash table */

void createchtab(void)
{
int t0,dot = 0;
struct direct *de;
DIR *dir;
struct chnode *cc;

	holdintr();
	if (chtab)
		freehtab(chtab,freechnode);
	chtab = newhtable(101);
	for (t0 = 0; t0 != pathct; t0++)
		if (!strcmp(".",path[t0]))
			{
			dot = 1;
			break;
			}
	for (t0 = pathct-1; t0 >= 0; t0--)
		if (!strcmp(".",path[t0]))
			dot = 0;
		else
			{
			dir = opendir(path[t0]);
			if (!dir)
				{
				zerr("%e: %s",errno,path[t0]);
				continue;
				}
			readdir(dir); readdir(dir);
			while (de = readdir(dir))
				{
				cc = alloc(sizeof(struct chnode));
				cc->type = (dot) ? EXCMD_POSTDOT : EXCMD_PREDOT;
				cc->globstat = GLOB;
				cc->u.nam = tricat(path[t0],"/",de->d_name);
				addhnode(strdup(de->d_name),cc,chtab,freechnode);
				}
			closedir(dir);
			}
	addintern(chtab);
	noholdintr();
}

void freechnode(void *a)
{
struct chnode *c = (struct chnode *) a;

	if (c->type != BUILTIN)
		free(c->u.nam);
	free(c);
}

void freestr(void *a)
{
	free(a);
}

void freeanode(void *a)
{
struct anode *c = (struct anode *) a;

	free(c->text);
	free(c);
}

void freeredir(void *a)
{
struct fnode *f = (struct fnode *) a;

	if (f)
		{
		if (f->type == HEREDOC)
			close(f->u.fd2);
		else
			free(f->u.name);
		free(f);
		}
}

void freeshfunc(void *a)
{
	freelist((list) a);
}

void freepm(void *a)
{
struct pmnode *pm = a;

	if (!pm->isint)
		free(pm->u.str);
	free(pm);
}

void restoretty(void)
{
	settyinfo(&shttyinfo);
}

void gettyinfo(struct ttyinfo *ti)
{
	if (jobbing)
		{
#ifndef BUGGY_GCC
#ifdef TERMIOS
		ioctl(SHTTY,TCGETS,&ti->termios);
#else
		ioctl(SHTTY,TIOCGETP,&ti->sgttyb);
		ioctl(SHTTY,TIOCGETC,&ti->tchars);
		ioctl(SHTTY,TIOCGLTC,&ti->ltchars);
#endif
		ioctl(SHTTY,TIOCGWINSZ,&ti->winsize);
#else
#ifdef TERMIOS
		ioctl(SHTTY,	(	0x40000000	|((sizeof( struct termios)&0xff		)<<16)|('T'<<8)| 8)  ,&ti->termios);
#else
		ioctl(SHTTY,(0x40000000|((sizeof(struct sgttyb)&0x1fff)<<16)|
			('t'<<8)|8),&ti->sgttyb);
		ioctl(SHTTY,(0x40000000|((sizeof(struct tchars)&0x1fff)<<16)|
			('t'<<8)|18),&ti->tchars);
		ioctl(SHTTY,(0x40000000|((sizeof(struct ltchars)&0x1fff)<<16)|
			('t'<<8)|116),&ti->ltchars);
#endif
		ioctl(SHTTY,(	0x40000000	|((sizeof( struct winsize)&0xff		)<<16)|('t'<<8)| 104) 	,&ti->winsize);
#endif
		}
}

void settyinfo(struct ttyinfo *ti)
{
	if (jobbing)
		{
#ifndef BUGGY_GCC
#ifdef TERMIOS
		ioctl(SHTTY,TCSETS,&ti->termios);
#else
		ioctl(SHTTY,TIOCSETP,&ti->sgttyb);
		ioctl(SHTTY,TIOCSETC,&ti->tchars);
		ioctl(SHTTY,TIOCSLTC,&ti->ltchars);
#endif
		ioctl(SHTTY,TIOCSWINSZ,&ti->winsize);
#else
#ifdef TERMIOS
		ioctl(SHTTY,	(	0x80000000	|((sizeof( struct termios)&0xff		)<<16)|('T'<<8)| 9)  ,&ti->termios);
#else
		ioctl(SHTTY,(0x80000000|((sizeof( struct sgttyb)&0x1fff)<<16)|
			('t'<<8)|9),&ti->sgttyb);
		ioctl(SHTTY,(0x80000000|((sizeof(struct tchars)&0x1fff)<<16)|
			('t'<<8)|17),&ti->tchars);
		ioctl(SHTTY,(0x80000000|((sizeof(struct ltchars)&0x1fff)<<16)|
			('t'<<8)|117),&ti->ltchars);
#endif
		ioctl(SHTTY,(	0x80000000	|((sizeof( struct winsize)&0xff		)<<16)|('t'<<8)| 103) 	,&ti->winsize);
#endif
		}
}

int zyztem(char *s,char *t)
{
#ifdef WAITPID
int pid,statusp;

	if (!(pid = fork()))
		{
		s = tricat(s," ",t);
		execl("/bin/sh","sh","-c",s,(char *) 0);
		_exit(1);
		}
	waitpid(pid,&statusp,WUNTRACED);
	if (WIFEXITED(SP(statusp)))
		return WEXITSTATUS(SP(statusp));
	return 1;
#else
	if (!waitfork())
		{
		s = tricat(s," ",t);
		execl("/bin/sh","sh","-c",s,(char *) 0);
		_exit(1);
		}
	return 0;
#endif
}

#ifndef WAITPID

/* fork a process and wait for it to complete without confusing
	the SIGCHLD handler */

int waitfork(void)
{
int pipes[2];
char x;

	pipe(pipes);
	if (!fork())
		{
		close(pipes[0]);
		signal(SIGCHLD,SIG_DFL);
		if (!fork())
			return 0;
		wait(NULL);
		_exit(0);
		}
	close(pipes[1]);
	read(pipes[0],&x,1);
	close(pipes[0]);
	return 1;
}

#endif

/* move a fd to a place >= 10 */

int movefd(int fd)
{
int fe;

	if (fd == -1)
		return fd;
	if ((fe = dup(fd)) < 10)
		fe = movefd(fe);
	close(fd);
	return fe;
}

/* move fd x to y */

void redup(int x,int y)
{
	if (x != y)
		{
		dup2(x,y);
		close(x);
		}
}

void settrap(char *s,int empty)
{
int t0;

	if (strncmp(s,"TRAP",4))
		return;
	for (t0 = 0; t0 != SIGCOUNT+2; t0++)
		if (!strcmp(s+4,sigs[t0]))
			{
			if (jobbing && (t0 == SIGTTOU || t0 == SIGTSTP || t0 == SIGTTIN
					|| t0 == SIGPIPE))
				{
				zerr("can't trap SIG%s in interactive shells",s);
				return;
				}
			if (empty)
				{
				sigtrapped[t0] = 2;
				if (t0 && t0 < SIGCOUNT && t0 != SIGCHLD)
					{
					signal(t0,SIG_IGN);
					sigtrapped[t0] = 2;
					}
				}
			else
				{
				if (t0 && t0 < SIGCOUNT && t0 != SIGCHLD)
					signal(t0,handler);
				sigtrapped[t0] = 1;
				}
			return;
			}
	zerr("warning: undefined signal name: SIG%s",s+4);
}

void unsettrap(char *s)
{
int t0;

	if (strncmp(s,"TRAP",4))
		return;
	for (t0 = 0; t0 != SIGCOUNT+2; t0++)
		if (!strcmp(s+4,sigs[t0]))
			{
			if (jobbing && (t0 == SIGTTOU || t0 == SIGTSTP || t0 == SIGTTIN
					|| t0 == SIGPIPE))
				{
				zerr("can't trap SIG%s in interactive shells",s);
				return;
				}
			sigtrapped[t0] = 0;
			if (t0 == SIGINT)
				intr();
			else if (t0 && t0 < SIGCOUNT && t0 != SIGCHLD)
				signal(t0,SIG_DFL);
			return;
			}
	zerr("warning: undefined signal name: SIG%s",s+4);
}

void dotrap(int sig)
{
char buf[16];
list l;
int sav;

	sav = sigtrapped[sig];
	if (sav == 2)
		return;
	sigtrapped[sig] = 2;
	sprintf(buf,"TRAP%s",sigs[sig]);
	if (l = gethnode(buf,shfunchtab))
		newrunlist(l);
	sigtrapped[sig] = sav;
}

/* get the text corresponding to a command */

char *gettext(comm comm)
{
char *s,*t;

	switch(comm->type)
		{
		case SIMPLE:
			return getsimptext(comm);
		case SUBSH:
			t = getltext(comm->left);
			s = tricat("(",t,")");
			free(t);
			return s;
		case CURSH:
		case SHFUNC:
			t = getltext(comm->left);
			s = tricat("{",t,"}");
			free(t);
			return s;
		case CFOR:
			return getfortext((struct fornode *) comm->info,comm);
		case CWHILE:
			return getwhiletext((struct whilenode *) comm->info);
		case CREPEAT:
			return getrepeattext((struct repeatnode *) comm->info);
		case CIF:
			return getiftext((struct ifnode *) comm->info);
		case CCASE:
			return getcasetext((struct casenode *) comm->info,comm->args);
		case CSELECT:
			return getfortext((struct fornode *) comm->info,comm);
		default:
			return strdup("(...)");
		}
	return NULL;
}

char *getltext(list l)
{
char *s,*t,*u;

	s = getl2text(l->left);
	if (l->type == ASYNC)
		{
		t = dyncat(s," &");
		free(s);
		s = t;
		}
	if (!l->right)
		return s;
	t = getltext(l->right);
	u = tricat(s,(l->type == SYNC) ? "\n" : "",t);
	free(s);
	free(t);
	return u;
}

char *getl2text(list2 l)
{
char *s,*t,*u;

	s = getptext(l->left);
	if (!l->right)
		return s;
	t = getl2text(l->right);
	u = tricat(s,(l->type == ORNEXT) ? " || " : " && ",t);
	free(s);
	free(t);
	return u;
}

char *getptext(pline p)
{
char *s,*t,*u;

	s = gettext(p->left);
	if (!p->right)
		return s;
	t = getptext(p->right);
	u = tricat(s," | ",t);
	free(s);
	free(t);
	return u;
}

char *getsimptext(comm comm)
{
int len = 0;
Node n;
char *fstr[] = {
	">",">!",">>",">>!","<","<<","<&",">&",">&-"
	};
struct fnode *f;
char *s,*t,u = '=';

	for (n = comm->args->first; n; n = n->next)
		len += strlen(n->dat)+1;
	if (comm->vars)
		for (n = comm->vars->first; n; n = n->next)
			len += strlen(n->dat)+1;
	for (n = comm->redir->first; n; n = n->next)
		{
		f = n->dat;
		switch(f->type)
			{
			case WRITE: case WRITENOW: case APP: case APPNOW: case READ:
				len += strlen(fstr[f->type])+strlen(f->u.name)+8;
				break;
			case HEREDOC: case MERGE: case MERGEOUT:
				len += strlen(fstr[f->type])+10;
				break;
			case CLOSE:
				len += 10;
				break;
			case INPIPE:
			case OUTPIPE:
				len += strlen(f->u.name)+10;
				break;
			}
		}
	len += strlen(comm->cmd);
	s = t = zalloc(len+5);
	if (comm->vars)
		for (n = comm->vars->first; n; n = n->next)
			{
			strucpy(&t,n->dat);
			*t++ = u;
			u = ('='+' ')-u;
			}
	strucpy(&t,comm->cmd);
	*t++ = ' ';
	for (n = comm->args->first; n; n = n->next)
		{
		strucpy(&t,n->dat);
		*t++ = ' ';
		}
	for (n = comm->redir->first; n; n = n->next)
		{
		f = n->dat;
		switch(f->type)
			{
			case WRITE: case WRITENOW: case APP: case APPNOW: case READ:
				if (f->fd1 != ((f->type == READ) ? 0 : 1))
					*t++ = '0'+f->fd1;
				strucpy(&t,fstr[f->type]);
				*t++ = ' ';
				strucpy(&t,f->u.name);
				*t++ = ' ';
				break;
			case HEREDOC: case MERGE: case MERGEOUT:
				if (f->fd1 != ((f->type == MERGEOUT) ? 1 : 0))
					*t++ = '0'+f->fd1;
				strucpy(&t,fstr[f->type]);
				*t++ = ' ';
				sprintf(t,"%d ",f->u.fd2);
				t += strlen(t);
				break;
			case CLOSE:
				*t++ = '0'+f->fd1;
				strucpy(&t,">&- ");
				break;
			case INPIPE:
			case OUTPIPE:
				if (f->fd1 != ((f->type == INPIPE) ? 0 : 1))
					*t++ = '0'+f->fd1;
				strucpy(&t,(f->type == INPIPE) ? "< " : "> ");
				strucpy(&t,f->u.name);
				strucpy(&t," ");
				len += strlen(f->u.name)+6;
				break;
			}
		}
	t[-1] = '\0';
	return s;
}

char *getfortext(struct fornode *n,comm comm)
{
char *s,*t,*u,*v;

	s = getltext(n->list);
	u = dyncat((comm->type == CFOR) ? "for " : "select ",n->name);
	if (comm->args)
		{
		t = makehlist(comm->args,0);
		v = tricat(u," in ",t);
		free(u);
		free(t);
		u = v;
		}
	v = dyncat(u,"\n do ");
	free(u);
	u = tricat(v,s,"\n done");
	free(v);
	free(s);
	return u;
}

char *getwhiletext(struct whilenode *n)
{
char *s,*t,*u,*v;

	t = getltext(n->cont);
	v = getltext(n->loop);
	s = tricat((!n->cond) ? "while " : "until ",t,"\n do ");
	u = tricat(s,v,"\n done");
	free(s);
	free(t);
	free(v);
	return u;
}

char *getrepeattext(struct repeatnode *n)
{
char buf[100];
char *s,*t,*u;

	s = getltext(n->list);
	sprintf(buf,"%d",n->count);
	t = tricat("repeat ",buf," do ");
	u = tricat(t,s,"\n done");
	free(s);
	free(t);
	return u;
}

char *getiftext(struct ifnode *n)
{
int fst = 1;
char *v = strdup("");
char *s,*t,*u,*w;

	while (n && n->ifl)
		{
		s = getltext(n->ifl);
		t = getltext(n->thenl);
		u = tricat((fst) ? "if " : "elif ",s,"\n then ");
		w = tricat(u,t,"\n ");
		free(s);
		free(t);
		free(u);
		s = dyncat(v,w);
		free(v);
		free(w);
		v = s;
		fst = 0;
		n = n->next;
		}
	if (n)
		{
		s = getltext(n->thenl);
		t = tricat(v,"else ",s);
		u = dyncat(t,"\n fi");
		free(s);
		free(t);
		free(v);
		return u;
		}
	u = dyncat(v,"\n fi");
	free(v);
	return u;
}

char *getcasetext(struct casenode *n,table l)
{
char *s,*t,*u,*v;

	s = tricat("case ",l->first->dat," in ");
	while (n)
		{
		u = getltext(n->list);
		t = tricat(n->pat,") ",u);
		v = tricat(s,t," ;;\n");
		free(s);
		free(t);
		free(u);
		s = v;
		n = n->next;
		}
	t = dyncat(s,"esac\n");
	free(s);
	return t;
}

/* copy t into *s and update s */

void strucpy(char **s,char *t)
{
char *u = *s;

	while (*u++ = *t++);
	*s = u-1;
}

void checkrmall(void)
{
	fflush(stdin);
	fprintf(stderr,"zsh: are you sure you want to delete "
		"all the files? ");
	fflush(stderr);
	ding();
	errflag |= !getquery();
}

int getquery(void)
{
char c;
int yes = 0;

	setcbreak();
	if (read(SHTTY,&c,1) == 1 && (c == 'y' || c == 'Y'))
		yes = 1;
	unsetcbreak();
	if (c != '\n')
		write(2,"\n",1);
	return yes;
}

static int d;
static char *guess,*best;

void spscan(char *s,char *junk)
{
int nd;

	nd = spdist(s,guess);
	if (nd <= d && nd != 3)
		{
		best = s;
		d = nd;
		}
}

/* spellcheck a command */

void spckcmd(char **s)
{
char *t;

	if (gethnode(*s,chtab) || gethnode(*s,shfunchtab))
		return;
	for (t = *s; *t; t++)
		if (*t == '/')
			return;
	if (access(*s,F_OK) == 0)
		return;
	best = NULL;
	guess = *s;
	d = 3;
	listhtable(chtab,spscan);
	listhtable(shfunchtab,spscan);
	if (best)
		{
		fprintf(stderr,"zsh: correct to `%s' (y/n)? ",best);
		fflush(stderr);
		ding();
		if (getquery())
			{
			free(*s);
			*s = strdup(best);
			}
		}
}

void addlocal(char *s)
{
	if (locallist)
		addnode(locallist,strdup(s));
}

/* perform pathname substitution on a PATH or CDPATH component */

void pathsub(char **s)
{
	if (**s == '=')
		**s = Equals;
	if (**s == '~')
		**s = Tilde;
	filesub((void **) s);
}

#ifndef STRFTIME
int strftime(char *buf,int bufsize,char *fmt,struct tm *tm)
{
char *astr[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
char *estr[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul",
	"Aug","Sep","Oct","Nov","Dec"};
char *lstr[] = {"12"," 1"," 2"," 3"," 4"," 5"," 6"," 7"," 8"," 9","10","11"};

	while (*fmt)
		if (*fmt == '%')
			{
			fmt++;
			switch(*fmt++)
				{
				case 'a':
					strucpy(&buf,astr[tm->tm_wday]);
					break;
				case 'b':
					strucpy(&buf,estr[tm->tm_mon]);
					break;
				case 'd':
					*buf++ = '0'+tm->tm_mday/10;
					*buf++ = '0'+tm->tm_mday%10;
					break;
				case 'e':
					*buf++ = (tm->tm_mday > 9) ? '0'+tm->tm_mday/10 : ' ';
					*buf++ = '0'+tm->tm_mday%10;
					break;
				case 'k':
					*buf++ = (tm->tm_hour > 9) ? '0'+tm->tm_hour/10 : ' ';
					*buf++ = '0'+tm->tm_hour%10;
					break;
				case 'l':
					strucpy(&buf,lstr[tm->tm_hour%12]);
					break;
				case 'm':
					*buf++ = '0'+tm->tm_mon/10;
					*buf++ = '0'+tm->tm_mon%10;
					break;
				case 'M':
					*buf++ = '0'+tm->tm_min/10;
					*buf++ = '0'+tm->tm_min%10;
					break;
				case 'p':
					*buf++ = (tm->tm_hour > 11) ? 'P' : 'A';
					*buf++ = 'M';
					break;
				case 'S':
					*buf++ = '0'+tm->tm_sec/10;
					*buf++ = '0'+tm->tm_sec%10;
					break;
				case 'y':
					*buf++ = '0'+tm->tm_year/10;
					*buf++ = '0'+tm->tm_year%10;
					break;
				default:
					exit(20);
				}
			}
		else
			*buf++ = *fmt++;
	*buf = '\0';
	return 0;
}
#endif

#ifndef PUTENV
int putenv(char *a)
{
char *b = a;

	while (*a++ != '=');
	a[-1] = '\0';
	setenv(strdup(b),strdup(a));
	free(b);
	return 0;
}
#endif

int ppcount(void)
{
Node n;
int val = -1;

	for (n = pparms->first; n; n = n->next,val++);
	return val;
}

