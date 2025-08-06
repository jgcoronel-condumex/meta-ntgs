// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/fs.h>          /* register_chrdev_region, struct file_operations */
#include <linux/cdev.h>        /* cdev */
#include <linux/uaccess.h>     /* copy_to_user / copy_from_user */
#include <linux/device.h>      /* class_create / device_create */

#define DRV_NAME "simple_char"
#define BUF_SIZE 256

static dev_t devnum;           /* major + minor */
static struct cdev sc_cdev;
static struct class *sc_class;
static char sc_buffer[BUF_SIZE];
static size_t sc_len;          /* bytes válidos en el buffer */

/* ---------- file_operations ---------- */
static int sc_open(struct inode *inode, struct file *filp)
{
	pr_info(DRV_NAME ": open()\n");
	return 0;                  /* éxito */
}

static int sc_release(struct inode *inode, struct file *filp)
{
	pr_info(DRV_NAME ": release()\n");
	return 0;
}

static ssize_t sc_read(struct file *filp, char __user *ubuf,
                       size_t len, loff_t *offset)
{
	size_t to_copy;

	if (*offset >= sc_len)      /* EOF */
		return 0;

	to_copy = min(len, sc_len - (size_t)*offset);

	if (copy_to_user(ubuf, sc_buffer + *offset, to_copy))
		return -EFAULT;

	*offset += to_copy;
	return to_copy;
}

static ssize_t sc_write(struct file *filp, const char __user *ubuf,
                        size_t len, loff_t *offset)
{
	size_t to_copy = min(len, (size_t)BUF_SIZE);

	if (copy_from_user(sc_buffer, ubuf, to_copy))
		return -EFAULT;

	sc_len = to_copy;
	*offset = 0;               /* reiniciar puntero */
	return to_copy;
}

static const struct file_operations sc_fops = {
	.owner   = THIS_MODULE,
	.open    = sc_open,
	.release = sc_release,
	.read    = sc_read,
	.write   = sc_write,
};

/* ---------- init / exit ---------- */
static int __init sc_init(void)
{
	int ret;

	/* 1) Reservar major/minor fijo: 1 dispositivo, minor = 0 */
	devnum = MKDEV(240, 0);                 /* mayor 240 es “experimental” */
	ret = register_chrdev_region(devnum, 1, DRV_NAME);
	if (ret) {
		pr_err(DRV_NAME ": cannot reserve major 240\n");
		return ret;
	}

	/* 2) Preparar cdev y asociar fops */
	cdev_init(&sc_cdev, &sc_fops);
	sc_cdev.owner = THIS_MODULE;

	ret = cdev_add(&sc_cdev, devnum, 1);
	if (ret) {
		pr_err(DRV_NAME ": cdev_add failed\n");
		goto unreg_region;
	}

	/* 4) Crear clase y /dev/simple_char para udev */
	sc_class = class_create(THIS_MODULE, "simple_class");
	if (IS_ERR(sc_class)) {
		ret = PTR_ERR(sc_class);
		goto del_cdev;
	}

	if (!device_create(sc_class, NULL, devnum, NULL, "simple_char")) {
		ret = -ENOMEM;
		goto destroy_class;
	}

	pr_info(DRV_NAME ": loaded (major=%d minor=%d)\n",
	        MAJOR(devnum), MINOR(devnum));
	return 0;

destroy_class:
	class_destroy(sc_class);
del_cdev:
	cdev_del(&sc_cdev);
unreg_region:
	unregister_chrdev_region(devnum, 1);
	return ret;
}

static void __exit sc_exit(void)
{
	device_destroy(sc_class, devnum);
	class_destroy(sc_class);
	cdev_del(&sc_cdev);
	unregister_chrdev_region(devnum, 1);
	pr_info(DRV_NAME ": unloaded\n");
}

module_init(sc_init);
module_exit(sc_exit);
MODULE_AUTHOR("Ejemplo");
MODULE_LICENSE("GPL");
