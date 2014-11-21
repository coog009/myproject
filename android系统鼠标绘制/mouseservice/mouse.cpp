/* this function only record mouse x,y value */

#include "mouse.h"

MotionMapper::MotionMapper(int bounds_x, int bounds_y)
{
    mBoundsX = bounds_x;
    mBoundsY = bounds_y;

    mAccumulateX = 0;
    mAccumulateY = 0;
}

MotionMapper::~MotionMapper()
{
}

void MotionMapper::process(struct input_event *event, Device *device)
{
    if (!(test_bit(BTN_MOUSE, device->keyBitmask)
            && test_bit(REL_X, device->relBitmask)
            && test_bit(REL_Y, device->relBitmask))) {
        printf("this device is not cursor\n");
        return;
    }

    //int rawWidth = device->rawPointerAxes.x.maxValue - device->rawPointerAxes.x.minValue + 1;
    //int rawHeight = device->rawPointerAxes.y.maxValue - device->rawPointerAxes.y.minValue + 1;

    if (event->type == EV_REL) {
        switch (event->code) {
            case REL_X:
                float scale_x;
                scale_x = 1.0;
                //scale_x = float(mBoundsX) / rawWidth;
                mAccumulateX += (event->value * scale_x);
                if (mAccumulateX < 0) {
                    mAccumulateX = 0;
                } else if (mAccumulateX > mBoundsX) {
                    mAccumulateX = mBoundsX;
                }
                printf("x move orig %d scale %f\n", event->value, scale_x);
                break;
            case REL_Y:
                float scale_y;
                scale_y = 1.0;
                //scale_y = float(mBoundsY) / rawHeight;
                mAccumulateY += (event->value * scale_y);
                if (mAccumulateY < 0) {
                    mAccumulateY = 0;
                } else if (mAccumulateY > mBoundsY) {
                    mAccumulateY = mBoundsY;
                }
                printf("y move orig %d scale %f\n", event->value, scale_y);
                break;
        }
    }

    dump();
}

void MotionMapper::getXY(int &x, int &y)
{
    x = mAccumulateX;
    y = mAccumulateY;
}

void MotionMapper::dump()
{
    printf("mouse pos x:%d y:%d\n", mAccumulateX, mAccumulateY);
}

Mouse::Mouse()
{
    FD_ZERO(&mSelectFds);

    mNotifyFd = inotify_init();
    mNotifyWd = inotify_add_watch(mNotifyFd, DEVICE_PATH, IN_DELETE | IN_CREATE);
    FD_SET(mNotifyFd, &mSelectFds);

    mMaxFd = mNotifyFd;

    isRunning = true;

    /* add mapper */
    Mapper *mapper = new MotionMapper(1440, 900);

    mMappers.add(String8("motion"), mapper);

    mFbFd = open("/dev/graphics/fb0", O_RDWR | O_CLOEXEC);
    if (!mFbFd) {
        printf("framebuffer open failed\n");
    }
}

Mouse::~Mouse()
{
    inotify_rm_watch(mNotifyFd, mNotifyWd);

    while (!mDevices.isEmpty()) {
        Device *device = mDevices.valueAt(0);
        close(device->id);
        delete device;
        mDevices.removeItemsAt(0);
    }

    while (!mMappers.isEmpty()) {
        Mapper *mapper = mMappers.valueAt(0);
        delete mapper;
        mMappers.removeItemsAt(0);
    }

    close(mFbFd);
}

void Mouse::open_dev(char *filename)
{
    int fd = open(filename,  O_RDWR | O_CLOEXEC);

    if (fd < 0) {
        printf("open %s failed\n", filename);
    } else {
        printf("open %s succeed\n", filename);
    }
    
    FD_SET(fd, &mSelectFds);

    if (fd > mMaxFd) {
        mMaxFd = fd;
    }

    Device *device = new Device(fd);
    mDevices.add(String8(filename), device);
}

void Mouse::close_dev(char *filename)
{
    int index = mDevices.indexOfKey(String8(filename));
    if (index < 0) {
        printf("could not find %s in device\n", filename);
        return; 
    }

    Device *device = mDevices.valueAt(index);
    printf("now close %s\n", (mDevices.keyAt(index)).string());
    close(device->id);
    FD_CLR(device->id, &mSelectFds);
    delete device;
    mDevices.removeItemsAt(index);
}

void Mouse::close_dev(int fd)
{
    for (int i = 0;i < mDevices.size();i++) {
        Device *device = mDevices.valueAt(i);
        if (device->id == fd) {
            close(device->id);
            FD_CLR(device->id, &mSelectFds);
            delete device;
            mDevices.removeItemsAt(i);
            break;
        }
    }
}

