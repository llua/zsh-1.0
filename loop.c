/*

	loop.c - parsing and executing loop constructs

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

/* parse a for/select loop */

int parfor(comm comm,int isfor)
{
struct fornode *node = alloc(sizeof(struct fornode));
char *comnam = (isfor) ? "for" : "select";

	comm->type = (isfor) ? CFOR : CSELECT;
	comm->info = node;
	if (peek != STRING)
		{
		zerr("parse error in %s: identifier expected",comnam);
		errflag = 1;
		return 0;
		}
	node->name = tstr;
	matchit();
	node->list = NULL;
	node->inflag = 0;
	if (peek == STRING && !strcmp("in",tstr))
	   {
		node->inflag = 1;
		free(tstr);
		matchit();
		while (peek == STRING)
			{
			addnode(comm->args,tstr);
			matchit();
			}
		}
	if (peek != NEWLIN && peek != SEMI)
		{
		zerr("parse error: bad token in '%s' list",comnam);
		freecmd(comm);
		return 1;
		}
	incmd = 0;
	matchit();
	while (peek == NEWLIN)
		matchit();
	if (peek != DO)
		{
		zerr("parse error: 'do' expected");
		freecmd(comm);
		return 1;
		}
	matchit();
	if (!(node->list = parlist(1)))
		{
		freecmd(comm);
		return 1;
		}
	if (peek != DONE)
		{
		zerr("parse error: 'done' expected");
		freecmd(comm);
		return 1;
		}
	matchit();
	return 0;
}
 
int parcase(comm comm)
{
struct casenode *node = alloc(sizeof(struct casenode)),*last = NULL;
char *tok; /* add FREES to this function */

	comm->type = CCASE;
	comm->info = node;
	if (peek != STRING)
		{
		zerr("parse error in case: identifier expected");
		errflag = 1;
		return 0;
		}
	addnode(comm->args,tstr);
	matchit();
	if (peek != STRING || strcmp(tstr,"in"))
		{
		zerr("parse error in case: `in' expected");
		errflag = 1;
		return 0;
		}
	while (tok = getcasepat())
		{
		node = alloc(sizeof(struct casenode));
		node->pat = tok;
		if (last)
			last->next = node;
		else
			comm->info = node;
		(last = node)->list = parlist(1);
		if (peek != DSEMI)
			{
			zerr("parse error: ;; expected");
			return 1;
			}
		}
	if (!last)
		{
		zerr("null case construct");
		return 1;
		}
	return 0;
}

/* get a case pattern: foo) */

char *getcasepat(void)
{
int c,bsiz = 256,ct = 0,pct = 0,qt = 0;
char *buf = zalloc(bsiz),*ptr,*s;
 
	peek = EMPTY;
	while (znspace(c = hgetc()))
		{
		if (c == '\n')
			{
			hwbegin();
			hwaddc('\n');
			hwadd();
			}
		}
	ptr = buf;
	hwbegin();
	hwaddc(c);
	while (c != ')' || pct)
		{
		for (s = tokens; *s; s++)
			if (*s == c)
				{
				c = (s-tokens)+Pound;
				break;
				}
		if (qt)
			{
			if (c == '\'')
				{
				qt = 0;
				c = hgetc();
				continue;
				}
			if (c == EOF)
				{
				qt = 0;
				continue;
				}
			}
		else
			{
			if (c == '\\')
				c = hgetc();
			if (c == '\'')
				{
				qt = 1;
				c = hgetc();
				continue;
				}
			if (c == Inpar)
				pct++;
			if (c == Outpar)
				pct--;
			if (ct == 4 && (znspace(c)||c ==';'||c=='&') && !strncmp(buf,"esac",4))
				{
				hungetc(c);
				hwadd();
				free(buf);
				matchit();
				return NULL;
				}
			if (c == '\n' || c == EOF)
				{
				free(buf);
				zerr("parse error: 'esac' expected");
				return NULL;
				}
			}
		if (c == HERR)
			{
			free(buf);
			return NULL;
			}
		*ptr++ = c;
		if (++ct == bsiz)
			{
			ptr = zalloc(bsiz *= 2);
			memcpy(ptr,buf,ct);
			free(buf);
			buf = ptr;
			ptr = buf+ct;
			}
		c = hgetc();
		}
	*ptr = 0;
	hwadd();
	return buf;
}
 
