setenv() { export $1=$2 }

umask 022

PATH=$HOME/scr:$HOME/bin/$HOSTTYPE:/usr/local/bin:/usr/ucb:\
/usr/bin:/bin:/usr/local/bin/X11:/usr/etc:/etc
CDPATH=$HOME:/usr:/
export MANPATH=/usr/man:/usr/local/man

HISTSIZE=50
setopt ignoreeof

PROMPT='%n@%m [%h] %# '

alias a alias

a vi. 	'vi ~/.cshrc'
a s. 	'. ~/.cshrc'
a more. 'more ~/.cshrc'

chpwd() { pwd; ls -F }
lsl() { ls -algF $* | more }

a back cd -
a h history
a cls 	/usr/ucb/clear
a type 	cat
a copy 	cp
a move 	mv
a del 	rm
a lo 	exit
a lsa 	'ls -aF'
a dir   'ls -l'
a ll    'ls -F'

