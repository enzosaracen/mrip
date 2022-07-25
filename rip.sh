#!/bin/bash

FOPT="bestvideo[ext=mp4]+bestaudio[ext=m4a]/mp4"

for v in "$@"; do
	pushd vid
	yt-dlp -f "$FOPT" $v
	popd

	NAME="vid/$(yt-dlp -f "$FOPT" $v --get-filename)"
	LEN=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "$NAME")
	HEIGHT=$(ffprobe -v error -show_entries stream=height -of default=noprint_wrappers=1 "$NAME" | sed 's/.*=//g')
	yes | ffmpeg -i "$NAME" -filter:v "crop=iw:2:0:$(($HEIGHT-$HEIGHT/8))" tmp/out.mp4

	./rip $LEN
	OUT="mid/$(basename "$NAME" | cut -f1 -d '.').mid"
	./write
	cp out.mid "$OUT"
done
