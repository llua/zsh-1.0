/*

	exec.c - command execution

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
#include <sys/dir.h>

#define execerr() { if (forked) exit(1); freecmd(comm); \
	closemnodes(mfds); errflag = 1; return; }
#define magicerr() { if (magic) putc('\n',stderr); errflag = 1; }

/* execute a string */

void execstring(char *s)
{
list l;

	hungets(strdup("\n"));
	hungets(s);
	strinbeg();
	if (!(l = parlist(1)))
		{
		strinend();
		hflush();
		return;
		}
	if (peek != EOF && peek != EMPTY)
		{
		strinend();
		hflush();
		return;
		}
	strinend();
	execlist(l);
}

/* duplicate a list and run it */

void newrunlist(list l)
{
list a = duplist(l); runlist(a);
}

/* fork and set limits */

int phork(void)
{
int pid = fork(),t0;

	if (pid == -1)
		{
		zerr("fork failed: %e",errno);
		return -1;
		}
	if (!pid)
		for (t0 = 0; t0 != RLIM_NLIMITS; t0++)
			setrlimit(t0,limits+t0);
	return pid;
}

/* execute a current shell command */

void execcursh(comm comm)
{
	runlist(comm->left);
	comm->left = NULL;
}

/* execve an external command */

void execute(char *arg0,table args)
{
char **argv;
char *z,*s,buf[MAXPATHLEN],buf2[MAXPATHLEN];
struct chnode *cn;
int t0,tl,ee = 0;

#define zexecve(X,Y,Z) {execve(z=(X),Y,Z);\
	if(errno!=ENOENT){ee = errno;strcpy(buf2,buf);}}

	cn = gethnode(arg0,chtab);
	if (s = getenv("STTY"))
		zyztem("stty",s);
	if (z = getenv("ARGV0"))
		z = strdup(z);
	else
		z = arg0;
	argv = makecline(z,args);
	fixsigs();
	if (cn)
		{
		if (cn->type == EXCMD_POSTDOT)
			{zexecve(arg0,argv,environ);}
		{zexecve(cn->u.nam,argv,environ);}
		}
	for (s = arg0; *s; s++)
		if (*s == '/')
			{
			execve(arg0,argv,environ);
			zerr("%e: %s",errno,arg0);
			_exit(1);
			}
	for (t0 = pathct; t0; t0--,path++)
		if (**path == '.')
			{zexecve(arg0,argv,environ);}
		else
			{
			tl = strlen(*path);
			strcpy(buf,*path);
			buf[tl] = '/';
			if (strlen(arg0)+strlen(buf)+1 >= MAXPATHLEN)
				{
				zerr("command too long: %s",arg0);
				_exit(1);
				}
			strcpy(buf+tl+1,arg0);
			{zexecve(buf,argv,environ);}
			}
	if (ee)
		{
		z = buf2;
		errno = ee;
		goto errs;
		}
	zerr("command not found: %s",arg0);
	_exit(1);
errs:
	zerr("%e: %s",errno,z);
	_exit(1);
}

#define try(X) { if (!access(X,X_OK)) return strdup(X); }

/* get the pathname of a command */

char *findcmd(char *arg0)
{
char *s,buf[MAXPATHLEN];
int t0,tl;
struct chnode *cn = gethnode(arg0,chtab);
char **pp = path;

	if (cn)
		{
		if (cn->type == EXCMD_POSTDOT)
			{
			strcpy(buf,"./");
			strcat(buf,arg0);
			try(buf);
			}
		try(cn->u.nam);
		}
	for (s = arg0; *s; s++)
		if (*s == '/')
			{
			try(arg0);
			goto failed;
			}
	for (t0 = pathct; t0; t0--,pp++)
		if (**pp == '.')
			{
			strcpy(buf,"./");
			strcat(buf,arg0);
			try(buf);
			}
		else
			{
			tl = strlen(*pp);
			strcpy(buf,*pp);
			buf[tl] = '/';
			strcpy(buf+tl+1,arg0);
			try(buf);
			}
failed:
	return NULL;
}

void execlist(list list)
{
	execlist1(list);
	freelist(list);
}

