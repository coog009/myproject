#ifndef __MOUSE_H__
#define __MOUSE_H__

#include <stdio.h>
#include <linux/input.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>

#include <dirent.h>
#include <linux/fb.h>
#include <sys/mman.h>

#include <utils/KeyedVector.h>
#include <utils/List.h>
#include <utils/String8.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>

#include <SkCanvas.h>
#include <SkColor.h>
#include <SkPaint.h>
#include <SkXfermode.h>
#include <SkBitmap.h>
#include <SkImageDecoder.h>

#include <sys/socket.h>
#include <netinet/in.h>

#define MSG_UPDATE 0
#define MSG_HEART 1

/* input path */
#define DEVICE_PATH "/dev/mouse"

//#define UNIX_PATH "/cache/mouse_unix"
//#define TMP_IMAGE_STORE "/cache/tmp_cursor.dat"

#define CURSOR_IMAGE_PATH "/res/images/pointer_arrow.png"
#define IMAGE_HOT_X 6
#define IMAGE_HOT_Y 6

#define DEFAULT_WIDTH (30)
#define DEFAULT_HEIGHT  (30)

/* dev max name */
#define DEV_NAME 1024

#define SERVER_PORT 10240

using namespace android;

typedef struct InputPosition {
    int x;
    int y;
} InputPosition;

typedef struct CursorBounds {
    int width;
    int height;
    int spotX;
    int spotY;
} CursorBounds;

typedef struct CmdHeader {
    int type;
    int length;
} CmdHeader;

class Mouse {
    public:
        Mouse();
        ~Mouse();
        /* must run in a thread */
        int Loop();
        int DrawCursor(CursorBounds desireBounds);
        int ProcessCmd(char *cmd, int cmdLength, CursorBounds &desireBounds);
        bool GetStatus();
    private:
        int OpenDev();
        int CloseDev();

    private:
        KeyedVector<String8, int> mAcceptFds;
        fd_set mSelectFds;

        int mDeviceFd;

        bool mDeviceOpened;
        int mMaxFd;

        bool mIsRunning;
        sp<SurfaceControl> mSurfaceControl;
        sp<SurfaceComposerClient> mSurfaceComposerClient;

        bool mMove;
        bool mUpdateCursor;
        bool mWantChange;
        bool mSurfaceVisible;

        int mUnixFd;
        InputPosition mPos;
        int mSpotX;
        int mSpotY;
        int mWidth;
        int mHeight;
        char *mDrawBitmaps;
        int mDrawBitmapsLength;

        SkBitmap *mDefaultBitmap;
        bool mUseDefaultBitmap;
};

#endif
