/*

	zsh.h - the header file, basically

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

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <signal.h>
#ifdef TERMIOS
#include <sys/termios.h>
#else
#include <sgtty.h>
#endif
#include <sys/param.h>
#include <sys/stat.h>

#define VERSIONSTR "zsh v1.0"

#define FOREVER for(;;)

/* size of job table */

#define MAXJOB 16

void *realloc(void *,int),*malloc(int),*calloc(int,int);

char *getenv(char *);

/* the tokens */

enum xfubar {
	HQUOT = -127,	/* quote char used for history */
	ALPOP,			/* marker, causes parser to pop alias stack */
	HERR,				/* history error indicator */
	Pound,			/* # */
	String,			/* $ */
	Hat,				/* ^ */
	Star,				/* * */
	Inpar,			/* ( */
	Outpar,			/* ) */
	Qstring,			/* $, in quotes */
	Equals,			/* = (initial) */
	Bar,				/* |, except when used as a pipe char */
	Inbrace,			/* {, except when used for current shells */
	Outbrace,		/* }, except when used for current shells */
	Inbrack,			/* [ */
	Outbrack,		/* ] */
	Tick,				/* ` */
	Inang,			/* <, except when used for redirection */
	Outang,			/* >, except when used for redirection */
	Quest,			/* ? */
	Tilde,			/* ~ (initial) */
	Qtick,			/* `, in quotes */
	Comma,			/* , */
	Nularg			/* marker, keeps explicit null arguments around,
							does some other stuff */
	};

/* HQUOT separately defined in readline.c */

/* returns true if X is a token */

#define istok(X) (((char) (X)) <= Nularg)

/* HQUOT in the form of a string */

#define HQUOTS "\x81"

/* ALPOP in the form of a string */

#define ALPOPS " \x82"

extern char **environ;

/* list of tokens */

extern char *tokens;

/* tokens used in peek variable with gettok/matchit */
/* do not confuse with above tokens; above tokens appear in strings,
	following tokens possible values of 'peek' variable */

enum xpeek {
	EMPTY,			/* nothing gotten yet */
	SEMI,				/* ; */
	DSEMI,			/* ;; */
	AMPER,			/* & */
	DAMPER,			/* && */
	NEWLIN,			/* \n */
	INPAR,			/* ( */
	INBRACE,			/* { */
	OUTPAR,			/* ) */
	OUTBRACE,		/* } */
	OUTANG,			/* > */
	OUTANGBANG,		/* >! */
	DOUTANG,			/* >> */
	DOUTANGBANG,	/* >>! */
	INANG,			/* < */
	DINANG,			/* << */
	INANGAMP,		/* <& */
	OUTANGAMP,		/* >& */
	OUTANGAMPBANG,	/* >&! */
	DOUTANGAMP,		/* >>& */
	DOUTANGAMPBANG,	/* >>&! */
	BAR,				/* | */
	DBAR,				/* || */
	BARAMP,			/* |& */
	BANG,				/* ! */
	STRING,			/* string of chars and tokens */
	ENVSTRING,		/* string of chars and tokens with a = in it */
						/* the below are all reserved words */
	DO,
	DONE,
	ESAC,
	THEN,
	ELIF,
	ELSE,
	FI,
	FOR,
	CASE,
	IF,
	WHILE,
	FUNC,
	REPEAT,
	TIME,
	UNTIL,
	EXEC,
	COMMAND,
	SELECT,
	COPROC
	};

/* linked list data type */

typedef struct xlist *table;
typedef struct xnode *Node;
 
struct xnode {
   Node next,last;
   void *dat;
   };
struct xlist {
   Node first,last;
   };


typedef struct pnode *pline;
typedef struct lnode *list;
typedef struct l2node *list2;
typedef struct cnode *comm;
typedef struct jobnode *job;

/* tree element for lists */

struct lnode {
   struct l2node *left;
   struct lnode *right;
   int type;
   };