void execlist1(list list)
{
	if (breaks)
		return;
	switch(list->type)
		{
		case SYNC:
		case ASYNC:
			execlist2(list->left,list->type,!list->right);
			if (sigtrapped[SIGDEBUG])
				dotrap(SIGDEBUG);
			if (sigtrapped[SIGERR] && lastval)
				dotrap(SIGERR);
			if (list->right && !retflag)
				execlist1(list->right);
			break;
		}
}

void execlist2(list2 list,int type,int last1)
{
	switch(list->type)
		{
		case END:
			execpline(list,type,last1);
			break;
		case ORNEXT:
			if (!execpline(list,SYNC,0))
				execlist2(list->right,type,last1);
			break;
		case ANDNEXT:
			if (execpline(list,SYNC,0))
				execlist2(list->right,type,last1);
			break;
		}
}

int execpline(list2 l,int how,int last1)
{
int ipipe[2] = {0,0},opipe[2] = {0,0};

	sigblock(sigmask(SIGCHLD));
	curjob = getfreejob(); 
	initjob(l->flags);
	if (l->flags & PFLAG_COPROC)
		{
		how = ASYNC;
		mpipe(ipipe);
		mpipe(opipe);
		if (spin)
			{
			close(spin);
			close(spout);
			}
		spin = ipipe[0];
		spout = opipe[1];
		}
	execpline2(l->left,how,opipe[0],ipipe[1],last1);
	if (how == ASYNC)
		{
		spawnjob();
		sigsetmask(0);
		return 1;
		}
	else
		{
		waitjobs();
		sigsetmask(0);
		if (l->flags & PFLAG_NOT)
			lastval = !lastval;
		return !lastval;
		}
}

void execpline2(pline pline,int how,int input,int output,int last1)
{
int pid;
int pipes[2];

	if (breaks)
		return;
	if (!pline)
		return;
	if (pline->type == END)
		{
		execcomm(pline->left,input,output,how==ASYNC,last1);
		pline->left = NULL;
		}
	else
		{
		mpipe(pipes);

		/* if we are doing "foo | bar" where foo is a current
			shell command, do foo in the current shell and do
			the rest of the pipeline in a subshell. */

		if (pline->left->type >= CURSH && how == SYNC)
			{
			if (!(pid = fork()))
				{
				close(pipes[1]);
				entersubsh(1);
				exiting = 1;
				execpline2(pline->right,ASYNC,pipes[0],output,1);
				_exit(lastval);
				}
			else if (pid == -1)
				{
				zerr("fork failed: %e",errno);
				errflag = 1;
				}
			else
				{
				char *s,*text;

				close(pipes[0]);
				text = s = getptext(pline->right);
				for (;*s;s++)
				if (*s == '\n')
					*s = ';';
				untokenize(text);
				addproc(pid,text)->lastfg = 1;
				freepline(pline->right);
				pline->right = NULL;
				}
			}

		/* otherwise just do the pipeline normally. */

		execcomm(pline->left,input,pipes[1],how==ASYNC,0);
		pline->left = NULL;
		close(pipes[1]);
		if (pline->right)
			{
			execpline2(pline->right,how,pipes[0],output,last1);
			close(pipes[0]);
			}
		}
}

/* make the argv array */

char **makecline(char *nam,struct xlist *list)
{
int ct = 0;
Node node;
char **argv,**ptr;

	if (isset(XTRACE))
		{
		for (node = list->first; node; node = node->next,ct++);
		ptr = argv = (char **) zalloc((ct+2)*sizeof(char *));
		*ptr++ = nam;
		fprintf(stderr,"+ %s",nam);
		if (list->first)
			fputc(' ',stderr);
		for (node = list->first; node; node = node->next)
			if (*(char *) node->dat)
				{
				*ptr++ = node->dat;
				untokenize(node->dat);
				fputs(node->dat,stderr);
				if (node->next)
					fputc(' ',stderr);
				}
		*ptr = NULL;
		fputc('\n',stderr);
		return(argv);
		}
	else
		{
		for (node = list->first; node; node = node->next,ct++);
		ptr = argv = (char **) zalloc((ct+2)*sizeof(char *));
		*ptr++ = nam;
		for (node = list->first; node; node = node->next)
			if (*(char *) node->dat)
				{
				*ptr++ = node->dat;
				untokenize(node->dat);
				}
		*ptr = NULL;
		return(argv);
		}
}

