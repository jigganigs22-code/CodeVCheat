CC      = xcrun -sdk iphoneos clang
CFLAGS  = -arch arm64 -mios-version-min=14.0 -O2 -Wall -Wextra -Werror \
          -fno-stack-protector -fvisibility=hidden -fomit-frame-pointer
LDFLAGS = -dynamiclib -install_name @executable_path/libcheat.dylib \
          -framework Foundation -framework UIKit

SRC     = src/cheat.c
OUT     = libcheat.dylib

.PHONY: all clean

all: $(OUT)

$(OUT): $(SRC) src/offsets.h src/hook.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC)

clean:
	rm -f $(OUT)
