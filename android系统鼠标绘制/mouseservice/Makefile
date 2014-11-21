CC=/home/zhaojun/ndk/android-ndk-r9/toolchains/arm-linux-androideabi-4.8/prebuilt/linux-x86/bin/arm-linux-androideabi-g++ --sysroot=/home/zhaojun/ndk/android-ndk-r9/platforms/android-18/arch-arm
CFLAGS=-O3 -march=armv7-a -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=softfp -ffast-math
LDFLAGS=-L.

all:mouse

mouse:mouse.cpp
	$(CC) $< -o $@ ${CFLAGS} ${LDFLAGS}

clean:
	rm mouse
