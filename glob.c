/*

	glob.c - filename generation and brace expansion

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
#ifndef INT_MAX
#include <limits.h>
#endif
#include <sys/dir.h>
#include <sys/errno.h>

#define exists(X) (access(X,0) == 0)
#define magicerr() { if (magic) putc('\n',stderr); errflag = 1; }

static int gtype;	/* file type for (X) */
static int mode;	/* != 0 if we are parsing glob patterns */
static int sense; /* (X) or (^X) */
static int pathpos;	/* position in pathbuf */
static int matchsz;	/* size of matchbuf */
static int matchct;	/* number of matches found */
static char pathbuf[MAXPATHLEN];	/* pathname buffer */
static char **matchbuf;		/* array of matches */
static char **matchptr;		/* &matchbuf[matchct] */

/* pathname component in filename patterns */

/* qath is a struct xpath * */

struct xpath {
	qath next;
	comp comp;
	int closure;	/* 1 if this is a (foo/)# */
	};
struct xcomp {
	comp nx1,nx2;
	char *str;
	};

void glob(table list,Node *np)
{
Node node = (*np)->last;	/* the node before this one */
Node next = (*np)->next;	/* the node after this one */
int sl;		/* length of the pattern */
char *ostr;	/* the pattern before the parser chops it up */
char *wd;	/* the cwd */
qath q;		/* pattern after parsing */
char *str = (*np)->dat;	/* the pattern */

	sl = strlen(str);
	ostr = strdup(str);
	remnode(list,*np);
	gtype = 0;
	if (str[sl-1] == Outpar)	/* check for (X) or (^X) */
		{
		if (sl > 4 && str[sl-4] == Inpar && str[sl-3] == Hat)
			sense = 1;
		else if (sl > 3 && str[sl-3] == Inpar)
			sense = 0;
		else
			sense = 2;
		if (sense != 2)
			{
			str[sl-3-sense] = '\0';
			switch (str[sl-2])
				{
				case '@': gtype = S_IFLNK; break;
				case '=': gtype = S_IFSOCK; break;
				case Inang: gtype = S_IFIFO; break;
				case '/': gtype = S_IFDIR; break;
				case '.': gtype = S_IFREG; break;
				case Star:  gtype = 0100; break;
				case '%': gtype = S_IFCHR; break;
				case 'R': gtype = 0004; break;
				case 'W': gtype = 0002; break;
				case 'X': gtype = 0001; break;
				case 'r': gtype = 0400; break;
				case 'w': gtype = 0200; break;
				case 'x': gtype = 0100; break;
				default:
					magicerr();
					zerr("unknown file attribute");
					return;
				};
			}
		}
	else if (str[sl-1] == '/')		/* foo/ == foo(/) */
		{
		gtype = S_IFDIR;
		sense = 0;
		}
	if (*str == '/')	/* pattern has absolute path */
		{
		wd = zgetwd();
		str++;
		chdir("/");
		pathbuf[0] = '/';
		pathbuf[pathpos = 1] = '\0';
		}
	else		/* pattern is relative to cwd */
		{
		wd = NULL;
		pathbuf[pathpos = 0] = '\0';
		}
	q = parsepat(str);
	if (!q || errflag)	/* if parsing failed */
		{
		if (isset(NOBADPATTERN))
			{
			insnode(list,node,ostr);
			return;
			}
		magicerr();
		zerr("bad pattern: %s",ostr);
		free(ostr);
		return;
		}
	matchptr = matchbuf = (char **) zalloc((matchsz = 16)*sizeof(char *));
	matchct = 0;
	scanner(q);		/* do the globbing */
	freepath(q);
	if (wd)		/* reset cwd */
		{
		chdir(wd);
		free(wd);
		}
	if (!matchct && unset(NULLGLOB))
		if (unset(NONOMATCH))
			{
			if (!errflag)
				{
				magicerr();
				zerr("no matches found: %s",ostr);
				}
			free(matchbuf);
			free(ostr);
			errflag = 1;
			return;
			}
		else
			{
			*matchptr++ = strdup(ostr);
			matchct = 1;
			}
	qsort(&matchbuf[0],matchct,sizeof(char *),notstrcmp);
	matchptr = matchbuf;
	while (matchct--)			/* insert matches in the arg list */
		insnode(list,node,*matchptr++);
	free(matchbuf);
	free(ostr);
	if (magic)
		magic = 2;	/* tell readline we did something */
	*np = (next) ? next->last : list->last;
}

