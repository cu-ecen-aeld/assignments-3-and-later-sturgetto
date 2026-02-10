/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;
char write_buf[512];
int write_idx = 0;
int read_idx = 0;

MODULE_AUTHOR("Steven Turgetto"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    /**
     * TODO: handle open
     */
    // Initialize device state
    filp->private_data = &aesd_device; // Store driver-specific data
    return 0; // Success
}

int aesd_release(struct inode *inode, struct file *filp)
{
    /**
     * TODO: handle release
     */
    // Free memory allocated to store written data.
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = 0;
    /**
     * TODO: handle read
     */
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    int index = aesd_device.cbuf.out_offs;
    void *ptemp = kmalloc(1024, GFP_KERNEL);
    char *pchar = (char*)ptemp;

    do
    {
	PDEBUG("index %d, size %ld, read_idx %d", index, aesd_device.cbuf.entry[index].size, read_idx);
        memcpy(&(pchar[read_idx]), aesd_device.cbuf.entry[index].buffptr, aesd_device.cbuf.entry[index].size);
        read_idx += aesd_device.cbuf.entry[index].size;
	index++;
	if (index == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
            index = 0;
    }
    while (index != aesd_device.cbuf.in_offs);
    if (copy_to_user(buf, ptemp, read_idx) != 0)
    {
        retval = -ENOMEM;
    }
    else
    {
        retval = read_idx;
    }
    kfree(ptemp);
    read_idx=0;
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    // If the existing data + incoming data causes an overrun, return -ENOMEM.
    if (count + write_idx > 512)
        return retval;
    // Copy incoming data over.
    if (copy_from_user(&(write_buf[write_idx]), buf, count))
        return retval;
    // several writes may occur without a \n at the end. Exit with the amount copied.
    write_idx += count;
    if (write_buf[write_idx-1] == '\n')
    {
        // malloc a buffer to store the incoming data
        void* ptemp = kmalloc(write_idx, GFP_KERNEL);
        if (ptemp == NULL)
            return retval;
        // Copy the cached data to the new buffer.
	write_buf[write_idx] = '\0';
        memcpy(ptemp, write_buf, write_idx);
        const struct aesd_buffer_entry add_entry = {ptemp,write_idx};
        // Store the new buffer
        aesd_circular_buffer_add_entry(&aesd_device.cbuf, &add_entry);
	write_idx = 0;
    }
    return count;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    aesd_circular_buffer_init(&aesd_device.cbuf);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;

    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    
    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

