/*

	config.local.h - local machine configuration file

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

/* a string corresponding to the host type */

#define HOSTTYP "sun4"

/* define if you prefer "suspended" to "stopped" */

#define USE_SUSPENDED

/* the path of zsh in the file system */

#define MYSELF "/usr/princeton/bin/zsh"

/* the default editor for the fc builtin */

#define DEFFCEDIT "/usr/ucb/vi"

/* the file to source whenever zsh is run; if undefined, don't source
	anything */

#define GLOBALZSHRC "/etc/zshrc"

/* the file to source whenever zsh is run as a login shell; if
	undefined, don't source anything */

#define GLOBALZLOGIN "/etc/zlogin"

/* the default HISTSIZE */

#define DEFAULT_HISTSIZE 128

/* the path of utmp */

#define UTMP_FILE "/etc/utmp"

/* the path of wtmp */

#define WTMP_FILE "/var/adm/wtmp"

/* define if you have problems with job control or tty modes.
	gcc-cpp does not seem to handle ioctls correctly. */

/*#define BUGGY_GCC*/

/* define if you like interactive comments */

/*#define INTERACTIVE_COMMENTS*/

/* define if you want warnings about nonexistent path components */

#define PATH_WARNINGS

