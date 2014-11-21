#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <asm/uaccess.h>

#include "mouse.h"

static int mouse_major = 0;
static int mouse_minor = 0;

static struct class *mouse_class = NULL;
static struct mouse_reg_dev *mouse_dev = NULL;

static int mouse_open(struct inode *inode, struct file *filp);
static int mouse_release(struct inode *inode, struct file *filp);
static ssize_t mouse_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t mouse_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

static struct file_operations mouse_fops = {
    .owner = THIS_MODULE,
    .open = mouse_open,
    .release = mouse_release,
    .read = mouse_read,
    .write = mouse_write,
};

static ssize_t mouse_val_show(struct device *dev, struct device_attribute *attr, char *buf);
//static ssize_t mouse_val_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

static DEVICE_ATTR(val, S_IRUGO, mouse_val_show, NULL);

static int mouse_open(struct inode *inode, struct file *filp)
{
    struct mouse_reg_dev *dev;

    dev = container_of(inode->i_cdev, struct mouse_reg_dev, dev);
    filp->private_data = dev;

    return 0;
}

static int mouse_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t mouse_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t err = 0;
    struct mouse_reg_dev *dev = filp->private_data;

    if (down_interruptible(&(dev->sem))) {
        return -ERESTARTSYS;
    }

    if (count < sizeof(dev->pos)) {
        goto out;
    }

    if (copy_to_user(buf, &(dev->pos), sizeof(dev->pos))) {
        err = -EFAULT;
        goto out;
    }

    err = sizeof(dev->pos);

out:

    up(&(dev->sem));

    return err;
}

static ssize_t mouse_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct mouse_reg_dev *dev = filp->private_data;
    ssize_t err = 0;

    if (down_interruptible(&(dev->sem))) {
        return -ERESTARTSYS;
    }

    if (count != sizeof(dev->pos)) {
        goto out;
    }

    if (copy_from_user(&(dev->pos), buf, count)) {
        err = -EFAULT;
        goto out;
    }

    err = sizeof(dev->pos);

out:
    up(&(dev->sem));

    return err;
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

static int __init mouse_init(void)
{
    int err = -1;
    dev_t dev = 0;
    struct device *temp = NULL;

    printk(KERN_ALERT"Initializing freg device.\n");

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
        printk(KERN_ALERT"Failed to setup mouse device: %d.\n");
        goto cleanup;
    }

    mouse_class = class_create(THIS_MODULE, MOUSE_DEVICE_CLASS_NAME);
    if (IS_ERR(mouse_class)) {
        err = PTR_ERR(mouse_class);
        printk(KERN_ALERT"Failed to create mouse device class.\n");
        goto destroy_cdev;
    }

    temp = device_create(mouse_class, NULL, dev, NULL, "s", MOUSE_DEVICE_FILE_NAME);
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

static void __exit mouse_exit(void)
{
    dev_t devno = MKDEV(mouse_major, mouse_minor);
    printk(KERN_ALERT"Destroy mouse device.\n");

    mouse_remove_proc();

    if (mouse_class) {
        device_destroy(mouse_class, MKDEV(mouse_major, mouse_minor));
        class_destroy(mouse_class);
    }

    if (mouse_dev) {
        cdev_del(&(mouse_dev->dev));
        kfree(mouse_dev);
    }

    unregister_chrdev_region(devno, 1);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mouse Register Driver");

module_init(mouse_init);
module_exit(mouse_exit);