/* untokenize the command line and remove null arguments */

void fixcline(table l)
{
Node node,next;

	for (node = l->first; node; node = next)
		{
		next = node->next;
		if (*(char *) node->dat)
			untokenize(node->dat);
		else
			remnode(l,node);
		}
}

void untokenize(char *s)
{
	for (; *s; s++)
		if (istok(*s))
			if (*s == HQUOT || *s == Nularg)
				chuck(s--);
			else
				*s = tokens[*s-Pound];
}

/* add vars to the environment */

void addenv(table list)
{
char *s,*t;

	while (s = getnode(list))
		{
		dovarsubs(&s);
		if (errflag)
			break;
		untokenize(s);
		t = getnode(list);
		dovarsubs(&t);
		if (errflag)
			break;
		untokenize(t);
		setparm(s,t,1,0);
		}
}

/* nonzero if we shouldn't clobber a file */

int dontclob(struct fnode *f)
{
struct stat buf;

	if (isset(CLOBBER) || f->type & 1)
		return 0;
	if (stat(f->u.name,&buf) == -1)
		return 1;
	return S_ISREG(buf.st_mode);
}

/* close an mnode (success) */

void closemn(struct mnode *mfds[10],int fd)
{
	if (mfds[fd])
		{
		if (mfds[fd]->ct > 1)
			if (mfds[fd]->rflag == 0)
				catproc(mfds[fd]);
			else
				teeproc(mfds[fd]);
		free(mfds[fd]);
		mfds[fd] = NULL;
		}
}

/* close all the mnodes (failure) */

void closemnodes(struct mnode *mfds[10])
{
int t0,t1;

	for (t0 = 0; t0 != 10; t0++)
		if (mfds[t0])
			{
			for (t1 = 0; t1 != mfds[t0]->ct; t1++)
				close(mfds[t0]->fds[t1]);
			free(mfds[t0]);
			mfds[t0] = NULL;
			}
}

/* add a fd to an mnode */
/* an mnode is a list of fds associated with a certain fd.
	thus if you do "foo >bar >ble", the mnode for fd 1 will have
	two fds, the result of open("bar",...), and the result of
	open("ble",....). */

void addfd(int forked,int save[10],struct mnode *mfds[10],int fd1,int fd2,int rflag)
{
int pipes[2];

	if (!mfds[fd1])	/* starting a new mnode */
		{
		mfds[fd1] = alloc(sizeof(struct mnode));
		if (!forked && fd1 != fd2 && fd1 < 10)
			save[fd1] = movefd(fd1);
		redup(fd2,fd1);
		mfds[fd1]->ct = 1;
		mfds[fd1]->fds[0] = fd1;
		mfds[fd1]->rflag = rflag;
		}
	else
		{
		if (mfds[fd1]->rflag != rflag)
			{
			zerr("file mode mismatch on fd %d",fd1);
			errflag = 1;
			return;
			}
		if (mfds[fd1]->ct == 1)		/* split the stream */
			{
			mfds[fd1]->fds[0] = movefd(fd1);
			mfds[fd1]->fds[1] = movefd(fd2);
			mpipe(pipes);
			mfds[fd1]->pipe = pipes[1-rflag];
			redup(pipes[rflag],fd1);
			mfds[fd1]->ct = 2;
			}
		else		/* add another fd to an already split stream */
			mfds[fd1]->fds[mfds[fd1]->ct++] = movefd(fd2);
		}
}