enum ltype {
   SYNC,		/* ; */
	ASYNC		/* & */
   };

/* tree element for sublists */

struct l2node {
	struct pnode *left;
	struct l2node *right;
	int type;
	int flags; /* one of PFLAGS below; applies to pnode *left */
	};

enum l2type {
	ORNEXT = 10,	/* || */
	ANDNEXT			/* && */
	};

#define PFLAG_TIMED 4		/* time ... */
#define PFLAG_NOT 1			/* ! ... */
#define PFLAG_COPROC 32		/* coproc ... */

/* tree element for pipes */

struct pnode {
   struct cnode *left;
   struct pnode *right;
   int type;
   };

enum ptype {
   END,		/* pnode *right is null */
	PIPE		/* pnode *right is the rest of the pipeline */
   };

/* tree element for commands */

struct cnode {
   struct lnode *left;	/* for SUBSH/CURSH/SHFUNC */
   char *cmd;				/* command name */
   table args;				/* argmument list (char *'s) */
   table redir;			/* i/o redirections (struct fnode *'s) */
	table vars;				/* parameter list (char *'s), can be null;
									two entries in table for each parameter
									assignment; "name" and "value" */
   int type;
	int flags;
	void *info;				/* pointer to appropriate control structure,
									if this is a CFOR, CWHILE, etc. */
   };

enum ctype {
	SIMPLE,		/* simple command */
	SUBSH,		/* ( left ) */
	CURSH,		/* { left } */
	SHFUNC,		/* converted to { left } in execcomm */
	CFOR,
	CWHILE,
	CREPEAT,
	CIF,
	CCASE,
	CSELECT
	};
#define CFLAG_EXEC 1			/* exec ... */
#define CFLAG_COMMAND 2		/* command ... */

struct fnode {
	union {
		char *name;		/* source/dest filename */
		int fd2;			/* source/dest file descriptor */
		} u;
   int type;
	int fd1;				/* affected file descriptor */
   };

enum ftype {
	WRITE,			/* #> name */
	WRITENOW,		/* #>! name */
	APP,				/* #>> name */
	APPNOW,			/* #>>! name */
	READ,				/* #< name */
	HEREDOC,			/* #<< fd2 */
	MERGE,			/* #<& fd2 */
	MERGEOUT,		/* #>& fd2 */
	CLOSE,			/* #>&-, #<&- */
	INPIPE,			/* #< name, where name is <(...) */
	OUTPIPE,			/* #> name, where name is >(...)  */
	NONE
	};

struct fornode {		/* for/select */
	char *name;			/* parameter to assign values to */
	list list;			/* list of names to loop through */
	int inflag;			/* != 0 if 'in ...' was specified */
	};
struct casenode {		/* arg list of cnode struct contains word to test */
	struct casenode *next;	/* next pattern */
	char *pat;
	list list;			/* list to execute */
	};
struct ifnode {
	struct ifnode *next;
	list ifl;		/* if/elif test list (can be null in case of else) */
	list thenl;		/* then list */
	};
struct whilenode {
	list cont;		/* condition */
	list loop;		/* list to execute until condition met */
	int cond;		/* 0 for while, 1 for until */
	};
struct repeatnode {
	int count;		/* # of iterations */
	list list;
	};


/* structure used for multiple i/o redirection */
/* one for each fd open */

struct mnode {
	int ct;				/* # of redirections on this fd */
	int rflag;			/* 0 if open for reading, 1 if open for writing */
	int pipe;			/* fd of pipe if ct > 1 */
	int fds[NOFILE];
   }; 

/* node used in command hash table */

struct chnode 
{
	int type;
	int globstat;		/* status of filename gen for this command */
	union {
		char *nam;		/* full pathname if type != BUILTIN */
		int (*func)();	/* func to exec if type == BUILTIN */
		} u;
	};

