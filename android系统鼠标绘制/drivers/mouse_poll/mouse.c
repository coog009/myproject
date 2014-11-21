#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/platform_device.h>

#include "mouse.h"

static int mouse_major = 0;
static int mouse_minor = 0;

static struct class *mouse_class = NULL;
static struct mouse_reg_dev *mouse_dev = NULL;

static int mouse_open(struct inode *inode, struct file *filp);
static int mouse_release(struct inode *inode, struct file *filp);
static ssize_t mouse_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t mouse_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static unsigned int mouse_poll(struct file *filp, poll_table *wait);

static struct file_operations mouse_fops = {
    .owner = THIS_MODULE,
    .open = mouse_open,
    .release = mouse_release,
    .read = mouse_read,
    .write = mouse_write,
    .poll = mouse_poll,
};

static ssize_t mouse_val_show(struct device *dev, struct device_attribute *attr, char *buf);
//static ssize_t mouse_val_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

static DEVICE_ATTR(val, S_IRUGO, mouse_val_show, NULL);

static int mouse_open(struct inode *inode, struct file *filp)
{
    struct mouse_reg_dev *dev;

    dev = container_of(inode->i_cdev, struct mouse_reg_dev, dev);
    filp->private_data = dev;

#ifndef USE_RING
    return 0;
#else
    if (down_interruptible(&(dev->sem))) {
        return -ERESTARTSYS;
    }

    if (!dev->buffer) {
        /* allocate the buffer */
        dev->buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
        if (!dev->buffer) {
            up(&dev->sem);
            return -ENOMEM;
        }
    }
    dev->buffersize = BUFFER_SIZE;
    dev->end = dev->buffer + dev->buffersize;
    dev->rp = dev->wp = dev->buffer; /* rd and wr from the beginning */

    /* use f_mode,not  f_flags: it's cleaner (fs/open.c tells why) */
    if (filp->f_mode & FMODE_READ)
        dev->nreaders++;
    if (filp->f_mode & FMODE_WRITE)
        dev->nwriters++;
    up(&dev->sem);

    return nonseekable_open(inode, filp);
#endif
}

static int mouse_release(struct inode *inode, struct file *filp)
{
#ifdef USE_RING
    struct mouse_reg_dev *dev = filp->private_data;

    down(&dev->sem);
    if (filp->f_mode & FMODE_READ)
        dev->nreaders--;
    if (filp->f_mode & FMODE_WRITE)
        dev->nwriters--;
    if (dev->nreaders + dev->nwriters == 0) {
        kfree(dev->buffer);
        dev->buffer = NULL; /* the other fields are not checked on open */
    }
    up(&dev->sem);
#endif
    return 0;
}

static ssize_t mouse_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct mouse_reg_dev *dev = filp->private_data;

    if (down_interruptible(&(dev->sem))) {
        return -ERESTARTSYS;
    }

#ifndef USE_RING
    if (count < sizeof(dev->pos)) {
        up(&dev->sem);
        return -EFAULT;
    }

    if (copy_to_user(buf, &(dev->pos), sizeof(dev->pos))) {
        up(&dev->sem);
        return -EFAULT;
    }
    up(&dev->sem);
#else
    while (dev->rp == dev->wp) { /* nothing to read */
        up(&dev->sem); /* release the lock */
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        //PDEBUG("\"%s\" reading: going to sleep\n", current->comm);
        if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))
            return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
        /* otherwise loop, but first reacquire the lock */
        if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;
    }
    /* ok, data is there, return something */
    if (dev->wp > dev->rp)
        count = min(count, (size_t)(dev->wp - dev->rp));
    else /* the write pointer has wrapped, return data up to dev->end */
        count = min(count, (size_t)(dev->end - dev->rp));
    if (copy_to_user(buf, dev->rp, count)) {
        up (&dev->sem);
        return -EFAULT;
    }
    dev->rp += count;
    if (dev->rp == dev->end)
        dev->rp = dev->buffer; /* wrapped */
    up (&dev->sem);

    /* finally, awake any writers and return */
    wake_up_interruptible(&dev->outq);
    //PDEBUG("\"%s\" did read %li bytes\n",current->comm, (long)count);
#endif

    return count;
}

#ifdef USE_RING
static int spacefree(struct mouse_reg_dev *dev)
{
    if (dev->rp == dev->wp)
        return dev->buffersize - 1;
    return ((dev->rp + dev->buffersize - dev->wp) % dev->buffersize) - 1;
}

