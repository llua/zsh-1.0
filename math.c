/*

	math.c - evaluating arithmetic expressions

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

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <assert.h>

extern int errflag;
extern char *setiparm(char *,long,int);
extern long getiparm(char *);
extern void zerr(char *,...);
extern char *strdup(char *);

char *ptr;

typedef int LV;

long yyval;
LV yylval;

/* nonzero means we are not evaluating, just parsing */

int noeval = 0;

/* != 0 means recognize unary plus, minus, etc.
	The parser was originally written in lex, hence the name. */

int initial = 1;

void mathparse(int);

/* LR = left-to-right associativity
	RL = right-to-left associativity
	BOO = short-circuiting boolean */

enum xtyp { LR,RL,BOO };

enum xtok {
INPAR, OUTPAR, NOT, COMP, POSTPLUS,
POSTMINUS, UPLUS, UMINUS, AND, XOR,
OR, MUL, DIV, MOD, PLUS,
MINUS, SHLEFT, SHRIGHT, LES, LEQ,
GRE, GEQ, DEQ, NEQ, DAND,
DOR, DXOR, QUEST, COLON, EQ,
PLUSEQ, MINUSEQ, MULEQ, DIVEQ, MODEQ,
ANDEQ, XOREQ, OREQ, SHLEFTEQ, SHRIGHTEQ,
DANDEQ, DOREQ, DXOREQ, COMMA, EOI,
PREPLUS, PREMINUS, NUM, ID, TOKCOUNT
};

/* precedences */

int prec[TOKCOUNT] = {
1,200,2,2,2,
2,2,2,3,4,
5,6,6,6,7,
7,8,8,9,9,
9,9,10,10,11,
12,12,13,13,14,
14,14,14,14,14,
14,14,14,14,14,
14,14,14,15,200,
2,2,0,0,
};

#define TOPPREC 15

int type[TOKCOUNT] = {
LR,LR,RL,RL,RL,
RL,RL,RL,LR,LR,
LR,LR,LR,LR,LR,
LR,LR,LR,LR,LR,
LR,LR,LR,LR,BOO,
BOO,LR,RL,RL,RL,
RL,RL,RL,RL,RL,
RL,RL,RL,RL,RL,
BOO,BOO,RL,RL,RL,
RL,RL,LR,LR,
};

#define LVCOUNT 32

/* table of lvalues (variables) */

int lvc;
char *lvals[LVCOUNT];

