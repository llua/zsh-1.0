/*

	builtin.c - handles builtin commands

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
#include <sys/signal.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <utmp.h>
#define MAXPATHLEN 1024

int echo(comm comm)
{
table list;
char *str;
int nline = 1;
 
	list = comm->args;
	if (full(list))
		if (!strcmp("--",list->first->dat))
			free(getnode(list));
		else if (!strcmp("-n",list->first->dat))
			{
			free(getnode(list));
			nline = 0;
			}
	if (str = getnode(list))
		{
		printf("%s",str);
		free(str);
		while (str = getnode(list))
			{
			printf(" %s",str);
			free(str);
			}
		}
	if (nline)
		printf("\n");
	return 0;
}

/* print the directory stack */

void pdstack(void)
{
Node node;

	printdir(cwd);
	for (node = dirstack->first; node; node = node->next)
		{
		putchar(' ');
		printdir(node->dat);
		}
	putchar('\n');
}

/* exit */

int zexit(comm comm)
{
	if (interact)
		if (!stopmsg)
			{
			checkjobs();
			if (stopmsg)
				{
				stopmsg = 2;
				return 1;
				}
			}
		else
			killrunjobs();
	if (islogin && unset(NORCS))
		sourcehome(".zlogout");
	if (comm && full(comm->args))
		lastval = atoi(getnode(comm->args));
	if (sigtrapped[SIGEXIT])
		dotrap(SIGEXIT);
	exit(lastval); return 0;
}

/* return */

int zreturn(comm comm)
{
	if (full(comm->args))
		lastval = atoi(getnode(comm->args));
	retflag = 1;
	return lastval;
}

int logout(comm comm)
{
	if (!islogin)
		{
		zerrnam("logout","not login shell");
		return 1;
		}
	return zexit(comm);
}

int Unset(comm comm)
{
table list = comm->args;
char *s;
 
	while (full(list))
		{
		s = getnode(list);
		unsetparm(s);
		free(s);
		}
	return 0;
}

int set(comm comm)
{
char *s,*t;

	if (!full(comm->args))
		{
		char **p = environ;

		while (*p)
			puts(*p++);
		listhtable(parmhtab,(void (*)(char *,char *)) pparm);
		return 0;
		}
	s = getnode(comm->args);
	t = getnode(pparms);
	freetable(pparms,freestr);
	pparms = newtable();
	addnode(pparms,t);
	while (s)
		{
		addnode(pparms,s);
		s = getnode(comm->args);
		}
	return 0;
}

struct option {
	char *name;
	char id;
	};

static struct option optns[] = {
	"clobber",'1',
	"nobadpattern",'2',
	"nonomatch",'3',
	"globdots",'4',
	"notify",'5',
	"allexport",'a',
	"errexit",'e',
	"bgnice",'6',
	"ignoreeof",'7',
	"keyword",'k',
	"markdirs",'8',
	"monitor",'m',
	"noexec",'n',
	"noglob",'F',
	"norcs",'f',
	"shinstdin",'s',
	"nounset",'u',
	"verbose",'v',
	"xtrace",'x',
	"interactive",'i',
	"autolist",'9',
	"correct",'0',
	"dextract",'A',
	"nobeep",'B',
	"printexitvalue",'C',
	"pushdtohome",'D',
	"pushdsilent",'E',
	"nullglob",'G',
	"rmstarsilent",'H',
	"ignorebraces",'I',
	"cdablevars",'J',
	"nobanghist",'K',
	NULL,0
};

int setopt(comm comm)
{
	return csetopt(comm,0);
}

int unsetopt(comm comm)
{
	return csetopt(comm,1);
}

/* common code for setopt and unsetopt */

int csetopt(comm comm,int isun)
{
char *s,*os,*cmd;
int flag;
struct option *opp;

	cmd = (isun) ? "unsetopt" : "setopt";
	if (!full(comm->args))
		{
		if (isun)
			return 0;
		for (opp = optns; opp->name; opp++)
			if (opts[opp->id] == OPT_SET)
				puts(opp->name);
		return 0;
		}
	while ((os = s = getnode(comm->args)) && ((flag = *s == '-') || *s == '+'))
		{
		while (*++s)
			if (*s == INTERACTIVE || *s == MONITOR)
				zerrnam(cmd,"can't change that option");
			else if (opts[*s & 0x7f] != OPT_INVALID)
				opts[*s & 0x7f] = (flag^isun) ? OPT_SET : OPT_UNSET;
			else
				zerrnam(cmd,"illegal option: %c",*s);
		free(os);
		}
	if (!s)
		return 0;
	while (s)
		{
		for (opp = optns; opp->name; opp++)
			if (!strcmp(opp->name,s))
				break;
		if (opp->name)
			{
			if (opp->id == INTERACTIVE || opp->id == MONITOR)
				zerrnam(cmd,"can't change that option");
			else
				opts[opp->id] = (isun) ? OPT_UNSET : OPT_SET;
			}
		else
			{
			zerrnam(cmd,"no such option: %s",s);
			free(s);
			return 1;
			}
		free(s);
		s = getnode(comm->args);
		}
	return 0;
}

/* print a positional parameter */

void pparm(char *s,struct pmnode *t)
{
	if (s && t)
		if (t->isint)
			printf("%s=%ld\n",s,t->u.val);
		else
			{
			printf("%s=",s);
			niceprint(t->u.str);
			putchar('\n');
			}
}

int dirs(comm comm)
{
	if (comm->args->first)
		{
		if (dirstack)
			freetable(dirstack,freestr);
		dirstack = comm->args;
		comm->args = NULL;
		}
	else
		pdstack();
	return 0;
}

/* call func once for each entry in a hash table */

void listhtable(htable ht,void (*func)(char *,char *))
{
int t0;
struct hnode *hn;

	for (t0 = ht->hsize-1; t0 >= 0; t0--)
		for (hn = ht->nodes[t0]; hn; hn = hn->hchain)
			func(hn->nam,hn->dat);
}

/* print an alias (used with listhtable) */

void palias(char *s,struct anode *t)
{
	if (t && t->cmd >= 0)
		{
		printf((t->cmd) ? "alias %-13s " : "alias -a %-10s ",s);
		niceprint(t->text);
		putchar('\n');
		}
}

