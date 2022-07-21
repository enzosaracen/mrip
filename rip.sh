#!/bin/bash

if [ "$#" -ne 1 ]; then
	exit 1
fi

NAME=$1
LEN=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 $1)
WIDTH=$(ffprobe -v error -show_entries stream=width -of default=noprint_wrappers=1 $1 | sed 's/.*=//g')
HEIGHT=$(ffprobe -v error -show_entries stream=height -of default=noprint_wrappers=1 $1 | sed 's/.*=//g')
yes | ffmpeg -i $1 -filter:v "crop=$WIDTH:2:0:$(($HEIGHT-$HEIGHT/8))" tmp/out.mp4

./rip $LEN

OUT="mid/$(basename "$NAME" | cut -f1 -d '.').mid"

./write

echo "write to '$OUT' (y/n)"
read -n1 Y
if [ "$Y" = "y" ]; then
	cp out.mid "$OUT"
fi
