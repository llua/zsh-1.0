/*

	subst.c - various substitution

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
#define MAXPATHLEN 1024

#define magicerr() { if (magic) putc('\n',stderr); }

/* do substitutions before fork */

void prefork(table list)
{
Node node = list->first;

	while (node)
		{
		char *str,*str3;
		
		str = str3 = node->dat;
		if (!magic && str[1] == Inpar && (*str == Inang ||
				*str == Outang || *str == Equals))
			{
			if (*str == Inang)
				node->dat = getoutproc(str+2);		/* <(...) */
			else if (*str == Equals)
				node->dat = getoutputfile(str+2);	/* =(...) */
			else
				node->dat = getinproc(str+2);			/* >(...) */
			free(str);
			if (!node->dat)
				{
				zerr("parse error in process substitution");
				errflag = 1;
				return;
				}
			}
		else while (*str)
			{
			if (*str == String || *str == Qstring)
				if (str[1] != Inpar)
					if (str[1] == '*' || str[1] == Star)
						parminsall(list,&node,&str,&str3);	/* $* */
					else if (str[1] == Inbrack)
						{
						arithsuber((void **) &str,&str3);	/* $[...] */
						if (magic)
							magic = 2;
						node->dat = str3;
						}
					else
						{
						parmsuber(str,&str3);		/* $foo */
						node->dat = str = str3;
						if (errflag)
							return;
						continue;
						}
			str++;
			if (errflag)
				return;
			}
		node = node->next;
		}
}

void postfork(table list,int globstat)
{
Node node = list->first;
int glb = 1;

	if (isset(NOGLOBOPT) || globstat != GLOB)
		glb = 0;
	while (node)
		{
		char *str,*str3;
		
		str = str3 = node->dat;
		while (*str)
			{
			if (((*str == String || *str == Qstring) && str[1] == Inpar) ||
					*str == Tick || *str == Qtick)
				comminsall(list,&node,&str,&str3);	/* `...`,$(...) */
			str++;
			if (errflag)
				return;
			}
		
		/* now we remove the Nulargs tokens if this is not a null
			arguments.  The lexical analyzer throws these in so that
			zsh will not look at this:

			$PA"TH"

			and expand it to $PATH.  But after parameter substitution
			these are only a nuisance, so we remove them. */

		if (*(char *) node->dat)
			remnulargs(node->dat);
		
		if (unset(IGNOREBRACES))
			while (hasbraces(node->dat))
				xpandbraces(list,&node);
		filesub(&node->dat);
		if (errflag)
			return;
		if (glb)
			{
			if (haswilds(node->dat))
				glob(list,&node);
			if (errflag)
				return;
			}
		else if (globstat == MOSTGLOB && *(char *) node->dat != '-')
			glb = 1;
		node = node->next;
		}
}

/* strdup, but returns "Nularg" if this is a null string */

void *nstrdup(void *s)
{
char *t = s,u[] = {Nularg,'\0'};

	if (!*t)
		return strdup(u);
	return strdup(t);
}

/* $* */

void parminsall(table l,Node *nn,char **aptr,char **bptr)
{
char *str3 = *aptr,*str = *bptr;
Node n = *nn,where = n->last;
table pl;

	if (magic)
		magic = 2;
	*str3 = '\0';
	str3 += 2;
	remnode(l,n);
	pl = duptable(pparms,nstrdup);
	free(getnode(pl));
	if (pl->first)	/* if $# > 1 */
		{
		char *ptr;
		Node tmp;

		ptr = pl->first->dat;
		pl->first->dat = dyncat(str,ptr);
		free(ptr);
		ptr = pl->last->dat;
		*bptr = pl->last->dat = dyncat(ptr,str3);
		*aptr = *bptr+strlen(str)+strlen(ptr)-1;
		free(ptr);
		tmp = where->next;
		where->next = pl->first;
		pl->last->next = tmp;
		pl->first->last = where;
		if (tmp)
			tmp->last = pl->last;
		else
			l->last = pl->last;
		*nn = pl->last;
		}
	else	/* just remove the $* */
		{
		insnode(l,where,*bptr = dyncat(str,str3));
		*nn = where->next;
		*aptr = *bptr+strlen(str)-1;
		}
}

