/*    gcc -o videoframe videoframe.c -lavformat -lavcodec -lavutil -lz -lm -lpthread -lswscale  */  
#include <X11/Xlib.h>
#include <X11/Xutil.h>      /* BitmapOpenFailed, etc. */
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#include <libavformat/avformat.h>  
#include <libavcodec/avcodec.h>  
#include <libavutil/avutil.h>  
#include <libswscale/swscale.h>  
#include <stdio.h>

#include "ffmpeg.h"
#include "sysdeps.h"
#include "vaapi_compat.h"
#include "vaapi.h"
#include "image.h"

static void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame)  
{  
    FILE *pFile;  
    char szFilename[32];  
    int y;  
    sprintf(szFilename, "frame%d.ppm", iFrame);  
    pFile = fopen(szFilename, "wb");  
    if(!pFile)  
        return;  
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);  //ppm ÎÄ¼þÍ·  
    for(y=0; y<height; y++)  
        fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);  
    fclose(pFile);  
}  

int main(int argc, const char *argv[])  
{  
    Display *display = XOpenDisplay(getenv("DISPLAY"));
    Window win;
    XImage *ximage;
    XShmSegmentInfo shminfo;

    win = XCreateSimpleWindow(display, RootWindow(display, DefaultScreen(display)),
            0, 0, 1, 1, 0,
            BlackPixel(display, DefaultScreen(display)),
            BlackPixel(display, DefaultScreen(display)));

    XMapWindow(display, win);
    XSync(display, False);

#if 1   
    AVFormatContext *pFormatCtx = NULL;  
    int i, videoStream;  
    AVCodecContext *pCodecCtx;  
    AVCodec *pCodec;  
    AVFrame *pFrame;  
    AVFrame *pFrameRGB;  
    AVPacket packet;  
    int frameFinished;  
    int numBytes;  
    uint8_t *buffer;  
#endif    
	avcodec_init();
    av_register_all();
	ffmpeg_vaapi_init(display);
	
    if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0) {  
        return -1;  
    }  
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0) {  
        return -1;  
    }  
    av_dump_format(pFormatCtx, -1, argv[1], 0);  
    videoStream = -1;  
    for(i=0; i<pFormatCtx->nb_streams; i++)  
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {  
            videoStream = i;  
            break;  
        }  
    if(videoStream == -1) {  
        return -1;  
    }  
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
#if 0
#ifdef USE_XVBA
    if (AV_CODEC_ID_H264 == pCodecCtx->codec_id)
        pCodec = &ff_h264_xvba_decoder;
    else if (AV_CODEC_ID_VC1 == pCodecCtx->codec_id)
        pCodec = &ff_vc1_xvba_decoder;
    else
#endif
#endif
	ffmpeg_init_context(pCodecCtx);
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);  
    printf("codec name %s\n", pCodec->name);

    if(pCodec == NULL) {  
        return -1;  
    }  
    if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {  
        return -1;  
    }  
    pFrame = avcodec_alloc_frame();  
    if(pFrame == NULL) {  
        return -1;  
    }  
    pFrameRGB = avcodec_alloc_frame();  
    if(pFrameRGB == NULL) {  
        return -1;  
    }  

    printf("compress level:%d\n", pCodecCtx->compression_level);

    numBytes = avpicture_get_size(AV_PIX_FMT_RGB32, pCodecCtx->width, pCodecCtx->height);  
    //buffer = av_malloc(numBytes);  
    //avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB32, pCodecCtx->width, pCodecCtx->height);  

    XResizeWindow(display, win, pCodecCtx->width, pCodecCtx->height);
    XSync(display, False);

    ximage = XShmCreateImage(display, DefaultVisual(display, 0), DefaultDepth(display, 0),
            ZPixmap, 0, &shminfo, pCodecCtx->width, pCodecCtx->height);

    shminfo.shmid = shmget(IPC_PRIVATE, numBytes, IPC_CREAT | 0777);
    if (shminfo.shmid < 0)
        exit(-1);

    shminfo.shmaddr = ximage->data = (unsigned char *)shmat(shminfo.shmid, 0, 0);
    if(shminfo.shmaddr == (char *) -1)
        exit(-1);

	Image *image = image_create(pCodecCtx->width, pCodecCtx->width, IMAGE_ARGB, ximage->data);
	if (!image) {
		printf("image create error\n");
	}
    
    avpicture_fill((AVPicture *)pFrameRGB, ximage->data, AV_PIX_FMT_RGB32, pCodecCtx->width, pCodecCtx->height);  

    shminfo.readOnly = False;
    XShmAttach(display, &shminfo);

    struct SwsContext *img_convert_ctx = NULL;  
    //img_convert_ctx = sws_getCachedContext(img_convert_ctx, pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB32, SWS_BILINEAR, NULL, NULL, NULL);  
    //if(!img_convert_ctx) {  
    //    fprintf(stderr, "Cannot initialize sws conversion context\n");  
    //    exit(1);  
    //}  

    i = 0;  
    while(av_read_frame(pFormatCtx, &packet) >=0) {  
        if(packet.stream_index == videoStream) {  
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);  
            //printf("%d\n", packet.size);
            if(frameFinished) {
				get_image((VASurfaceID)(uintptr_t)pFrame->data[3], image);
                //sws_scale(img_convert_ctx,(const uint8_t * const *)image->data, pFrame->linesize, 0 , pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);  
                //memcpy(ximage->data, pFrameRGB->data[0], numBytes);
                XShmPutImage(display, win, DefaultGC(display, 0), ximage, 0, 0, 0, 0, pCodecCtx->width, pCodecCtx->height, False);
                XFlush(display);
                i++;
                //printf("frame decode %d\n", i);
                //if(i++ < 50) {  
                //    SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);  
                //}  
            } else {
                printf("no frame decoded\n");
            }
        }  
        av_free_packet(&packet);  
        //usleep(20000);
    }  
    av_free(buffer);  
    XDestroyImage(ximage);
    av_free(pFrameRGB);  
    av_free(pFrame);
	ffmpeg_vaapi_exit();
    avcodec_close(pCodecCtx);  
    avformat_close_input(&pFormatCtx);  
    return 0;  
}  
