#ifndef __MOUSE_H__
#define __MOUSE_H__

#include <linux/cdev.h>
#include <linux/semaphore.h>

#define MOUSE_DEVICE_NODE_NAME "mouse"
#define MOUSE_DEVICE_FILE_NAME "mouse"
#define MOUSE_DEVICE_PROC_NAME "mouse"
#define MOUSE_DEVICE_CLASS_NAME "mouse"

typedef struct input_position {
    int x;
    int y;
} input_position;

struct mouse_reg_dev {
    input_position pos;
    struct semaphore sem;
    struct cdev dev;
};

#endif
