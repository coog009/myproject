/* this function only record mouse x,y value */

#include <android/native_window.h>

#include "mouse.h"

Mouse::Mouse()
{
    FD_ZERO(&mSelectFds);

    /* initialize unix socket */
    struct sockaddr_in addr;
    //unlink(UNIX_PATH);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT) ; 
    addr.sin_addr.s_addr = INADDR_ANY ; 
    //strcpy(addr.sun_path, UNIX_PATH);
    //unsigned int len = strlen(addr.sun_path) + sizeof(addr.sun_family);
    int len = sizeof(struct sockaddr_in) ; 

    mUnixFd = socket(AF_INET, SOCK_STREAM, 0);
    if (mUnixFd < 0) {
        printf("unix_fd create error\n");
    }

    int opt = 1;
    setsockopt(mUnixFd, SOL_SOCKET,
            SO_REUSEADDR, (const void *) &opt, sizeof(opt));

    if (bind(mUnixFd, (struct sockaddr *)&addr, len) < 0) {
        printf("unix_fd bind error\n");
    } 

    if (listen(mUnixFd, 5) < 0) {
        printf("unix_fd listen error %d\n", errno);
    }

    FD_SET(mUnixFd, &mSelectFds);
    if (mUnixFd > mMaxFd) {
        mMaxFd = mUnixFd;
    }

    mDefaultBitmap = new SkBitmap();

    bool result = SkImageDecoder::DecodeFile(CURSOR_IMAGE_PATH,
            mDefaultBitmap,SkBitmap::kARGB_8888_Config, SkImageDecoder::kDecodePixels_Mode);

    if (!result) {
        printf("bitmap decode failed.\n");
        exit(-1);
    } else {
        printf("bitmap decode succeed. width %d height %d\n", mDefaultBitmap->width(), mDefaultBitmap->height());
    }

    mUseDefaultBitmap = true;
    mSurfaceComposerClient = new SurfaceComposerClient();

    mWidth = mDefaultBitmap->width();
    mHeight = mDefaultBitmap->height();
    mSpotX = IMAGE_HOT_X;
    mSpotY = IMAGE_HOT_Y;

    mSurfaceControl = mSurfaceComposerClient->createSurface(String8("MouseDev"), mWidth, mHeight, PIXEL_FORMAT_RGBA_8888, 0);

    mMove = false;
    mUpdateCursor = true;
    mWantChange = true;
    mSurfaceVisible = true;
    mIsRunning = true;
}

Mouse::~Mouse()
{
    delete mDefaultBitmap;
    close(mUnixFd);
    close(mDeviceFd);
}

int Mouse::OpenDev()
{
    mDeviceFd = open(DEVICE_PATH,  O_RDWR | O_CLOEXEC);

    if (mDeviceFd < 0) {
        printf("open %s failed\n", DEVICE_PATH);
        return -1;
    } else {
        printf("open %s succeed\n", DEVICE_PATH);
    }

    FD_SET(mDeviceFd, &mSelectFds);

    if (mDeviceFd > mMaxFd) {
        mMaxFd = mDeviceFd;
    }

    mDeviceOpened = true;

    return 0;
}

int Mouse::CloseDev()
{
    close(mDeviceFd);
    FD_CLR(mDeviceFd, &mSelectFds);
    mDeviceOpened = false;

    return 0;
}

