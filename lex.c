/*

	lex.c - lexical analysis

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

/* match the current token and get another
	(actually just get another) */

void matchit(void)
{
	do
		gettok();
	while (exalias());
}

int len = 0,bsiz = 256;
char *bptr;

/* add a char to the string buffer */

void add(int c)
{
	*bptr++ = c;
	if (bsiz == ++len)
		bptr = len+(tstr = realloc(tstr,bsiz *= 2));
}

/* get a token */

void gettok(void)
{
int bct = 0,pct = 0;
int c,d,intpos = 1;
static int dbparens = 0;

beginning:
	hlastw = NULL;
	tstr = NULL;
	while (zspace(c = hgetc()) || c == '\t' || c == ' ');
	firstln = 0;
	hwbegin();
	hwaddc(c);
	if (dbparens)	/* handle ((...)) */
		{
		int pct = 2;

		peek = STRING;
		len = dbparens = 0;
		bptr = tstr = zalloc(bsiz = 256);
		for (;;)
			{
			if (c == '(')
				pct++;
			else if (c == ')')
				pct--;
			else if (c == '\n')
				{
				zerr("parse error: )) expected");
				peek = HERR;
				free(tstr);
				return;
				}
			else if (c == '$')
				c = Qstring;
			if (pct >= 2)
				add(c);
			if (pct)
				c = hgetc();
			else
				break;
			}
		*bptr = '\0';
		return;
		}
	peekfd = -1;
	if (isdigit(c))	/* handle 1< foo */
		{
		d = hgetc();
		hungetc(d);
		if (d == '>' || d == '<')
			{
			peekfd = c-'0';
			c = hgetc();
			}
		}

	/* chars in initial position in word */

	switch (c)
		{
		case '\\':
			d = hgetc();
			if (d == '\n')
				goto beginning;
			hungetc(d);
			break;
		case EOF:
			peek = EOF;
			return;
		case HERR:
			peek = HERR;
			return;
		case '\n':
			peek = NEWLIN;
			return;
		case ';':
			d = hgetc();
			if (d != ';')
				{
				hungetc(d);
				peek = SEMI;
				}
			else
				peek = DSEMI;
			return;
		case '!':
			if (!incmd)
				{
				peek = BANG;
				return;
				}
			break;
		case '&':
			d = hgetc();
			if (d != '&')
				{
				hungetc(d);
				peek = AMPER;
				}
			else
				peek = DAMPER;
			return;
		case '|':
			d = hgetc();
			if (d == '|')
				peek = DBAR;
			else if (d == '&')
				peek = BARAMP;
			else
				{
				hungetc(d);
				peek = BAR;
				}
			return;
		case '(':
			if (incmd)
				break;
			d = hgetc();
			if (d == '(')
				{
				peek = STRING;
				tstr = strdup("let");
				dbparens = 1;
				return;
				}
			hungetc(d);
			peek = INPAR;
			return;
		case ')':
			peek = OUTPAR;
			return;
		case '{':
			if (incmd)
				break;
			peek = INBRACE;
			return;
		case '}':
			peek = OUTBRACE;
			return;
		case '<':
			d = hgetc();
			if (incmd && d == '(')
				{
				hungetc(d);
				break;
				}
			else if (d == '<')
				{
				int e = hgetc();

				hungetc(e);
				if (e == '(')
					{
					hungetc(d);
					peek = INANG;
					}
				else
					peek = DINANG;
				}
			else if (d == '&')
				peek = INANGAMP;
			else
				{
				peek = INANG;
				hungetc(d);
				}
			return;
		case '>':
			d = hgetc();
			if (d == '(')
				{
				hungetc(d);
				break;
				}
			else if (d == '&')
				{
				d = hgetc();
				if (d == '!')
					peek = OUTANGAMPBANG;
				else
					{
					hungetc(d);
					peek = OUTANGAMP;
					}
				}
			else if (d == '!')
				peek = OUTANGBANG;
			else if (d == '>')
				{
				d = hgetc();
				if (d == '&')
					{
					d = hgetc();
					if (d == '!')
						peek = DOUTANGAMPBANG;
					else
						{
						hungetc(d);
						peek = DOUTANGAMP;
						}
					}
				else if (d == '!')
					peek = DOUTANGBANG;
				else if (d == '(')
					{
					hungetc(d);
					hungetc('>');
					peek = OUTANG;
					}
				else
					{
					hungetc(d);
					peek = DOUTANG;
					}
				}
			else
				{
				hungetc(d);
				peek = OUTANG;
				}
			return;
		case '#':
#ifndef INTERACTIVE_COMMENTS
			if (interact)
				break;
#endif
			while ((c = hgetch()) != '\n' && !istok(c) && c != EOF);
			if (c == '\n')
				peek = NEWLIN;
			else
				errflag = 1;
			return;
		}

	/* we've started a string, now get the rest of it, performing
		tokenization */

	peek = STRING;
	len = 0;
	bptr = tstr = zalloc(bsiz = 256);
	for(;;)
		{
		if (c == ';' || c == '&' || c == EOF ||
				c == HERR || c == '\03' || c == '\n' || 
				c == ' ' || c == '\t' || znspace(c))
			break;
		if (c == '#')
			c = Pound;
		else if (c == ')')
			{
			if (!pct)
				break;
			pct--;
			c = Outpar;
			}
		else if (c == ',')
			c = Comma;
		else if (c == '|')
			{
			if (!pct)
				break;
			c = Bar;
			}
		else if (c == '$')
			{
			d = hgetc();

			c = String;
			if (d == '[')
				{
				add(String);
				add(Inbrack);
				while ((c = hgetc()) != ']' && !istok(c) && c != EOF)
					add(c);
				c = Outbrack;
				}
			else if (d == '(')
				{
				add(String);
				skipcomm();
				c = Outpar;
				}
			else
				hungetc(d);
			}
		else if (c == '^')
			c = Hat;
		else if (c == '[')
			c = Inbrack;
		else if (c == ']')
			c = Outbrack;
		else if (c == '*')
			c = Star;
		else if (intpos && c == '~')
			c = Tilde;
		else if (c == '?')
			c = Quest;
		else if (c == '(')
			{
			int d = hgetc();

			hungetc(d);
			if (!incmd)
				break;
#if 0
			if (d != ')' && intpos)
				{
				add(Inang);
				skipcomm();
				c = Outpar;
				}
			else
#endif
				{
				pct++;
				c = Inpar;
				}
			}
		else if (c == '{')
			{
			c = Inbrace;
			bct++;
			}
		else if (c == '}')
			{
			if (!bct)
				break;
			c = Outbrace;
			bct--;
			}
		else if (c == '>')
			{
			d = hgetc();
			if (d != '(')
				{
				hungetc(d);
				break;
				}
			add(Outang);
			skipcomm();
			c = Outpar;
			}
		else if (c == '<')
			{
			d = hgetc();
			if (!(isdigit(d) || d == '-' || d == '>' || d == '(' || d == ')'))
				{
				hungetc(d);
				break;
				}
			c = Inang;
			if (d == '(')
				{
				add(c);
				skipcomm();
				c = Outpar;
				}
			else if (d == ')')
				hungetc(d);
			else
				{
				add(c);
				c = d;
				while (c != '>' && !istok(c) && c != EOF)
					add(c),c = hgetc();
				if (c == EOF)
					{
					errflag = 1;
					peek = EOF;
					return;
					}
				c = Outang;
				}
			}
		else if (c == '=')
			{
			if (intpos)
				{
				d = hgetc();
				if (d != '(')
					{
					hungetc(d);
					c = Equals;
					}
				else
					{
					add(Equals);
					skipcomm();
					c = Outpar;
					}
				}
			else if (peek != ENVSTRING)
				{
				peek = ENVSTRING;
				intpos = 2;
				}
			}
		else if (c == '\\')
			{
			c = hgetc();

			if (c == '\n')
				{
				c = hgetc();
				continue;
				}
			add(c);
			c = hgetc();
			continue;
			}
		else if (c == '\'')
			{
			add(Nularg);

			/* we add the Nularg to prevent this:

			echo $PA'TH'

			from printing the path. */

			while ((c = hgetc()) != '\'' && !istok(c) && c != EOF)
				add(c);
			if (c == EOF)
				{
				errflag = 1;
				peek = EOF;
				return;
				}
			c = Nularg;
			}
		else if (c == HQUOT)
			{
			add(Nularg);
			while ((c = hgetc()) != HQUOT && !istok(c) && c != EOF)
				add(c);
			if (c == EOF)
				{
				errflag = 1;
				peek = EOF;
				return;
				}
			c = Nularg;
			}
		else if (c == '\"')
			{
			add(Nularg);
			while ((c = hgetc()) != '\"' && !istok(c) && c != EOF)
				if (c == '\\')
					{
					c = hgetc();
					if (c != '\n')
						{
						if (c != '$' && c != '\\' && c != '\"' && c != '`')
							add('\\');
						add(c);
						}
					}
				else
					{
					if (c == '$')
						{
						int d = hgetc();

						if (d == '(')
							{
							add(Qstring);
							skipcomm();
							c = Outpar;
							}
						else if (d == '[')
							{
							add(String);
							add(Inbrack);
							while ((c = hgetc()) != ']' && c != EOF)
								add(c);
							c = Outbrack;
							}
						else
							{
							c = Qstring;
							hungetc(d);
							}
						}
					else if (c == '`')
						c = Qtick;
					add(c);
					}
			if (c == EOF)
				{
				errflag = 1;
				peek = EOF;
				return;
				}
			c = Nularg;
			}
		else if (c == '`')
			{
			add(Tick);
			while ((c = hgetc()) != '`' && !istok(c) && c != EOF)
				if (c == '\\')
					{
					c = hgetc();
					if (c != '\n')
						{
						if (c != '`' && c != '\\' && c != '$')
							add('\\');
						add(c);
						}
					}
				else
					{
					if (c == '$')
						c = String;
					add(c);
					}
			if (c == EOF)
				{
				errflag = 1;
				peek = EOF;
				return;
				}
			c = Tick;
			}
		add(c);
		c = hgetc();
		if (intpos)
			intpos--;
		}
	if (c == HERR)
		{
		free(tstr);
		peek = HERR;
		return;
		}
	hungetc(c);
	*bptr = '\0';
}

