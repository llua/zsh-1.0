/*

	init.c - initialization, main loop

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

void main(int argc, char **argv)
{
int notect = 0;

	setflags();
	parseargs(argv);
	setmoreflags();
	setupvals();
	initialize();
	runscripts();
	FOREVER
		{
		do
			loop();
		while (peek != EOF);
		if (!(isset(IGNOREEOF) && interact))
			{
			if (interact)
				fputs(islogin ? "logout\n" : "exit\n",stderr);
			zexit(NULL);
			continue;
			}
		zerrnam("\nzsh",(!islogin) ? "use 'exit' to exit."
			: "use 'logout' to logout.");
		notect++;
		if (notect == 10)
			zexit(NULL);
		}
}

/* keep executing lists until EOF found */

void loop(void)
{
list list;

	FOREVER
		{
		peek = EMPTY;
		if (interact)
			preprompt();
		hbegin();		/* init history mech */
		intr();			/* interrupts on */
		ainit();			/* init alias mech */
		if (!(list = parlist1(0)))
			{				/* if we couldn't parse a list */
			hend();
			if (!errflag)
				if (peek == OUTPAR)
					{
					errflag = 1;
					zerr("mismatched parenthesis");
					}
				else if (peek == OUTBRACE)
					{
					errflag = 1;
					zerr("mismatched brace");
					}
				else if (peek != EOF && peek != EMPTY && peek != NEWLIN)
					{
					errflag = 1;
					zerr("semicolon or newline expected");
					}
			if (peek == EOF && !errflag)
				return;
			continue;
			}
		if (peek != EMPTY && peek != EOF)
			{
			if (peek == OUTPAR)
				zerr("mismatched parenthesis");
			else if (peek == OUTBRACE)
				zerr("mismatched brace");
			else
				zerr("semicolon or newline expected");
			hend();
			errflag = 1;
			}
		else if (hend())
			{
			if (stopmsg)		/* unset 'you have stopped jobs' flag */
				stopmsg--;
			execlist(list);
			}
		if (ferror(stderr))
			{
			zerr("write error");
			clearerr(stderr);
			}
		if (subsh)				/* how'd we get this far in a subshell? */
			exit(lastval);
		if ((!interact && errflag) || retflag)
			return;
		if ((opts['t'] == OPT_SET) || (lastval && opts[ERREXIT] == OPT_SET))
			{
			if (sigtrapped[SIGEXIT])
				dotrap(SIGEXIT);
			exit(lastval);
			}
		}
}

void setflags(void)
{
int c;

	for (c = 0; c != 128; c++)
		opts[c] = OPT_INVALID;
	opts['a'] = opts['e'] = opts['f'] = opts['h'] = opts['k'] = opts['n'] = 
		opts['s'] = opts['t'] = opts['u'] = opts['v'] = opts['x'] = 
		opts['c'] = OPT_UNSET;
	opts['i'] = (isatty(0)) ? OPT_SET : OPT_UNSET;
	for (c = '0'; c <= '9'; c++)
		opts[c] = OPT_UNSET;
	for (c = 'A'; c <= 'K'; c++)
		opts[c] = OPT_UNSET;
	opts[BGNICE] = opts[NOTIFY] = OPT_SET;
}

static char *cmd;

void parseargs(char **argv)
{
char *argv0 = *argv;
int bk = 0;

	islogin = **(argv++) == '-';
	SHIN = 0;
	while (!bk && *argv && **argv == '-')
		{
		while (*++*argv)
			{
			if (opts[**argv] == OPT_INVALID)
				{
				zerr("bad option: -%c",**argv);
				exit(1);
				}
			opts[**argv] = OPT_SET;
			if (bk = **argv == 'b')
				break;
			if (**argv == 'c') /* -c command */
				{
				argv++;
				if (!*argv)
					{
					zerr("string expected after -c");
					exit(1);
					}
				cmd = strdup(*argv);
				opts[INTERACTIVE] = OPT_UNSET;
				break;
				}
			}
		argv++;
		}
	pparms = newtable();
	if (*argv)
		{
		if (opts[SHINSTDIN] == OPT_UNSET)
			{
			SHIN = movefd(open(argv0 = *argv,O_RDONLY));
			if (SHIN == -1)
				{
				zerr("can't open input file: %s",*argv);
				exit(1);
				}
			opts[INTERACTIVE] = OPT_UNSET;
			argv++;
			}
		addnode(pparms,argv0);  /* assign positional parameters */
		while (*argv)
			addnode(pparms,strdup(*argv++));
		}
	else
		{
		addnode(pparms,strdup(argv0));
		opts[SHINSTDIN] = OPT_SET;
		}
}

void setmoreflags(void)
{
	setlinebuf(stderr);
	setlinebuf(stdout);
	subsh = 0;
	opts[MONITOR] = (interact) ? OPT_SET : OPT_UNSET;
	if (jobbing)
		{
		SHTTY = movefd((isatty(0)) ? dup(0) : open("/dev/tty",O_RDWR));
		if (SHTTY == -1)
			opts[MONITOR] = OPT_UNSET;
		else
			gettyinfo(&shttyinfo);	/* get tty state */
		if ((shpgrp = getpgrp(0)) <= 0)
			opts[MONITOR] = OPT_UNSET;
		}
	else
		SHTTY = -1;
}