int parif(comm comm)
{
struct ifnode *node = alloc(sizeof(struct ifnode));
 
	comm->type = CIF;
	comm->info = node;
do_then:
	node->next = NULL;
	if (!(node->ifl = parlist(1)))
		{
		freecmd(comm);
		return 1;
		}
	if (peek != THEN)
		{
		zerr("parse error: 'then' expected");
		freecmd(comm);
		return 1;
		}
	matchit();
	if (!(node->thenl = parlist(1)))
		{
		freecmd(comm);
		return 1;
		}
	if (peek == ELIF)
		{
		matchit();
		node = node->next = alloc(sizeof(struct ifnode));
		goto do_then;
		}
	else if (peek == ELSE)
		{
		matchit();
		node = node->next = alloc(sizeof(struct ifnode));
		node->next = NULL;
		node->ifl = NULL;
		if (!(node->thenl = parlist(1)))
			{
			freecmd(comm);
			return 1;
			}
		}
	if (peek != FI)
		{
		zerr("parse error: 'fi' expected");
		freecmd(comm);
		return 1;
		}
	matchit();
	return 0;
}

/* parse while or until */

int parwhile(comm comm,int cond)
{
struct whilenode *node = alloc(sizeof (struct whilenode));
 
	comm->type = CWHILE;
	comm->info = node;
	node->cond = cond;
	node->loop = node->cont = NULL;
	if (!(node->cont = parlist(1)))
		{
		freecmd(comm);
		return NULL;
		}
	if (peek != DO)
		{
		zerr("parse error: 'do' expected");
		freecmd(comm);
		return 1;
		}
	matchit();
	node->loop = parlist(1);
	if (peek != DONE)
		{
		zerr("parse error: 'done' expected");
		freecmd(comm);
		return 1;
		}
	matchit();
	return 0;
}
 
int parrepeat(comm comm)
{
struct repeatnode *node = alloc(sizeof (struct repeatnode));

	comm->type = CREPEAT;
	comm->info = node;
	node->list = NULL;
	if (peek != STRING || !isdigit(*tstr))
		{
		zerr("parse error: number expected");
		freecmd(comm);
		return 1;
		}
	node->count = atoi(tstr);
	free(tstr);
	incmd = 0;
	do
		matchit();
	while (peek == NEWLIN);
	if (peek != DO)
		{
		zerr("parse error: 'do' expected");
		freecmd(comm);
		return 1;
		}
	matchit();
	node->list = parlist(1);
	if (peek != DONE)
		{
		zerr("parse error: 'done' expected");
		freecmd(comm);
		return 1;
		}
	matchit();
	return 0;
}

void execfor(comm comm)
{
list list;
struct fornode *node;
char *str;
table args;
int cj = curjob;

	loops++;
	exiting = 0;
	node = comm->info;
	args = comm->args;
	if (!node->inflag)
		{
		args = duptable(pparms,dupstr);
		freestr(getnode(args));
		}
	while (str = getnode(args))
		{
		setparm(strdup(node->name),str,0,0);
		list = duplist(node->list);
		execlist(list);
		if (breaks)
			{
			breaks--;
			if (breaks || !contflag)
				break;
			contflag = 0;
			}
		}
	curjob = cj;
}