int Mouse::DrawCursor(CursorBounds desireBounds)
{
    status_t status;

    if (mWantChange && mSurfaceVisible) {
        SurfaceComposerClient::openGlobalTransaction();
        status = mSurfaceControl->show();
        if (status) {
            printf("Error %d setting sprite surface show.\n", status);
        }
        SurfaceComposerClient::closeGlobalTransaction();
        mWantChange = false;
    }
    if (mWantChange && !mSurfaceVisible) {
        SurfaceComposerClient::openGlobalTransaction();
        status = mSurfaceControl->hide();
        if (status) {
            printf("Error %d setting sprite surface hide.\n", status);
        }
        SurfaceComposerClient::closeGlobalTransaction();
        mWantChange = false;
    }
        
    if (mUpdateCursor && mSurfaceVisible) {
        SurfaceComposerClient::openGlobalTransaction();

        status = mSurfaceControl->setLayer(INT_MAX - 1);
        if (status) {
            printf("Error %d setting sprite surface setlayer.\n", status);
        }
        
        SurfaceComposerClient::closeGlobalTransaction();

        SurfaceComposerClient::openGlobalTransaction();

        if (desireBounds.width != mWidth || desireBounds.height != mHeight) {
            //SurfaceComposerClient::openGlobalTransaction();
            status = mSurfaceControl->setSize(mWidth, mHeight);
            if (status) {
                printf("Error %d setting sprite surface show.\n", status);
            }

            mWidth = desireBounds.width;
            mHeight = desireBounds.height;
        }

        SurfaceComposerClient::closeGlobalTransaction();

        sp<Surface> surface = mSurfaceControl->getSurface();
        ANativeWindow_Buffer outBuffer;
        status = surface->lock(&outBuffer, NULL);
        SkBitmap surfaceBitmap;
        ssize_t bpr = outBuffer.stride * bytesPerPixel(outBuffer.format);
        surfaceBitmap.setConfig(SkBitmap::kARGB_8888_Config,
                outBuffer.width, outBuffer.height, bpr);
        
        printf("surface bpr size %d stride %d width %d height %d\n", bpr, outBuffer.stride, outBuffer.width, outBuffer.height);

        SkBitmap srcBitmap;
        bpr = desireBounds.width * bytesPerPixel(outBuffer.format);
        srcBitmap.setConfig(SkBitmap::kARGB_8888_Config, desireBounds.width,
                desireBounds.height, bpr);

        printf("srcbitmap bpr size %d width %d height %d\n", bpr, desireBounds.width, desireBounds.height);

        surfaceBitmap.setPixels(outBuffer.bits);
        srcBitmap.setPixels(mDrawBitmaps);

        SkCanvas surfaceCanvas(surfaceBitmap);

        SkPaint paint;
        paint.setXfermodeMode(SkXfermode::kSrc_Mode);

        surfaceCanvas.drawBitmap(srcBitmap, 0, 0, &paint);

        if (outBuffer.width > uint32_t(srcBitmap.width())) {
            paint.setColor(0); // transparent fill color
            surfaceCanvas.drawRectCoords(srcBitmap.width(), 0, outBuffer.width, srcBitmap.height(), paint);
        }
        if (outBuffer.height > uint32_t(srcBitmap.height())) {
            paint.setColor(0); // transparent fill color
            surfaceCanvas.drawRectCoords(0, srcBitmap.height(), outBuffer.width, outBuffer.height, paint);
        }

        status = surface->unlockAndPost();
        if (status) {
            printf("Error %d unlocking and posting sprite surface after drawing.\n", status);
        }

        mUpdateCursor = false;
    }

    if (mMove && mSurfaceVisible) {
        if (desireBounds.spotX != mSpotX) {
            mSpotX = desireBounds.spotX;
        }
        
        if (desireBounds.spotY != mSpotY) {
            mSpotY = desireBounds.spotY;
        }

        SurfaceComposerClient::openGlobalTransaction();

        status = mSurfaceControl->setPosition(mPos.x - mSpotX, mPos.y - mSpotY);
        if (status) {
            printf("Error %d setting sprite surface position.\n", status);
        }

        SurfaceComposerClient::closeGlobalTransaction();
        
        mMove = false;
    }

    return 0;
}

