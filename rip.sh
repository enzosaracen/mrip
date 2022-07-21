#!/bin/bash

FOPT="best[height<=1080;ext=mp4]"

for v in "$@"; do
	pushd vid
	youtube-dl -f "$FOPT" $v
	popd

	NAME="vid/$(youtube-dl -f "$FOPT" $v --get-filename)"
	LEN=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 $NAME)
	#WIDTH=$(ffprobe -v error -show_entries stream=width -of default=noprint_wrappers=1 $NAME | sed 's/.*=//g')
	HEIGHT=$(ffprobe -v error -show_entries stream=height -of default=noprint_wrappers=1 $NAME | sed 's/.*=//g')
	yes | ffmpeg -i "$NAME" -filter:v "crop=iw:2:0:$(($HEIGHT-$HEIGHT/8))" tmp/out.mp4

	./rip $LEN
	OUT="mid/$(basename "$NAME" | cut -f1 -d '.').mid"
	./write
	cp out.mid "$OUT"
done