int Mouse::scan_dir(char *dirname)
{
    char devname[DEV_NAME];
    char *filename;
    DIR *dir;
    struct dirent *de;

    dir = opendir(dirname);

    if (dir == NULL) {
        return -1;
    }

    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    while((de = readdir(dir))) {
        if(de->d_name[0] == '.' &&
                (de->d_name[1] == '\0' ||
                 (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;
        strcpy(filename, de->d_name);
        open_dev(devname);
    }

    closedir(dir);

    return 0;
}

void Mouse::draw_cursor(Mapper *mapper)
{
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    char *fbp = 0;
    MotionMapper *motion_mapper = (MotionMapper *)mapper;

    if (ioctl(mFbFd, FBIOGET_FSCREENINFO, &finfo)) {
        printf("read fixed information error\n");
        return;
    }

    if (ioctl(mFbFd, FBIOGET_VSCREENINFO, &vinfo)) {
        printf("read variable information error\n");
        return;
    }

    fbp = (char *)mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, mFbFd, 0);
    if ((int)fbp == -1) {
        printf("mmap failed\n");
        return;
    }

    int x, y;
    motion_mapper->getXY(x, y);
    for (int i = x;i < vinfo.xres && i < (x + 20);i++) {
        for (int j = y;j < vinfo.yres && j < (y + 20);j++) {
            int location = (i + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) + 
                (j + vinfo.yoffset) * finfo.line_length;

            if (vinfo.bits_per_pixel == 32) {
                *(fbp + location) = 100;
                *(fbp + location + 1) = 100;
                *(fbp + location + 2) = 100;
                *(fbp + location + 3) = 0;
            }
        }
    }

    munmap(fbp, finfo.smem_len);
}

void Mouse::loop()
{
    if (scan_dir(DEVICE_PATH) < 0) {
        printf("scan dir failed\n");
        return;
    } else {
        printf("open succeed\n");
    }

    fd_set readfds;
    
    while(isRunning)
    {
        readfds = mSelectFds;

        int retval;

        retval = select(mMaxFd + 1, &readfds, NULL, NULL, NULL);

        if (retval==0) {
            printf("select time out\n");
        }

        if (FD_ISSET(mNotifyFd, &readfds))
        {
            char devname[DEV_NAME];
            char *filename;
            struct inotify_event *event;
            char event_buf[512];
            int event_size;
            int event_pos = 0;
            int res;
            res = read(mNotifyFd, &event_buf, sizeof(event_buf));
            if (res < (int)sizeof(*event)) {
                printf("could not get event, %s\n", strerror(errno));
                return;
            }

            strcpy(devname, DEVICE_PATH);
            filename = devname + strlen(devname);
            *filename++ = '/';

            while (res >= (int)sizeof(*event)) {
                event = (struct inotify_event *)(event_buf + event_pos);
                if(event->len) {
                    strcpy(filename, event->name);
                    if(event->mask & IN_CREATE) {
                        open_dev(devname);
                    } else {
                        close_dev(devname);
                    }
                }
                event_size = sizeof(*event) + event->len;
                res -= event_size;
                event_pos += event_size;
            }
        } else {
            for (int i = 0;i < mDevices.size();i++) {
                Device *device = mDevices.valueAt(i);
                int read_fd = device->id;
                if (FD_ISSET(read_fd, &readfds)) {
                    struct input_event btn[1024];
                    int read_size = read(read_fd, btn, sizeof(struct input_event) * 1024);
                    if (read_size <= 0 && errno == ENODEV) {
                        printf("read fd %d error\n", read_fd);
                        close_dev(read_fd);
                        continue;
                    } else if (read_size % sizeof(struct input_event) != 0) {
                        printf("read size is error\n");
                        continue;
                    }
                    int btn_nums = read_size / sizeof(struct input_event);
                    printf("read btn nums %d\n", btn_nums);
                    for (int l = 0;l < btn_nums;l++) {
                        for (int j = 0;j < mMappers.size();j++) {
                            Mapper *mapper = mMappers.valueAt(j);
                            mapper->process(&btn[l], device);
                            //draw_cursor(mapper);
                        }
                    }
                }
            }
        }
    }
}

Mapper *Mouse::get_mapper(char *mapper_name)
{
    int index = mMappers.indexOfKey(String8(mapper_name));

    if (index < 0) {
        return NULL;
    }

    return mMappers.valueAt(index);
}

bool Mouse::get_status()
{
    return isRunning;
}

void draw_thread(void *arg)
{
    Mouse *test = (Mouse *)arg;

    Mapper *mapper = test->get_mapper("motion");

    if (mapper == NULL) {
        printf("can not find mapper\n");
        return;
    }

    while (test->get_status()) {
        test->draw_cursor(mapper);
        usleep(30000);
    }
}

int main(int argc,char **argv)
{
    Mouse *test = new Mouse();

    pthread_t pid;

    pthread_create(&pid, NULL, (void *)draw_thread, test);

    test->loop();

    pthread_join(pid, NULL);

    return 0;
}