/* print a shell function (used with listhtable) */

void pshfunc(char *s,list l)
{
char *t;

	t = getltext(l);
	untokenize(t);
	printf("%s() {\n%s\n}\n",s,t);
	free(t);
}

void niceprint(char *s)
{
	niceprintf(s,stdout);
}

void niceprintf(char *s,FILE *f)
{
	for (; *s; s++)
		{
		if (*s >= 32 && *s <= 126)
			fputc(*s,f);
		else if (*s == '\n')
			{
			putc('\\',f);
			putc('n',f);
			}
		else
			{
			putc('^',f);
			fputc(*s | 0x40,f);
			}
		}
}

/* build a command line from a linked list */

char *buildline(table t)
{
char *str = strdup(""),*s,*os;

	while (s = getnode(t))
		{
		os = str;
		str = (*os) ? tricat(os," ",s) : strdup(s);
		free(s);
		free(os);
		}
	return str;
}

int Alias(comm comm)
{
char *s1,*s2;
int anyflag = 0;

	if (!(s1 = getnode(comm->args)))
		{
		listhtable(alhtab,(void (*)(char *,char *)) palias);
		return 0;
		}
	if (!strcmp(s1,"-a"))
		{
		anyflag = 1;
		free(s1);
		if (!(s1 = getnode(comm->args)))
			{
			zerrnam("alias","alias -a requires 2 arguments");
			return 1;
			}
		}
	if (!comm->args->first)
		{
		palias(s1,gethnode(s1,alhtab));
		free(s1);
		return 0;
		}
	s2 = buildline(comm->args);
	addhnode(s1,mkanode(s2,!anyflag),alhtab,freeanode);
	return 0;
}

int cd(comm comm)
{
	if (!full(comm->args))
		return chcd("cd",strdup(getparm("HOME")));
	if (comm->args->first->next)
		{
		char *s,*t,*u,*v;
		int sl,tl;

		if (comm->args->first->next->next)
			{
			zerrnam("cd","too many arguments");
			return 1;
			}
		s = getnode(comm->args);
		t = getnode(comm->args);
		if (!(u = (char *) strstr(cwd,s)))
			{
			zerrnam("cd","string not in pwd: %s",s);
			return 1;
			}
		sl = strlen(s);
		tl = strlen(t);
		v = zalloc((u-cwd)+tl+strlen(u+sl)+1);
		strncpy(v,cwd,u-cwd);
		strcpy(v+(u-cwd),t);
		strcat(v,u+sl);
		free(s);
		free(t);
		return chcd("cd",v);
		}
	return chcd("cd",getnode(comm->args));
}

int dot(comm comm)
{
char *s;

	if (!full(comm->args))
		return 1;
	s = getnode(comm->args);
	if (source(s))
		{
		zerrnam(".","%e: %s",errno,s);
		free(s);
		return 1;
		}
	free(s);
	return 0;
}

int Umask(comm comm)
{
char *s,*t;
int t0;

	if (!full(comm->args))
		{
		t0 = umask(0);
		umask(t0);
		printf("%03o\n",t0);
		return 0;
		}
	s = getnode(comm->args);
	t0 = strtol(s,&t,8);
	if (*t)
		{
		zerrnam("umask","bad umask");
		free(s);
		return 1;
		}
	umask(t0);
	free(s);
	return 0;
}

int which(comm comm)
{
struct chnode *chn;
struct anode *a;
char *str = getnode(comm->args),*cnam;

	if (!str)
		{
		zerrnam("which","argument required");
		return 1;
		}
	if ((a = gethnode(str,alhtab)) && a->cmd)
		{
		if (a->cmd < 0)
			printf("%s: shell reserved word\n",str);
		else
			printf("%s: aliased to %s\n",str,a->text);
		free(str);
		return 0;
		}
	if (gethnode(str,shfunchtab))
		printf("%s: shell function\n",str);
	else if (chn = gethnode(str,chtab))
		{
		if (chn->type != BUILTIN)
			puts(chn->u.nam);
		else
			printf("%s: shell built-in command\n",str);
		}
	else if (!(cnam = findcmd(str)))
		{
		zerr("command not found: %s",str);
		free(str);
		return 1;
		}
	else
		puts(cnam);
	free(str);
	return 0;
}
 
int popd(comm comm)
{
int val = 0;
char *s;
Node node;

	if (comm->args->first && *(s = comm->args->first->dat) == '+')
		val = atoi(s+1);
	if (val--)
		{
		if (val < 0)
			node = dirstack->last;
		else
			for (node = dirstack->first; val && node; node = node->next,
				val--);
		free(remnode(dirstack,node));
		if (unset(PUSHDSILENT))
			pdstack();
		return 0;
		}
	else
		{
		if (!full(dirstack))
			{
			zerrnam("popd","dir stack empty");
			return 1;
			}
		val = chcd("popd",getnode(dirstack));
		if (unset(PUSHDSILENT))
			pdstack();
		return val;
		}
}
 
int pushd(comm comm)
{
char *s;
int num;

	if (comm->args->first)
		{
		s = getnode(comm->args);
		
		if (*s == '+')
			{
			if (!(num = atoi(s+1)))
				{
				free(s);
				return 0;
				}
			free(s);
			if (isset(DEXTRACT))
				{
				Node n = dirstack->first;

				insnode(dirstack,(Node) dirstack,strdup(cwd));
				while (--num && n)
					n = n->next;
				if (!n)
					{
					zerrnam("pushd","not that many dir stack entries");
					return 1;
					}
				insnode(dirstack,(Node) dirstack,remnode(dirstack,n));
				}
			else
				{
				addnode(dirstack,strdup(cwd));
				while(--num)
					addnode(dirstack,getnode(dirstack));
				}
			num = chcd("pushd",getnode(dirstack));
			if (unset(PUSHDSILENT))
				pdstack();
			return num;
			}
		pushnode(dirstack,strdup(cwd));
		num = chcd("pushd",s);
		if (num)
			free(getnode(dirstack));
		else if (unset(PUSHDSILENT))
			pdstack();
		return num;
		}
	else
		{
		char *s;
		
		if (isset(PUSHDTOHOME))
			s = strdup(getparm("HOME"));
		else
			s = getnode(dirstack);
		if (!s)
			{
			zerrnam("pushd","dir stack empty");
			return 1;
			}
		pushnode(dirstack,strdup(cwd));
		num = chcd("pushd",s);
		if (num)
			free(getnode(dirstack));
		else if (unset(PUSHDSILENT))
			pdstack();
		return num;
		}
}

