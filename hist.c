/*

	hist.c - ! history	

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

int lastc;

/* add a character to the current history word */

void hwaddc(int c)
{
	if (hlastw)
		{
		if (c == EOF || c == HERR)
			return;
		*hlastp++ = c;
		if (hlastp-hlastw == hlastsz)
			{
			hlastw = realloc(hlastw,hlastsz *= 2);
			hlastp = hlastw+(hlastsz/2);
			}
		}
}

#define habort() { errflag = 1; return HERR; }

/* get a character after performing history substitution */

int hgetc(void)
{
int c,ev,farg,larg,argc,marg = -1,cflag = 0,bflag = 0;
char buf[256],*ptr;
table slist,elist;

tailrec:
	c = hgetch();
	if (stophist)
		{
		hwaddc(c);
		return c;
		}
	if (firstch && c == '^' && !(ungots && !magic))
		{
		firstch = 0;
		hungets(strdup(":s^"));
		c = '!';
		goto hatskip;
		}
	if (c != ' ')
		firstch = 0;
	if (c == '\\')
		{
		int g = hgetch();
		
		if (g != '!')
			hungetch(g);
		else
			{
			hwaddc('!');
			return '!';
			}
		}
	if (c != '!' || (ungots && !magic))
		{
		hwaddc(c);
		return c;
		}
hatskip:
	if ((c = hgetch()) == '{')
		{
			bflag = cflag = 1;
			c = hgetch();
			}
		if (c == '\"')
			{
			stophist = 1;
			goto tailrec;
			}
		if (!cflag && znspace(c) || c == '=' || c == '(')
			{
			hungetch(c);
			hwaddc('!');
			return '!';
			}
		cflag = 0;
		ptr = buf;

		/* get event number */

		if (c == '?')
			{
			FOREVER
				{
				c = hgetch();
				if (c == '?' || c == '\n')
					break;
				else
					*ptr++ = c;
				}
			if (c != '\n')
				c = hgetch();
			*ptr = NULL;
			ev = hconsearch(last = strdup(buf),&marg);
			if (ev == -1)
				{
				herrflush();
				zerr("no such event: %s",buf);
				habort();
				}
			}
		else
			{
			int t0;
	 
			FOREVER
				{
				if (znspace(c) || c == ':' || c == '^' || c == '$' || c == '*'
						|| c == '%' || c == '}')
					break;
				if (ptr != buf && c == '-')
					break;
				*ptr++ = c;
				if (c == '#' || c == '!')
					{
					c = hgetch();
					break;
					}
				c = hgetch();
				}
			*ptr = 0;
			if (!*buf)
				ev = dev;
			else if (t0 = atoi(buf))
				ev = (t0 < 0) ? cev+t0 : t0;
			else if (*buf == '!')
				ev = cev-1;
			else if (*buf == '#')
				ev = cev;
			else if ((ev = hcomsearch(buf)) == -1)
				{
				zerr("event not found: %s",buf);
				while (c != '\n')
					c = hgetch();
				habort();
				}
			}

		/* get the event */

		if (!(elist = getevent(dev = ev)))
			habort();

		/* extract the relevant arguments */

		argc = getargc(elist)-1;
		if (c == ':')
			{
			cflag = 1;
			c = hgetch();
			}
		if (c == '*')
			{
			farg = 1;
			larg = argc;
			cflag = 0;
			}
		else
			{
			hungetch(c);
			larg = farg = getargspec(argc,marg);
			if (larg == -2)
				habort();
			if (farg != -1)
				cflag = 0;
			c = hgetch();
			if (c == '*')
				{
				cflag = 0;
				larg = argc;
				}
			else if (c == '-')
				{
				cflag = 0;
				larg = getargspec(argc,marg);
				if (larg == -2)
					habort();
				if (larg == -1)
					larg = argc-1;
				}
			else
				hungetch(c);
			}
		if (farg == -1)
			farg = 0;
		if (larg == -1)
			larg = argc;
		if (!(slist = getargs(elist,farg,larg)))
			habort();

		/* do the modifiers */

		FOREVER
			{
			c = (cflag) ? ':' : hgetch();
			cflag = 0;
			if (c == ':')
				{
				int gbal = 0;
			
				if ((c = hgetch()) == 'g')
					{
					gbal = 1;
					c = hgetch();
					}
				switch(c)
					{
					case 'p':
						hflag = 2;
						break;
					case 'h':
						if (!apply1(gbal,remtpath,slist))
							{
							herrflush();
							zerr("modifier failed: h");
							habort();
							}
						break;
					case 'e':
						if (!apply1(gbal,rembutext,slist))
							{
							herrflush();
							zerr("modifier failed: e");
							habort();
							}
						break;
					case 'r':
						if (!apply1(gbal,remtext,slist))
							{
							herrflush();
							zerr("modifier failed: r");
							habort();
							}
						break;
					case 't':
						if (!apply1(gbal,remlpaths,slist))
							{
							herrflush();
							zerr("modifier failed: t");
							habort();
							}
						break;
					case 's':
						{
						int del;
						char *ptr1,*ptr2;
					
						del = hgetch();
						ptr1 = hdynread(del);
						if (!ptr1)
							habort();
						ptr2 = hdynread2(del);
						if (strlen(ptr1))
							{
							if (last)
								free(last);
							last = ptr1;
							}
						if (rast)
							free(rast);
						rast = ptr2;
						}
					case '&':
						if (last && rast)
							{
							if (subst(gbal,slist,last,rast))
								habort();
							}
						else
							{
							herrflush();
							zerr("no previous substitution with &");
							habort();
							}
						break;
					case 'q':
						apply1(0,quote,slist);
						break;
					case 'x':
						apply1(0,quotebreak,slist);
						break;
					default:
						herrflush();
						zerr("illegal modifier: %c",c);
					habort();
					break;
				}
			}
		else
			{
			if (c != '}' || !bflag)
				hungetch(c);
			if (c != '}' && bflag)
				{
				zerr("'}' expected");
				habort();
				}
			break;
			}
		}
	
	/* stuff the resulting string in the input queue and start over */

	hungets(makehlist(slist,1));
	hflag |= 1;
	goto tailrec;
}