int notstrcmp(char **a,char **b)
{
char *c = *b,*d = *a;

	for (; *c == *d && *c; c++,d++);
	return ((int) (unsigned char) *c-(int) (unsigned char) *d);
}

/* add a match to the list */

void insert(char *s)
{
struct stat buf;

	if (isset(MARKDIRS) && !lstat(s,&buf) && S_ISDIR(buf.st_mode)) /* grrr */
		{
		char *t;
		int ll = strlen(s);

		t = zalloc(ll+2);
		strcpy(t,s);
		t[ll] = '/';
		t[ll+1] = '\0';
		free(s);
		s = t;
		}
	*matchptr++ = s;
	if (++matchct == matchsz)
		{
		matchbuf = (char **) realloc(matchbuf,sizeof(char **)*(matchsz *= 2));
		matchptr = matchbuf+matchct;
		}
}

/* check to see if str is eligible for filename generation */

int haswilds(char *str)	
{
	if (!str[1] && (*str == Inbrack || *str == Outbrack))
		return 0;
	if (str[0] == '%')
		return 0;
	for (; *str; str++)
		if (!strncmp(str,"..../",5))
			return 1;
		else if (*str == Pound || *str == Hat || *str == Star ||
				*str == Bar || *str == Inbrack || *str == Inang ||
				*str == Quest)
			return 1;
	return 0;
}

/* check to see if str is eligible for brace expansion */

int hasbraces(char *str)
{
int mb,bc,cmct1,cmct2;
char *lbr = NULL;

	if (str[0] == Inbrace && str[1] == Outbrace)
		return 0;
	for (mb = bc = cmct1 = cmct2 = 0; *str; str++)
		{
		if (*str == Inbrace)
			{
			if (!bc)
				lbr = str;
			bc++;
			if (str[4] == Outbrace && str[2] == '-') /* {a-z} */
				{
				cmct1++;
				if (bc == 1)
					cmct2++;
				}
			}
		else if (*str == Outbrace)
			{
			bc--;
			if (!bc && !cmct2)
				{
				*lbr = '{';
				*str = '}';
				}
			cmct2 = 0;
			}
		else if (*str == Comma && bc)
			{
			cmct1++;
			if (bc == 1)
				cmct2++;
			}
		if (bc > mb)
			mb = bc;
		if (bc < 0)
			return 0;
		}
	return (mb && bc == 0 && cmct1);
}

/* expand stuff like >>*.c */

int xpandredir(struct fnode *fn,table tab)
{
table fake;
char *nam;
struct fnode *ff;
int ret = 0;

	fake = newtable();
	addnode(fake,fn->u.name);
	prefork(fake);
	if (!errflag)
		postfork(fake,GLOB);
	if (errflag)
		{
		freetable(fake,freestr);
		return 0;
		}
	if (fake->first && !fake->first->next)
		{
		fn->u.name = fake->first->dat;
		untokenize(fn->u.name);
		}
	else
		while (nam = getnode(fake))
			{
			ff = alloc(sizeof(struct fnode));
			ff->u.name = nam;
			ff->type = fn->type;
			addnode(tab,ff);
			ret = 1;
			}
	free(fake);
	return ret;
}

/* concatenate s1 and s2 in dynamically allocated buffer */

char *dyncat(char *s1,char *s2)
{
char *ptr;
 
	ptr = zalloc(strlen(s1)+strlen(s2)+1);
	strcpy(ptr,s1);
	strcat(ptr,s2);
	return ptr;
}

/* concatenate s1, s2, and s3 in dynamically allocated buffer */

char *tricat(char *s1,char *s2,char *s3)
{
char *ptr;

	ptr = zalloc(strlen(s1)+strlen(s2)+strlen(s3)+1);
	strcpy(ptr,s1);
	strcat(ptr,s2);
	strcat(ptr,s3);
	return ptr;
}

/* brace expansion */