/* common code for pushd, popd, cd */

int chcd(char *cnam,char *cd)
{
char *s,*t;
char buf[MAXPATHLEN],*new = cd;
int t0,val,esav,pnew = 0;

	if (cd[0] == '-' && !cd[1])
		{
		free(cd);
		cd = getparm("OLDPWD");
		cd = strdup((cd) ? cd : ".");
		}
	if (*cd == '/')
		{
		val = chdir(new = cd);
		esav = errno;
		}
	else
		for (t0 = 0; t0 != cdpathct; t0++)
			{
			sprintf(buf,"%s/%s",cdpath[t0],cd);
			if ((val = chdir(new = buf)) != -1)
				{
				if (t0)
					pnew = 1;
				break;
				}
			if (t0 && errno != ENOENT && errno != ENOTDIR)
				zerrnam(cnam,"warning: %e: %s",errno,buf);
			if (!t0)
				esav = errno;
			}
	if (val == -1 && errno == ENOENT)
		{
		t = strdup(cd);
		if (isset(CDABLEVARS) && (s = getparm(t)) && *s == '/')
			if (chdir(new = s) != -1)
				{
				val = 0;
				pnew = 1;
				goto goneto;
				}
		free(t);
		zerrnam(cnam,"%e: %s",esav,cd);
		free(cd);
		return 1;
		}
goneto:
	if (val == -1)
		{
		zerrnam(cnam,"%e: %s",esav,cd);
		free(cd);
		return 1;
		}
	else
		{
		list l;

		if (cwd)
			setparm(strdup("OLDPWD"),cwd,0,0);
		cwd = findcwd(new);
		setparm(strdup("PWD"),strdup(cwd),0,0);
		if (pnew)
			{
			printdir(cwd);
			putchar('\n');
			}
		if (l = gethnode("chpwd",shfunchtab))
			newrunlist(l);
		}
	return 0;
}
 
int shift(comm comm)
{
char *s;
int sh = 1;
 
	if (comm->args->first && (s = comm->args->first->dat))
		sh = atoi(s);
	while (sh-- && pparms->first->next)
		remnode(pparms,pparms->first->next);
	return 0;
}
 
int unhash(comm comm)
{
char *s;
 
	if (!(s = getnode(comm->args)))
		{
		zerrnam("unhash","argument required");
		return 1;
		}
	while (s)
		{
		if (!gethnode(s,chtab))
			{
			zerrnam("unhash","not in command table: %s",s);
			return 1;
			}
		free(remhnode(s,chtab));
		free(s);
		s = getnode(comm->args);
		}
	return 0;
}

int rehash(comm comm)
{
	parsepath();
	return 0;
}

int hash(comm comm)
{
char *s,*t;
struct chnode *chn;

	if (!(s = getnode(comm->args)) || !(t = getnode(comm->args)))
		{
		zerrnam("hash","not enough arguments");
		if (s)
			free(s);
		return 1;
		}
	chn = alloc(sizeof(struct chnode));
	chn->type = EXCMD_PREDOT;
	chn->globstat = GLOB;
	chn->u.nam = t;
	addhnode(s,chn,chtab,freechnode);
	return 0;
}

int Break(comm comm)
{
char *s;
 
	if (!loops)
		{
		zerrnam("break","not in loop");
		return 1;
		}
	if (!(s = getnode(comm->args)))
		breaks = 1;
	else if (atoi(s))
		{
		breaks = atoi(s);
		free(s);
		}
	else
		{
		zerrnam("break","numeric argument required");
		free(s);
		return 1;
		}
	return 0;
}
 
int colon(comm comm)
{
	return 0;
}
 
int Glob(comm comm)
{
struct chnode *chn;
char *s;
 
	while (s = getnode(comm->args))
		{
		chn = gethnode(s,chtab);
		free(s);
		if (chn)
			chn->globstat = GLOB;
		}
	return 0;
}
 
int noglob(comm comm)
{
struct chnode *chn;
char *s;
 
	while (s = getnode(comm->args))
		{
		chn = gethnode(s,chtab);
		free(s);
		if (chn)
			chn->globstat = NOGLOB;
		}
	return 0;
}
 
int mostglob(comm comm)
{
struct chnode *chn;
char *s;
 
	while (s = getnode(comm->args))
		{
		chn = gethnode(s,chtab);
		free(s);
		if (chn)
			chn->globstat = MOSTGLOB;
		}
	return 0;
}

int unfunction(comm comm)
{
char *s1;
list l;

	while (s1 = getnode(comm->args))
		{
		unsettrap(s1);
		if (l = remhnode(s1,shfunchtab))
			freelist(l);
		free(s1);
		}
	return 0;
}

int unalias(comm comm)
{
char *s1;
struct anode *an;

	while (s1 = getnode(comm->args))
		{
		if (an = remhnode(s1,alhtab))
			freeanode(an);
		free(s1);
		}
	return 0;
}

/* != 0 if s is a prefix of t */

int prefix(char *s,char *t)
{
	while (*s && *t && *s == *t) s++,t++;
	return (!*s);
}

/* convert %%, %1, %foo, %?bar? to a job number */

int getjob(char *s,char *prog)
{
int t0,retval;
char *os = s;

	if (*s++ != '%')
		{
		zerrnam(prog,"bad job specifier");
		retval = -1; goto done;
		}
	if (*s == '%' || *s == '+' || !*s)
		{
		if (topjob == -1)
			{
			zerrnam(prog,"no current job");
			retval = -1; goto done;
			}
		retval = topjob; goto done;
		}
	if (*s == '-')
		{
		if (prevjob == -1)
			{
			zerrnam(prog,"no previous job");
			retval = -1; goto done;
			}
		retval = prevjob; goto done;
		}
	if (isdigit(*s))
		{
		t0 = atoi(s);
		if (t0 && t0 < MAXJOB && jobtab[t0].stat && t0 != curjob)
			{ retval = t0; goto done; }
		zerrnam(prog,"no such job");
		retval = -1; goto done;
		}
	if (*s == '?')
		{
		struct procnode *pn;

		for (t0 = 0; t0 != MAXJOB; t0++)
			if (jobtab[t0].stat && t0 != curjob)
				for (pn = jobtab[t0].procs; pn; pn = pn->next)
					if (strstr(pn->text,s+1))
						{ retval = t0; goto done; }
		zerrnam(prog,"job not found: %s",s);
		retval = -1; goto done;
		}
	for (t0 = 0; t0 != MAXJOB; t0++)
		if (jobtab[t0].stat && jobtab[t0].procs && t0 != curjob && 
				prefix(s,jobtab[t0].procs->text))
			{ retval = t0; goto done; }
	zerrnam(prog,"job not found: %s",s);
	retval = -1;
done:
	free(os);
	return retval;
}

