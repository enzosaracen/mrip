#!/bin/bash

FOPT="bestvideo[ext=mp4]"

for v in "$@"; do
	pushd vid
	yt-dlp -f "$FOPT" $v
	popd

	NAME="vid/$(yt-dlp -f "$FOPT" $v --get-filename)"
	LEN=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "$NAME")

	./rip "$NAME" $LEN
	OUT="mid/$(basename "$NAME" | cut -f1 -d '.').mid"
	./write
	cp out.mid "$OUT"
done