char *dynread(char stop)
{
int bsiz = 256,ct = 0,c;
char *buf = zalloc(bsiz),*ptr;
 
	ptr = buf;
	while ((c = hgetc()) != stop)
		{
		*ptr++ = c;
		if (++ct == bsiz)
			{
			buf = realloc(buf,bsiz *= 2);
			ptr = buf+ct;
			}
		}
	*ptr = 0;
	return buf;
}

int filesub(void **namptr)
{
char *str = *namptr,*cnam;

	if (*str == Tilde && str[1] != '=')
		{
		if (str[1] == '+')
			{
			char *foo = strdup(cwd),*t = str;	/* ~+ */

			str+=2;
			modify((void **) &foo,&str);
			*namptr = dyncat(cwd,str);
			free(foo);
			free(t);
			return 1;
			}
		else if (str[1] == '-')			/* ~- */
			{
			char *foo,*t = str;

			if (cnam = getparm("OLDPWD"))
				foo = cnam;
			else
				foo = cwd;
			str += 2;
			foo = strdup(foo);
			modify((void **) &foo,&str);
			*namptr = dyncat(foo,str);
			free(t);
			free(foo);
			return 1;
			}
		if (isalpha(str[1]))		/* ~foo */
			{
			char *ptr,*home;
 
			for (ptr = ++str; *ptr && !istok(*ptr) && (isalnum(*ptr) || *ptr == '-'); ptr++)
				if (*ptr == '-')
					*ptr = '-';
			if (!(home = gethome(str,ptr-str)))
				{
				if (magic)
					home = completehome(str,ptr-str);
				if (!home)
					{
					magicerr();
					zerr("user not found: %l",ptr-str,str);
					errflag = 1;
					return 0;
					}
				}
			modify((void **) &home,&ptr);
			*namptr = dyncat(home,ptr);
			free(home);
			free(str-1);
			return 1;
			}
		else if (str[1] == '/')	/* ~/foo */
			{
			*namptr = dyncat(home,str+1);
			free(str);
			return 1;
			}
		else if (!str[1])		/* ~ by itself */
			{
			free(str);
			*namptr = strdup(home);
			return 1;
			}
		}
	if (*str == Equals && !istok(str[1]) && (isalnum(str[1]) || str[1] == '-'))
		{
		char *ptr,*s,*ds;
		int val;
		
		untokenize(str);
		if (isalpha(str[1]))		/* =foo */
			{
			struct chnode *chn;
			struct anode *t;
			char sav,*pp;
 
			for (pp = str+1; *pp && *pp != ':'; pp++);
			sav = *pp;
			*pp = '\0';
			if ((t = gethnode(str+1,alhtab)) && t->cmd)
				if (t->cmd >= 0)
					cnam = strdup(t->text);
				else
					{
					magicerr();
					zerr("%s: shell reserved word",str+1);
					errflag = 1;
					return 0;
					}
		 	else if (chn = gethnode(str+1,chtab))
				if (chn->type != BUILTIN)
					cnam = strdup(chn->u.nam);
				else
					{
					magicerr();
					zerr("%s: shell built-in command",str+1);
					errflag = 1;
					return 0;
					}
			else if (!(cnam = findcmd(str+1)))
				{
				magicerr();
				zerr("%s not found",str+1);
				errflag = 1;
				return 0;
				}
			*namptr = cnam;
			if ((*pp = sav) == ':')
				{
				modify(namptr,&pp);
				s = *namptr;
				*namptr = dyncat(*namptr,pp);
				free(s);
				}
			free(str);
			return 1;
			}
		if (str[1] == '-') 	/* =- */
			{
			val = -1;
			ptr = str+2;
			}
		else
			val = strtol(str+1,&ptr,10);	/* =# */
		ds = dstackent(val);
		if (!ds)
			return 1;
		s = strdup(ds);
		modify((void **) &s,&ptr);
		*namptr = dyncat(s,ptr);
		free(s);
		free(str);
		return 1;
		}
	return 0;
}

/* get a user's directory */

char *gethome(char *user,int len)
{
char sav,*str;
struct passwd *pw;
 
	sav = user[len];
	user[len] = '\0';
	if (!(pw = getpwnam(user)))
		{
		user[len] = sav;
		return NULL;
		}
	str = xsymlink(pw->pw_dir);
	user[len] = sav;
	return str;
}