/* begin reading a string */

void strinbeg(void)
{
	strin = 1;
}

/* done reading a string */

void strinend(void)
{
	strin = 0;
	firstch = 1;
	hflag = 0;
	free(ungots);
	ungotptr = ungots = NULL;
	peek = EMPTY;
}

static char *line = NULL,*oline = NULL;

/* stuff a whole FILE into the input queue */

int stuff(char *fn)
{
FILE *in;
char *buf;
int len;

	if (!(in = fopen(fn,"r")))
		{
		zerr("can't open %s",fn);
		return 1;
		}
	fseek(in,0,2);
	len = ftell(in);
	fseek(in,0,0);
	buf = alloc(len+1);
	if (!(fread(buf,len,1,in)))
		{
		zerr("read error on %s",fn);
		fclose(fn);
		free(buf);
		return 1;
		}
	fclose(in);
	if (!line)
		line = oline = buf;
	else
		{
		line = dyncat(line,buf);
		free(oline);
		oline = line;
		}
	return 0;
}

/* get a char without history */

int hgetch(void)
{
char *pmpt = NULL,*s;

	if (ungots)
		{
		if (*ungotptr)
			{
			if (*ungotptr == ALPOP)	/* done expanding an alias,
												pop the alias stack */
				{
				if (!alix)
					{
					ungotptr++;
					return lastc = HERR;
					}
				alstack[--alix]->inuse = 0;
				s = alstack[alix]->text;
				if (*s && s[strlen(s)-1] == ' ')
					alstat = ALSTAT_MORE;
				else
					alstat = ALSTAT_JUNK;
				ungotptr++;
				return lastc = hgetch();
				}
			return lastc = *ungotptr++;
			}
		if (strin)
			return lastc = EOF;
		ungotptr = 0;
		free(ungots);
		ungots = NULL;
		}
kludge:
	if (errflag)
		{
		if (oline)
			free(oline);
		oline = line = NULL;
		return lastc = HERR;
		}
	if (line && *line)
		return lastc = (*line++);
	if (line)
		free(oline);
	if (interact)
		if (!firstln)
			pmpt = putprompt("PROMPT2");
		else
			pmpt = putprompt("PROMPT");
	if (interact && SHTTY == -1)
		write(2,pmpt,strlen(pmpt));
	oline = line = (interact && SHTTY != -1) ? readline(pmpt) : 
		fgets(zalloc(256),256,bshin);
	if (isset(VERBOSE) && line)
		fputs(line,stderr);
	if (!line)
		return lastc = EOF;
	if (line[strlen(line)-1] == '\n')
		lineno++;
	firstch = 1;
	firstln = 0;
	goto kludge;
}