int Mouse::Loop()
{
    fd_set readfds;
    struct timeval timeout = {0, 10000};
    bool isDeviceAdd = false;

    if (access(DEVICE_PATH, 0) != 0) {
        printf("device do not exist\n");
        return -1;
    }

    /* open /dev/mouse */
    OpenDev();

    CursorBounds desireBounds;
    desireBounds.width = mWidth;
    desireBounds.height = mHeight;
    desireBounds.spotX = mSpotX;
    desireBounds.spotY = mSpotY;
    
    mDrawBitmaps = (char *)mDefaultBitmap->getPixels();

    printf("now draw default cursor bitmap\n");
    DrawCursor(desireBounds);
    printf("finish draw default cursor bitmap\n");

    while(mIsRunning)
    {
        readfds = mSelectFds;

        int retval;

        retval = select(mMaxFd + 1, &readfds, NULL, NULL, NULL);

        if (retval == 0) {
            printf("select time out\n");
            continue;
        }

        if (FD_ISSET(mUnixFd, &readfds)) {
            struct sockaddr_in address;
            int addrLength;
            int acceptFd = accept(mUnixFd, (struct sockaddr *)&address, &addrLength);
            if (acceptFd > mMaxFd) {
                mMaxFd = acceptFd;
            }

            mAcceptFds.add(String8("cursor"), acceptFd);
            FD_SET(acceptFd, &mSelectFds);
            printf("new connect accept\n");
        }

        if (FD_ISSET(mDeviceFd, &readfds)) {
            InputPosition tmpPos;
            bool isOK = true;
            int read_size = read(mDeviceFd, &tmpPos, sizeof(tmpPos));;
            if (read_size <= 0 && errno == ENODEV) {
                printf("device is closed\n");
                isOK = false;
            } else if(read_size > 0 && read_size % sizeof(tmpPos) != 0) {
                printf("read fd %d error\n", mDeviceFd);
                isOK = false;
            }

            if (isOK) {
                memcpy(&mPos, &tmpPos, sizeof(mPos));
                mMove = true;
            }
            DrawCursor(desireBounds);
            //printf("mouse move\n");
        }

        for (int i = 0;i < mAcceptFds.size();i++) {
            int acceptFd = mAcceptFds.valueAt(i);
            if (FD_ISSET(acceptFd, &readfds)) {
                CmdHeader header;
                int read_size;
                read_size = read(acceptFd, &header, sizeof(CmdHeader));
                if (read_size == 0) {
                    printf("remote close unix socket\n");
                    FD_CLR(acceptFd, &mSelectFds);
                    mAcceptFds.removeItemsAt(i);
                    close(acceptFd);
                    mWantChange = true;
                    mUpdateCursor = true;
                    if (mDrawBitmaps != NULL && !mUseDefaultBitmap) {
                        char *freeHeader = mDrawBitmaps - sizeof(CmdHeader) + sizeof(CursorBounds);
                        free(freeHeader);
                    }

                    desireBounds.width = mDefaultBitmap->width();
                    desireBounds.height = mDefaultBitmap->height();
                    desireBounds.spotX = IMAGE_HOT_X;
                    desireBounds.spotY = IMAGE_HOT_Y;

                    mUseDefaultBitmap = true;
                    mDrawBitmaps = mDefaultBitmap->getPixels();
                    DrawCursor(desireBounds);
                    continue;
                }
                char *data = (char *)malloc(sizeof(CmdHeader) + header.length);
                memcpy(data, &header, sizeof(CmdHeader));
                read_size = read(acceptFd, data + sizeof(CmdHeader), header.length);
                ProcessCmd(data, sizeof(CmdHeader) + header.length, desireBounds);
                DrawCursor(desireBounds);
                mUseDefaultBitmap = false;
            }
        }
    }

    return 0;
}

bool Mouse::GetStatus()
{
    return mIsRunning;
}

int Mouse::ProcessCmd(char *cmd, int cmdLength, CursorBounds &desireBounds)
{
    CmdHeader *header = (CmdHeader *)cmd;
    switch (header->type) {
        case MSG_UPDATE:                           
            memcpy(&desireBounds, cmd + sizeof(CmdHeader), sizeof(CursorBounds));
            if (mDrawBitmaps != NULL && !mUseDefaultBitmap) {
                char *freeHeader = mDrawBitmaps - sizeof(CmdHeader) - sizeof(CursorBounds);
                free(freeHeader);
            }
            mDrawBitmaps = cmd + sizeof(CmdHeader) + sizeof(CursorBounds);
            mUpdateCursor = true;
            mWantChange = true;
            printf("receive update msg width %d height %d hotx %d hoty %d\n", desireBounds.width,
                    desireBounds.height, desireBounds.spotX, desireBounds.spotY);
            break;
        default:
            printf("unknow message receive.\n");
            break;
    }

    return 0;
}

void draw_thread(void *arg)
{
    Mouse *test = (Mouse *)arg;
    test->Loop();
}

int main(int argc,char **argv)
{
    Mouse *test = new Mouse();

    sp<ProcessState> proc(ProcessState::self());
    proc->startThreadPool();

    pthread_t pid;

    pthread_create(&pid, NULL, (void *)draw_thread, test);

    IPCThreadState::self()->joinThreadPool();
    pthread_join(pid, NULL);
    return 0;
}