enum chtype {
	EXCMD_PREDOT,		/* external command in PATH before . */
	EXCMD_POSTDOT,		/* external command in PATH after . */
	BUILTIN
	};

/* value for globstat field in chnode

	sample command: foo -xyz -pdq bar ble boz */

enum globx {
	GLOB,			/* all args globbed */
	MOSTGLOB,	/* ble, boz globbed */
	NOGLOB		/* no args globbed */
	};

/* node used in parameter hash table */

struct pmnode {
	union {
		char *str;		/* value */
		long val;		/* value if declared integer */
		} u;
	int isint;			/* != 0 if declared integer */
	};

/* tty state structure */

struct ttyinfo {
#ifdef TERMIOS
	struct termios termios;
#else
	struct sgttyb sgttyb;
	struct tchars tchars;
	struct ltchars ltchars;
#endif
	struct winsize winsize;
	};

extern struct ttyinfo shttyinfo;

/* entry in job table */

struct jobnode {
	long gleader;					/* process group leader of this job */
	int stat;
	char *cwd;						/* current working dir of shell when
											this job was spawned */
	struct procnode *procs;		/* list of processes */
	table filelist;				/* list of files to delete when done */
	struct ttyinfo ttyinfo;		/* saved tty state */
	};

#define STAT_CHANGED 1		/* status changed and not reported */
#define STAT_STOPPED 2		/* all procs stopped or exited */
#define STAT_TIMED 4			/* job is being timed */
#define STAT_DONE 8
#define STAT_LOCKED 16		/* shell is finished creating this job,
										may be deleted from job table */
#define STAT_INUSE 64		/* this job entry is in use */

#define SP_RUNNING -1		/* fake statusp for running jobs */

/* node in job process lists */

struct procnode {
	struct procnode *next;
	long pid;
	char *text;						/* text to print when 'jobs' is run */
	int statusp;					/* return code from wait3() */
	int lastfg;						/* set if this procnode represents a
											fragment of a pipeline run in a subshell
											for commands like:

											foo | bar | ble

											where foo is a current shell function
											or control structure.  The command
											actually executed is:

											foo | (bar | ble)

											That's two procnodes in the parent
											shell, the latter having this flag set. */
	struct timeval ru_utime;
	struct timeval ru_stime;
	time_t bgtime;					/* time job was spawned */
	time_t endtime;				/* time job exited */
	};

/* node in alias hash table */

struct anode {
	char *text;			/* expansion of alias */
	int cmd;				/* one for regular aliases,
								zero for -a aliases,
								negative for reserved words */
	int inuse;			/* alias is being expanded */
	};

/* node in sched list */

struct schnode {
	struct schnode *next;
	char *cmd;		/* command to run */
	time_t time;	/* when to run it */
	};

#define MAXAL 20	/* maximum number of aliases expanded at once */

typedef struct xhtab *htable;

/* node in hash table */

struct hnode {
	struct hnode *hchain;
	char *nam;
	void *dat;
	};

/* hash table structure */

struct xhtab {
	int hsize,ct;
	struct hnode **nodes;	 /* array of size hsize */
	};

typedef struct xpath *qath;	/* used in globbing - see glob.c */
typedef struct xcomp *comp;	/* "" */

extern char *sys_errlist[];
extern int errno;

#define pushnode(X,Y) insnode(X,(Node) X,Y)

#define OPT_INVALID 1	/* opt is invalid, like -$ */
#define OPT_UNSET 0
#define OPT_SET 2

