/*

	parse.c - parsing

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

/* parse a list, but return : instead of NULL */

list parlist(int nest)
{
list l1;
list2 l2;
pline p;
comm c;

	if (l1 = parlist1(nest))
		return l1;
	if (errflag)
		return NULL;
	c = alloc(sizeof *c);
	c->cmd = strdup("");
	c->args = newtable();
	c->redir = newtable();
	c->type = SIMPLE;
	p = alloc(sizeof *p);
	p->left = c;
	p->type = END;
	l2 = alloc(sizeof *l2);
	l2->left = p;
	l2->type = END;
	l1 = alloc(sizeof *l1);
	l1->left = l2;
	l1->type = SYNC;
	return l1;
}

/* parse a list */

list parlist1(int nest)
{
list l1 = (list) alloc(sizeof *l1);
int isnl;

	incmd = 0;
	if (peek == EMPTY)
		matchit();
	if (nest)
		while (peek == NEWLIN || peek == SEMI)
			matchit();
	if (!(l1->left = parlist2()))
		{
		free(l1);
		return NULL;
		}
	l1->type = (peek == AMPER) ? ASYNC : SYNC;
	if ((isnl = peek == NEWLIN) || peek == SEMI || peek == AMPER)
		peek = EMPTY;
	if ((nest || !isnl) && peek == EMPTY)
		{
		if (!(l1->right = parlist1(nest)))
			{
			if (!errflag)
				{
				if (peek == NEWLIN)
					peek = EMPTY;
				return l1;
				}
			freelist2(l1->left);
			free(l1);
			return NULL;
			}
		}
	else
		l1->right = NULL;
	return l1;
}

/* parse a sublist */

list2 parlist2(void)
{
list2 l2 = (list2) alloc(sizeof *l2);
int iter = 0;

	for (;;)
		{
		if (peek == BANG)
			{
			l2->flags |= PFLAG_NOT;
			matchit();
			}
		else if (peek == TIME)
			{
			l2->flags |= PFLAG_TIMED;
			matchit();
			}
		else if (peek == COPROC)
			{
			l2->flags |= PFLAG_COPROC;
			matchit();
			}
		else
			break;
		iter = 1;
		}
	if (!(l2->left = parpline()))
		{
		free(l2);
		if (!errflag && iter)
			{
			zerr("parse error: pipeline expected");
			errflag = 1;
			}
		return NULL;
		}
	if (peek == DAMPER || peek == DBAR)
		{
		l2->type = (peek == DAMPER) ? ANDNEXT : ORNEXT;
		matchit();
		while (peek == NEWLIN)
			matchit();
		if (!(l2->right = parlist2()))
			{
			if (!errflag)
				{
				zerr("invalid null command");
				errflag = 1;
				}
			freepline(l2->left);
			free(l2);
			return NULL;
			}
		}
	else
		{
		l2->type = END;
		l2->right = NULL;
		}
	return l2;
}

/* parse a pipeline */

pline parpline(void)
{
pline p = (pline) alloc(sizeof *p);

	if (!(p->left = parcmd()))
		{
		free(p);
		return NULL;
		}
	if (peek == HERR)
		{
		freecmd(p->left);
		free(p);
		return NULL;
		}
	if (peek == BAR || peek == BARAMP)
		{
		if (peek == BARAMP)
			{
			struct fnode *f;

			f = alloc(sizeof *f);
			f->type = MERGEOUT;
			f->fd1 = 2;
			f->u.fd2 = 1;
			addnode(p->left->redir,f);
			}
		matchit();
		while (peek == NEWLIN)
			matchit();
		p->type = PIPE;
		if (!(p->right = parpline()))
			{
			if (!errflag)
				{
				zerr("invalid null command");
				errflag = 1;
				}
			freecmd(p->left);
			free(p);
			return NULL;
			}
		}
	else
		{
		p->type = END;
		p->right = NULL;
		}
	return p;
}

/* parse a command */