int fg(comm comm)
{
char *s1;
int ocj = curjob,job;

	scanjobs();
	if (!jobbing)
		{
		zerr("no job control in this shell.");
		return 1;
		}
	if (s1 = getnode(comm->args))
		job = getjob(s1,"fg");
	else
		{
		if (topjob == -1 || topjob == curjob)
			{
			zerrnam("fg","no current job");
			return 1;
			}
		job = topjob;
		}
	if (job == -1)
		return 1;
	makerunning(jobtab+job);
	if (topjob == job)
		{
		topjob = prevjob;
		prevjob = job;
		}
	if (prevjob == job)
		setprevjob();
	printjob(jobtab+job,-1);
	curjob = job;
	if (strcmp(jobtab[job].cwd,cwd))
		{
		printf("(pwd: ");
		printdir(jobtab[job].cwd);
		printf(")\n");
		}
	settyinfo(&jobtab[job].ttyinfo);
	attachtty(jobtab[job].gleader);
	killpg(jobtab[job].gleader,SIGCONT);
	waitjobs();
	curjob = ocj;
	return 0;
}

int bg(comm comm)
{
char *s1;
int job,stopped;

	scanjobs();
	if (!jobbing)
		{
		zerr("no job control in this shell.");
		return 1;
		}
	if (s1 = getnode(comm->args))
		job = getjob(s1,"bg");
	else
		{
		if (topjob == -1 || topjob == curjob)
			{
			zerrnam("bg","no current job");
			return 1;
			}
		job = topjob;
		}
	if (job == -1)
		return 1;
	if (!(jobtab[job].stat & STAT_STOPPED))
		{
		zerrnam("bg","job already in background");
		return 1;
		}
	stopped = jobtab[job].stat & STAT_STOPPED;
	if (stopped)
		makerunning(jobtab+job);
	if (topjob == job)
		{
		topjob = prevjob;
		prevjob = -1;
		}
	if (prevjob == job)
		prevjob = -1;
	if (prevjob == -1)
		setprevjob();
	if (topjob == -1)
		{
		topjob = prevjob;
		setprevjob();
		}
	printjob(jobtab+job,(stopped) ? -1 : 0);
	if (stopped)
		killpg(jobtab[job].gleader,SIGCONT);
	return 0;
}

int jobs(comm comm)
{
int job,lng = 0;

	if (full(comm->args))
		{
		if (comm->args->first->next || (strcmp(comm->args->first->dat,"-l")
				&& strcmp(comm->args->first->dat,"-p")))
			{
			zerrnam("jobs","usage: jobs [ -lp ]");
			return 1;
			}
		lng = (strcmp(comm->args->first->dat,"-l")) ? 2 : 1;
		}
	for (job = 0; job != MAXJOB; job++)
		if (job != curjob && jobtab[job].stat)
			printjob(job+jobtab,lng);
	stopmsg = 2;
	return 0;
}

int Kill(comm comm)
{
int sig = SIGTERM;
char *s;

	s = getnode(comm->args);
	if (s && *s == '-')
		{
		if (isdigit(s[1]))
			sig = atoi(s+1);
		else
			{
			if (s[1] == 'l' && s[2] == '\0')
				{
				printf("%s",sigs[0]);
				for (sig = 1; sig != SIGCOUNT; sig++)
					printf(" %s",sigs[sig]);
				putchar('\n');
				return 0;
				}
			for (sig = 0; sig != SIGCOUNT; sig++)
				if (!strcmp(sigs[sig],s+1))
					break;
			if (sig == SIGCOUNT)
				{
				zerrnam("kill","unknown signal: SIG%s",s+1);
				zerrnam("kill","type kill -l for a list of signals");
				return 1;
				}
			}
		s = getnode(comm->args);
		}
	while (s)
		{
		if (*s == '%')
			{
			int job = getjob(s,"kill");

			if (killjb(jobtab+job,sig) == -1)
				{
				zerrnam("kill","kill failed: %e",errno);
				return 1;
				}
			if (jobtab[job].stat & STAT_STOPPED && sig == SIGCONT)
				jobtab[job].stat &= ~STAT_STOPPED;
			if (sig != SIGKILL)
				killpg(jobtab[job].gleader,SIGCONT);
			}
		else
			if (kill(atoi(s),sig) == -1)
				{
				zerrnam("kill","kill failed: %e");
				return 1;
				}
		s = getnode(comm->args);
		}
	return 0;
}

int export(comm comm)
{
char *s,*t;
struct pmnode *pm;

	while (s = getnode(comm->args))
		{
		if (t = strchr(s,'='))
			{
			*t = '\0';
			if (pm = gethnode(s,parmhtab))
				freepm(remhnode(s,parmhtab));
			*t = '=';
			putenv(s);
			}
		else
			{
			if (!(pm = gethnode(s,parmhtab)))
				{
				if (!getenv(s))
					{
					zerrnam("export","parameter not set: %s",s);
					free(s);
					return 1;
					}
				}
			else if (pm->isint)
				{
				zerrnam("export","can't export integer parameters");
				free(s);
				return 1;
				}
			else
				{
				t = tricat(s,"=",pm->u.str);
				putenv(t);
				free(remhnode(s,parmhtab));
				}
			}
		}
	return 0;
}