void execcomm(comm comm,int input,int output,int bkg,int last1)
{
int type;
long pid = 0;
table args = comm->args;
int save[10] = {0,0,0,0,0,0,0,0,0,0},gstat;
struct fnode *fn;
struct mnode *mfds[10] = {0,0,0,0,0,0,0,0,0,0};
int fil,forked = 0,iscursh = 0,t0;
struct chnode *chn = NULL;
char *text;
list l;

	if ((type = comm->type) == SIMPLE && !*comm->cmd)
		{
		if (comm->vars)
			addvars(comm->vars);
		return;
		}
	if (comm->cmd)
		{
		if (*comm->cmd == '%')
			{
			if (full(args))
				{
				zerrnam(comm->cmd,"too many arguments");
				return;
				}
			addnode(args,comm->cmd);
			comm->cmd = strdup((bkg) ? "bg" : "fg");
			bkg = 0;
			}
		docmdsubs(&comm->cmd);
		if (errflag)
			{
			freecmd(comm);
			lastval = 1;
			return;
			}
		untokenize(comm->cmd);
		}
	if (jobbing)	/* get the text associated with this command */
		{
		char *s;
		s = text = gettext(comm);
		for (;*s;s++)
			if (*s == '\n')
				*s = ';';
		untokenize(text);
		}
	else
		text = NULL;
	prefork(comm->args);	/* do prefork substitutions */
	if (comm->cmd && !(comm->flags & CFLAG_COMMAND))
		{
		if (isset(CORRECT) && jobbing)
			spckcmd(&comm->cmd);
		if ((l = gethnode(comm->cmd,shfunchtab)) &&
				!(comm->flags & CFLAG_COMMAND))
			{
			insnode(comm->args,(Node) comm->args,comm->cmd);
			comm->cmd = NULL;
			comm->left = duplist(l);
			type = comm->type = SHFUNC;
			}
		else
			chn = gethnode(comm->cmd,chtab);
		}
	if (unset(RMSTARSILENT) && jobbing && chn && chn->type != BUILTIN &&
			!strcmp(comm->cmd,"rm") && full(comm->args) &&
			((char *) comm->args->first->dat)[0] == Star &&
			((char *) comm->args->first->dat)[1] == '\0')
		checkrmall();
	if (errflag)
		{
		freecmd(comm);
		lastval = 1;
		return;
		}
	
	/* this is nonzero if comm is a current shell procedure */

	iscursh = (type >= CURSH) || (type == SIMPLE && chn &&
		chn->type == BUILTIN);

	gstat = (chn) ? chn->globstat : GLOB;

	/* if this command is backgrounded or (this is an external
		command and we are not exec'ing it) or this is a builtin
		with output piped somewhere, then fork.  If this is the
		last stage in a subshell pipeline, don't fork, but make
		the rest of the function think we forked. */

	if (bkg || !(iscursh || (comm->flags & CFLAG_EXEC)) ||
			(chn && chn->type == BUILTIN && output))
		{
		pid = (last1 && execok()) ? 0 : phork();
		if (pid == -1)
			return;
		if (pid)
			{
			if (pid == -1)
				zerr("%e",errno);
			else
				(void) addproc(pid,text);
			return;
			}
		entersubsh(bkg);
		forked = 1;
		}
	if (bkg && isset(BGNICE))	/* stupid */
		nice(5);
	if (input)		/* add pipeline input/output to mnodes */
		addfd(forked,save,mfds,0,input,0);
	if (output)
		addfd(forked,save,mfds,1,output,1);
	spawnpipes(comm->redir);		/* do process substitutions */
	while (full(comm->redir))
		if ((fn = getnode(comm->redir))->type == INPIPE)
			{
			if (fn->u.fd2 == -1)
				execerr();
			addfd(forked,save,mfds,fn->fd1,fn->u.fd2,0);
			free(fn);
			}
		else if (fn->type == OUTPIPE)
			{
			if (fn->u.fd2 == -1)
				execerr();
			addfd(forked,save,mfds,fn->fd1,fn->u.fd2,1);
			free(fn);
			}
		else
			{
			if (!(fn->type == HEREDOC || fn->type == CLOSE || fn->type ==
					MERGE || fn->type == MERGEOUT))
				if (xpandredir(fn,comm->redir))
					continue;
			if (fn->type == READ || fn->type == HEREDOC)
				{
				fil = (fn->type == READ) ? open(fn->u.name,O_RDONLY) : fn->u.fd2;
				if (fil == -1)
					{
					if (errno != EINTR)
						zerr("%e: %s",errno,fn->u.name);
					execerr();
					}
				addfd(forked,save,mfds,fn->fd1,fil,0);
				if (fn->type == READ)
					free(fn->u.name);
				}
			else if (fn->type == CLOSE)
				{
				if (!forked && fn->fd1 < 3)
					{
					zerr("can't close fd %d without forking",fn->fd1);
					execerr();
					}
				closemn(mfds,fn->fd1);
				close(fn->fd1);
				}
			else if (fn->type == MERGE || fn->type == MERGEOUT)
				{
				fil = dup(fn->u.fd2);
				if (mfds[fn->fd1])
					redup(fil,fn->fd1);
				else
					addfd(forked,save,mfds,fn->fd1,fil,fn->type == MERGEOUT);
				}
			else
				{
				if (fn->type >= APP)
					fil = open(fn->u.name,dontclob(fn) ?
						O_WRONLY|O_APPEND : O_WRONLY|O_APPEND|O_CREAT,0666);
				else
					fil = open(fn->u.name,dontclob(fn) ? 
						O_WRONLY|O_CREAT|O_EXCL : O_WRONLY|O_CREAT|O_TRUNC,0666);
				if (fil == -1)
					{
					if (errno != EINTR)
						zerr("%e: %s",errno,fn->u.name);
					execerr();
					}
				addfd(forked,save,mfds,fn->fd1,fil,1);
				free(fn->u.name);
				}
			free(fn);
			}
	postfork(comm->args,gstat);	/* perform postfork substitutions */
	if (errflag)
		{
		lastval = 1;
		goto err;
		}
	
	/* we are done with redirection.  close the mnodes, spawning
		tee/cat processes as necessary. */
	for (t0 = 0; t0 != 10; t0++)
		closemn(mfds,t0);

	if (unset(NOEXEC))
		if (type >= CURSH)	/* current shell proc */
			{
			void (*func[])(struct cnode *) = {execcursh,execshfunc,
				execfor,execwhile,execrepeat,execif,execcase,execselect};
	
			fixcline(comm->args);
			(func[type-CURSH])(comm);
			fflush(stdout);
			if (ferror(stdout))
				{
				zerr("write error: %e",errno);
				clearerr(stdout);
				}
			}
		else if (iscursh)		/* builtin */
			{
			int (*func)() = chn->u.func;

			if (comm->vars)
				{
				addvars(comm->vars);
				comm->vars = NULL;
				}
			fixcline(comm->args);
			if (func == test)		/* let test know if it is test or [ */
				insnode(comm->args,(Node) comm->args,strdup(comm->cmd));
			lastval = func(comm);
			if (isset(PRINTEXITVALUE) && lastval)
				zerr("exit %d",lastval);
			}
		else
			{
			if (comm->vars)
				addenv(comm->vars);
			if (type == SIMPLE)
				{
				closem();
				execute(comm->cmd,args);
				}
			else	/* ( ... ) */
				execlist(comm->left);
			}
err:
	if (forked)
		_exit(lastval);
	fixfds(save);
	freecmd(comm);
}