/* unget a character */

void hungetch(int c)
{
static char ubuf2[] = {'x',0};

	if (c == EOF)
		return;
	ubuf2[0] = c;
	hungets(strdup(ubuf2));
}

/* unget a character and remove it from the history word */

void hungetc(int c)
{
	if (hlastw)
		{
		if (hlastw == hlastp)
			zerr("hungetc attempted at buffer start");
		else
			hlastp--;
		}
	hungetch(c);
}

void hflush(void)
{
	if (ungots)
		free(ungots);
	ungots = ungotptr = NULL;
}

/* unget a string into the input queue */

void hungets(char *str)
{
	if (ungots && !*ungotptr)
		{
		free(ungots);
		ungots = NULL;
		}
	if (ungots)
		{
		char *ptr;
		
		ptr = dyncat(str,ungotptr);
		free(ungots);
		free(str);
		ungotptr = ungots = ptr;
		}
	else
		ungots = ungotptr = str;
}

/* initialize the history mechanism */

void hbegin(void)
{
	firstln = firstch = 1;
	histremmed = errflag = hflag = 0;
	stophist = isset(NOBANGHIST);
	if (interact)
		{
		inittty();
		dev = cev++;
		while (cev-tfev >= tevs)
			{
			freetable(getnode(histlist),freestr);
			tfev++;
			}
		addnode(histlist,curtab = newtable());
		}
}

void inittty(void)
{
	attachtty(shpgrp);
	settyinfo(&shttyinfo);
}

/* say we're done using the history mechanism */

int hend(void)
{
char *ptr;
int flag;

	if (!interact)
		return 1;
	if (!curtab)
		return 0;
	flag = hflag;
	hflag = 0;
	if (curtab->first && (*(char *) curtab->last->dat == '\n'))
		free(remnode(curtab,curtab->last));
	if (!curtab->first)
		{
		freetable(remnode(histlist,histlist->last),freestr);
		cev--;
		flag = 0;
		}
	if (flag)
		{
		fprintf(stderr,"%s\n",ptr = makehlist(curtab,0));
		free(ptr);
		}
	curtab = NULL;
	return !(flag & 2 || errflag);
}

/* remove the current line from the history list */

void remhist(void)
{
	if (!interact)
		return;
	if (!histremmed)
		{
		histremmed = 1;
		freetable(remnode(histlist,histlist->last),freestr);
		cev--;
		}
}

/* begin a word */

void hwbegin(void)
{
	if (hlastw)
		free(hlastw);
	hlastw = hlastp = zalloc(hlastsz = 32);
}

/* add a word to the history list */

char *hwadd(void)
{
char *ret = hlastw;

	if (hlastw)
		*hlastp = '\0';
	if (hlastw && lastc != EOF && !errflag)
		if (curtab && !alix/* && alstat != ALSTAT_JUNK*/)
			{
			addnode(curtab,hlastw);
			hlastw = NULL;
			}
	if (alstat == ALSTAT_JUNK)
		alstat = 0;
	return ret;
}

/* get an argument specification */

int getargspec(int argc,int marg)
{
int c,ret = -1;
 
	if ((c = hgetch()) == '0')
		return 0;
	if (isdigit(c))
		{
		ret = 0;
		while (isdigit(c))
			{
			ret = ret*10+c-'0';
			c = hgetch();
			}
		hungetch(c);
		}
	else if (c == '^')
		ret = 1;
	else if (c == '$')
		ret = argc;
	else if (c == '%')
		{
		if (marg == -1)
			{
			herrflush();
			zerr("%% with no previous word matched");
			return -2;
			}
		ret = marg;
		}
	else
		hungetch(c);
	return ret;
}

/* do ?foo? search */

int hconsearch(char *str,int *marg)
{
int t0,t1;
Node node,node2;
 
	if (cev-tfev < 1)
		return -1;
	for (t0 = cev-1,node = histlist->last->last;
			t0 >= tfev; t0--,node = node->last)
		for (t1 = 0,node2 = ((table) node->dat)->first; node2; t1++,node2 =
				node2->next)
			if (strstr(node2->dat,str))
				{
				*marg = t1;
				return t0;
				}
	return -1;
}

/* do !foo search */

