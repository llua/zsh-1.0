#! /bin/make -f
#
#	makefile for zsh
#
#	This file is part of zsh, the Z shell.
#
#  zsh is free software; no one can prevent you from reading the source
#  code, or giving it to someone else.
#  This file is copyrighted under the GNU General Public License, which
#  can be found in the file called COPYING.
#
#  Copyright (C) 1990 Paul Falstad
#
#  zsh is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY.  No author or distributor accepts
#  responsibility to anyone for the consequences of using it or for
#  whether it serves any particular purpose or works at all, unless he
#  says so in writing.  Refer to the GNU General Public License
#  for full details.
#
#  Everyone is granted permission to copy, modify and redistribute
#  zsh, but only under the conditions described in the GNU General Public
#  License.   A copy of this license is supposed to have been given to you
#  along with zsh so you can know your rights and responsibilities.
#  It should be in a file named COPYING.
#
#  Among other things, the copyright notice and this notice must be
#  preserved on all copies.
#
OBJS=hist.o glob.o table.o subst.o builtin.o loop.o vars.o\
parse.o lex.o init.o jobs.o exec.o zhistory.o utils.o math.o test.o
READLINE=readline/funmap.o readline/keymaps.o readline/readline.o
BINDIR=/usr/local/bin
MANDIR=/usr/local/man/man1
CFLAGS=-O -fstrength-reduce -fomit-frame-pointer -finline-functions \
	-fsigned-char -fdelayed-branch
CC=gcc
ZSHPATH=zsh
THINGS_TO_TAR=zsh.1 COPYING README INSTALL makefile sample.zshrc \
sample.zlogin sample.zshrc.mine sample.zlogin.mine \
alias.pro builtin.c builtin.pro config.h config.local.h \
exec.c exec.pro funcs.h glob.c glob.pro hist.c hist.pro init.c \
init.pro jobs.c jobs.pro lex.c lex.pro loop.c loop.pro math.c \
math.pro parse.c parse.pro subst.c subst.pro table.c \
table.pro test.c test.pro utils.c utils.pro vars.c vars.pro zhistory.c \
readline/chardefs.h readline/emacs_keymap.c \
readline/funmap.c readline/history.h readline/keymaps.c \
readline/keymaps.h readline/readline.c readline/readline.h \
readline/vi_keymap.c readline/vi_mode.c readline/Makefile \
zsh.h proto

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

$(ZSHPATH): $(OBJS) $(READLINE)
	$(CC) -o $(ZSHPATH) $(OBJS) $(READLINE) -s -ltermcap

$(OBJS): config.h

zhistory.o: readline/history.h

$(READLINE):
	cd readline;make

clean:
	rm -f *.o zsh core readline/*.o

install: zsh
	install -s -m 755 zsh $(BINDIR)
	install -m 444 zsh.1 $(MANDIR)

tar: zsh.tar.Z

zsh.tar: $(THINGS_TO_TAR)
	tar -cf zsh.tar $(THINGS_TO_TAR)

zsh.tar.Z: zsh.tar
	compress -f zsh.tar

shar: $(THINGS_TO_TAR)
	bundle $(THINGS_TO_TAR) > zsh.shar

