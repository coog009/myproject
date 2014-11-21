XCFLAGS=-DUSE_FFFMPEG_VAAPI -DHAVE_SWSCALE `pkg-config --cflags x11` `pkg-config --cflags xext`
XLDFLAGS=`pkg-config --libs x11` `pkg-config --libs xext` -lavcodec -lavformat -lswscale -lswresample -lavutil -lva -lva-x11

SOURCE_FILES= ffmpeg_player.c 

CC=gcc -g

all:ffmpeg_player

ffmpeg_player:ffmpeg_player.c
	$(CC) -o $@ $< $(XCFLAGS) $(XLDFLAGS)

clean:
	rm -rf ffmpeg_player ffmpeg_player.o