void xpandbraces(table list,Node *np)
{
Node node = (*np),last = node->last;
char *str = node->dat,*str3 = str,*str2;
int prev;

	if (magic)
		magic = 2;
	for (; *str != Inbrace; str++);
 	if (str[2] == '-' && str[4] == Outbrace)	 /* {a-z} */
		{
		char c1,c2;

		remnode(list,node);
		chuck(str);
		c1 = *str;
		chuck(str);
		chuck(str);
		c2 = *str;
		chuck(str);
		if (istok(c1))
			c1 = tokens[c1-Pound];
		if (istok(c2))
			c2 = tokens[c2-Pound];
		if (c1 < c2)
			for (; c2 >= c1; c2--)	/* {a-z} */
				{
				*str = c2;
				insnode(list,last,strdup(str3));
				}
		else
			for (; c2 <= c1; c2++)	/* {z-a} */
				{
				*str = c2;
				insnode(list,last,strdup(str3));
				}
		free(str3);
		*np = last->next;
		return;
		}
	prev = str-str3;
	str2 = getparen(str++);
	if (!str2)
		{
		errflag = 1;
		zerr("how did you get this error?");
		return;
		}
	remnode(list,node);
	node = last;
	FOREVER
		{
		char *zz,*str4;
		int cnt;
		
		for (str4 = str, cnt = 0; cnt || *str != Comma && *str !=
				Outbrace; str++)
			if (*str == Inbrace)
				cnt++;
			else if (*str == Outbrace)
				cnt--;
			else if (!*str)
				exit(10);
		zz = zalloc(prev+(str-str4)+strlen(str2)+1);
		strncpy(zz,str3,prev);
		zz[prev] = '\0';
		strncat(zz,str4,str-str4);
		strcat(zz,str2);
		insnode(list,node,zz);
		node = node->next;
		if (*str != Outbrace)
			str++;
		else
			break;
		}
	free(str3);
	*np = last->next;
}

/* get closing paren, given pointer to opening paren */

char *getparen(char *str)
{
int cnt = 1;
char typein = *str++,typeout = typein+1;

	for (; *str && cnt; str++)
		if (*str == typein)
			cnt++;
		else if (*str == typeout)
			cnt--;
	if (!str && cnt)
		return NULL;
	return str;
}

/* check to see if a matches b (b is not a filename pattern) */

int matchpat(char *a,char *b)
{
comp c;
int val;

	c = parsereg(b);
	if (!c)
		{
		zerr("bad pattern: %s");
		errflag = 1;
		return NULL;
		}
	val = doesmatch(a,c,0);
	freecomp(c);
	return val;
}

/* do the ${foo%%bar}, ${foo#bar} stuff */
/* please do not laugh at this code. */

void getmatch(char **sp,char *pat,int dd)
{
comp c;
char *t,*lng = NULL,cc,*s = *sp;

	c = parsereg(pat);
	if (!c)
		{
		magicerr();
		zerr("bad pattern: %s",pat);
		return;
		}
	if (!(dd & 2))
		{
		for (t = s; t==s || t[-1]; t++)
			{
			cc = *t;
			*t = '\0';
			if (doesmatch(s,c,0))
				{
				if (!(dd & 1))
					{
					*t = cc;
					t = strdup(t);
					free(s);
					freecomp(c);
					*sp = t;
					return;
					}
				lng = t;
				}
			*t = cc;
			}
		if (lng)
			{
			t = strdup(lng);
			free(s);
			freecomp(c);
			*sp = t;
			return;
			}
		}
	else
		{
		for (t = s+strlen(s); t >= s; t--)
			{
			if (doesmatch(t,c,0))
				{
				if (!(dd & 1))
					{
					*t = '\0';
					freecomp(c);
					return;
					}
				lng = t;
				}
			}
		if (lng)
			{
			*lng = '\0';
			freecomp(c);
			return;
			}
		}
	freecomp(c);
}

/* add a component to pathbuf */

void addpath(char *s)
{
	while (pathbuf[pathpos++] = *s++);
	pathbuf[pathpos-1] = '/';
	pathbuf[pathpos] = '\0';
}

/* do the globbing */