int integer(comm comm)
{
char *s,*t;
struct pmnode *uu;

	while (s = getnode(comm->args))
		{
		if (t = strchr(s,'='))
			{
			*t = '\0';
			setparm(s,t+1,0,1);
			*t = '=';
			}
		else
			{
			uu = gethnode(s,parmhtab);
			if (uu)
				{
				if (!uu->isint)
					{
					uu->isint = 1;
					uu->u.val = matheval(uu->u.str);
					}
				}
			else
				setiparm(s,0,1);
			}
		}
	return 0;
}

static char *recs[] = {
	"cputime","filesize","datasize","stacksize","coredumpsize",
	"resident","descriptors"
	};

int limit(comm comm)
{
char *s;
int hard = 0,t0,lim;
long val;

	if (!(s = getnode(comm->args)))
		{
		showlimits(0,-1);
		return 0;
		}
	if (!strcmp(s,"-s"))
		{
		if (full(comm->args))
			zerr("limit","arguments after -s ignored");
		for (t0 = 0; t0 != RLIM_NLIMITS; t0++)
			if (setrlimit(t0,limits+t0) < 0)
				zerr("limit","setrlimit failed: %e",errno);
		return 0;
		}
	if (!strcmp(s,"-h"))
		{
		hard = 1;
		free(s);
		if (!(s = getnode(comm->args)))
			{
			showlimits(1,-1);
			return 0;
			}
		}
	while (s)
		{
		for (lim = -1, t0 = 0; t0 != RLIM_NLIMITS; t0++)
			if (!strncmp(recs[t0],s,strlen(s)))
				{
				if (lim != -1)
					lim = -2;
				else
					lim = t0;
				}
		if (lim < 0)
			{
			zerrnam("limit",
				(lim == -2) ? "ambiguous resource specification: %s"
								: "no such resource: %s",s);
			free(s);
			return 1;
			}
		free(s);
		if (!(s = getnode(comm->args)))
			{
			showlimits(hard,lim);
			return 0;
			}
		if (!lim)
			{
			char *ss = s;
			
			val = strtol(s,&s,10);
			if (*s)
				if ((*s == 'h' || *s == 'H') && !s[1])
					val *= 3600L;
				else if ((*s == 'm' || *s == 'M') && !s[1])
					val *= 60L;
				else if (*s == ':')
					val = val*60+strtol(s+1,&s,10);
				else
					{
					zerrnam("limit","unknown scaling factor: %s",s);
					free(ss);
					return 1;
					}
			}
#ifdef RLIMIT_NOFILE
		else if (lim == RLIMIT_NOFILE)
			val = strtol(s,&s,10);
#endif
		else
			{
			char *ss = s;
			
			val = strtol(s,&s,10);
			if (!*s || ((*s == 'k' || *s == 'K') && !s[1]))
				val *= 1024L;
			else if ((*s == 'M' || *s == 'm') && !s[1])
				val *= 1024L*1024;
			else
				{
				zerrnam("limit","unknown scaling factor: %s",s);
				free(ss);
				return 1;
				}
			free(ss);
			}
		if (hard)
			if (val > limits[lim].rlim_max && geteuid())
				{
				zerrnam("limit","can't raise hard limits");
				return 1;
				}
			else
				{
				limits[lim].rlim_max = val;
				if (limits[lim].rlim_max < limits[lim].rlim_cur)
					limits[lim].rlim_cur = limits[lim].rlim_max;
				}
		else
			if (val > limits[lim].rlim_max)
				{
				zerrnam("limit","limit exceeds hard limit");
				return 1;
				}
			else
				limits[lim].rlim_cur = val;
		s = getnode(comm->args);
		}
	return 0;
}

int unlimit(comm comm)
{
char *s = getnode(comm->args);
int hard = 0,t0,lim;

	if (s && !strcmp(s,"-h"))
		{
		hard = 1;
		if (geteuid())
			{
			zerrnam("unlimit","can't remove hard limits");
			return 1;
			}
		free(s);
		s = getnode(comm->args);
		}
	if (!s)
		{
		for (t0 = 0; t0 != RLIM_NLIMITS; t0++)
			{
			if (hard)
				limits[t0].rlim_max = RLIM_INFINITY;
			else
				limits[t0].rlim_cur = limits[t0].rlim_max;
			}
		return 0;
		}
	while (s)
		{
		for (lim = -1, t0 = 0; t0 != RLIM_NLIMITS; t0++)
			if (!strncmp(recs[t0],s,strlen(s)))
				{
				if (lim != -1)
					lim = -2;
				else
					lim = t0;
				}
		if (lim < 0)
			{
			zerrnam("unlimit",
				(lim == -2) ? "ambiguous resource specification: %s"
								: "no such resource: %s",s);
			free(s);
			return 1;
			}
		free(s);
		if (hard)
			limits[lim].rlim_max = RLIM_INFINITY;
		else
			limits[lim].rlim_cur = limits[lim].rlim_max;
		s = getnode(comm->args);
		}
	return 0;
}

void showlimits(int hard,int lim)
{
int t0;
long val;

	for (t0 = 0; t0 != RLIM_NLIMITS; t0++)
		if (t0 == lim || lim == -1)
			{
			printf("%-16s",recs[t0]);
			val = (hard) ? limits[t0].rlim_max : limits[t0].rlim_cur;
			if (val == RLIM_INFINITY)
				printf("unlimited\n");
			else if (!t0)
				printf("%d:%02d:%02d\n",(int) (val/3600),
					(int) (val/60) % 60,(int) (val % 60));
#ifdef RLIMIT_NOFILE
			else if (t0 == RLIMIT_NOFILE)
				printf("%d\n",(int) val);
#endif
			else if (val >= 1024L*1024L)
				printf("%ldMb\n",val/(1024L*1024L));
			else
				printf("%ldKb\n",val/1024L);
			}
}