/* restore fds after redirecting a builtin */

void fixfds(int save[10])
{
int t0;

	for (t0 = 0; t0 != 10; t0++)
		if (save[t0])
			redup(save[t0],t0);
}

void entersubsh(int bkg)
{
	if (!jobbing)
		{
		if (bkg && isatty(0))
			{
			close(0);
			if (open("/dev/null",O_RDWR))
				{
				zerr("can't open /dev/null: %e",errno);
				_exit(1);
				}
			}
		}
	else if (!jobtab[curjob].gleader)
		{
		setpgrp(0L,jobtab[curjob].gleader = getpid());
		if (!bkg)
			attachtty(jobtab[curjob].gleader);
		}
	else
		setpgrp(0L,jobtab[curjob].gleader);
	subsh = 1;
	if (SHTTY != -1)
		{
		close(SHTTY);
		SHTTY = -1;
		}
	if (jobbing)
		{
		signal(SIGTTOU,SIG_DFL);
		signal(SIGTTIN,SIG_DFL);
		signal(SIGTSTP,SIG_DFL);
		signal(SIGPIPE,SIG_DFL);
		}
	if (interact)
		{
		signal(SIGTERM,SIG_DFL);
		if (sigtrapped[SIGINT])
			signal(SIGINT,SIG_IGN);
		}
	if (!sigtrapped[SIGQUIT])
		signal(SIGQUIT,SIG_DFL);
	opts[MONITOR] = OPT_UNSET;
	clearjobtab();
}