int hcomsearch(char *str)
{
int t0;
Node node,node2;
 
	if (cev-tfev < 1)
		return -1;
	for (t0 = cev-1,node = histlist->last->last; t0 >= tfev;
			t0--,node = node->last)
		if ((node2 = ((table) node->dat)->first)->dat &&
				strstr(node2->dat,str))
			return t0;
	return -1;
}

/* apply func to a list */

int apply1(int gflag,int (*func)(void **),table list)
{
Node node;
int flag = 0;
 
	for (node = list->first; node; node = node->next)
		if ((flag |= func(&node->dat)) && !gflag)
			return 1;
	return flag;
}

/* various utilities for : modifiers */

int remtpath(void **junkptr)
{
char *str = *junkptr,*cut;
 
	if (cut = strrchr(str,'/'))
		{
		*cut = NULL;
		return 1;
		}
	return 0;
}
 
int remtext(void **junkptr)
{
char *str = *junkptr,*cut;
 
	if ((cut = strrchr(str,'.')) && cut != str)
		{
		*cut = NULL;
		return 1;
		}
	return 0;
}
 
int rembutext(void **junkptr)
{
char *str = *junkptr,*cut;
 
	if ((cut = strrchr(str,'.')) && cut != str)
		{
		*junkptr = strdup(cut+1);  /* .xx or xx? */
		free(str);
		return 1;
		}
	return 0;
}
 
int remlpaths(void **junkptr)
{
char *str = *junkptr,*cut;
 
	if (cut = strrchr(str,'/'))
		{
		*cut = NULL;
		*junkptr = strdup(cut+1);
		free(str);
		return 1;
		}
	return 0;
}
 
int subst(int gbal,table slist,char *ptr1,char *ptr2)
{
Node node;
int iflag = 0;
 
	for (node = slist->first; node; )
		if (subststr(&node->dat,ptr1,ptr2,gbal))
			{
			iflag = 1;
			if (!gbal)
				return 0;
			}
		else
			node = node->next;
	if (!iflag)
		{
		herrflush();
		zerr("string not found: %s",ptr1);
		return 1;
		}
	return 0;
}
 
int subststr(void **strptr,char *in,char *out,int gbal)
{
char *str = *strptr,*cut,*sptr,*ss;
int ret = 0;

maze:
	if (cut = (char *) strstr(str,in))
		{
		char *incop;
		
		incop = strdup(in);
		*cut = '\0';
		cut += strlen(in);
		ss = *strptr;
		*strptr = tricat(*strptr,sptr = convamps(out,incop),cut);
		free(ss);
		free(sptr);
		free(incop);
		if (gbal)
			{
			str = (char *) *strptr+(cut-str)+strlen(in);
			ret = 1;
			goto maze;
			}
		return 1;
		}
	return ret;
}
 
char *convamps(char *out,char *in)
{
char *ptr,*ret,*pp;
int slen,inlen = strlen(in);
 
	for (ptr = out, slen = 0; *ptr; ptr++,slen++)
		if (*ptr == '\\')
			ptr++;
		else if (*ptr == '&')
			slen += inlen-1;
	ret = pp = zalloc(slen+1);
	for (ptr = out; *ptr; ptr++)
		if (*ptr == '\\')
			*pp++ = *++ptr;
		else if (*ptr == '&')
			{
			strcpy(pp,in);
			pp += inlen;
			}
		else
			*pp++ = *ptr;
	*pp = '\0';
	return ret;
}

/* make a string out of a history list */

char *makehlist(table tab,int freeit)
{
int ccnt;
Node node;
char *ret,*ptr;
char sep = *ifs;

	for (ccnt = 0, node = (freeit == 2) ? tab->first->next : tab->first;
			node; node = node->next)
		ccnt += strlen(node->dat)+1;
	if (!ccnt)
		return strdup("");
	ptr = ret = zalloc(ccnt);
	for (node = (freeit == 2) ? tab->first->next : tab->first;
			node; node = node->next)
		{
		strcpy(ptr,node->dat);
		ptr += strlen(node->dat);
		if (freeit == 1)
			free(node->dat);
		*ptr++ = sep;
		}
	*--ptr = '\0';
	return ret;
}

table quietgetevent(int ev)
{
Node node;
 
	ev -= tfev;
	for (node = histlist->first; ev && node; node = node->next, ev--);
	if (ev)
		return NULL;
	return node->dat;
}

