#!/bin/sh
ffmpeg -vn -i foo.wav -ac 2 -ar 44100 -f wav -acodec adpcm_ms adpcm.wav