/* close all shell files */

void closem(void)
{
int t0;

	for (t0 = 10; t0 != NOFILE; t0++)
		close(t0);
}

/* get here document */

int gethere(char *str)
{
char pbuf[256],*nam = gettemp();
int tfil = creat(nam,0666);
FILE *in = fdopen(SHIN,"r");

	FOREVER
		{
		fgetline(pbuf,256,in);
		if (strcmp(str,pbuf))
			{
			pbuf[strlen(pbuf)] = '\n';
			write(tfil,pbuf,strlen(pbuf));
			}
		else
			break;
		}
	close(tfil);
	tfil = open(nam,O_RDONLY);
	unlink(nam);
	free(nam);
	return(tfil);
}

void catproc(struct mnode *mn)
{
int len,t0;
char *buf;

	if (phork())
		{
		for (t0 = 0; t0 != mn->ct; t0++)
			close(mn->fds[t0]);
		close(mn->pipe);
		return;
		}
	closeallelse(mn);
	buf = zalloc(4096);
	for (t0 = 0; t0 != mn->ct; t0++)
		while (len = read(mn->fds[t0],buf,4096))
			write(mn->pipe,buf,len);
	_exit(0);
}
 
void teeproc(struct mnode *mn)
{
int len,t0;
char *buf;

	if (phork())
		{
		for (t0 = 0; t0 != mn->ct; t0++)
			close(mn->fds[t0]);
		close(mn->pipe);
		return;
		}
	buf = zalloc(4096);
	closeallelse(mn);
	while ((len = read(mn->pipe,buf,4096)) > 0)
		for (t0 = 0; t0 != mn->ct; t0++)
			write(mn->fds[t0],buf,len);
	_exit(0);
}

void closeallelse(struct mnode *mn)
{
int t0,t1;

	for (t0 = 0; t0 != NOFILE; t0++)
		if (mn->pipe != t0)
			{
			for (t1 = 0; t1 != mn->ct; t1++)
				if (mn->fds[t1] == t0)
					break;
			if (t1 == mn->ct)
				close(t0);
			}
}

/* strtol() doesn't work right on my system */

long int zstrtol(const char *s,char **t,int base)
{
int ret = 0;
 
	for (; *s >= '0' && *s < ('0'+base); s++)
		ret = ret*base+*s-'0';
	if (t)
		*t = (char *) s;
	return ret;
}

/* $(...) */

table getoutput(char *cmd,int qt)
{
list list;
int pipes[2];

	if (*cmd == '<')
		{
		int stream;
		char *fi;

		fi = strdup(cmd+1);
		if (*fi == '~')
			*fi = Tilde;
		else if (*fi == '=')
			*fi = Equals;
		filesub((void **) &fi);
		if (errflag)
			return NULL;
		stream = open(fi,O_RDONLY);
		if (stream == -1)
			{
			magicerr();
			zerr("%e: %s",errno,cmd+1);
			return NULL;
			}
		return readoutput(stream,qt);
		}
	hungets(strdup(cmd));
	strinbeg();
	if (!(list = parlist(1)))
		{
		strinend();
		hflush();
		return NULL;
		}
	if (peek != EOF && peek != EMPTY)
		{
		strinend();
		hflush();
		return NULL;
		}
	strinend();
	mpipe(pipes);
	if (phork())
		{
		close(pipes[1]);
		return readoutput(pipes[0],qt);
		}
	subsh = 1;
	close(pipes[0]);
	redup(pipes[1],1);
	entersubsh(0);
	signal(SIGTSTP,SIG_IGN);
	exiting = 1;
	execlist(list);
	close(1);
	exit(0);  return NULL;
}

/* read output of command substitution */