table getevent(int ev)
{
Node node;
int oev = ev;
 
	ev -= tfev;
	for (node = histlist->first; ev && node; node = node->next, ev--);
	if (ev || !node)
		{
		herrflush();
		zerr("no such event: %d",oev);
		return NULL;
		}
	return node->dat;
}
 
int getargc(table tab)
{
int argc;
Node node;
 
	for (argc = 0, node = tab->first; node; node = node->next, argc++);
	return argc;
}
 
table getargs(table elist,int arg1,int arg2)
{
table ret = newtable();
Node node;
int oarg1 = arg1,oarg2 = arg2;
 
	for (node = elist->first; arg1 && node; node = node->next, arg1--,arg2--);
	if (!node)
		{
		herrflush();
		zerr("no such word in event: %d",oarg1);
		return NULL;
		}
	for (arg2++; arg2 && node; node = node->next, arg2--)
		addnode(ret,strdup(node->dat));
	if (arg2 && !node)
		{
		herrflush();
		zerr("no such word in event: %d",oarg2);
		return NULL;
		}
	return ret;
}

int quote(void **tr)
{
char *ptr,*rptr,**str = (char **) tr;
int len = 1;
 
	for (ptr = *str; *ptr; ptr++,len++)
		if (*ptr == '\'')
			len += 3;
	ptr = *str;
	*str = rptr = alloc(len);
	for (ptr = *str; *ptr; )
		if (*ptr == '\'')
			{
			*rptr++ = '\'';
			*rptr++ = '\\';
			*rptr++ = '\'';
			*rptr++ = '\'';
			ptr++;
			}
		else
			*rptr++ = *ptr++;
	return 0;
}
 
int quotebreak(void **tr)
{
char *ptr,*rptr,**str = (char **) tr;
int len = 1;
 
	for (ptr = *str; *ptr; ptr++,len++)
		if (*ptr == '\'')
			len += 3;
		else if (znspace(*ptr))
			len += 2;
	ptr = *str;
	*str = rptr = alloc(len);
	for (ptr = *str; *ptr; )
		if (*ptr == '\'')
			{
			*rptr++ = '\'';
			*rptr++ = '\\';
			*rptr++ = '\'';
			*rptr++ = '\'';
			ptr++;
			}
		else if (znspace(*ptr))
			{
			*rptr++ = '\'';
			*rptr++ = *ptr++;
			*rptr++ = '\'';
			}
		else
			*rptr++ = *ptr++;
	return 0;
}

void stradd(char **p,char *d)
{
char *s = *p;

	while (*s++ = *d++);
	*p = s-1;
}

/* get a prompt string */

char *putprompt(char *fm)
{
char *ss,*ttyname(int);
static char buf[256];
char *bp = buf;
int t0;
struct tm *tm = NULL;
time_t timet;

	clearerr(stdin);
	fm = getparm(fm);
	for(;*fm;fm++)
		{
		if (bp-buf >= 220)
			break;
		if (*fm == '%')
			switch (*++fm)
				{
				case '~':
					if (!strncmp(cwd,home,t0 = strlen(home)))
						{
						*bp++ = '~';
						stradd(&bp,cwd+t0);
						break;
						}
				case 'd':
				case '/':
					stradd(&bp,cwd);
					break;
				case 'c':
				case '.':
					for (ss = cwd+strlen(cwd); ss > cwd; ss--)
						if (*ss == '/')
							{
							ss++;
							break;
							}
					stradd(&bp,ss);
					break;
				case 'h':
				case '!':
					sprintf(bp,"%d",cev);
					bp += strlen(bp);
					break;
				case 'M':
					stradd(&bp,hostM);
					break;
				case 'm':
					stradd(&bp,hostm);
					break;
				case 'S':
					if (tgetstr("so",&bp))
						bp--;
					break;
				case 's':
					if (tgetstr("se",&bp))
						bp--;
					break;
				case 'B':
					if (tgetstr("md",&bp))
						bp--;
					break;
				case 'b':
					if (tgetstr("me",&bp))
						bp--;
					break;
				case 'U':
					if (tgetstr("us",&bp))
						bp--;
					break;
				case 'u':
					if (tgetstr("ue",&bp))
						bp--;
					break;
				case 't':
				case '@':
					timet = time(NULL);
					tm = localtime(&timet);
					strftime(bp,16,"%l:%M%p",tm);
					if (*bp == ' ')
						chuck(bp);
					bp += strlen(bp);
					break;
				case 'T':
					timet = time(NULL);
					tm = localtime(&timet);
					strftime(bp,16,"%k:%M",tm);
					bp += strlen(bp);
					break;
				case '*':
					timet = time(NULL);
					tm = localtime(&timet);
					strftime(bp,16,"%k:%M:%S",tm);
					bp += strlen(bp);
					break;
				case 'n':
					stradd(&bp,username);
					break;
				case 'w':
					timet = time(NULL);
					tm = localtime(&timet);
					strftime(bp,16,"%a %e",tm);
					bp += strlen(bp);
					break;
				case 'W':
					timet = time(NULL);
					tm = localtime(&timet);
					strftime(bp,16,"%m/%d/%y",tm);
					bp += strlen(bp);
					break;
				case 'D':
					timet = time(NULL);
					tm = localtime(&timet);
					strftime(bp,16,"%y-%m-%d",tm);
					bp += strlen(bp);
					break;
				case 'l':
					if (ss = ttyname(SHTTY))
						stradd(&bp,ss+8);
					else
						stradd(&bp,"(none)");
					break;
				case '%':
					*bp++ = '%';
					break;
				case '#':
					*bp++ = (geteuid()) ? '%' : '#';
					break;
				default:
					*bp++ = '%';
					*bp++ = *fm;
					break;
				}
		else	
			*bp++ = *fm;
		}
	*bp = '\0';
	return buf;
}