int sched(comm comm)
{
char *s = getnode(comm->args);
time_t t;
long h,m;
struct tm *tm;
struct schnode *sch,*sch2,*schl;
int t0;

	if (s && *s == '-')
		{
		t0 = atoi(s+1);

		if (!t0)
			{
			zerrnam("sched","usage for delete: sched -<item#>.");
			return 1;
			}
		for (schl = (struct schnode *) &scheds, sch = scheds, t0--;
				sch && t0; sch = (schl = sch)->next, t0--);
		if (!sch)
			{
			zerrnam("sched","not that many entries");
			return 1;
			}
		schl->next = sch->next;
		free(sch->cmd);
		free(sch);
		return 0;
		}
	if (s && !full(comm->args))
		{
		zerrnam("sched","not enough arguments");
		return 1;
		}
	if (!s)
		{
		char tbuf[40];

		for (t0 = 1, sch = scheds; sch; sch = sch->next,t0++)
			{
			t = sch->time;
			tm = localtime(&t);
			strftime(tbuf,20,"%a %b %e %k:%M:%S",tm);
			printf("%3d %s %s\n",t0,tbuf,sch->cmd);
			}
		return 0;
		}
	if (*s == '+')
		{
		h = strtol(s+1,&s,10);
		if (*s != ':')
			{
			zerrnam("sched","bad time specifier");
			return 1;
			}
		m = strtol(s+1,&s,10);
		if (*s)
			{
			zerrnam("sched","bad time specifier");
			return 1;
			}
		t = time(NULL)+h*3600+m*60;
		}
	else
		{
		h = strtol(s,&s,10);
		if (*s != ':')
			{
			zerrnam("sched","bad time specifier");
			return 1;
			}
		m = strtol(s+1,&s,10);
		if (*s && *s != 'a' && *s != 'p')
			{
			zerrnam("sched","bad time specifier");
			return 1;
			}
		t = time(NULL);
		tm = localtime(&t);
		t -= tm->tm_sec+tm->tm_min*60+tm->tm_hour*3600;
		if (*s == 'p')
			h += 12;
		t += h*3600+m*60;
		if (t < time(NULL))
			t += 3600*24;
		}
	sch = alloc(sizeof(struct schnode));
	sch->time = t;
	sch->cmd = buildline(comm->args);
	sch->next = NULL;
	for (sch2 = (struct schnode *) &scheds; sch2->next; sch2 = sch2->next);
	sch2->next = sch;
	return 0;
}

int eval(comm comm)
{
char *s = buildline(comm->args);
list list;

	hungets(s);
	strinbeg();
	if (!(list = parlist(1)))
		{
		hflush();
		strinend();
		return 1;
		}
	strinend();
	runlist(list);
	return lastval;
}

int Brk(comm comm)
{
	printf("%lx\n",(long) sbrk(0));
	return 0;
}

static struct utmp *wtab;
static int wtabsz;

int log(comm comm)
{
	if (!getparm("WATCH"))
		return 1;
	if (wtab)
		free(wtab);
	wtab = (struct utmp *) zalloc(1);
	wtabsz = 0;
	watch();
	return 0;
}

int let(comm comm)
{
char *str;
long val;

	while (str = getnode(comm->args))
		val = matheval(str);
	return !val;
}

int Read(comm comm)
{
char *str,*pmpt;
int r = 0,bsiz,c,gotnl = 0;
char *buf,*bptr;
char cc;

	str = getnode(comm->args);
	if (str && !strcmp(str,"-r"))
		{
		r = 1;
		str = getnode(comm->args);
		}
	if (!str)
		str = strdup("REPLY");
	if (interact)
		{
		for (pmpt = str; *pmpt && *pmpt != '?'; pmpt++);
		if (*pmpt++)
			{
			write(2,pmpt,strlen(pmpt));
			*pmpt = '\0';
			}
		}
	while (full(comm->args))
		{
		buf = bptr = zalloc(bsiz = 64);
		FOREVER
			{
			if (gotnl)
				break;
			if (read(0,&cc,1) == 1)
				c = cc;
			else
				c = EOF;
			if (c == EOF || znspace(c))
				break;
			*bptr++ = c;
			if (bptr == buf+bsiz)
				{
				buf = realloc(buf,bsiz *= 2);
				bptr = buf+(bsiz/2);
				}
			}
		if (c == EOF)
			return 1;
		if (c == '\n')
			gotnl = 1;
		*bptr = '\0';
		setparm(str,buf,0,0);
		str = getnode(comm->args);
		}
	buf = bptr = zalloc(bsiz = 64);
	if (!gotnl)
		FOREVER
			{
			if (read(0,&cc,1) == 1)
				c = cc;
			else
				c = EOF;
			if (c == EOF || c == '\n')
				break;
			*bptr++ = c;
			if (bptr == buf+bsiz)
				{
				buf = realloc(buf,bsiz *= 2);
				bptr = buf+(bsiz/2);
				}
			*bptr = '\0';
			}
	if (c == EOF)
		return 1;
	setparm(str,buf,0,0);
	return 0;
}

int fc(comm comm)
{
char *ename = getparm("FCEDIT"),*str,*s;
int n = 0,l = 0,r = 0,first = -1,last = -1,retval;
table subs = NULL;

	if (!interact)
		{
		zerrnam("fc","not interactive shell");
		return 1;
		}
	remhist();
	if (!ename)
		ename = DEFFCEDIT;
	str = getnode(comm->args);
	for (; str && *str == '-' && str[1] && !isdigit(str[1]);
			str = getnode(comm->args))
		while (str[1])
			switch(*++str)
				{
				case 'e':
					if (str[1])
						{
						zerrnam("fc","editor name expected after -e");
						return 1;
						}
					ename = getnode(comm->args);
					if (!ename)
						{
						zerrnam("fc","editor name expected after -e");
						return 1;
						}
					break;
				case 'n':
					n = 1;
					break;
				case 'l':
					l = 1;
					break;
				case 'r':
					r = 1;
					break;
				default:
					zerrnam("fc","bad option: %c",*str);
					return 1;
				}
	subs = newtable();
	while (str)
		{
		for (s = str; *s && *s != '='; s++);
		if (!*s)
			break;
		*s++ = '\0';
		addnode(subs,str);
		addnode(subs,s);
		str = getnode(comm->args);
		}
	if (str)
		{
		first = fcgetcomm(str);
		if (first == -1)
			{
			freetable(subs,freestr);
			return 1;
			}
		str = getnode(comm->args);
		}
	if (str)
		{
		last = fcgetcomm(str);
		if (last == -1)
			{
			freetable(subs,freestr);
			return 1;
			}
		if (full(comm->args))
			{
			zerrnam("fc","too many arguments");
			freetable(subs,freestr);
			return 1;
			}
		}
	if (first == -1)
		{
		first = (l) ? cev-16 : cev;
		if (last == -1)
			last = cev;
		}
	if (first < tfev)
		first = tfev;
	if (last == -1)
		last = first;
	if (l)
		retval = fclist(stdout,!n,r,first,last,subs);
	else
		{
		FILE *out;
		char *fn = gettemp();

		out = fopen(fn,"w");
		if (!out)
			zerrnam("fc","can't open temp file: %e",errno);
		else
			{
			retval = 1;
			if (!fclist(out,0,r,first,last,subs))
				if (fcedit(ename,fn))
					if (stuff(fn))
						zerrnam("fc","%e: %s",errno,s);
					else
						retval = 0;
			}
		unlink(fn);
		free(fn);
		}
	freetable(subs,freestr);
	return retval;
}