static int mouse_getwritespace(struct mouse_reg_dev *dev, struct file *filp)
{
    while (spacefree(dev) == 0) { /* full */
        DEFINE_WAIT(wait);

        up(&dev->sem);
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        //PDEBUG("\"%s\" writing: going to sleep\n",current->comm);
        prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
        if (spacefree(dev) == 0)
            schedule();
        finish_wait(&dev->outq, &wait);
        if (signal_pending(current))
            return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
        if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;
    }
    return 0;
}   
#endif

static ssize_t mouse_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct mouse_reg_dev *dev = filp->private_data;
#ifdef USG_RING
    int result;
#endif

    if (down_interruptible(&(dev->sem))) {
        return -ERESTARTSYS;
    }
#ifndef USE_RING
    if (count != sizeof(dev->pos)) {
        up(&dev->sem);
        return -EFAULT;
    }

    if (copy_from_user(&dev->pos, buf, count)) {
        up(&dev->sem);
        return -EFAULT;
    }

    dev->can_be_read = 1;
#else
    /* Make sure there's space to write */
    result = mouse_getwritespace(dev, filp);
    if (result)
        return result; /* scull_getwritespace called up(&dev->sem) */

    /* ok, space is there, accept something */
    count = min(count, (size_t)spacefree(dev));
    if (dev->wp >= dev->rp)
        count = min(count, (size_t)(dev->end - dev->wp)); /* to end-of-buf */
    else /* the write pointer has wrapped, fill up to rp-1 */
        count = min(count, (size_t)(dev->rp - dev->wp - 1));
    //PDEBUG("Going to accept %li bytes to %p from %p\n", (long)count, dev->wp, buf);
    if (copy_from_user(dev->wp, buf, count)) {
        up (&dev->sem);
        return -EFAULT;
    }
    dev->wp += count;

    if (count > 0)
        memcpy(&dev->pos, dev->wp - sizeof(input_position), sizeof(input_position));

    if (dev->wp == dev->end)
        dev->wp = dev->buffer; /* wrapped */
#endif
    up(&dev->sem);

    /* finally, awake any reader */
    wake_up_interruptible(&dev->inq);  /* blocked in read() and select() */

    return count;
}

static unsigned int mouse_poll(struct file *filp, poll_table *wait)
{
    struct mouse_reg_dev *dev = filp->private_data;
    unsigned int mask = 0;

    down(&dev->sem);

    poll_wait(filp, &dev->inq,  wait);

#ifndef USE_RING
    if (dev->can_be_read == 1) {
        mask |= POLLIN | POLLRDNORM;
        dev->can_be_read = 0;
    }
#else
    poll_wait(filp, &dev->outq, wait);
    if (dev->rp != dev->wp)
        mask |= POLLIN | POLLRDNORM;    /* readable */
    if (spacefree(dev))
        mask |= POLLOUT | POLLWRNORM;   /* writable */
#endif

    up(&dev->sem);

    return mask;
}

static ssize_t __mouse_get_val(struct mouse_reg_dev *dev, char *buf)
{
    int x, y;

    if (down_interruptible(&(dev->sem))) {
        return -ERESTARTSYS;
    }

    x = dev->pos.x;
    y = dev->pos.y;

    up(&(dev->sem));

    return snprintf(buf, PAGE_SIZE, "mouse x:%d y:%d\n", x, y);
}

static ssize_t mouse_val_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct mouse_reg_dev *hdev = (struct mouse_reg_dev *)dev_get_drvdata(dev);

    return __mouse_get_val(hdev, buf);
}

static ssize_t mouse_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    if (off > 0) {
        *eof = 1;
        return 0;
    }

    return __mouse_get_val(mouse_dev, page);
}

static void mouse_create_proc(void)
{
    struct proc_dir_entry *entry;

    entry = create_proc_entry(MOUSE_DEVICE_PROC_NAME, 0, NULL);

    if (entry) {
        //entry->owner = THIS_MODULE;
        entry->read_proc = mouse_proc_read;
    }
}

static void mouse_remove_proc(void)
{
    remove_proc_entry(MOUSE_DEVICE_PROC_NAME, NULL);
}

