/*

	jobs.c - job control

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
#include <sys/errno.h>

#define WCOREDUMPED(x) ((x)&0x80)

/* != 0 means the handler is active */

static int handling = 0;

/* != 0 means the shell is waiting for a job to complete */

static int waiting = 0;

/* != 0 means readline is active */

extern int rl_active;

/* != 0 means readline is waiting for a keypress */

extern int rl_waiting;

#ifdef INTHANDTYPE
#define RETURN return 0
#else
#define RETURN return
#endif

/* the signal handler */

HANDTYPE handler(int sig,int code)
{
long pid;
int statusp;
struct jobnode *jn;
struct procnode *pn;
struct rusage ru;

	if (sig == SIGINT)
		{
		if (sigtrapped[SIGINT])
			dotrap(SIGINT);
		else
			errflag = 1;
		RETURN;
		}
	if (sig != SIGCHLD)
		{
		dotrap(sig);
		RETURN;
		}
	for (;;)
		{
		pid = wait3(&statusp,WNOHANG|WUNTRACED,&ru);
		if (pid == -1)
			{
			if (errno != ECHILD)
				zerr("%e",errno);
			RETURN;
			}
		if (!pid)
			RETURN;
		findproc(pid,&jn,&pn);	/* find the procnode of this pid */
		if (jn)
			{
			pn->statusp = statusp;
			handling = 1;
			pn->ru_utime = ru.ru_utime;
			pn->ru_stime = ru.ru_stime;
			pn->endtime = time(NULL);
			updatestatus(jn);
			handling = 0;
			}
		else if (WIFSTOPPED(SP(statusp)))
			kill(pid,SIGKILL);	/* kill stopped untraced children */
		}
	if (rl_active)
		rl_prep_terminal();
	RETURN;
}

/* change job table entry from stopped to running */

void makerunning(struct jobnode *jn)
{
struct procnode *pn;

	jn->stat &= ~STAT_STOPPED;
	for (pn = jn->procs; pn; pn = pn->next)
		if (WIFSTOPPED(SP(pn->statusp)))
			pn->statusp = SP_RUNNING;
}

/* update status of job, possibly printing it */

void updatestatus(struct jobnode *jn)
{
struct procnode *pn;
int notrunning = 1,alldone = 1,val,job = jn-jobtab,somestopped = 0;

	for (pn = jn->procs; pn; pn = pn->next)
		{
		if (pn->statusp == SP_RUNNING)
			notrunning = 0;
		if (pn->statusp == SP_RUNNING || WIFSTOPPED(SP(pn->statusp)))
			alldone = 0;
		if (WIFSTOPPED(SP(pn->statusp)))
			somestopped = 1;
		if (!pn->next && jn)
			val = (WIFSIGNALED(SP(pn->statusp))) ?
				0200 | WTERMSIG(SP(pn->statusp)) : WEXITSTATUS(SP(pn->statusp));
		}
	if (!notrunning)
		return;
	if (somestopped && (jn->stat & STAT_STOPPED))
		return;
	jn->stat |= (alldone) ? STAT_CHANGED|STAT_DONE :
		STAT_CHANGED|STAT_STOPPED;
	if (!alldone)
		gettyinfo(&jn->ttyinfo);
	if (job == curjob)
		{
		if (!val)
			gettyinfo(&shttyinfo);
		else
			settyinfo(&shttyinfo);
		lastval = val;
		}
	if (jn->stat & STAT_STOPPED)
		{
		prevjob = topjob;
		topjob = job;
		}
	if ((isset(NOTIFY) || job == curjob) && jn->stat & STAT_LOCKED)
		printjob(jn,0);
	if (sigtrapped[SIGCHLD] && job != curjob)
		dotrap(SIGCHLD);
}

/* find procnode and jobnode associated with pid */

void findproc(int pid,struct jobnode **jptr,struct procnode **pptr)
{
struct procnode *pn;
int jn;

	for (jn = 1; jn != MAXJOB; jn++)
		for (pn = jobtab[jn].procs; pn; pn = pn->next)
			if (pn->pid == pid)
				{
				*pptr = pn;
				*jptr = jobtab+jn;
				return;
				}
	*pptr = NULL;
	*jptr = NULL;
}

static char *sigmsg[32] = {
	"done","hangup","interrupt","quit",
	"illegal instruction","trace trap","IOT instruction","EMT instruction",
	"floating exception","killed","bus error","segmentation fault",
	"bad system call","broken pipe","SIGALRM","terminated",
#ifdef USE_SUSPENDED
	"SIGURG","suspended (signal)","suspended","continued",
	"SIGCHLD","suspended (tty input)","suspended (tty output)","SIGIO",
#else
	"SIGURG","stopped (signal)","stopped","continued",
	"SIGCHLD","stopped (tty input)","stopped (tty output)","SIGIO",
#endif
	"CPU time limit exceeded","file size limit exceeded","SIGVTALRM","SIGPROF",
	"window changed","resource lost","SIGUSR1","SIGUSR2"
	};