/* expand aliases, perhaps */

int exalias(void)
{
struct anode *an;
char *s;

	s = hwadd();
	if (alix != MAXAL && (an = gethnode(s,alhtab)) && !an->inuse &&
			!(an->cmd && incmd && alstat != ALSTAT_MORE))
		{
		if (an->cmd < 0)
			{
			peek = DO-an->cmd-1;
			return 0;
			}
		an->inuse = 1;
		hungets(strdup(ALPOPS));
		hungets(strdup((alstack[alix++] = an)->text));
		alstat = 0;
		if (tstr)
			free(tstr);
		return 1;
		}
	return 0;
}

/* != 0 if c is not a newline, but in IFS */

int zspace(int c)
{
	if (c == '\n')
		return 0;
	return znspace(c);
}

/* != 0 if c is in IFS */

int znspace(int c)
{
char *ptr = ifs;

	while (*ptr)
		if (*ptr++ == c)
			return 1;
	return 0;
}

/* skip (...) */

void skipcomm(void)
{
int pct = 1,c;

	c = Inpar;
	do
		{
		add(c);
reget:
		c = hgetc();
		if (istok(c) || c == EOF)
			break;
		else if (c == '(') pct++;
		else if (c == ')') pct--;
		else if (c == '\\')
			{
			add(c);
			c = hgetc();
			}
		else if (c == '\'')
			{
			add(c);
			while ((c = hgetc()) != '\'' && !istok(c) && c != EOF)
				add(c);
			}
		else if (c == HQUOT)
			{
			while ((c = hgetc()) != HQUOT && !istok(c) && c != EOF)
				add(c);
			goto reget;
			}
		else if (c == '\"')
			{
			add(c);
			while ((c = hgetc()) != '\"' && !istok(c) && c != EOF)
				if (c == '\\')
					{
					add(c);
					add(hgetc());
					}
				else add(c);
			}
		else if (c == '`')
			{
			add(c);
			while ((c = hgetc()) != '`' && c != HERR && c != EOF)
				if (c == '\\') add(c), add(hgetc());
				else add(c);
			}
		}
	while(pct);
}

