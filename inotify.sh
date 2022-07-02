#!/bin/sh -
cd "${1:-$HOME/mcchost/log}" || exit

PREV=''
FN2=''
FND='/dev/null'

inotifywait -q -e modify -e create -m . |
while read dot event FN
do
    case "$FN" in
    *-*-*.log) ;;
    * ) continue;;
    esac

    if [ "$FN" != "$FN2" ] 
    then
	if [ "$FN" != "$PREV" ]
	then
	    FN2="$PREV"
	    PREV="$FN"
	    [ "$FN2" != '' ] &&
		FND="$FN2"
	fi
    fi

    cat "$FND" "$FN" | tail -100 > /tmp/_in_log.new
    diff -e /tmp/_in_log.old /tmp/_in_log.new |
	sed -e '/^[0-9]*a$/d' -e '/^\.$/d' -e '/^[0-9,]*d$/d'

    mv /tmp/_in_log.new /tmp/_in_log.old
done

exit

# This leaves "tail" commands running.
FN="$(date +%F).log"
inotifywait -q -e create -m . |
while
  tail -f "$FN" & PID=$!
  read dot event NF
do  case "$NF" in
    *-*-*.log) FN="$NF" ;;
    esac
    kill $PID
done

exit

inotifywait -m -e create -q . | while read dir event file; do
    kill %tail
    tail -f "$file" &
done
kill %tail