static int __mouse_setup_dev(struct mouse_reg_dev *dev)
{
    int err;
    dev_t devno = MKDEV(mouse_major, mouse_minor);

    memset(dev, 0, sizeof(struct mouse_reg_dev));

    init_waitqueue_head(&(dev->inq));
#ifdef USE_RING
    init_waitqueue_head(&(dev->outq));
#endif

    cdev_init(&(dev->dev), &mouse_fops);
    dev->dev.owner = THIS_MODULE;
    dev->dev.ops = &mouse_fops;

    err = cdev_add(&(dev->dev), devno, 1);
    if (err) {
        return err;
    }

    sema_init(&(dev->sem), 1);
    memset(&dev->pos, 0, sizeof(dev->pos));

    return 0;
}

static int __devinit mouse_probe(struct platform_device *pdev)
{
    int err = -1;
    dev_t dev = 0;
    struct device *temp = NULL;

    printk(KERN_ALERT"Initializing mouse device probe.\n");

    err = alloc_chrdev_region(&dev, 0, 1, MOUSE_DEVICE_NODE_NAME);
    if (err) {
        printk(KERN_ALERT"Failed to alloc char dev region.\n");
        goto fail;
    }

    mouse_major = MAJOR(dev);
    mouse_minor = MINOR(dev);

    mouse_dev = kmalloc(sizeof(struct mouse_reg_dev), GFP_KERNEL);
    if (!mouse_dev) {
        err = -ENOMEM;
        printk(KERN_ALERT"Failed to alloc mouse device.\n");
        goto unregister;
    }

    err = __mouse_setup_dev(mouse_dev);
    if (err) {
        printk(KERN_ALERT"Failed to setup mouse device: %d.\n", err);
        goto cleanup;
    }

    mouse_class = class_create(THIS_MODULE, MOUSE_DEVICE_CLASS_NAME);
    if (IS_ERR(mouse_class)) {
        err = PTR_ERR(mouse_class);
        printk(KERN_ALERT"Failed to create mouse device class.\n");
        goto destroy_cdev;
    }

    temp = device_create(mouse_class, NULL, dev, NULL, "%s", MOUSE_DEVICE_FILE_NAME);
    if (IS_ERR(temp)) {
        err = PTR_ERR(temp);
        printk(KERN_ALERT"Failed to create mouse device.\n");
        goto destroy_class;
    }

    err = device_create_file(temp, &dev_attr_val);
    if (err < 0) {
        printk(KERN_ALERT"Failed to create attriute val of mouse device.\n");
        goto destroy_device;
    }

    dev_set_drvdata(temp, mouse_dev);

    mouse_create_proc();

    printk(KERN_ALERT"Succedded to initialize mouse device.\n");

    return 0;

destroy_device:
    device_destroy(mouse_class, dev);
destroy_class:
    class_destroy(mouse_class);
destroy_cdev:
    cdev_del(&(mouse_dev->dev));
cleanup:
    kfree(mouse_dev);
unregister:
    unregister_chrdev_region(MKDEV(mouse_major, mouse_minor), 1);
fail:
    return err;
}

static void __devinit mouse_remove(struct platform_device *pdev)
{
    dev_t devno = MKDEV(mouse_major, mouse_minor);
    printk(KERN_ALERT"Destroy mouse remove.\n");

    mouse_remove_proc();

    if (mouse_class) {
        device_destroy(mouse_class, MKDEV(mouse_major, mouse_minor));
        class_destroy(mouse_class);
    }

    if (mouse_dev) {
        cdev_del(&(mouse_dev->dev));
#ifdef USE_RING
        if (mouse_dev->buffer != NULL) {
            kfree(mouse_dev->buffer);
        }
#endif
        kfree(mouse_dev);
    }

    unregister_chrdev_region(devno, 1);
}

static struct platform_driver mouse_device_driver = {  
    .probe = mouse_probe,  
    .remove = __devexit_p(mouse_remove),  
    .driver = {  
        .name = "mymouse",  
        .owner = THIS_MODULE,  
    }  
};  

static int __init mouse_init(void)
{
    printk(KERN_ALERT"mouse_device_driver init.\n");
    return platform_driver_register(&mouse_device_driver);  
}  

static void __exit mouse_exit(void)  
{  
    printk(KERN_ALERT"mouse_device_driver exit.\n");
    platform_driver_unregister(&mouse_device_driver);  
}  

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mouse Register Driver");

module_init(mouse_init);
module_exit(mouse_exit);
