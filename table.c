/*

	table.c - linked list and hash table management

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

/* get an empty linked list header */

table newtable()
{
table list;
 
	list = alloc(sizeof(*list));
	list->first = 0;
	list->last = (Node) list;
	return list;
}

/* get an empty hash table */

htable newhtable(int size)
{
htable ret;
 
	ret = alloc(sizeof(struct xhtab));
	ret->hsize = size;
	ret->nodes = alloc(size*sizeof(struct hnode *));
	return ret;
}
 
/* Peter Weinberger's hash function */

int hasher(char *s)
{
unsigned hash = 0,g;
 
	for (; *s; s++)
		{
		hash = (hash << 4) + *s;
		if (g = hash & 0xf0000000)
			{
			hash ^= g;
			hash ^= g >> 24;
			}
		}
	return hash;
}

/* add a node to a hash table */

void addhnode(char *nam,void *dat,htable ht,void (*freefunc)(void *))
{
int hval = hasher(nam) % ht->hsize;
struct hnode *hp = ht->nodes[hval],*hn;
 
	for (; hp; hp = hp->hchain)
		if (!strcmp(hp->nam,nam))
			{
			freefunc(hp->dat);
			hp->dat = dat;
			free(nam);
			return;
			}
	hn = (void *) alloc(sizeof(struct hnode));
	hn->nam = nam;
	hn->dat = dat;
	hn->hchain = ht->nodes[hval];
	ht->nodes[hval] = hn;
	if (++ht->ct == ht->hsize*4)
		expandhtab(ht);
}

/* expand hash tables when they get too many entries */

void expandhtab(htable ht)
{
struct hnode *hp,**arr,**ha,*hn;
int osize = ht->hsize,nsize = osize*8;

	ht->hsize = nsize;
	arr = ht->nodes;
	ht->nodes = alloc(nsize*sizeof(struct hnode *));
	for (ha = arr; osize; osize--,ha++)
		for (hn = *ha; hn; )
			{
			addhnode(hn->nam,hn->dat,ht,NULL);
			hp = hn->hchain;
			free(hn);
			hn = hp;
			}
	free(arr);
}

/* get an entry in a hash table */

void *gethnode(char *nam,htable ht)
{
int hval = hasher(nam) % ht->hsize;
struct hnode *hn = ht->nodes[hval];
 
	for (; hn; hn = hn->hchain)
		if (!strcmp(hn->nam,nam))
			return hn->dat;
	return NULL;
}
 
void freehtab(htable ht,void (*freefunc)(void *))
{
int val;
struct hnode *hn,**hptr = &ht->nodes[0],*next;
 
	for (val = ht->hsize; val; val--,hptr++)
		for (hn = *hptr; hn; )
			{
			next = hn->hchain;
			freefunc(hn);
			hn = next;
			}
}

/* remove a hash table entry and return a pointer to it */

void *remhnode(char *nam,htable ht)
{
int hval = hasher(nam) % ht->hsize;
struct hnode *hn = ht->nodes[hval],*hp;
void *dat;

	if (!hn)
		return NULL;
	if (!strcmp(hn->nam,nam))
		{
		ht->nodes[hval] = hn->hchain;
		dat = hn->dat;
		free(hn->nam);
		free(hn);
		ht->ct--;
		return dat;
		}
	for (hp = hn, hn = hn->hchain; hn; hn = (hp = hn)->hchain)
		if (!strcmp(hn->nam,nam))
			{
			hp->hchain = hn->hchain;
			dat = hn->dat;
			free(hn->nam);
			free(hn);
			ht->ct--;
			return dat;
			}
	return NULL;
}
 
void *zalloc(int l)
{
void *z;
 
	if (!(z = malloc(l)))
		{
		zerr("fatal error: out of memory: restarting");
		execl(MYSELF,"zsh","-f",(void *) 0);
		exit(1);
		}
	return z;
}

void *alloc(int l)
{
void *z;
 
	if (!(z = calloc(l,1)))
		{
		zerr("fatal error: out of memory: restarting");
		execl(MYSELF,"zsh","-f",(void *) 0);
		exit(1);
		}
	return z;
}

/* add a node to the end of a linked list */

void addnode(table list,void *str)
{
	insnode(list,list->last,str);
}

/* insert a node in a linked list after 'last' */

void insnode(table list,Node last,void *dat)
{
Node tmp;
 
	tmp = last->next;
	last->next = alloc(sizeof(*tmp));
	last->next->last = last;
	last->next->dat = dat;
	last->next->next = tmp;
	if (tmp)
		tmp->last = last->next;
	else
		list->last = last->next;
}

/* remove a node from a linked list */

void *remnode(table list,Node nd)
{
void *dat;

	nd->last->next = nd->next;
	if (nd->next)
		nd->next->last = nd->last;
	else
		list->last = nd->last;
	free(nd);
	dat = nd->dat;
	return dat;
}

/* delete a character in a string */

void chuck(char *str)
{
	while (str[0] = str[1])
		str++;
}

/* get a node in a linked list */

void *getnode(table list)
{
void *dat;
Node node = list->first;
 
	if (!node)
		return NULL;
	dat = node->dat;
	list->first = node->next;
	if (node->next)
		node->next->last = (Node) list;
	else
		list->last = (Node) list;
	free(node);
	return dat;
}

/* != 0 if the linked list has at least one entry */

int full(table list)
{
	return list->first != NULL;
}
 
void freetable(table tab,void (*freefunc)(void *))
{
Node node = tab->first,next;
 
	while (node)
		{
		next = node->next;
		freefunc(node);
		node = next;
		}
	free(tab);
}
 
char *strdup(char *str)
{
char *ret = zalloc(strlen(str)+1);
 
	strcpy(ret,str);
	return ret;
}

#ifndef STRSTR
const char *strstr(const char *s,const char *t)
{
const char *p1,*p2;
 
	for (; *s; s++)
		{
		for (p1 = s, p2 = t; *p2; p1++,p2++)
			if (*p1 != *p2)
				break;
		if (!*p2)
			 return (char *) s;
		}
	return NULL;
}
#endif