/* complete a user and try to get his home directory */

char *completehome(char *user,int len)
{
FILE *in;
char buf[MAXPATHLEN],*str;

	sprintf(buf,"%s/.zfriends",getparm("HOME"));
	if (!(in = fopen(buf,"r")))
		return NULL;
	while (fgetline(buf,MAXPATHLEN,in))
		if (!strncmp(user,buf,len))
			if (str = gethome(buf,strlen(buf)))
				{
				fclose(in);
				return str;
				}
	fclose(in);
	return NULL;
}

/* get the value of the parameter specified by the first len
	characters of s */

char *getsparmval(char *s,int len)
{
char sav = s[len];
char *val;
char buf[50];
int t0;

	if (len == 1 && (istok(*s) || !isalnum(*s)))
		switch(*s)
			{
			case Pound:
			case '#':
				sprintf(buf,"%d",ppcount());
				return strdup(buf);
			case '-':
				for (val = buf, t0 = ' '; t0 <= 'z'; t0++)
					if (opts[t0] == OPT_SET)
						*val++ = t0;
				*val = '\0';
				return strdup(buf);
			case '?':
			case Quest:
				sprintf(buf,"%d",lastval);
				return strdup(buf);
			case '$':
			case String:
				sprintf(buf,"%d",procnum);
				return strdup(buf);
			case '!':
				sprintf(buf,"%d",proclast);
				return strdup(buf);
			default:
				return NULL;
			}
	s[len] = '\0';
	if (isdigit(*s))
		{
		int num;
		Node node;
		
		num = atoi(s);
		s[len] = sav;
		for (node = pparms->first; node && num; num--,node = node->next);
		return (node) ? strdup(node->dat) : NULL;
		}
	val = getparm(s);
	s[len] = sav;
	return (val) ? strdup(val) : NULL;
}

/* set the parameter associated with the first len characters of s
	to v. */

void setparml(char *s,int len,char *v)
{
char c;
 
	c = s[len];
	s[len] = 0;
	if (isdigit(*s))
		{
		int num;
		Node node;
		
		num = atoi(s);
		for (node = pparms->first; node && num; num--,node = node->next);
		if (node)
			{
			free(node->dat);
			node->dat = v;
			}
		else
			{
			while (num--)
				addnode(pparms,strdup(""));
			addnode(pparms,v);
			}
		}
	else
		setparm(strdup(s),v,0,0);
	s[len] = c;
}

/* `...`, $(...) */

void comminsall(table l,Node *nn,char **aptr,char **bptr)
{
char *str3 = *aptr,*str = *bptr,*str2;
Node n = *nn,where = n->last;
table pl;
int s31 = (*str3 == Qtick || *str3 == Qstring);

	if (magic) magic = 2;
	if (*str3 == Tick || *str3 == Qtick)
		{
		*str3 = '\0';
		for (str2 = ++str3; *str3 != Tick && *str3 != Qtick; str3++);
		*str3++ = '\0';
		}
	else
		{
		*str3++ = '\0';
		for (str2 = ++str3; *str3 != Outpar; str3++);
		*str3++ = '\0';
		}
	remnode(l,n);
	if (!(pl = getoutput(str2,s31)))
		{
		magicerr();
		zerr("parse error in command substitution");
		errflag = 1;
		return;
		}
	if (pl->first)
		{
		char *ptr;
		Node tmp;

		ptr = pl->first->dat;
		pl->first->dat = dyncat(str,ptr);
		free(ptr);
		ptr = pl->last->dat;
		*bptr = pl->last->dat = dyncat(ptr,str3);
		*aptr = *bptr+strlen(str)+strlen(ptr)-1;
		free(ptr);
		tmp = where->next;
		where->next = pl->first;
		pl->last->next = tmp;
		pl->first->last = where;
		if (tmp)
			tmp->last = pl->last;
		else
			l->last = pl->last;
		/* free(tmp); */
		*nn = pl->last;
		free(pl);
		}
	else
		{
		insnode(l,where,*bptr = dyncat(str,str3));
		*nn = where->next;
		*aptr = *bptr+strlen(str)-1;
		}
}

/* do simple parameter substitution */

/*
	consider an argument like this:

	abcde${fgh:-ijk}lmnop

	aptr will point to the $.
	*bptr,ostr will point to the a.
	t will point to the f.
	u will point to the i.
	s will point to the l (eventually).
*/