void execselect(comm comm)
{
list list;
struct fornode *node;
char *str,*s;
table args;
Node n;
int cj = curjob,t0;

	loops++;
	node = comm->info;
	args = comm->args;
	exiting = 0;
	for (;;)
		{
		do
			{
			for (t0 = 1,n = args->first; n; n=n->next,t0++)
				fprintf(stderr,"%d %s\n",t0,n->dat);
			if (interact && SHTTY != -1)
				{
				inittty();
				str = readline(putprompt("PROMPT3"));
				}
			else
				str = fgets(zalloc(256),256,bshin);
			if (!str || errflag)
				{
				fprintf(stderr,"\n");
				goto done;
				}
			if (s = strchr(str,'\n'))
				*s = '\0';
			}
		while (!*str);
		setparm(strdup("REPLY"),str,0,0);
		t0 = atoi(str);
		if (!t0)
			str = "";
		else
			{
			for (t0--,n = args->first; n && t0; n=n->next,t0--);
			if (n)
				str = n->dat;
			else
				str = "";
			}
		setparm(strdup(node->name),strdup(str),0,0);
		list = duplist(node->list);
		execlist(list);
		if (breaks)
			{
			breaks--;
			if (breaks || !contflag)
				break;
			contflag = 0;
			}
		if (errflag)
			break;
		}
done:
	curjob = cj;
}
 
void execwhile(comm comm)
{
list list;
struct whilenode *node;
int cj = curjob; 

	loops++;
	node = comm->info;
	exiting = 0;
	FOREVER
		{
		list = duplist(node->cont);
		execlist(list);
		if (!((lastval == 0) ^ node->cond))
			break;
		if (breaks)
			{
			breaks--;
			if (breaks || !contflag)
				break;
			contflag = 0;
			}
		list = duplist(node->loop);
		execlist(list);
		}
	curjob = cj;
}
 
void execrepeat(comm comm)
{
list list;
struct repeatnode *node;
int cj = curjob;

	loops++;
	node = comm->info;
	exiting = 0;
	while (node->count--)
		{
		list = duplist(node->list);
		execlist(list);
		if (breaks)
			{
			breaks--;
			if (breaks || !contflag)
				break;
			contflag = 0;
			}
		if (lastval)
			break;
		}
	curjob = cj;
}
 
void execif(comm comm)
{
list list;
struct ifnode *node;
int cj = curjob;

	node = comm->info;
	exiting = 0;
	while (node)
		{
		if (node->ifl)
			{
			list = duplist(node->ifl);
			execlist(list);
			if (lastval)
				{
				node = node->next;
				continue;
				}
			}
		list = duplist(node->thenl);
		execlist(list);
		break;
		}
	curjob = cj;
}
 
void execcase(comm comm)
{
list list;
struct casenode *node;
char *word;
table args;
int cj = curjob;

	node = comm->info;
	args = comm->args;
	exiting = 0;
	if (!args->first || args->first->next)
		{
		zerr("bad case statement");
		errflag = 1;
		return;
		}
	word = args->first->dat;
	while (node)
		if (matchpat(word,node->pat))
			break;
		else
			node = node->next;
	if (node)
		{
		list = duplist(node->list);
		execlist(list);
		}
	curjob = cj;
}
 
list duplist(list xlist)
{
list nlist;
 
	if (!xlist)
		return NULL;
	nlist = alloc(sizeof(struct lnode));
	nlist->left = duplist2(xlist->left);
	nlist->right = duplist(xlist->right);
	nlist->type = xlist->type;
	return nlist;
}

void freelist(list xlist)
{
	if (xlist)
		{
		freelist2(xlist->left);
		freelist(xlist->right);
		free(xlist);
		}
}

list2 duplist2(list2 x)
{
list2 y;

	if (!x)
		return NULL;
	y = alloc(sizeof(struct l2node));
	*y = *x;
	y->left = duppline(x->left);
	y->right = duplist2(x->right);
	return y;
}

void freelist2(list2 x)
{
	if (x)
		{
		freepline(x->left);
		freelist2(x->right);
		free(x);
		}
}