/* lng = 0 means jobs 
	lng = 1 means jobs -l
	lng = 2 means jobs -p
*/

void printjob(struct jobnode *jn,int lng)
{
int job = jn-jobtab,len = 9,sig = -1,sflag = 0,llen,printed = 0;
int conted = 0,lineleng = getlineleng(),doputnl = 0;
struct procnode *pn;
extern void rl_on_new_line(void);

	if (lng < 0)
		{
		conted = 1;
		lng = 0;
		}

	/* find length of longest signame, check to see if we
		really need to print this job */

	for (pn = jn->procs; pn; pn = pn->next)
		{
		if (pn->statusp != SP_RUNNING)
			if (WIFSIGNALED(SP(pn->statusp)))
				{
				sig = WTERMSIG(SP(pn->statusp));
				llen = strlen(sigmsg[sig]);
				if (WCOREDUMPED(pn->statusp))
					llen += 14;
				if (llen > len)
					len = llen;
				if (sig != SIGINT && sig != SIGPIPE)
					sflag = 1;
				if (sig == SIGINT && job == curjob && interact)
					doputnl = 1;
				}
			else if (WIFSTOPPED(SP(pn->statusp)))
				{
				sig = WSTOPSIG(SP(pn->statusp));
				if (strlen(sigmsg[sig]) > len)
					len = strlen(sigmsg[sig]);
				}
			else if (isset(PRINTEXITVALUE) && WEXITSTATUS(SP(pn->statusp)))
				sflag = 1;
		}
	if (doputnl)
		putc('\n',stderr);

	/* print if necessary */

	if (interact && jobbing && ((jn->stat & STAT_STOPPED) || sflag ||
			job != curjob))
		{
		int len2,fline = 1;
		struct procnode *qn;

		if (handling && (!waiting || jn->stat & STAT_STOPPED))
			putc('\n',stderr);
		for (pn = jn->procs; pn;)
			{
			len2 = ((job == curjob) ? 5 : 10)+len; /* 2 spaces */
			if (lng)
				qn = pn->next;
			else for (qn = pn->next; qn; qn = qn->next)
				{
				if (qn->statusp != pn->statusp)
					break;
				if (strlen(qn->text)+len2+((qn->next) ? 3 : 0) > lineleng)
					break;
				len2 += strlen(qn->text)+2;
				}
			if (job != curjob)
				if (fline)
					fprintf(stderr,"[%d]  %c ",jn-jobtab,(job == topjob) ? '+' :
						(job == prevjob) ? '-' : ' ');
				else
					fprintf(stderr,(job > 9) ? "        " : "       ");
			else
				fprintf(stderr,"zsh: ");
			if (lng)
				if (lng == 1)
					fprintf(stderr,"%d ",pn->pid);
				else
					{
					fprintf(stderr,"%d ",jn->gleader);
					lng = 0;
					}
			if (pn->statusp == SP_RUNNING)
				if (!conted)
					fprintf(stderr,"running%*s",len-7+2,"");
				else
					fprintf(stderr,"continued%*s",len-9+2,"");
			else if (WIFEXITED(SP(pn->statusp)))
				if (WEXITSTATUS(SP(pn->statusp)))
					fprintf(stderr,"exit %-4d%*s",WEXITSTATUS(SP(pn->statusp)),
						len-9+2,"");
				else
					fprintf(stderr,"done%*s",len-4+2,"");
			else if (WIFSTOPPED(SP(pn->statusp)))
				fprintf(stderr,"%-*s",len+2,sigmsg[WSTOPSIG(SP(pn->statusp))]);
			else if (WCOREDUMPED(pn->statusp))
				fprintf(stderr,"%s (core dumped)%*s",
					sigmsg[WTERMSIG(SP(pn->statusp))],
					len-14+2-strlen(sigmsg[WTERMSIG(SP(pn->statusp))]),"");
			else
				fprintf(stderr,"%-*s",len+2,sigmsg[WTERMSIG(SP(pn->statusp))]);
			for (; pn != qn; pn = pn->next)
				fprintf(stderr,(pn->next) ? "%s | " : "%s",pn->text);
			putc('\n',stderr);
			fline = 0;
			}
		printed = 1;
		}

	/* print "(pwd now: foo)" messages */

	if (interact && job==curjob && strcmp(jn->cwd,cwd))
		{
		printf("(pwd now: ");
		printdir(cwd);
		printf(")\n");
		fflush(stdout);
		}

	/* delete job if done */

	if (jn->stat & STAT_DONE)
		{
		static struct jobnode zero;
		struct procnode *nx;
		char *s;

		if (jn->stat & STAT_TIMED)
			{
			dumptime(jn);
			printed = 1;
			}
		for (pn = jn->procs; pn; pn = nx)
			{
			nx = pn->next;
			if (pn->text)
				free(pn->text);
			free(pn);
			}
		free(jn->cwd);
		if (jn->filelist)
			{
			while (s = getnode(jn->filelist))
				{
				unlink(s);
				free(s);
				}
			free(jn->filelist);
			}
		*jn = zero;
		if (job == topjob)
			{
			topjob = prevjob;
			prevjob = job;
			}
		if (job == prevjob)
			setprevjob();
		}
	else
		jn->stat &= ~STAT_CHANGED;
	if (printed && rl_active)
		{
		rl_on_new_line();
		if (rl_waiting)
			rl_redisplay();
		}
}