/* get the history event associated with s */

int fcgetcomm(char *s)
{
int cmd;

	if (cmd = atoi(s))
		{
		if (cmd < 0)
			cmd = cev+cmd+1;
		return cmd;
		}
	cmd = hcomsearch(s);
	if (cmd == -1)
		zerrnam("fc","event not found: %s",s);
	return cmd;
}

/* list a series of history events to a file */

int fclist(FILE *f,int n,int r,int first,int last,table subs)
{
int done = 0,ct;
Node node;
char *s;

	if (!subs->first)
		done = 1;
	last -= first;
	first -= tfev;
	if (r)
		first += last;
	for (node = histlist->first,ct = first; ct && node;
		node = node->next, ct--);
	first += tfev;
	while (last-- >= 0)
		{
		if (!node)
			{
			zerrnam("fc","no such event: %d",first);
			return 1;
			}
		s = makehlist(node->dat,0);
		done |= fcsubs(&s,subs);
		if (n)
			fprintf(f,"%5d  ",first);
		if (f == stdout)
			{
			niceprintf(s,f);
			putc('\n',f);
			}
		else
			fprintf(f,"%s\n",s);
		node = (r) ? node->last : node->next;
		(r) ? first-- : first++;
		}
	if (f != stdout)
		fclose(f);
	if (!done)
		{
		zerrnam("fc","no substitutions performed");
		return 1;
		}
	return 0;
}

/* perform old=new substituion */

int fcsubs(char **sp,table tab)
{
Node n;
char *s1,*s2,*s3,*s4,*s = *sp,*s5;
int subbed = 0;

	for (n = tab->first; n; )
		{
		s1 = n->dat;
		n = n->next;
		s2 = n->dat;
		n = n->next;
		s5 = s;
		while (s3 = (char *) strstr(s5,s1))
			{
			s4 = zalloc(1+(s3-s)+strlen(s2)+strlen(s3+strlen(s1)));
			strncpy(s4,s,s3-s);
			s4[s3-s] = '\0';
			strcat(s4,s2);
			s5 = s4+strlen(s4);
			strcat(s4,s3+strlen(s1));
			free(s);
			s = s4;
			subbed = 1;
			}
		}
	*sp = s;
	return subbed;
}

int fcedit(char *ename,char *fn)
{
	if (!strcmp(ename,"-"))
		return 1;
	return !zyztem(ename,fn);
}

int disown(comm comm)
{
char *str;
int t0;
static struct jobnode zero;

	while (str = getnode(comm->args))
		{
		t0 = getjob(str,"disown");
		if (t0 == -1)
			return 1;
		jobtab[t0] = zero;
		}
	return 0;
}

int function(comm comm)
{
	if (full(comm->args))
		{
		zerrnam("function","too many arguments");
		return 1;
		}
	listhtable(shfunchtab,(void (*)(char *,char *)) pshfunc);
	return 0;
}

int (*funcs[])(comm) = {
	echo,zexit,logout,Unset,dirs,
	Alias,cd,which,popd,
   dot,pushd,shift,unhash,Break,
	colon,Glob,noglob,mostglob,unalias,
	fg,bg,jobs,Kill,export,
	Umask,cd,limit,unlimit,eval,
	unfunction,set,Brk,log,builtin,
	sched,let,fc,
	rehash,hash,disown,test,Read,
	integer,setopt,unsetopt,zreturn,function,
	test,
	NULL
	};
char *funcnams[] = {
	"echo","exit","logout","unset","dirs",
	"alias","cd","which","popd",
	".","pushd","shift","unhash","break",
	":","glob","noglob","mostglob","unalias",
	"fg","bg","jobs","kill","export",
	"umask","chdir","limit","unlimit","eval",
	"unfunction","set","brk","log","builtin",
	"sched","let","fc",
	"rehash","hash","disown","test","read",
	"integer","setopt","unsetopt","return","function",
	"["
	};

int builtin(comm comm)
{
char *s;
int t0;

	s = getnode(comm->args);
	if (!s)
		{
		zerrnam("builtin","not enough arguments");
		return 1;
		}
	for (t0 = 0; funcs[t0]; t0++)
		if (!strcmp(funcnams[t0],s))
			break;
	if (!funcs[t0])
		{
		zerrnam("builtin","no such builtin: %s",s);
		return 1;
		}
	return (funcs[t0])(comm);
}

/* add builtins to the command hash table */

void addintern(htable ht)
{
int (**ptr)(comm);
struct chnode *ch;
char **nam;
 
	for (ptr = funcs, nam = funcnams; *ptr; ptr++,nam++)
		{
		ch = alloc(sizeof(struct chnode));
		ch->type = BUILTIN;
		ch->u.func = *ptr;
		addhnode(strdup(*nam),ch,ht,freechnode);
		}
}

/* get the time of login/logout for WATCH */

static time_t getlogtime(struct utmp *u,int inout)
{
FILE *in;
struct utmp uu;
int first = 1;

	if (inout)
		return u->ut_time;
	if (!(in = fopen(WTMP_FILE,"r")))
		return time(NULL);
	fseek(in,0,2);
	do
		{
		if (fseek(in,((first) ? -1 : -2)*sizeof(struct utmp),1))
			{
			fclose(in);
			return time(NULL);
			}
		first = 0;
		if (!fread(&uu,sizeof(struct utmp),1,in))
			{
			fclose(in);
			return time(NULL);
			}
		}
	while (memcmp(&uu,u,sizeof(struct utmp)));
	do
		if (!fread(&uu,sizeof(struct utmp),1,in))
			{
			fclose(in);
			return time(NULL);
			}
	while (strncmp(uu.ut_line,u->ut_line,8));
	fclose(in);
	return uu.ut_time;
}