table readoutput(int in,int qt)
{
table ret;
char *buf,*ptr;
int bsiz,c,cnt = 0;
FILE *fin;

	fin = fdopen(in,"r");
	ret = newtable();
	ptr = buf = zalloc(bsiz = 256);
	while ((c = fgetc(fin)) != EOF)
		if (!qt && znspace(c))
			{
			if (cnt)
				{
				*ptr = '\0';
				addnode(ret,strdup(buf));
				cnt = 0;
				ptr = buf;
				}
			}
		else
			{
			*ptr++ = c;
			if (++cnt == bsiz)
				{
				char *pp = zalloc(bsiz *= 2);
				
				memcpy(pp,buf,cnt);
				free(buf);
				ptr = (buf = pp)+cnt;
				}
			}
	if (qt && ptr != buf && ptr[-1] == '\n')
		ptr[-1] = '\0';
	if (cnt)
		addnode(ret,strdup(buf));
	free(buf);
	fclose(fin);
	return ret;
}

/* =(...) */

char *getoutputfile(char *cmd)
{
#ifdef WAITPID
int pid;
#endif
char *nam = gettemp(),*str;
int tfil;
list list;

	for (str = cmd; *str && *str != Outpar; str++);
	if (!*str)
		zerr("oops.");
	*str = '\0';
	hungets(strdup(cmd));
	strinbeg();
	if (!(list = parlist(1)))
		{
		hflush();
		strinend();
		return NULL;
		}
	if (peek != EOF && peek != EMPTY)
		{
		strinend();
		hflush();
		return NULL;
		}
	strinend();
	if (!jobtab[curjob].filelist)
		jobtab[curjob].filelist = newtable();
	addnode(jobtab[curjob].filelist,strdup(nam));
#ifdef WAITPID
	if (pid = phork())
		{
		waitpid(pid,NULL,WUNTRACED);
		return nam;
		}
#else
	if (waitfork())
		return nam;
#endif
	subsh = 1;
	close(1);
	entersubsh(0);
	tfil = creat(nam,0666);
	exiting = 1;
	execlist(list);
	close(1);
	exit(0); return NULL;
}

/* get a temporary named pipe */

char *namedpipe(void)
{
char *tnam = gettemp();

	mknod(tnam,0010666,0);
	return tnam;
}

/* <(...) */

char *getoutproc(char *cmd)
{
list list;
int fd;
char *pnam,*str;

	for (str = cmd; *str && *str != Outpar; str++);
	if (!*str)
		zerr("oops.");
	*str = '\0';
	hungets(strdup(cmd));
	strinbeg();
	if (!(list = parlist(1)))
		{
		strinend();
		hflush();
		return NULL;
		}
	if (peek != EOF && peek != EMPTY)
		{
		strinend();
		hflush();
		return NULL;
		}
	strinend();
	pnam = namedpipe();
	if (!jobtab[curjob].filelist)
		jobtab[curjob].filelist = newtable();
	addnode(jobtab[curjob].filelist,strdup(pnam));
	if (phork())
		return pnam;
	entersubsh(1);
	fd = open(pnam,O_WRONLY);
	if (fd == -1)
		{
		zerr("can't open %s: %e",pnam,errno);
		_exit(0);
		}
	redup(fd,1);
	fd = open("/dev/null",O_RDONLY);
	redup(fd,0);
	exiting = 1;
	execlist(list);
	close(1);
	_exit(0);  return NULL;
}

/* >(...) */

char *getinproc(char *cmd)
{
list list;
int pid,fd;
char *pnam,*str;

	for (str = cmd; *str && *str != Outpar; str++);
	if (!*str)
		zerr("oops.");
	*str = '\0';
	hungets(strdup(cmd));
	strinbeg();
	if (!(list = parlist(1)))
		{
		strinend();
		hflush();
		return NULL;
		}
	if (peek != EOF && peek != EMPTY)
		{
		strinend();
		hflush();
		return NULL;
		}
	strinend();
	pnam = namedpipe();
	if (!jobtab[curjob].filelist)
		jobtab[curjob].filelist = newtable();
	addnode(jobtab[curjob].filelist,strdup(pnam));
	if (pid = phork())
		return pnam;
	entersubsh(1);
	fd = open(pnam,O_RDONLY);
	redup(fd,0);
	exiting = 1;
	execlist(list);
	_exit(0);  return NULL;
}

/* > >(...) (does not use named pipes) */