void scanner(qath q)
{
comp c;

	if (q->closure)			/* (foo/)# */
		if (q->closure == 2)		/* (foo/)## */
			q->closure = 1;
		else
			scanner(q->next);
	if (c = q->comp)
		{
		if (!(c->nx1 || c->nx2) && !haswilds(c->str))
			if (q->next)
				{
				char *wd = NULL;
				
				if (errflag)
					return;
				if (islink(c->str) || !strcmp(c->str,".."))
					wd = zgetwd();
				if (!chdir(c->str))
					{
					int oppos = pathpos;
					
					addpath(c->str);
					scanner((q->closure) ? q : q->next);
					if (wd)
						chdir(wd);
					else if (strcmp(c->str,"."))
						chdir("..");
					pathbuf[pathpos = oppos] = '\0';
					}
				else
					{
					magicerr();
					zerr("%e: %s",errno,c->str);
					if (wd)
						chdir(wd);
					else if (strcmp(c->str,"."))
						chdir("..");
					return;
					}
				}
			else
				{
				if (exists(c->str))
					insert(dyncat(pathbuf,c->str));
				}
		else
			{
			char *fn;
			int type,type3,dirs = !!q->next;
			struct direct *de;
			DIR *lock = opendir(".");
			static struct stat buf;
			 
			if (lock == NULL)
				{
				magicerr();
				if (errno != EINTR)
					zerr("%e: %s",errno,pathbuf);
				return;
				}
			readdir(lock); readdir(lock); 	/* skip . and .. */
			while (de = readdir(lock))
				{
				if (errflag)
					break;
				fn = &de->d_name[0];
				if (dirs)
					{
					if (lstat(fn,&buf) == -1)
						{
						magicerr();
						zerr("%e: %s",errno,fn);
						}
					type3 = buf.st_mode & S_IFMT;
					if (type3 != S_IFDIR)
						continue;
					}
				else
					if (gtype)	/* do the (X) (^X) stuff */
						{
						if (lstat(fn,&buf) == -1)
							{
							if (errno != ENOENT)
								{
								magicerr();
								zerr("%e: %s",errno,fn);
								}
							continue;
							}
						type3 = (type = buf.st_mode) & S_IFMT;
						if (gtype & 0777)
							{
							if ((!(type & gtype) ^ sense) || type3 == S_IFLNK)
								continue;
							}
						else if (gtype == S_IFCHR)
							{
							if ((type3 != S_IFCHR && type3 != S_IFBLK) ^ sense)
								continue;
							}
						else if ((gtype != type3) ^ sense)
							continue;
						}
				if (doesmatch(fn,c,unset(GLOBDOTS)))
					if (dirs)
						{
						if (!chdir(fn))
							{
							int oppos = pathpos;
					
							addpath(fn);
							scanner((q->closure) ? q : q->next); /* scan next level */
							chdir("..");
							pathbuf[pathpos = oppos] = '\0';
							}
						}
					else
						insert(dyncat(pathbuf,fn));
				}
			closedir(lock);
			}
		}
	else
		{
		zerr("no idea how you got this error message.");
		errflag = 1;
		}
}

/* do the [..(foo)..] business */

int minimatch(char **pat,char **str)
{
char *pt = *pat+1,*s = *str;
	
	for (; *pt != Outpar; s++,pt++)
		if ((*pt != Quest || !*s) && *pt != *s)
			{
			*pat = getparen(*pat)-1;
			return 0;
			}
	*str = s-1;
	return 1;
}

/* see if str matches c; first means worry about matching . explicitly */

int doesmatch(char *str,comp c,int first)
{
char *pat = c->str;

	FOREVER
		{
		if (!*pat)
			{
			if (errflag)
				return 0;
			if (!(*str || c->nx1 || c->nx2))
				return 1;
			return (c->nx1 && doesmatch(str,c->nx1,first)) ||
				(c->nx2 && doesmatch(str,c->nx2,first));
			}
		if (first && *str == '.' && *pat != '.')
			return 0;
		if (*pat == Star)	/* final * is not expanded to ?#; returns success */
			return 1;
		first = 0;
		if (*pat == Quest && *str)
			{
			str++;
			pat++;
			continue;
			}
		if (*pat == Hat)
			return 1-doesmatch(str,c->nx1,first);
		if (*pat == Inbrack)
			if (pat[1] == Hat)
				{
				for (pat += 2; *pat != Outbrack && *pat; pat++)
					if (*pat == '-' && pat[-1] != Hat && pat[1] != Outbrack)
						{
						if (pat[-1] <= *str && pat[1] >= *str)
							break;
						}
					else if (*str == *pat)
						break;
				if (!*pat)
					{
					zerr("something is very wrong.");
					return 0;
					}
				if (*pat != Outbrack)
					break;
				pat++;
				str++;
				continue;
				}
			else
				{
				for (pat++; *pat != Outbrack && *pat; pat++)
					if (*pat == Inpar)
						{
						if (minimatch(&pat,&str))
							break;
						}
					else if (*pat == '-' && pat[-1] != Inbrack && pat[1] != Outbrack)
						{
						if (pat[-1] <= *str && pat[1] >= *str)
							break;
						}
					else if (*str == *pat)
						break;
				if (!pat || !*pat)
					{
					zerr("oh dear.  that CAN'T be right.");
					return 0;
					}
				if (*pat == Outbrack)
					break;
				for (str++; *pat != Outbrack; pat++);
				pat++;
				continue;
				}
		if (*pat == Inang)
			{
			int t1,t2,t3;
			char *ptr;

			if (*++pat == Outang)	/* handle <> case */
				{
				(void) zstrtol(str,&ptr,10);
				if (ptr == str)
					break;
				str = ptr;
				pat++;
				}
			else
				{
				t1 = zstrtol(str,&ptr,10);
				if (ptr == str)
					break;
				str = ptr;
				t2 = zstrtol(pat,&ptr,10);
				if (*ptr != '-')
					exit(31);
				t3 = zstrtol(ptr+1,&pat,10);
				if (!t3)
					t3 = INT_MAX;
				if (*pat++ != Outang)
					exit(21);
				if (t1 < t2 || t1 > t3)
					break;
				}
			continue;
			}
		if (*str == *pat)
			{
			str++;
			pat++;
			continue;
			}
		break;
		}
	return 0;
}

