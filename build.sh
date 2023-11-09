#!/bin/sh

../../../emscripten/emcc src/main.c ../cJSON/cJSON.c -o exa/oxygen.js -I ../cJSON/  -sASYNCIFY_IMPORTS=wait_event 