int getinpipe(char *cmd)
{
list list;
int pipes[2];
char *str = cmd;

	for (str = cmd; *str && *str != Outpar; str++);
	if (!*str)
		zerr("oops.");
	*str = '\0';
	hungets(strdup(cmd+2));
	strinbeg();
	if (!(list = parlist(1)))
		{
		strinend();
		hflush();
		return -1;
		}
	if (peek != EOF && peek != EMPTY)
		{
		strinend();
		hflush();
		return NULL;
		}
	strinend();
	mpipe(pipes);
	if (phork())
		{
		close(pipes[1]);
		return pipes[0];
		}
	close(pipes[0]);
	entersubsh(1);
	redup(pipes[1],1);
	exiting = 1;
	execlist(list);
	_exit(0);  return 0;
}

/* < <(...) */

int getoutpipe(char *cmd)
{
list list;
int pipes[2];
char *str;

	for (str = cmd; *str && *str != Outpar; str++);
	if (!*str)
		zerr("oops.");
	*str = '\0';
	hungets(strdup(cmd+2));
	strinbeg();
	if (!(list = parlist(1)))
		{
		strinend();
		hflush();
		return -1;
		}
	if (peek != EOF && peek != EMPTY)
		{
		strinend();
		hflush();
		return NULL;
		}
	strinend();
	mpipe(pipes);
	if (phork())
		{
		close(pipes[0]);
		return pipes[1];
		}
	close(pipes[1]);
	entersubsh(1);
	redup(pipes[0],0);
	exiting = 1;
	execlist(list);
	_exit(0);  return 0;
}

/* run a list, saving the current job num */

void runlist(list l)
{
int cj = curjob;

	execlist(l);
	curjob = cj;
}

char *gettemp(void)
{
	return mktemp(strdup("/tmp/zshXXXXXX"));
}

/* my getwd; all the other ones I tried confused the SIGCHLD handler */

char *zgetwd(void)
{
static char buf0[MAXPATHLEN];
char buf3[MAXPATHLEN],*buf2 = buf0+1;
struct stat sbuf;
struct direct *de;
DIR *dir;
ino_t ino = -1;
dev_t dev = -1;

	holdintr();
	buf2[0] = '\0';
	buf0[0] = '/';
	for(;;)
		{
		if (stat(".",&sbuf) < 0)
			{
			chdir(buf0);
			noholdintr();
			return strdup(".");
			}
		ino = sbuf.st_ino;
		dev = sbuf.st_dev;
		if (stat("..",&sbuf) < 0)
			{
			chdir(buf0);
			noholdintr();
			return strdup(".");
			}
		if (sbuf.st_ino == ino && sbuf.st_dev == dev)
			{
			chdir(buf0);
			noholdintr();
			return strdup(buf0);
			}
		dir = opendir("..");
		if (!dir)
			{
			chdir(buf0);
			noholdintr();
			return strdup(".");
			}
		chdir("..");
		readdir(dir); readdir(dir);
		while (de = readdir(dir))
			if (de->d_fileno == ino)
				{
				lstat(de->d_name,&sbuf);
				if (sbuf.st_dev == dev)
					goto match;
				}
		rewinddir(dir);
		readdir(dir); readdir(dir);
		while (de = readdir(dir))
			{
			lstat(de->d_name,&sbuf);
			if (sbuf.st_dev == dev)
				goto match;
			}
		noholdintr();
		closedir(dir);
		return strdup(".");
match:
		strcpy(buf3,de->d_name);
		if (*buf2)
			strcat(buf3,"/");
		strcat(buf3,buf2);
		strcpy(buf2,buf3);
		closedir(dir);
		}
}

/* open pipes with fds >= 10 */

void mpipe(int pp[2])
{
	pipe(pp);
	pp[0] = movefd(pp[0]);
	pp[1] = movefd(pp[1]);
}

/* do process substitution with redirection */

void spawnpipes(table l)
{
Node n = l->first;
struct fnode *f;

	for (; n; n = n->next)
		{
		f = n->dat;
		if (f->type == OUTPIPE)
			{
			char *str = f->u.name;
			f->u.fd2 = getoutpipe(str);
			free(str);
			}
		if (f->type == INPIPE)
			{
			char *str = f->u.name;
			f->u.fd2 = getinpipe(str);
			free(str);
			}
		}
}

