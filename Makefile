DEBUG ?= 0
EMCC ?= emcc

all: exa/oxygen.js exa/oxygen.wasm

exa/oxygen.js exa/oxygen.wasm &: src/main.c ../cJSON/cJSON.c | exa
	$(EMCC) $^ -o exa/oxygen.js -I ../cJSON/  -sASYNCIFY_IMPORTS=wait_event -DDEBUG=$(DEBUG)

exa:
	mkdir -p exa

clean:
	rm -f exa/oxygen.js exa/oxygen.wasm

.PHONY: all clean
