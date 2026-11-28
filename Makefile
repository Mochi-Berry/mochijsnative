# Build mochijsnative native plugin.
# ENGINE_INC must hold mbjs.h + mbjs-jit.h; MOCHI_INC holds mbjs_runtime.h;
# DYNALIB_DIR holds dynalib.c / dynalib_abi.c / dynalib_trampoline.c + dynalib.h.
ENGINE_INC ?= E:/bcf/mochiberry
MOCHI_INC ?= E:/libmochiberry/mochinode
DYNALIB_DIR ?= E:/libmochiberry/libdynalib
ROOT := mochi_modules/mochijsnative
SRC := $(ROOT)/src/mochijsnative.c
DYNALIB_SRCS := $(DYNALIB_DIR)/dynalib.c $(DYNALIB_DIR)/dynalib_abi.c $(DYNALIB_DIR)/dynalib_trampoline.c
TARGET ?= $(ROOT)/lib/mochijsnative_x86_64.dll

.PHONY: build clean

build:
	gcc -shared -O2 -o $(TARGET) $(SRC) $(DYNALIB_SRCS) \
		-I$(ENGINE_INC) -I$(MOCHI_INC) -I$(DYNALIB_DIR) -I$(ROOT)/include

clean:
	rm -f $(ROOT)/lib/*.dll $(ROOT)/lib/*.so $(ROOT)/lib/*.dylib