#define CLOBBER '1'
#define NOBADPATTERN '2'
#define NONOMATCH '3'
#define GLOBDOTS '4'
#define NOTIFY '5'
#define ALLEXPORT 'a'
#define ERREXIT 'e'
#define BGNICE '6'
#define IGNOREEOF '7'
#define KEYWORD 'k'
#define MARKDIRS '8'
#define MONITOR 'm'
#define NOEXEC 'n'
#define NOGLOBOPT 'F'
#define NORCS 'f'
#define SHINSTDIN 's'
#define NOUNSET 'u'
#define VERBOSE 'v'
#define XTRACE 'x'
#define INTERACTIVE 'i'
#define AUTOLIST '9'
#define CORRECT '0'
#define DEXTRACT 'A'
#define NOBEEP 'B'
#define PRINTEXITVALUE 'C'
#define PUSHDTOHOME 'D'
#define PUSHDSILENT 'E'
#define NULLGLOB 'G'
#define RMSTARSILENT 'H'
#define IGNOREBRACES 'I'
#define CDABLEVARS 'J'
#define NOBANGHIST 'K'

#define ALSTAT_MORE 1	/* last alias ended with ' ' */
#define ALSTAT_JUNK 2	/* don't put word in history list */

#undef isset
#define isset(X) (opts[X])
#define unset(X) (!opts[X])
#define interact (isset(INTERACTIVE))
#define jobbing (isset(MONITOR))
#define nointr() signal(SIGINT,SIG_IGN)

#define SIGCOUNT (SIGUSR2+1)
#define SIGERR (SIGUSR2+1)
#define SIGDEBUG (SIGUSR2+2)
#define SIGEXIT 0

#define SP(x) (*((union wait *) &(x)))

#ifndef WEXITSTATUS
#define	WEXITSTATUS(x)	(((union wait*)&(x))->w_retcode)
#define	WTERMSIG(x)	(((union wait*)&(x))->w_termsig)
#define	WSTOPSIG(x)	(((union wait*)&(x))->w_stopsig)
#endif

#ifndef S_ISBLK
#define	_IFMT		0170000
#define	_IFDIR	0040000
#define	_IFCHR	0020000
#define	_IFBLK	0060000
#define	_IFREG	0100000
#define	_IFLNK	0120000
#define	_IFSOCK	0140000
#define	_IFIFO	0010000
#define	S_ISBLK(m)	(((m)&_IFMT) == _IFBLK)
#define	S_ISCHR(m)	(((m)&_IFMT) == _IFCHR)
#define	S_ISDIR(m)	(((m)&_IFMT) == _IFDIR)
#define	S_ISFIFO(m)	(((m)&_IFMT) == _IFIFO)
#define	S_ISREG(m)	(((m)&_IFMT) == _IFREG)
#define	S_ISLNK(m)	(((m)&_IFMT) == _IFLNK)
#define	S_ISSOCK(m)	(((m)&_IFMT) == _IFSOCK)
#endif

/* buffered shell input for non-interactive shells */

extern FILE *bshin;

/* null-terminated array of pointers to strings containing elements
	of PATH and CDPATH */

extern char **path,**cdpath;

/* number of elements in aforementioned array */

extern int pathct,cdpathct;

/* error/break flag */

extern int errflag;

/* current history event number */

extern int cev;

/* if != 0, this is the first line of the command */

extern int firstln;

/* if != 0, this is the first char of the command (not including
	white space */

extern int firstch;

/* first event number in the history table */

extern int tfev;

/* capacity of history table */

extern int tevs;

/* if = 1, we have performed history substitution on the current line
 	if = 2, we have used the 'p' modifier */

extern int hflag;

/* default event (usually cev-1, that is, "!!") */

extern int dev;

/* != 0 if we are in the middle of parsing a command (== 0 if we
 	have not yet parsed the command word */

extern int incmd;

/* the list of history events */

extern table histlist;

/* the current history event (can be NULL) */

extern table curtab;

/* the directory stack */

extern table dirstack;

/* a string containing all the ungot characters (hungetch()) */

extern char *ungots;

/* the pointer to the next character to read from ungots */

extern char *ungotptr;

/* the contents of the IFS parameter */

extern char *ifs;

/* != 0 if this is a subshell */

extern int subsh;

/* # of break levels (break builtin) */

extern int breaks;

/* != 0 if we have a return pending (return builtin) */

extern int retflag;

/* # of nested loops we are in */

