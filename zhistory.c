/*

	zhistory.c - interface between readline an zsh's ideas of history

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
#include "readline/history.h"

/* bindings to make zsh and readline get along */

static int histline;

void using_history(void)
{
	histline = cev;
}

int where_history(void)
{
	return histline;
}

int history_set_pos(int pos)
{
	histline = pos;
	return 0;
}

HIST_ENTRY *current_history(void)
{
table tab;
char *str;
HIST_ENTRY *he;

	tab = quietgetevent(histline);
	if (!tab)
		return NULL;
	str = makehlist(tab,0);
	he = (HIST_ENTRY *) malloc(sizeof *he);
	he->line = str;
	he->data = NULL;
	return he;
}

HIST_ENTRY *previous_history(void)
{
	if (histline == tfev)
		return NULL;
	histline--;
	return current_history();
}

HIST_ENTRY *next_history(void)
{
	if (histline == cev)
		return NULL;
	histline++;
	return current_history();
}

char *extracthistarg(int num)
{
Node n;

	if (histlist->last == (Node) histlist ||
			histlist->last->last == (Node) histlist)
		return NULL;
	n = ((table) (histlist->last->last))->first;
	for (; n && num; num--,n = n->next);
	if (!n)
		return NULL;
	return strdup(n->dat);
}

