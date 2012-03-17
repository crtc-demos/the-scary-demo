#!/bin/sh
ffmpeg -vn -i "$1" -ac 2 -ar 44100 -f wav -acodec adpcm_ms "$2"