extern int loops;

/* # of continue levels */

extern int contflag;

/* the job currently being created/waited for (not current job in the sense
 	of '+' and '-', that's topjob */

extern int curjob;

/* the current job (+) */

extern int topjob;

/* the previous job (-) */

extern int prevjob;

/* hash table containing the aliases and reserved words */

extern htable alhtab;

/* hash table containing the parameters */

extern htable parmhtab;

/* hash table containing the builtins/hashed commands */

extern htable chtab;

/* hash table containing the shell functions */

extern htable shfunchtab;

/* the job table */

extern struct jobnode jobtab[MAXJOB];

/* the list of sched jobs pending */

extern struct schnode *scheds;

/* the last l for s/l/r/ history substitution */

extern char *last;

/* the last r for s/l/r/ history substitution */

extern char *rast;

/* if peek == STRING or ENVSTRING, the next token */

extern char *tstr;

/* who am i */

extern char *username;

/* the return code of the last command */

extern int lastval;

/* != 0 if this is a login shell */

extern int islogin;

/* the next token (enum xtok) */

extern int peek;

/* the file descriptor associated with the next token, if it
 	is something like '<' or '>>', etc. */

extern int peekfd;

/* input fd from the coprocess */

extern int spin;

/* output fd from the coprocess */

extern int spout;

/* the last time we checked mail */

extern time_t lastmailcheck;

/* the last modified date on the mail file last time we checked */

extern time_t lastmailval;

/* the last time we checked the people in the WATCH variable */

extern time_t lastwatch;

/* the last time we did the periodic() shell function */

extern time_t lastperiod;

/* $SECONDS = time(NULL) - shtimer */

extern time_t shtimer;

/* the size of the mail file last time we checked */

extern off_t lastmailsize;

/* $$ */

extern long procnum;

/* $! (the pid of the last background command invoked */

extern long proclast;

/* the process group of the shell */

extern long shpgrp;

/* the current working directory */

extern char *cwd;

/* the hostname, truncated after the '.' */

extern char *hostM;

/* the hostname */

extern char *hostm;

/* the home directory */

extern char *home;

/* the positional parameters */

extern table pparms;

/* the list of local variables we have to destroy */

extern table locallist;

/* the shell input fd */

extern int SHIN;

/* the shell tty fd */

extern int SHTTY;

/* the stack of aliases we are expanding */

extern struct anode *alstack[MAXAL];

/* the alias stack pointer; also, the number of aliases currently
 	being expanded */

extern int alix;

/* != 0 means we are reading input from a string */

extern int strin;

/* == 1 means we are doing TAB expansion
 	== 2 means expansion has occurred during TAB expansion */

extern int magic;

/* period between periodic() commands, in seconds */

extern int period;

/* != 0 means history is turned off (through !" or setopt nobanghist) */

extern int stophist;

/* != 0 means we have removed the current event from the history list */

extern int histremmed;

/* the options; e.g. if opts['a'] is nonzero, -a is turned on */

extern int opts[128];

/* LINENO */

extern int lineno;

/* != 0 means we have called execlist() and then intend to exit(),
 	so don't fork if not necessary */

extern int exiting;

/* the limits for child processes */

extern struct rlimit limits[RLIM_NLIMITS];

/* the tokens */

extern char *tokens;

/* the current word in the history list */

extern char *hlastw;

/* the pointer to the current character in the current word
 	in the history list */

extern char *hlastp;

/* the size of the current word in the history list */

extern int hlastsz;

/* the alias expansion status - if == ALSTAT_MORE, we just finished
extern 	expanding an alias ending with a space */

extern int alstat;

/* we have printed a 'you have stopped (running) jobs.' message */

extern int stopmsg;

/* the default tty state */

extern struct ttyinfo shttyinfo;

/* signal names */

extern char *sigs[];

/* signals that are trapped = 1, signals ignored =2 */

extern int sigtrapped[];

