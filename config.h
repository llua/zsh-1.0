/*

	config.h - configuration file

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

/* define this if you have WAITPID */

#define WAITPID

/* define if the return type of signal handlers is int */

/* #define INTHANDTYPE  */

/* the return type of handlers */

#define HANDTYPE void

/* define this if you have putenv */

#define PUTENV 

/* define this if you have strstr */

#define STRSTR

/* define this if you have strftime */

/* #define STRFTIME */

/* define if you have struct termios, else struct sgttyb */

#define TERMIOS

#include "config.local.h"

