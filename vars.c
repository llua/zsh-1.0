/*

	vars.c - variable declarations

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

/* buffered shell input for non-interactive shells */

FILE *bshin;

/* null-terminated array of pointers to strings containing elements
	of PATH and CDPATH */

char **path,**cdpath;

/* number of elements in aforementioned array */

int pathct,cdpathct;

/* error/break flag */

int errflag = 0;

/* current history event number */

int cev = 0;

/* if != 0, this is the first line of the command */

int firstln;

/* if != 0, this is the first char of the command (not including
	white space */

int firstch;

/* first event number in the history table */

int tfev = 1;

/* capacity of history table */

int tevs = 20;

/* if = 1, we have performed history substitution on the current line
	if = 2, we have used the 'p' modifier */

int hflag;

/* default event (usually cev-1, that is, "!!") */

int dev = 0;

/* != 0 if we are in the middle of parsing a command (== 0 if we
	have not yet parsed the command word */

int incmd = 0;

/* the list of history events */

table histlist;

/* the current history event (can be NULL) */

table curtab;

/* the directory stack */

table dirstack;

/* a string containing all the ungot characters (hungetch()) */

char *ungots;

/* the pointer to the next character to read from ungots */

char *ungotptr;

/* the contents of the IFS parameter */

char *ifs;

/* != 0 if this is a subshell */

int subsh;

/* # of break levels (break builtin) */

int breaks;

/* != 0 if we have a return pending (return builtin) */

int retflag;

/* # of nested loops we are in */

int loops;

/* # of continue levels */

int contflag;

/* the job currently being created/waited for (not current job in the sense
	of '+' and '-', that's topjob */

int curjob;

/* the current job (+) */

int topjob = -1;

/* the previous job (-) */

int prevjob = -1;

/* hash table containing the aliases and reserved words */

htable alhtab;

/* hash table containing the parameters */

htable parmhtab;

/* hash table containing the builtins/hashed commands */

htable chtab;

/* hash table containing the shell functions */

htable shfunchtab;

/* the job table */

struct jobnode jobtab[MAXJOB];

/* the list of sched jobs pending */

struct schnode *scheds = NULL;

/* the last l for s/l/r/ history substitution */

char *last;

/* the last r for s/l/r/ history substitution */

char *rast;

/* if peek == STRING or ENVSTRING, the next token */

char *tstr;

/* who am i */

char *username;

/* the return code of the last command */

int lastval;

/* != 0 if this is a login shell */

int islogin;

/* the next token (enum xtok) */

int peek;

/* the file descriptor associated with the next token, if it
	is something like '<' or '>>', etc. */

int peekfd;

/* input fd from the coprocess */

int spin = -1;

/* output fd from the coprocess */

int spout = -1;

/* the last time we checked mail */

time_t lastmailcheck;

/* the last modified date on the mail file last time we checked */

time_t lastmailval;

/* the last time we checked the people in the WATCH variable */

time_t lastwatch;

/* the last time we did the periodic() shell function */

time_t lastperiod;

/* $SECONDS = time(NULL) - shtimer */

time_t shtimer;

/* the size of the mail file last time we checked */

off_t lastmailsize;

/* $$ */

long procnum;

/* $! (the pid of the last background command invoked */

long proclast;

/* the process group of the shell */

long shpgrp;

/* the current working directory */

char *cwd;

/* the hostname, truncated after the '.' */

char *hostM;

/* the hostname */

char *hostm;

/* the home directory */

char *home;

/* the positional parameters */

table pparms;

/* the list of local variables we have to destroy */

table locallist;

/* the shell input fd */

int SHIN;

/* the shell tty fd */

int SHTTY;

/* the stack of aliases we are expanding */

struct anode *alstack[MAXAL];

/* the alias stack pointer; also, the number of aliases currently
	being expanded */

int alix = 0;

/* != 0 means we are reading input from a string */

int strin = 0;

/* == 1 means we are doing TAB expansion
	== 2 means expansion has occurred during TAB expansion */

int magic = 0;

/* period between periodic() commands, in seconds */

int period = 0;

/* != 0 means history is turned off (through !" or setopt nobanghist) */

int stophist = 0;

/* != 0 means we have removed the current event from the history list */

int histremmed = 0;

/* the options; e.g. if opts['a'] is nonzero, -a is turned on */

int opts[128];

/* LINENO */

int lineno;

/* != 0 means we have called execlist() and then intend to exit(),
	so don't fork if not necessary */

int exiting = 0;

/* the limits for child processes */

struct rlimit limits[RLIM_NLIMITS];

/* the tokens */

char *tokens = "#$^*()$=|{}[]`<>?~`,";

/* the current word in the history list */

char *hlastw;

/* the pointer to the current character in the current word
	in the history list */

char *hlastp;

/* the size of the current word in the history list */

int hlastsz;

/* the alias expansion status - if == ALSTAT_MORE, we just finished
	expanding an alias ending with a space */

int alstat;

/* we have printed a 'you have stopped (running) jobs.' message */

int stopmsg;

/* the default tty state */

struct ttyinfo shttyinfo;

/* signal names */

char *sigs[] = { "EXIT",
 "HUP", "INT", "QUIT", "ILL", "TRAP", "IOT", "EMT", "FPE", "KILL",
 "BUS", "SEGV", "SYS", "PIPE", "ALRM", "TERM", "URG", "STOP", "TSTP",
 "CONT", "CHLD", "TTIN", "TTOU", "IO", "XCPU", "XFSZ", "VTALRM", "PROF",
 "WINCH", "LOST", "USR1", "USR2", "ERR", "DEBUG"
};

/* signals that are trapped = 1, signals ignored =2 */

int sigtrapped[SIGCOUNT+2];

