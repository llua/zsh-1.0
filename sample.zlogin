eval $(tset -Q -s '?xterm')
stty dec crt kill ^U dsusp ^X
msgs -q
date
biff y

WATCH=$(echo $(<~/.friends) | tr ' ' :)
MAIL=/usr/spool/mail/$USERNAME
export DISPLAY=$(hostname):0.0

limit coredumpsize 0
/usr/games/fortune
uptime