/* print a login/logout event */

static void watchlog2(int inout,struct utmp *u,char *fmt)
{
char *p,buf[40],*bf;
int i;
time_t timet;
struct tm *tm = NULL;

	while (*fmt)
		if (*fmt != '%')
			putchar(*fmt++);
		else
			{
			fmt++;
			switch(*fmt++)
				{
				case 'n':
					printf("%.*s",8,u->ut_name);
					break;
				case 'a':
					printf("%s",(!inout) ? "logged off" : "logged on");
					break;
				case 'l':
					printf("%.*s",5,u->ut_line+3);
					break;
				case 'm':
					for (p = u->ut_host,i = 16; i && *p;i--,p++)
						{
						if (*p == '.' && !isdigit(p[1]))
							break;
						putchar(*p);
						}
					break;
				case 'M':
					printf("%.*s",16,u->ut_host);
					break;
				case 't':
				case '@':
					timet = getlogtime(u,inout);
					tm = localtime(&timet);
					strftime(buf,40,"%l:%M%p",tm);
					printf("%s",(*buf == ' ') ? buf+1 : buf);
					break;
				case 'T':
					timet = getlogtime(u,inout);
					tm = localtime(&timet);
					strftime(buf,40,"%k:%M",tm);
					printf("%s",buf);
					break;
				case 'w':
					timet = getlogtime(u,inout);
					tm = localtime(&timet);
					strftime(buf,40,"%a %e",tm);
					printf("%s",buf);
					break;
				case 'W':
					timet = getlogtime(u,inout);
					tm = localtime(&timet);
					strftime(buf,40,"%m/%d/%y",tm);
					printf("%s",buf);
					break;
				case 'D':
					timet = getlogtime(u,inout);
					tm = localtime(&timet);
					strftime(buf,40,"%y-%m-%d",tm);
					printf("%s",buf);
					break;
				case '%':
					putchar('%');
					break;
				case 'S':
					bf = buf;
					if (tgetstr("so",&bf))
						fputs(buf,stdout);
					break;
				case 's':
					bf = buf;
					if (tgetstr("se",&bf))
						fputs(buf,stdout);
					break;
				case 'B':
					bf = buf;
					if (tgetstr("md",&bf))
						fputs(buf,stdout);
					break;
				case 'b':
					bf = buf;
					if (tgetstr("me",&bf))
						fputs(buf,stdout);
					break;
				case 'U':
					bf = buf;
					if (tgetstr("us",&bf))
						fputs(buf,stdout);
					break;
				case 'u':
					bf = buf;
					if (tgetstr("ue",&bf))
						fputs(buf,stdout);
					break;
				default:
					putchar('%');
					putchar(fmt[-1]);
					break;
				}
			}
	putchar('\n');
}

/* check the list for login/logouts */

static void watchlog(int inout,struct utmp *u,char *w,char *fmt)
{
char *v;

	if (!strcmp(w,"all"))
		{
		watchlog2(inout,u,fmt);
		return;
		}
	for(;;)
		if (v = strchr(w,':'))
			{
			if (!strncmp(u->ut_name,w,v-w))
				watchlog2(inout,u,fmt);
			w = v+1;
			}
		else
			{
			if (!strncmp(u->ut_name,w,8))
				watchlog2(inout,u,fmt);
			break;
			}
}

/* compare 2 utmp entries */

static int ucmp(struct utmp *u,struct utmp *v)
{
	if (u->ut_time == v->ut_time)
		return strncmp(u->ut_line,v->ut_line,8);
	return u->ut_time - v->ut_time;
}

/* initialize the user list */

void readwtab(void)
{
struct utmp *uptr;
int wtabmax = 32;
FILE *in;

	wtabsz = 0;
	uptr = wtab = (struct utmp *) zalloc(wtabmax*sizeof(struct utmp));
	in = fopen(UTMP_FILE,"r");
	while (fread(uptr,sizeof(struct utmp),1,in))
		if (uptr->ut_host[0])
			{
			uptr++;
			if (++wtabsz == wtabmax)
				uptr = (wtab = (struct utmp *) realloc(wtab,(wtabmax*=2)*
					sizeof(struct utmp)))+wtabsz;
			}
	fclose(in);
	if (wtabsz)
		qsort(wtab,wtabsz,sizeof(struct utmp),ucmp);
}

/* check for login/logout events; executed before each prompt
	if WATCH is set */

void watch(void)
{
char *s = getparm("WATCH");
char *fmt = getparm("WATCHFMT");
FILE *in;
int utabsz = 0,utabmax = wtabsz+4,uct,wct;
struct utmp *utab,*uptr,*wptr;

	holdintr();
	if (!fmt)
		fmt = "%n has %a %l from %m.";
	if (!wtab)
		{
		readwtab();
		noholdintr();
		return;
		}
	uptr = utab = (struct utmp *) zalloc(utabmax*sizeof(struct utmp));
	in = fopen(UTMP_FILE,"r");
	while (fread(uptr,sizeof *uptr,1,in))
		if (uptr->ut_host[0])
			{
			uptr++;
			if (++utabsz == utabmax)
				uptr = (utab = (struct utmp *) realloc(utab,(utabmax*=2)*
					sizeof(struct utmp)))+utabsz;
			}
	fclose(in);
	noholdintr();
	if (errflag)
		{
		free(utab);
		return;
		}
	if (utabsz)
		qsort(utab,utabsz,sizeof(struct utmp),ucmp);

	wct = wtabsz; uct = utabsz;
	uptr = utab; wptr = wtab;
	if (errflag)
		{
		free(utab);
		return;
		}
	while ((uct || wct) && !errflag)
		if (!uct || (wct && ucmp(uptr,wptr) > 0))
			wct--,watchlog(0,wptr++,s,fmt);
		else if (!wct || (uct && ucmp(uptr,wptr) < 0))
			uct--,watchlog(1,uptr++,s,fmt);
		else
			uptr++,wptr++,wct--,uct--;
	free(wtab);
	wtab = utab;
	wtabsz = utabsz;
	fflush(stdout);
}