int yylex(void)
{
	for(;;)
		switch (*ptr++)
			{
			case '+':
				if (*ptr == '+' && (initial || !isalnum(*ptr)))
					{
					ptr++;
					return (initial) ? PREPLUS : POSTPLUS;
					}
				if (*ptr == '=') { initial = 1; ptr++; return PLUSEQ; }
				return (initial) ? UPLUS : PLUS;
			case '-':
				if (*ptr == '-' && (initial || !isalnum(*ptr)))
					{
					ptr++;
					return (initial) ? PREMINUS : POSTMINUS;
					}
				if (*ptr == '=') { initial = 1; ptr++; return MINUSEQ; }
				return (initial) ? UMINUS : MINUS;
			case '(': initial = 1; return INPAR;
			case ')': return OUTPAR;
			case '!': if (*ptr == '=')
						{ initial = 1; ptr++; return NEQ; }
						return NOT;
			case '~': return COMP;
			case '&': initial = 1;
				if (*ptr == '&') { if (*++ptr == '=')
				{ ptr++; return DANDEQ; } return DAND; }
				else if (*ptr == '=') { ptr++; return ANDEQ; } return AND;
			case '|': initial = 1;
				if (*ptr == '|') { if (*++ptr == '=')
				{ ptr++; return DOREQ; } return DOR; }
				else if (*ptr == '=') { ptr++; return OREQ; } return OR;
			case '^': initial = 1;
				if (*ptr == '^') { if (*++ptr == '=')
				{ ptr++; return DXOREQ; } return DXOR; }
				else if (*ptr == '=') { ptr++; return XOREQ; } return XOR;
			case '*': initial = 1;
				if (*ptr == '=') { ptr++; return MULEQ; } return MUL;
			case '/': initial = 1;
				if (*ptr == '=') { ptr++; return DIVEQ; } return DIV;
			case '%': initial = 1;
				if (*ptr == '=') { ptr++; return MODEQ; } return MOD;
			case '<': initial = 1; if (*ptr == '<')
				{ if (*++ptr == '=') { ptr++; return SHLEFTEQ; } return SHLEFT; }
				else if (*ptr == '=') { ptr++; return LEQ; } return LES;
			case '>': initial = 1; if (*ptr == '>')
				{ if (*++ptr == '=') { ptr++; return SHRIGHTEQ; } return SHRIGHT; }
				else if (*ptr == '=') { ptr++; return GEQ; } return GRE;
			case '=': initial = 1; if (*ptr == '=') { ptr++; return DEQ; }
				return EQ;
			case '?': initial = 1; return QUEST;
			case ':': initial = 1; return COLON;
			case ',': initial = 1; return COMMA;
			case '\0': initial = 1; ptr--; return EOI;
			case '[': initial = 0;
				{ int base = strtol(ptr,&ptr,10);
					yyval = strtol(ptr+1,&ptr,base); return NUM; }
			case ' ': case '\t':
				break;
			default:
				if (isdigit(*--ptr))
					{ initial = 0; yyval = strtol(ptr,&ptr,10); return NUM; }
				if (isalpha(*ptr) || *ptr == '$')
					{
					char *p,q;

					if (*ptr == '$')
						ptr++;
					p = ptr;
					if (lvc == LVCOUNT)
						{
						zerr("too many identifiers in expression");
						errflag = 1;
						return EOI;
						}
					initial = 0;
					while(isalpha(*++ptr));
					q = *ptr;
					*ptr = '\0';
					lvals[yylval = lvc++] = strdup(p);
					*ptr = q;
					return ID;
					}
				zerr("illegal character: %c",*ptr);
				errflag = 1;
				return EOI;
			}
}

/* the value stack */

#define STACKSZ 1000
int tok;			/* last token */
int sp = -1;	/* stack pointer */
struct value {
	LV lval;
	long val;
	} stack[STACKSZ];

void push(long val,LV lval)
{
	sp++;
	stack[sp].val = val;
	stack[sp].lval = lval;
}

long getvar(LV s)
{
long t;

	if (!(t = getiparm(lvals[s])))
		return 0;
	return t;
}

long setvar(LV s,long v)
{
	if (s == -1)
		{
		zerr("lvalue required");
		errflag = 1;
		return 0;
		}
	if (noeval)
		return v;
	setiparm(strdup(lvals[s]),v,0);
	return v;
}

int notzero(int a)
{
	if (a == 0)
		{
		errflag = 1;
		zerr("division by zero");
		return 0;
		}
	return 1;
}

#define pop2() { b = stack[sp--].val; a = stack[sp--].val; }
#define pop3() {c=stack[sp--].val;b=stack[sp--].val;a=stack[sp--].val;}
#define nolval() {stack[sp].lval=-1;}
#define pushv(X) { push(X,-1); }
#define pop2lv() { pop2() lv = stack[sp+1].lval; }
#define set(X) { push(setvar(lv,X),lv); }

