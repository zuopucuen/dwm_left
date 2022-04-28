#!/bin/bash
gcc -c alsa_audio.o alsa_audio.c -lasound
gcc -c barM.o barM.c -lX11
gcc -o barM barM.o alsa_audio.o -lX11 -lasound