pline duppline(pline xpline)
{
pline npline;
 
	if (!xpline)
		return NULL;
	npline = alloc(sizeof(struct pnode));
	npline->left = dupcomm(xpline->left);
	npline->right = duppline(xpline->right);
	npline->type = xpline->type;
	return npline;
}

void freepline(pline x)
{
	if (x)
		{
		freecmd(x->left);
		freepline(x->right);
		free(x);
		}
}

comm dupcomm(comm xcomm)
{
comm ncomm;
void *(*duprs[])(void *) = {dupfor,dupwhile,duprepeat,dupif,dupcase};
int type;
 
	if (!xcomm)
		return NULL;
	ncomm = alloc(sizeof(struct cnode));
	ncomm->left = duplist(xcomm->left);
	ncomm->cmd = dupstr(xcomm->cmd);
	ncomm->args = duptable(xcomm->args,dupstr);
	ncomm->redir = duptable(xcomm->redir,dupfnode);
	ncomm->vars = (xcomm->vars) ? duptable(xcomm->vars,dupstr) : NULL;
	ncomm->type = type = xcomm->type;
	if (type >= CFOR && type <= CCASE)
		ncomm->info = (duprs[type-CFOR])(xcomm->info);
	return ncomm;
}

void freecmd(comm x)
{
	if (x)
		{
		freelist(x->left);
		if (x->cmd)
			free(x->cmd);
		if (x->args)
			freetable(x->args,freestr);
		if (x->redir)
			freetable(x->redir,freeredir);
		if (x->vars)
			freetable(x->vars,freestr);
/*		if (x->info)
			freeinfo(x->info);*/
		free(x);
		}
}

void *dupstr(void *str)
{
	if (!str)
		return NULL;
	return strdup(str);
}

void *dupfnode(void *i)
{
struct fnode *fn = i,*nfn = alloc(sizeof(struct fnode));
 
	if (!i)
		return NULL;
	*nfn = *fn;
	if (nfn->type < HEREDOC)
		nfn->u.name = strdup(fn->u.name);
	return nfn;
}

void *dupfor(void *i)
{
struct fornode *nnode,*node = i;
 
	nnode = alloc(sizeof(struct fornode));
	*nnode = *(struct fornode *) i;
	nnode->name = strdup(node->name);
	nnode->list = duplist(node->list);
	return nnode;
}
 
void *dupcase(void *i)
{
struct casenode *nnode,*node = i;
 
	if (!i)
		return NULL;
	nnode = alloc(sizeof(struct casenode));
	nnode->next = dupcase(node->next);
	nnode->list = duplist(node->list);
	nnode->pat = strdup(node->pat);
	return nnode;
}
 
void *dupif(void *i)
{
struct ifnode *nnode,*node = i;
 
	if (!i)
		return NULL;
	nnode = alloc(sizeof(struct ifnode));
	nnode->next = dupif(node->next);
	nnode->ifl = duplist(node->ifl);
	nnode->thenl = duplist(node->thenl);
	return nnode;
}
 
void *dupwhile(void *i)
{
struct whilenode *nnode,*node = i;
 
	if (!i)
		return NULL;
	nnode = alloc(sizeof(struct whilenode));
	nnode->cond = node->cond;
	nnode->cont = duplist(node->cont);
	nnode->loop = duplist(node->loop);
	return nnode;
}

void *duprepeat(void *i)
{
struct repeatnode *nnode,*node = i;

	if (!i)
		return NULL;
	nnode = alloc(sizeof(struct repeatnode));
	nnode->count = node->count;
	nnode->list = duplist(node->list);
	return nnode;
}	

table duptable(table tab,void *(*func)(void *))
{
table ret;
Node node;

	ret = newtable();
	for (node = tab->first; node; node = node->next)
		addnode(ret,func(node->dat));
	return ret;
}