void parmsuber(char *aptr,char **bptr)
{
char *s = aptr,*t,*u,*val,*ostr = *bptr;
int brs;			/* != 0 means ${...}, otherwise $... */
int vlen;		/* the length of the name of the parameter */
int colf;		/* != 0 means we found a colon after the name */
int doub = 0;	/* != 0 means we have %%, not %, or ##, not # */

	/* first, remove the $ so *bptr is pointing to a null-terminated
		string containing the stuff before the $.  Then check for braces,
		and get the parameter name and value, if any. */

	*s++ = '\0';
	if (brs = (*s == '{' || *s == Inbrace))
		s++;
	t = s;
	if (istok(*s) || !isalnum(*s))
		{
		val = getsparmval(t,vlen = 1);
		if (!val)
			{
			*(char *) aptr = '$';
			if (brs)
				s[-1] = '{';
			return;
			}
		s++;
		}
	else
		{
		while (!istok(*s) && (isalnum(*s) || *s == '_'))
			s++;
		val = getsparmval(t,vlen = s-t);
		}

	/* val can still be NULL at this point. */

	if (colf = *s == ':')
		s++;
	
	/* check for ${..?...} or ${..=..} or one of those.  Only works
		if the name is in braces. */

	if (brs && (*s == '-' || *s == '=' || *s == '?' || *s == '+' || *s == '#' ||
			*s == '%' || *s == Quest || *s == Pound))
		{
		if (*s == s[1])
			{
			s++;
			doub = 1;
			}
		u = ++s;
		if (brs)
			while (*s != '}' && *s != Outbrace)
				s++;
		else
			{
			while (*s++);
			s--;
			}
		*s = '\0';
		switch (u[-1])
			{
			case '-':
				if (!val || (colf && !*val))
					val = strdup(u);
				break;
			case '=':
				if (!val || (colf && !*val))
					setparml(t,vlen,val = strdup(u));
				break;
			case '?':
			case Quest:
				if (!val || (colf && !*val))
					{
					magicerr();
					zerr("%s",(*u) ? u : "parameter not set");
					if (!interact)
						exit(1);
					else
						errflag = 1;
					return;
					}
				break;
			case '+':
				if (!val || (colf && !*val))
					val = strdup("");
				else
					val = strdup(u);
				break;
			case '#':
			case Pound:
				if (!val)
					val = strdup("");
				getmatch(&val,u,doub);
				break;
			case '%':
				if (!val)
					val = strdup("");
				getmatch(&val,u,doub+2);
				break;
			}
		}
	else		/* no ${...=...} or anything, but possible modifiers. */
		{
		if (!val)
			{
			if (isset(NOUNSET))
				{
				zerr("parameter not set: %l",vlen,t);
				errflag = 1;
				return;
				}
			val = strdup("");
			}
		if (colf)
			{
			s--;
			modify((void **) &val,&s);		/* do modifiers */
			}
		if (brs)
			{
			if (*s != '}' && *s != Outbrace)
				{
				zerr("closing brace expected");
				errflag = 1;
				return;
				}
			s++;
			}
		}
	if (errflag)
		{
		free(ostr);
		return;
		}
	*bptr = zalloc((char *) aptr-(*bptr)+strlen(val)+strlen(s)+1);
	strcpy(*bptr,ostr);
	strcat(*bptr,val);
	strcat(*bptr,s);
	free(ostr);
	if (magic)
		magic = 2;
}

/* arithmetic substitution */

void arithsuber(void **aptr,char **bptr)
{
char *s = *aptr,*t,buf[16];
long v;

	*s = '\0';
	for (; *s != Outbrack; s++);
	*s++ = '\0';
	v = matheval(*aptr+2);
	sprintf(buf,"%ld",v);
	t = zalloc(strlen(*bptr)+strlen(buf)+strlen(s)+1);
	strcpy(t,*bptr);
	strcat(t,buf);
	strcat(t,s);
	free(*bptr);
	*bptr = t;
}