comm parcmd(void)
{
comm c = (comm) alloc(sizeof *c);
list l;
char *str;
int flag,iter = 0;

	incmd = 0;
	c->left = NULL;
	c->cmd = NULL;
	c->args = newtable();
	c->redir = newtable();
	c->type = SIMPLE;
	c->vars = NULL;
	if (peek == EOF)
		return NULL;
foo:
	switch (peek)
		{
		case HERR:
			return NULL;
		case ENVSTRING:
			if (!c->vars)
				c->vars = newtable(); /* FIX */
			for (str = tstr; *str != '='; str++);
			*str++ = '\0';
			addnode(c->vars,tstr);
			addnode(c->vars,strdup(str));
			matchit();
			goto foo;
		case FOR:
			incmd = 1;
			matchit();
			if (parfor(c,1))
				return NULL;
			break;
		case SELECT:
			incmd = 1;
			matchit();
			if (parfor(c,0))
				return NULL;
			break;
		case CASE:
			incmd = 1;
			matchit();
			if (parcase(c))
				return NULL;
			break;
		case IF:
			matchit();
			if (parif(c))
				return NULL;
			break;
		case WHILE:
			matchit();
			if (parwhile(c,0))
				return NULL;
			break;
		case UNTIL:
			matchit();
			if (parwhile(c,1))
				return NULL;
			break;
		case REPEAT:
			incmd = 1;
			matchit();
			if (parrepeat(c))
				return NULL;
			break;
		case INPAR:
			matchit();
			c->type = SUBSH;
			if (!(c->left = parlist(1)))
				{
				freecmd(c);
				return NULL;
				}
			if (peek != OUTPAR)
				{
				freecmd(c);
				zerr("parse error: '}' expected");
				return NULL;
				}
			matchit();
			break;
		case INBRACE:
			matchit();
			c->type = CURSH;
			if (!(c->left = parlist(1)))
				{
				freecmd(c);
				return NULL;
				}
			if (peek != OUTBRACE)
				{
				freecmd(c);
				zerr("parse error: '}' expected");
				return NULL;
				}
			matchit();
			break;
		case FUNC:
			matchit();
			str = tstr;
			if (peek != STRING && peek != ENVSTRING)
				{
				c->cmd = strdup("function");
				incmd = 1;
				if (isredir())
					goto jump1;
				else
					goto jump2;
				}
			do	
				matchit();
			while (peek == NEWLIN);
			if (peek != INBRACE)
				{
				freecmd(c);
				zerr("parse error: '{' expected");
				return NULL;
				}
			matchit();
			flag = peek == OUTBRACE;
			if (!(l = parlist(1)))
				{
				freecmd(c);
				return NULL;
				}
			if (peek != OUTBRACE)
				{
				freelist(l);
				freecmd(c);
				zerr("parse error: '}' expected");
				return NULL;
				}
			matchit();
			settrap(str,flag);
			addhnode(str,l,shfunchtab,freeshfunc);
			c->cmd = strdup("");
			c->type = SIMPLE;
			break;
		case EXEC:
			c->flags |= CFLAG_EXEC;
			matchit();
			iter = 1;
			goto foo;
		case COMMAND:
			c->flags |= CFLAG_COMMAND;
			matchit();
			iter = 1;
			goto foo;
		default:
jump1:
			if (isredir())
				{
				if (parredir(c))
					{
					freecmd(c);
					return NULL;
					}
				goto foo;
				}
			if (!(peek == STRING || peek == ENVSTRING))
				{
				if (full(c->redir))
					{
					c->cmd = strdup("cat");
					return c;
					}
				if (c->vars)
					{
					c->cmd = strdup("");
					return c;
					}
				free(c->args);
				free(c->redir);
				free(c);
				if (iter && !errflag)
					{
					errflag = 1;
					zerr("parse error: command expected");
					}
				return NULL;
				}
jump2:
			while (peek == STRING || peek == ENVSTRING || isredir())
				if (isredir())
					{
					if (parredir(c))
						{
						freecmd(c);
						return NULL;
						}
					}
				else
					{
					if (tstr[0] == Inpar && tstr[1] == Outpar && !tstr[2])
						{
						free(tstr);
						incmd = 0;
						matchit();
						if (full(c->args))
							{
							zerr("illegal function definition");
							freecmd(c);
							return NULL;
							}
						while (peek == NEWLIN)
							matchit();
						if (peek != INBRACE)
							{
							freecmd(c);
							zerr("parse error: '{' expected");
							return NULL;
							}
						matchit();
						flag = peek == OUTBRACE;
						if (!(l = parlist(1)))
							{
							freecmd(c);
							return NULL;
							}
						if (peek != OUTBRACE)
							{
							freelist(l);
							freecmd(c);
							zerr("parse error: '}' expected");
							return NULL;
							}
						matchit();
						settrap(c->cmd,flag);
						addhnode(c->cmd,l,shfunchtab,freeshfunc);
						c->cmd = strdup("");
						c->type = SIMPLE;
						incmd = 0;
						return c;
						}
					if (peek == ENVSTRING && (!incmd || opts[KEYWORD] == OPT_SET))
						{
						if (!c->vars)
							c->vars = newtable(); /* FIX */
						for (str = tstr; *str != '='; str++);
						*str++ = '\0';
						addnode(c->vars,tstr);
						addnode(c->vars,strdup(str));
						}
					else if (c->cmd)
						addnode(c->args,tstr);
					else
						{
						c->cmd = tstr;
						incmd = 1;
						}
					matchit();
					}
			break;
		}
	while (isredir())
		if (parredir(c))
			{
			freecmd(c);
			return NULL;
			}
	incmd = 0;
	if (peek == HERR)
		{
		freecmd(c);
		return NULL;
		}
	return c;
}