void setupvals(void)
{
struct passwd *pwd;
char *ptr;

	shtimer = time(NULL);	/* init $SECONDS */
	/* build various hash tables; argument to newhtable is table size */
	alhtab = newhtable(37);
	parmhtab = newhtable(17);
	shfunchtab = newhtable(17);
	if (interact)
		{
		if (!getparm("PROMPT"))
			setparm(strdup("PROMPT"),strdup("%M%# "),0,0);
		if (!getparm("PROMPT2"))
			setparm(strdup("PROMPT2"),strdup("> "),0,0);
		if (!getparm("PROMPT3"))
			setparm(strdup("PROMPT3"),strdup("?# "),0,0);
		}
	if (!getparm("PATH"))
		setparm(strdup("PATH"),strdup("/bin:/usr/bin:/usr/ucb"),1,0);
	setparm(strdup("VERSION"),strdup(VERSIONSTR),1,0);
	home = xsymlink(getparm("HOME"));
	setparm(strdup("HOME"),strdup(home),0,0);
	setiparm(strdup("UID"),getuid(),1);
	setiparm(strdup("EUID"),geteuid(),1);
	setiparm(strdup("PPID"),getppid(),1);
	lineno = 0;
	pwd = getpwuid(getuid());
	setparm(strdup("USERNAME"),strdup(pwd->pw_name),0,0);
	username = strdup(pwd->pw_name);
	setparm(strdup("HOSTTYPE"),strdup(HOSTTYP),0,0);
	cwd = zgetwd();
	setparm(strdup("PWD"),strdup(cwd),0,0);
	if (!getparm("IFS"))
		{
		ifs = strdup(" \t\n");
		setparm(strdup("IFS"),strdup(ifs),0,0);
		}
	hostM = alloc(512);	/* get hostname, with and without .podunk.edu */
	hostm = hostM+256;
	gethostname(hostm,256);
	gethostname(hostM,256);
	procnum = getpid();
	for (ptr = hostM; *ptr && *ptr != '.'; ptr++);
	*ptr = '\0';
}

void initialize(void)
{
int t0;

	breaks = loops = incmd = 0;
	lastmailcheck = 0;
	lastmailval = -1;
	tfev = 1;
	tevs = DEFAULT_HISTSIZE;
	histlist = newtable();
	dirstack = newtable();
	ungots = ungotptr = NULL;
	signal(SIGQUIT,SIG_IGN);
	for (t0 = 0; t0 != RLIM_NLIMITS; t0++)
		getrlimit(t0,limits+t0);
	last = rast = NULL;
	proclast = 0;
	if (!interact || SHTTY == -1)
		bshin = fdopen(SHIN,"r");
	signal(SIGCHLD,handler);
	addreswords();
	addhnode(strdup("false"),mkanode(strdup("let 0"),1),alhtab,NULL);
	addhnode(strdup("history"),mkanode(strdup("fc -l"),1),alhtab,NULL);
	addhnode(strdup("nohup"),mkanode(strdup("nohup "),1),alhtab,NULL);
	addhnode(strdup("r"),mkanode(strdup("fc -e -"),1),alhtab,NULL);
	addhnode(strdup("true"),mkanode(strdup(":"),1),alhtab,NULL);
	addhnode(strdup("pwd"),mkanode(strdup("echo $PWD"),1),alhtab,NULL);
	parsepath();
	parsecdpath();
	if (jobbing)
		{
		signal(SIGTTOU,SIG_IGN);
		signal(SIGTSTP,SIG_IGN);
		signal(SIGTTIN,SIG_IGN);
		signal(SIGPIPE,SIG_IGN);
		attachtty(shpgrp);
		}
	if (interact)
		{
		signal(SIGTERM,SIG_IGN);
		intr();
		}
}

void addreswords(void)
{
char *reswds[] = {
	"do", "done", "esac", "then", "elif", "else", "fi", "for", "case",
	"if", "while", "function", "repeat", "time", "until", "exec", "command",
	"select", "coproc", NULL
	};
int t0;

	for (t0 = 0; reswds[t0]; t0++)
		addhnode(strdup(reswds[t0]),mkanode(NULL,-1-t0),alhtab,NULL);
}

void runscripts(void)
{
	if (interact)
		checkfirstmail();
	if (opts[NORCS] == OPT_UNSET)
		{
#ifdef GLOBALZSHRC
		source(GLOBALZSHRC);
#endif
		sourcehome(".zshrc");
		if (islogin)
			{
#ifdef GLOBALZLOGIN
			source(GLOBALZLOGIN);
#endif
			sourcehome(".zlogin");
			}
		}
	if (opts['c'] == OPT_SET)
		{
		close(SHIN);
		SHIN = movefd(open("/dev/null",O_RDONLY));
		hungets(cmd);
		strinbeg();
		}
}

void ainit(void)
{
	alix = 0;		/* reset alias stack */
	alstat = 0;
	firstln = 1;
}