void modify(void **str,char **ptr)
{
char *ptr1,*ptr2,*ptr3,del,*lptr;
int gbal;
 
	while (**ptr == ':')
		{
		lptr = *ptr;
		(*ptr)++;
		gbal = 0;
here:
		switch(*(*ptr)++)
			{
			case 'h':
				while (remtpath(str) && gbal);
				break;
			case 'r':
				while (remtext(str) && gbal);
				break;
			case 'e':
				while (rembutext(str) && gbal);
				break;
			case 't':
				while (remlpaths(str) && gbal);
				break;
			case 's':
				if (last)
					free(last);
				if (rast)
					free(rast);
				ptr1 = *ptr;
				del = *ptr1++;
				for (ptr2 = ptr1; *ptr2 != del && *ptr2; ptr2++);
				if (!*ptr2)
					{
					magicerr();
					zerr("bad subtitution");
					errflag = 1;
					return;
					}
				*ptr2++ = '\0';
				for (ptr3 = ptr2; *ptr3 != del && *ptr3; ptr3++);
				if (*ptr3)
					*ptr3++ = '\0';
				last = strdup(ptr1);
				rast = strdup(ptr2);
				*ptr = ptr3;
			case '&':
				if (last && rast)
					subststr(str,last,rast,gbal);
				break;
			case 'g':
				gbal = 1;
				goto here;
			default:
				*ptr = lptr;
				return;
			}
		}
}

/* get a directory stack entry */

char *dstackent(int val)
{
Node node;
 
	if ((val < 0 && !dirstack->first) || !val--)
		return cwd;
	if (val < 0)
		node = dirstack->last;
	else
		for (node = dirstack->first; node && val; val--,node = node->next);
	if (!node)
		{
		magicerr();
		zerr("not enough dir stack entries.");
		errflag = 1;
		return NULL;
		}
	return node->dat;
}
 
void execshfunc(comm comm)
{
table tab,oldlocals;
Node n;
char *s;

	tab = pparms;
	oldlocals = locallist;
	locallist = newtable();
	for (n = tab->first; n; n = n->next);
	pparms = comm->args;
	runlist(comm->left);
	retflag = 0;
	comm->left = NULL;
	pparms = tab;
	while (s = getnode(locallist))
		{
		void *z = remhnode(s,parmhtab);
		if (z)
			freepm(z);
		}
	free(locallist);
	locallist = oldlocals;
}

/* make an alias hash table node */

struct anode *mkanode(char *txt,int cmflag)
{
struct anode *ptr = (void *) alloc(sizeof(struct anode));

	ptr->text  = txt;
	ptr->cmd = cmflag;
	ptr->inuse = 0;
	return ptr;
}

/* perform TAB substitution */

char *docompsubs(char *str,int *i)
{
table fake,curt = curtab;
char *s,*t;
int ct = 0;

	for (s = str; *s; s++)
		for (t = tokens; *t; t++)
			if (*s == *t)
				{
				*s = (t-tokens)+Pound;
				break;
				}
	curtab = NULL;
	magic = 1;
	fake = newtable();
	addnode(fake,str);
	prefork(fake);
	if (!errflag)
		postfork(fake,GLOB);
	if (fake->first && fake->first->next)
		ct = 1;
	t = s = buildline(fake);
	free(fake);
	rl_prep_terminal();
	if (errflag)
		{
		rl_on_new_line();
		rl_redisplay();
		errflag = 0;
		magic = 0;
		curtab = curt;
		*i = 0;
		return NULL;
		}
	*i = (magic == 2) + ct;
	magic = 0;
	curtab = curt;
	untokenize(s);
	return s;
}

/* perform substitution on the command name */

void docmdsubs(char **str)
{
table fake;
char *s = strdup(*str);

	fake = newtable();
	addnode(fake,*str);
	prefork(fake);
	if (!errflag) postfork(fake,GLOB);
	if (errflag)
		{
		free(fake);
		free(s);
		return;
		}
	if (fake->first && fake->first->next)
		{
		zerr("%s: ambiguous",s);
		errflag = 1;
		free(fake);
		free(s);
		return;
		}
	*str = getnode(fake);
	free(s);
	free(fake);
}

/* perform substitution on the variables */

void dovarsubs(char **str)
{
table fake;
char *s;

	fake = newtable();
	addnode(fake,*str);
	prefork(fake);
	if (!errflag) postfork(fake,GLOB);
	if (errflag)
		return;
	s = buildline(fake);
	untokenize(s);
	*str = s;
	free(fake);
}