/* != 0 if peek is a redirection operator */

int isredir(void)
{
	return (peek >= OUTANG && peek <= DOUTANGAMPBANG);
}

/* get fd associated with str */

int getfdstr(char *s)
{
	if (s[1])
		return -1;
	if (isdigit(*s))
		return *s-'0';
	if (*s == 'p')
		return -2;
	return -1;
}

int parredir(comm c)
{
struct fnode *fn = (struct fnode *) alloc(sizeof *fn);
int pk = peek,ic = incmd,mrg2 = 0;

	fn->type = peek-OUTANG+WRITE;
	if (peek == OUTANGAMP)
		fn->type = MERGEOUT;
	if (peekfd != -1)
		fn->fd1 = peekfd;
	else if (peek <= DOUTANGBANG || peek >= OUTANGAMP)
		fn->fd1 = 1;
	else
		fn->fd1 = 0;
	incmd = 1;
	matchit();
	incmd = ic;
	if (peek != STRING)
		{
		zerr("parse error: filename expected");
		return 1;
		}
	
	if ((*tstr == Inang || *tstr == Outang) && tstr[1] == Inpar)
		{
		if (fn->type == WRITE)
			fn->type = OUTPIPE;
		else if (fn->type == READ)
			fn->type = INPIPE;
		else
			{
			zerr("parse error: bad process redirection");
			return 1;
			}
		fn->u.name = tstr;
		}
	else if (fn->type == HEREDOC)
		fn->u.fd2 = gethere(tstr);
	else if (pk >= OUTANGAMP && getfdstr(tstr) == -1)
		{
		mrg2 = 1;
		fn->u.name = tstr;
		fn->type = pk-OUTANGAMP;
		}
	else if (pk > OUTANGAMPBANG)
		{
		zerr("parse error: filename expected");
		return 1;
		}
	else if (pk == OUTANGAMPBANG)
		{
		struct fnode *fe = alloc(sizeof *fe);

		fe->fd1 = fn->fd1;
		fe->type = CLOSE;
		addnode(c->redir,fe);
		fn->u.fd2 = getfdstr(tstr);
		if (fn->u.fd2 == -2)
			fn->u.fd2 = spout;
		fn->type = MERGEOUT;
		}
	else if (fn->type == MERGE || fn->type == MERGEOUT)
		{
		if (*tstr == '-')
			fn->type = CLOSE;
		else
			{
			fn->u.fd2 = getfdstr(tstr);
			if (fn->u.fd2 == -2)
				fn->u.fd2 = (pk == OUTANGAMP) ? spout : spin;
			}
		}
	else
		fn->u.name = tstr;
	addnode(c->redir,fn);
	if (mrg2)
		{
		struct fnode *fe = alloc(sizeof *fe);

		fe->fd1 = 2;
		fe->u.fd2 = fn->fd1;
		fe->type = MERGEOUT;
		addnode(c->redir,fe);
		}
	matchit();
	return 0;
}