void op(int what)
{
long a,b,c;
LV lv;

	switch(what) {
		case NOT: stack[sp].val = !stack[sp].val; nolval(); break;
		case COMP: stack[sp].val = ~stack[sp].val; nolval(); break;
		case POSTPLUS: (void) setvar(stack[sp].lval,stack[sp].val+1); break;
		case POSTMINUS: (void) setvar(stack[sp].lval,stack[sp].val-1); break;
		case UPLUS: nolval(); break;
		case UMINUS: stack[sp].val = -stack[sp].val; nolval(); break;
		case AND: pop2(); pushv(a&b); break;
		case XOR: pop2(); pushv(a^b); break;
		case OR: pop2(); pushv(a|b); break;
		case MUL: pop2(); pushv(a*b); break;
		case DIV: pop2(); if (notzero(b)) pushv(a/b); break;
		case MOD: pop2(); if (notzero(b)) pushv(a%b); break;
		case PLUS: pop2(); pushv(a+b); break;
		case MINUS: pop2(); pushv(a-b); break;
		case SHLEFT: pop2(); pushv(a<<b); break;
		case SHRIGHT: pop2(); pushv(a>>b); break;
		case LES: pop2(); pushv(a<b); break;
		case LEQ: pop2(); pushv(a<=b); break;
		case GRE: pop2(); pushv(a>b); break;
		case GEQ: pop2(); pushv(a>=b); break;
		case DEQ: pop2(); pushv(a==b); break;
		case NEQ: pop2(); pushv(a!=b); break;
		case DAND: pop2(); pushv(a&&b); break;
		case DOR: pop2(); pushv(a||b); break;
		case DXOR: pop2(); pushv(a&&!b||!a&&b); break;
		case QUEST: pop3(); pushv((a)?b:c); break;
		case COLON: break;
		case EQ: pop2lv(); set(b); break;
		case PLUSEQ: pop2lv(); set(a+b); break;
		case MINUSEQ: pop2lv(); set(a-b); break;
		case MULEQ: pop2lv(); set(a*b); break;
		case DIVEQ: pop2lv(); if (notzero(b)) set(a/b); break;
		case MODEQ: pop2lv(); if (notzero(b)) set(a%b); break;
		case ANDEQ: pop2lv(); set(a&b); break;
		case XOREQ: pop2lv(); set(a^b); break;
		case OREQ: pop2lv(); set(a|b); break;
		case SHLEFTEQ: pop2lv(); set(a<<b); break;
		case SHRIGHTEQ: pop2lv(); set(a>>b); break;
		case DANDEQ: pop2lv(); set(a&&b); break;
		case DOREQ: pop2lv(); set(a||b); break;
		case DXOREQ: pop2lv(); set(a&&!b||!a&&b); break;
		case COMMA: pop2(); pushv(b); break;
		case PREPLUS: stack[sp].val = setvar(stack[sp].lval,
			stack[sp].val+1); break;
		case PREMINUS: stack[sp].val = setvar(stack[sp].lval,
			stack[sp].val-1); break;
		default: fprintf(stderr,"whoops.\n"); exit(1);
	}
}

void bop(int tok)
{
	switch (tok) {
		case DAND: case DANDEQ: if (!stack[sp].val) noeval++; break;
		case DOR: case DOREQ: if (stack[sp].val) noeval++; break;
		};
}

long matheval(char *s)
{
int t0;

	for (t0 = 0; t0 != LVCOUNT; t0++)
		lvals[t0] = NULL;
	lvc = 0;
	ptr = s;
	sp = -1;
	mathparse(TOPPREC);
	if (!errflag && sp)
		zerr("arithmetic error: unbalanced stack");
	for (t0 = 0; t0 != lvc; t0++)
		free(lvals[t0]);
	return stack[0].val;
}

/* operator-precedence parse the string and execute */

void mathparse(int pc)
{
	if (errflag)
		return;
	tok = yylex();
	while (prec[tok] <= pc)
		{
		if (errflag)
			return;
		if (tok == NUM)
			push(yyval,-1);
		else if (tok == ID)
			push(getvar(yylval),yylval);
		else if (tok == INPAR)
			{
			mathparse(TOPPREC);
			if (tok != OUTPAR)
				exit(1);
			}
		else if (tok == QUEST)
			{
			int q = stack[sp].val;
			if (!q) noeval++;
			mathparse(prec[QUEST]-1);
			if (!q) noeval--; else noeval++;
			mathparse(prec[QUEST]);
			if (q) noeval--;
			op(QUEST);
			continue;
			}
		else
			{
			int otok = tok,onoeval = noeval;

			if (type[otok] == BOO)
				bop(otok);
			mathparse(prec[otok]-(type[otok] != RL));
			noeval = onoeval;
			op(otok);
			continue;
			}
		tok = yylex();
		}
}

