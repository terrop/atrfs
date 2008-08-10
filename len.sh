#!/bin/sh
for a in *.flv; do
	echo "$a";
	len=$(mplayer -identify -frames 0 -ao null -vo null -- "$a" | grep ID_LENGTH)
	sec=$(echo $len | cut -d= -f2)
	setfattr -n user.length -v "$sec\000" -- "$a"
done
