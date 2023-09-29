piano video to midi converter

converts videos displaying a piano with keys that light up as the song plays into midi

these types of videos are often made with programs like synthesia, but mrip will work as long as there are clearly defined key colors and key press colors

color detection ranges were tweaked to fit the most popular video types, but they are not certain and can be tailored to fit different needs

this project uses [yt-dlp](https://github.com/yt-dlp/yt-dlp) to download videos much faster than the standard [youtube-dl](https://github.com/ytdl-org/youtube-dl), but if `yt-dlp` isn't working, you can replace all instances of `yt-dlp` with `youtube-dl` in `mrip.sh`

make sure to run `make` before trying to run `./mrip.sh`, `ffmpeg` is also required to crop the videos

run `./mrip.sh [youtube link]`

this will download the youtube video to `vid/` and write the midi file to `mid/`

the video will be cropped to a one pixel line of the keys stored in `tmp/out.mp4`

the file `tmp/nout` will be written to sequentially, and a line will be printed for each video frame processed. once the processing is done, `tmp/nout` will be copied to `mid/$videoname.mid` and `out.mid`

ripping the midi takes quite a bit of time, and there are definitely performance improvements to be made

due to how piano keys work and the cropping to smaller keysets in some of these videos, you may be prompted to choose between offsets if the key offset is ambiguous

during testing i have converted many piano tutorials, and the midis of all of these are provided in `mid/` already