void herrflush(void)
{
	if (strin)
		hflush();
	else while (lastc != '\n' && lastc != HERR)
		hgetch();
	if (magic)
		putc('\n',stderr);
}

/* read an arbitrary amount of data into a buffer until stop is found */

char *hdynread(char stop)
{
int bsiz = 256,ct = 0,c;
char *buf = zalloc(bsiz),*ptr;
 
	ptr = buf;
	while ((c = hgetch()) != stop && c != '\n')
		{
		if (c == '\\')
			c = hgetch();
		*ptr++ = c;
		if (++ct == bsiz)
			{
			buf = realloc(buf,bsiz *= 2);
			ptr = buf+ct;
			}
		}
	*ptr = 0;
	if (c == '\n')
		{
		hungetch('\n');
		zerr("delimiter expected");
		errflag = 1;
		free(buf);
		return NULL;
		}
	return buf;
}
 
char *hdynread2(char stop)
{
int bsiz = 256,ct = 0,c;
char *buf = zalloc(bsiz),*ptr;
 
	ptr = buf;
	while ((c = hgetch()) != stop && c != '\n')
		{
		if (c == '\n')
			{
			hungetch(c);
			break;
			}
		if (c == '\\')
			if ((c = hgetch()) == '&')
				c = '&';
		*ptr++ = c;
		if (++ct == bsiz)
			{
			buf = realloc(buf,bsiz *= 2);
			ptr = buf+ct;
			}
		}
	*ptr = 0;
	if (c == '\n')
		hungetch('\n');
	return buf;
}

/* set cbreak mode, or the equivalent */

void setcbreak(void)
{
struct ttyinfo ti;

	ti = shttyinfo;
#ifdef TERMIOS
	ti.termios.c_lflag &= ~ICANON;
	ti.termios.c_cc[VMIN] = 1;
	ti.termios.c_cc[VTIME] = 0;
#else
	ti.sgttyb.sg_flags |= CBREAK;
#endif
	settyinfo(&ti);
}

int getlineleng(void)
{
int z;

	z = shttyinfo.winsize.ws_col;
	return (z) ? z : 80;
}

void unsetcbreak(void)
{
	settyinfo(&shttyinfo);
}

/* give the tty to some process */

void attachtty(long pgrp)
{
static int ep = 0;

	if (jobbing)
#ifdef BUGGY_GCC
		if (SHTTY != -1 && ioctl(SHTTY,(0x80000000|((sizeof(int)&0xff)<<16)|
			('t'<<8)|118),&pgrp) == -1 && !ep)
#else
		if (SHTTY != -1 && ioctl(SHTTY,TIOCSPGRP,&pgrp) == -1 && !ep)
#endif
			{
			zerr("can't set tty pgrp: %e",errno);
			opts[MONITOR] = OPT_UNSET;
			ep =1;
			}
}