static char *pptr;

qath parsepat(char *str)
{
	mode = 0;
	pptr = str;
	return parseqath();
}

comp parsereg(char *str)
{
	mode = 1;
	pptr = str;
	return parsecompsw();
}

qath parseqath(void)
{
comp c1;
qath p1;

	if (pptr[0] == '.' && pptr[1] == '.' && pptr[2] == '.' && pptr[3] ==
			'.' && pptr[4] == '/')
		{
		pptr[0] = Inpar;
		pptr[1] = Star;
		pptr[2] = '/';
		pptr[3] = Outpar;
		pptr[4] = Pound;	/* "..../" -> "( * /)#" */
		}
	if (*pptr == Inpar)
		{
		char *str;
		int pars = 1;

		for (str = pptr+1; *str && pars; str++)
			if (*str == Inpar)
				pars++;
			else if (*str == Outpar)
				pars--;
		if (str[0] != Pound || str[-1] != Outpar || str[-2] != '/')
			goto kludge;
		pptr++;
		if (!(c1 = parsecompsw()))
			return NULL;
		if (pptr[0] == '/' && pptr[1] == Outpar && pptr[2] == Pound)
			{
			int pdflag = 0;

			pptr += 3;
			if (*pptr == Pound)
				{
				pdflag = 1;
				pptr++;
				}
			p1 = (qath) alloc(sizeof(struct xpath));
			p1->comp = c1;
			p1->closure = 1+pdflag;
			p1->next = parseqath();
			return (p1->comp) ? p1 : NULL;
			}
		}
	else
		{
kludge:
		if (!(c1 = parsecompsw()))
			return NULL;
		if (*pptr == '/' || !*pptr)
			{
			int ef = *pptr == '/';

			p1 = (qath) alloc(sizeof(struct xpath));
			p1->comp = c1;
			p1->closure = 0;
			p1->next = (*pptr == '/') ? (pptr++,parseqath()) : NULL;
			return (ef && !p1->next) ? NULL : p1;
			}
		}
	magicerr();
	errflag = 1;
	return NULL;
}

