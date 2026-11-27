# Build mochijsnative native plugin. ENGINE_INC must hold mbjs.h + mbjs_runtime.h.
ENGINE_INC ?= ../libmochiberry
TARGET ?= mochi_modules/lib/mochijsnative_x86_64.dll

.PHONY: build clean

build:
	gcc -shared -O2 -o $(TARGET) mochi_modules/src/mochijsnative.c -I$(ENGINE_INC) -Imochi_modules/include 

clean:
	rm -f mochi_modules/lib/*.dll mochi_modules/lib/*.so mochi_modules/lib/*.dylib