/* set the previous job to something reasonable */

void setprevjob(void)
{
int t0;

	for (t0 = MAXJOB-1; t0; t0--)
		if (jobtab[t0].stat && jobtab[t0].stat & STAT_STOPPED &&
				t0 != topjob && t0 != curjob)
			break;
	if (!t0)
		for (t0 = MAXJOB-1; t0; t0--)
			if (jobtab[t0].stat && t0 != topjob && t0 != curjob)
				break;
	prevjob = (t0) ? t0 : -1;
}

/* initialize a job table entry */

void initjob(int flags)
{
	jobtab[curjob].cwd = strdup(cwd);
	jobtab[curjob].stat = (flags & PFLAG_TIMED) | STAT_INUSE;
	jobtab[curjob].ttyinfo = shttyinfo;
	jobtab[curjob].gleader = 0;
}

/* add a process to the current job */

struct procnode *addproc(long pid,char *text)
{
struct procnode *procnode;

	if (!jobtab[curjob].gleader)
		jobtab[curjob].gleader = proclast = pid;
	proclast = pid;
	procnode = alloc(sizeof(struct procnode));
	procnode->pid = pid;
	procnode->text = text;
	procnode->next = NULL;
	procnode->statusp = SP_RUNNING;
	procnode->bgtime = time(NULL);
	if (jobtab[curjob].procs)
		{
		struct procnode *n;

		for (n = jobtab[curjob].procs; n->next && !n->next->lastfg; n = n->next);
		procnode->next = n->next;
		n->next = procnode;
		}
	else
		jobtab[curjob].procs = procnode;
	return procnode;
}

/* determine if it's all right to exec a command without
	forking in last component of subshells; it's not ok if we have files
	to delete */

int execok(void)
{
struct jobnode *jn;

	if (!exiting)
		return 0;
	for (jn = jobtab+1; jn != jobtab+MAXJOB; jn++)
		if (jn->stat && jn->filelist)
			return 0;
	return 1;
}

/* wait for a SIGCHLD, wait for the handler to execute, and return */

void chldsuspend(void)
{
struct sigvec vec = { handler,sigmask(SIGCHLD),SV_INTERRUPT };

	sigvec(SIGCHLD,&vec,NULL);
	sigpause(0);
	signal(SIGCHLD,handler);
}

/* wait for the current job to finish */

void waitjobs(void)
{
static struct jobnode zero;
struct jobnode *jn;

	if (jobtab[curjob].procs)	/* if any forks were done */
		{
		jobtab[curjob].stat |= STAT_LOCKED;
		waiting = 1;
		if (jobtab[curjob].stat & STAT_CHANGED)
			printjob(jobtab+curjob,0);
		while (jobtab[curjob].stat &&
				!(jobtab[curjob].stat & (STAT_DONE|STAT_STOPPED)))
			chldsuspend();
		waiting = 0;
		}
	else	/* else do what printjob() usually does */
		{
		char *s;

		jn = jobtab+curjob;
		free(jn->cwd);
		if (jn->filelist)
			{
			while (s = getnode(jn->filelist))
				{
				unlink(s);
				free(s);
				}
			free(jn->filelist);
			}
		*jn = zero;
		}
	curjob = -1;
}

/* clear jobtab when entering subshells */

void clearjobtab(void)
{
static struct jobnode zero;
int t0;

	for (t0 = 1; t0 != MAXJOB; t0++)
		jobtab[curjob] = zero;
}

/* get a free entry in the job table to use */

