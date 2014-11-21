#ifndef __MOUSE_H__
#define __MOUSE_H__

#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/poll.h>
#include <linux/sched.h>

#define MOUSE_DEVICE_NODE_NAME "mouse"
#define MOUSE_DEVICE_FILE_NAME "mouse"
#define MOUSE_DEVICE_PROC_NAME "mouse"
#define MOUSE_DEVICE_CLASS_NAME "mouse"

#define BUFFER_SIZE 4096

typedef struct input_position {
    int x;
    int y;
} input_position;

struct mouse_reg_dev {
    wait_queue_head_t inq, outq;
#ifndef USE_RING
    int can_be_read;
#else
    char *buffer, *end;
    int buffersize;                    /* used in pointer arithmetic */
    char *rp, *wp;                     /* where to read, where to write */
    int nreaders, nwriters;            /* number of openings for r/w */
#endif
    input_position pos;
    struct semaphore sem;
    struct cdev dev;
};

#endif
