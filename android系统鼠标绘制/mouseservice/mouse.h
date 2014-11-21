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

#if 0
/* type */
#define EV_SYN 0x00,
#define EV_KEY 0x01,
#define EV_REL 0x02,

/* code */
#define REL_X 0x00
#define REL_Y 0x01
#define REL_Z 0x02
#define REL_RX 0x03
#define REL_RY 0x04
#define REL_RZ 0x05
#define REL_HWHEEL 0x06
#define REL_DIAL 0x07
#define REL_WHEEL 0x08
#define REL_MISC 0x09
#define REL_MAX 0x0f
#endif

/* input path */
#define DEVICE_PATH "/dev/input"

/* dev max name */
#define DEV_NAME 1024

using namespace android;

#define test_bit(bit, array)    (array[bit/8] & (1<<(bit%8)))

typedef struct RawAbsoluteAxisInfo {
    bool valid; // true if the information is valid, false otherwise
    int32_t minValue;  // minimum value
    int32_t maxValue;  // maximum value
    int32_t flat;      // center flat position, eg. flat == 8 means center is between -8 and 8
    int32_t fuzz;      // error tolerance, eg. fuzz == 4 means value is +/- 4 due to noise
    int32_t resolution; // resolution in units per mm or radians per mm
} RawAbsoluteAxisInfo;

typedef struct RawPointerAxes {
    RawAbsoluteAxisInfo x;
    RawAbsoluteAxisInfo y;
    RawAbsoluteAxisInfo pressure;
    RawAbsoluteAxisInfo touchMajor;
    RawAbsoluteAxisInfo touchMinor;
    RawAbsoluteAxisInfo toolMajor;
    RawAbsoluteAxisInfo toolMinor;
    RawAbsoluteAxisInfo orientation;
    RawAbsoluteAxisInfo distance;
    RawAbsoluteAxisInfo tiltX;
    RawAbsoluteAxisInfo tiltY;
    RawAbsoluteAxisInfo trackingId;
    RawAbsoluteAxisInfo slot;
} RawPointerAxes;

typedef struct Device {
    Device(int fd);
    int getAbsoluteAxisInfo(int axis, RawAbsoluteAxisInfo* outAxisInfo);
    uint8_t absBitmask[(ABS_MAX + 1) / 8];
    uint8_t relBitmask[(REL_MAX + 1) / 8];
    uint8_t swBitmask[(SW_MAX + 1) / 8];
    uint8_t ledBitmask[(LED_MAX + 1) / 8];
    uint8_t ffBitmask[(FF_MAX + 1) / 8];
    uint8_t propBitmask[(INPUT_PROP_MAX + 1) / 8];
    uint8_t keyBitmask[(KEY_MAX + 1) / 8];
    int id;
    RawPointerAxes rawPointerAxes;
} Device;

Device::Device(int fd)
{
    memset(keyBitmask, 0, sizeof(keyBitmask));
    memset(absBitmask, 0, sizeof(absBitmask));
    memset(relBitmask, 0, sizeof(relBitmask));
    memset(swBitmask, 0, sizeof(swBitmask));
    memset(ledBitmask, 0, sizeof(ledBitmask));
    memset(ffBitmask, 0, sizeof(ffBitmask));
    memset(propBitmask, 0, sizeof(propBitmask));

    id = fd;

    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBitmask)), keyBitmask);
    ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBitmask)), absBitmask);
    ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relBitmask)), relBitmask);
    ioctl(fd, EVIOCGBIT(EV_SW, sizeof(swBitmask)), swBitmask);
    ioctl(fd, EVIOCGBIT(EV_LED, sizeof(ledBitmask)), ledBitmask);
    ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ffBitmask)), ffBitmask);
    ioctl(fd, EVIOCGPROP(sizeof(propBitmask)), propBitmask);

    getAbsoluteAxisInfo(ABS_X, &rawPointerAxes.x);
    getAbsoluteAxisInfo(ABS_Y, &rawPointerAxes.y);
}

int Device::getAbsoluteAxisInfo(int axis, RawAbsoluteAxisInfo* outAxisInfo)
{
    if (axis >= 0 && axis <= ABS_MAX) {
        if (test_bit(axis, absBitmask)) {
            struct input_absinfo info;
            if(ioctl(id, EVIOCGABS(axis), &info)) {
                printf("ioctrl error\n");
                return -errno;
            }

            if (info.minimum != info.maximum) {
                outAxisInfo->valid = true;
                outAxisInfo->minValue = info.minimum;
                outAxisInfo->maxValue = info.maximum;
                outAxisInfo->flat = info.flat;
                outAxisInfo->fuzz = info.fuzz;
                outAxisInfo->resolution = info.resolution;
            }

            return 0;
        }
    }

    return -1;
}

class Mapper {
public:
    virtual ~Mapper();
    virtual void process(struct input_event *event, Device *device) = 0;
    virtual void dump() = 0;
};

Mapper::~Mapper()
{
}

class MotionMapper : public Mapper {
    public:
    MotionMapper(int bounds_x, int bounds_y);
    virtual ~MotionMapper();
    void process(struct input_event *event, Device *device);
    void getXY(int &x, int &y);
    void dump();

private:
    int mAccumulateX;
    int mAccumulateY;
    int mBoundsX;
    int mBoundsY;

};

class Mouse {
public:
    Mouse();
    ~Mouse();
    /* must run in a thread */
    void loop();
    void draw_cursor(Mapper *mapper);
    Mapper *get_mapper(char *mapper_name);
    bool get_status();

private:
    int scan_dir(char *dirname);
    void open_dev(char *filename);
    void close_dev(char *filename);
    void close_dev(int fd);

private:
    KeyedVector<String8, Device*> mDevices;
    KeyedVector<String8, Mapper*> mMappers;
    int mNotifyFd;
    int mNotifyWd;
    fd_set mSelectFds;
    bool isRunning;
    int mMaxFd;
    int mFbFd;
};

#endif