int getfreejob(void)
{
int mask,t0;

	FOREVER	
		{
		mask = sigblock(sigmask(SIGCHLD));
		for (t0 = 1; t0 != MAXJOB; t0++)
			if (!jobtab[t0].stat)
				{
				sigsetmask(mask);
				jobtab[t0].stat |= STAT_INUSE;
				return t0;
				}
		sigsetmask(mask);
		sleep(1);
		}
}

/* print pids for & */

void spawnjob(void)
{
struct procnode *pn;

	if (!subsh)
		{
		if (topjob == -1 || !(jobtab[topjob].stat & STAT_STOPPED))
			{
			topjob = curjob;
			setprevjob();
			}
		else if (prevjob == -1 || !(jobtab[prevjob].stat & STAT_STOPPED))
			prevjob = curjob;
		if (interact && jobbing)
			{
			fprintf(stderr,"[%d]",curjob);
			for (pn = jobtab[curjob].procs; pn; pn = pn->next)
				fprintf(stderr," %d",pn->pid);
			fprintf(stderr,"\n");
			}
		}
	jobtab[curjob].stat |= STAT_LOCKED;
	curjob = -1;
}

void fixsigs(void)
{
	sigsetmask(0);
}

/* timing */

static void addtimeval(struct timeval *s,struct timeval *t)
{
	s->tv_sec += t->tv_sec+(s->tv_usec+t->tv_usec)/1000000;
	s->tv_usec = (s->tv_usec+t->tv_usec)%1000000;
}

static void printtime(time_t real,struct timeval *u,struct timeval *s,char *desc)
{
	if (!desc)
		desc = "";
	fprintf(stderr,"real: %lds  user: %ld.%03lds  sys: %ld.%03lds\n",
		real,u->tv_sec,u->tv_usec/1000,s->tv_sec,s->tv_usec/1000);
}

static void printheader(void)
{
	fprintf(stderr,"  real       user      system\n");
}

static void printtimes(time_t real,struct timeval *u,struct timeval *s,char *desc)
{
	if (!desc)
		desc = "";
	fprintf(stderr,"% 8lds  % 4d.%03lds  % 4d.%03lds  %s\n",
		real,u->tv_sec,u->tv_usec/1000,s->tv_sec,s->tv_usec/1000,desc);
}

void dumptime(struct jobnode *jn)
{
struct procnode *pn = jn->procs;
struct timeval utot = { 0,0 },stot = { 0,0 };
time_t maxend,minbeg;

	if (!jn->procs)
		return;
	if (!jn->procs->next)
		printtime(pn->endtime-pn->bgtime,&pn->ru_utime,&pn->ru_stime,pn->text);
	else
		{
		maxend = jn->procs->endtime;
		minbeg = jn->procs->bgtime;
		printheader();
		for (pn = jn->procs; pn; pn = pn->next)
			{
			printtimes(pn->endtime-pn->bgtime,&pn->ru_utime,&pn->ru_stime,pn->text);
			addtimeval(&utot,&pn->ru_utime);
			addtimeval(&stot,&pn->ru_stime);
			if (pn->endtime > maxend)
				maxend = pn->endtime;
			if (pn->bgtime < minbeg)
				minbeg = pn->bgtime;
			}
		printtimes(maxend-minbeg,&utot,&stot,"total");
		}
}

/* SIGHUP any jobs left running */

void killrunjobs(void)
{
int t0,killed = 0;

	for (t0 = 1; t0 != MAXJOB; t0++)
		if (t0 != curjob && jobtab[t0].stat &&
				!(jobtab[t0].stat & STAT_STOPPED))
			{
			killpg(jobtab[t0].gleader,SIGHUP);
			killed++;
			}
	if (killed)
		zerr("warning: %d jobs SIGHUPed",killed);
}

/* check to see if user has jobs running/stopped */

void checkjobs(void)
{
int t0;

	for (t0 = 1; t0 != MAXJOB; t0++)
		if (t0 != curjob && jobtab[t0].stat)
			break;
	if (t0 != MAXJOB)
		{
		if (jobtab[t0].stat & STAT_STOPPED)
			{
#ifdef USE_SUSPENDED
			zerr("you have suspended jobs.");
#else
			zerr("you have stopped jobs.");
#endif
			}
		else
			zerr("you have running jobs.");
		stopmsg = 1;
		}
}

/* send a signal to a job (simply involves killpg if monitoring is on) */

int killjb(struct jobnode *jn,int sig)
{
struct procnode *pn;
int err;

	if (jobbing)
		return(killpg(jn->gleader,sig));
	for (pn = jn->procs; pn; pn = pn->next)
		if ((err = kill(pn->pid,sig)) == -1 && errno != ESRCH)
			return -1;
	return err;
}