comp parsecomp(void)
{
comp c = (comp) alloc(sizeof(struct xcomp)),c1,c2;
char *s = c->str = alloc(MAXPATHLEN*2),*ls = NULL;

	c->nx2 = NULL;
	while (*pptr && (mode || *pptr != '/') && *pptr != Bar &&
			*pptr != Outpar)
		{
		if (*pptr == Hat)
			{
			*s++ = Hat;
			*s++ = '\0';
			pptr++;
			if (!(c->nx1 = parsecomp()))
				{
				free(c->str);
				free(c);
				return NULL;
				}
			return c;
			}
		if (*pptr == Star && pptr[1] && (mode || pptr[1] != '/'))
			{
			*s++ = '\0';
			pptr++;
			c->nx1 = c1 = (comp) alloc(sizeof(struct xcomp));
			*((c1->nx1 = c1)->str = strdup("?")) = Quest;
			c->nx2 = c1->nx2 = parsecomp();
			return (c->nx2) ? c : NULL;
			}
		if (*pptr == Inpar)
			{
			*s++ = '\0';
			pptr++;
			c->nx1 = c1 = parsecompsw();
			if (*pptr != Outpar)
				{
				errflag = 1;
				free(c);
				free(c->str);
				return NULL;
				}
			pptr++;
			if (*pptr == Pound)
				{
				int dpnd = 0;

				pptr++;
				if (*pptr == Pound)
					{
					pptr++;
					dpnd = 1;
					}
				c2 = parsecomp();
				if (dpnd)
					c->nx2 = NULL;
				else
					c->nx2 = c2;
				if (!c2)
					{
					free(c);
					free(c->str);
					return NULL;
					}
				adjustcomp(c1,c2,c1);
				return c;
				}
			c2 = parsecomp();
			if (!c2)
				{
				free(c);
				free(c->str);
				return NULL;
				}
			adjustcomp(c1,c2,NULL);
			return c;
			}
		if (*pptr == Pound)
			{
			int dpnd = 0;

			*s = '\0';
			pptr++;
			if (*pptr == Pound)
				{
				pptr++;
				dpnd = 1;
				}
			if (!ls)
				return NULL;
			c->nx1 = c1 = (comp) alloc(sizeof(struct xcomp));
			(c1->nx2 = c1)->str = strdup(ls);
			c->nx2 = c1->nx1 = parsecomp();
			if (!c->nx2)
				{
				free(c);
				free(c->str);
				return NULL;
				}
			if (dpnd)
				c->nx2 = NULL;
			*ls++ = '\0';
			return c;
			}
		ls = s;
		if (*pptr == Inang)
			{
			int dshct;

			dshct = (pptr[1] == Outang);
			*s++ = *pptr++;
			while (*pptr && (*s++ = *pptr++) != Outang)
				if (s[-1] == '-')
					dshct++;
				else if (!isdigit(s[-1]))
					break;
			if (s[-1] != Outang || dshct != 1)
				{
				free(c);
				free(c->str);
				return NULL;
				}
			}
		else if (*pptr == Inbrack)
			{
			while (*pptr && (*s++ = *pptr++) != Outbrack);
			if (s[-1] != Outbrack)
				{
				free(c);
				free(c->str);
				return NULL;
				}
			}
		else if (istok(*pptr) && *pptr != Star && *pptr != Quest)
			*s++ = tokens[*pptr++-Pound];
		else
			*s++ = *pptr++;
		}
	*s++ = '\0';
	c->nx1 = NULL;
	return c;
}

comp parsecompsw(void)
{
comp c1,c2,c3;

	c1 = parsecomp();
	if (!c1)
		return NULL;
	if (*pptr == Bar)
		{
		c2 = (comp) alloc(sizeof(struct xcomp));
		pptr++;
		c3 = parsecompsw();
		if (!c3)
			return NULL;
		c2->str = strdup("");
		c2->nx1 = c1;
		c2->nx2 = c3;
		return c2;
		}
	return c1;
}

#define MARKER ((void *) 1L)

void adjustcomp(comp c1,comp c2,comp c3)
{
comp z;

	if (c1->nx1 == c2 && c1->nx2 == c3)
		return;
	if (c1->nx1)
		{
		if ((z = c1->nx1) == MARKER)
			return;
		c1->nx1 = MARKER;
		adjustcomp(z,c2,c3);
		c1->nx1 = z;
		}
	if (c1->nx2)
		adjustcomp(c1->nx2,c2,c3);
	if (!(c1->nx1 || c1->nx2))
		{
		c1->nx1 = c2;
		c1->nx2 = c3;
		}
}

void freepath(qath p)
{
	if (p)
		{
		freepath(p->next);
		freecomp(p->comp);
		free(p);
		}
}

void freecomp(comp c)
{
	if (c && c->str)
		{
		free(c->str);
		c->str = NULL;
		freecomp(c->nx1);
		freecomp(c->nx2);
		free(c);
		}
}

/* tokenize and see if ss matches tt */

int patmatch(char *ss,char *tt)
{
char *s = ss,*t;

	for (; *s; s++)
		if (*s == '\\')
			chuck(s);
		else
			for (t = tokens; *t; t++)
				if (*t == *s)
					{
					*s = (t-tokens)+Pound;
					break;
					}
	return matchpat(ss,tt);
}

/* remove unnecessary Nulargs */

void remnulargs(char *s)
{
int nl = *s;
char *t = s;

	while (*s)
		if (*s == Nularg)
			chuck(s);
		else
			s++;
	if (!*t && nl)
		{
		t[0] = Nularg;
		t[1] = '\0';
		}
}

