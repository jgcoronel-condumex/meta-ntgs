// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>   // copy_from_user, copy_to_user, access_ok
#include <linux/slab.h>

#include "blink_api.h"
#include "blink_ioctl.h"

static dev_t devt;
static struct cdev cdev_ctrl;
static struct class *cls;

static int set_ms_by_id(__u32 id, __u32 ms)
{
    if (ms < 1) ms = 1;

    switch (id) {
    case BLINK_ID_KTHREAD: return kthread_blink_nodt_set_period(ms);
    case BLINK_ID_TIMER:   return timer_blink_nodt_set_period(ms);
    case BLINK_ID_HRTIMER: return hrtimer_blink_nodt_set_period(ms);
    default: return -EINVAL;
    }
}

static int get_ms_by_id(__u32 id, __u32 *ms)
{
    if (!ms) return -EINVAL;

    switch (id) {
    case BLINK_ID_KTHREAD: return kthread_blink_nodt_get_period(ms);
    case BLINK_ID_TIMER:   return timer_blink_nodt_get_period(ms);
    case BLINK_ID_HRTIMER: return hrtimer_blink_nodt_get_period(ms);
    default: return -EINVAL;
    }
}

static long blinkctl_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    void __user *up = (void __user *)arg;

    switch (cmd) {

    case BLINK_IOC_SET_MS: {
        struct blink_ioc_ms a;
        if (copy_from_user(&a, up, sizeof(a)))
            return -EFAULT;
        return set_ms_by_id(a.id, a.ms);
    }

    case BLINK_IOC_GET_MS: {
        struct blink_ioc_ms a;
        int ret;

        if (copy_from_user(&a, up, sizeof(a)))
            return -EFAULT;
        ret = get_ms_by_id(a.id, &a.ms);
        if (ret) return ret;
        if (copy_to_user(up, &a, sizeof(a)))
            return -EFAULT;
        return 0;
    }

    case BLINK_IOC_SET_MS_FROM_PTR: {
        struct blink_ioc_ptr p;
        __u32 ms;

        if (copy_from_user(&p, up, sizeof(p)))
            return -EFAULT;

        /* IMPORTANTE (DEMO):
         * p.user_ptr es una dirección de USER SPACE.
         * NO PODEMOS desreferenciarla directamente en kernel:
         *   ms = *(__u32 *)p.user_ptr;  // ¡MAL! Puede crashear.
         * En su lugar, siempre: access_ok + copy_from_user.
         */
        if (!access_ok((void __user *)(uintptr_t)p.user_ptr, sizeof(ms)))
            return -EFAULT;

        if (copy_from_user(&ms, (void __user *)(uintptr_t)p.user_ptr, sizeof(ms)))
            return -EFAULT;

        pr_info("blinkctl: user_ptr=0x%llx (leido ms=%u) id=%u\n",
                p.user_ptr, ms, p.id);

        return set_ms_by_id(p.id, ms);
    }

    case BLINK_IOC_ECHO: {
        struct blink_ioc_echo e;
        char *kbuf;
        __u32 i;

        if (copy_from_user(&e, up, sizeof(e)))
            return -EFAULT;

        if (e.len == 0 || e.len > 256)  /* limitamos por demo */
            return -EINVAL;

        if (!access_ok((void __user *)(uintptr_t)e.user_ptr, e.len))
            return -EFAULT;

        kbuf = memdup_user((void __user *)(uintptr_t)e.user_ptr, e.len);
        if (IS_ERR(kbuf))
            return PTR_ERR(kbuf);

        /* “Procesar”: a mayúsculas */
        for (i = 0; i < e.len; i++)
            if (kbuf[i] >= 'a' && kbuf[i] <= 'z')
                kbuf[i] -= 32;

        if (copy_to_user((void __user *)(uintptr_t)e.user_ptr, kbuf, e.len)) {
            kfree(kbuf);
            return -EFAULT;
        }
        kfree(kbuf);
        return 0;
    }

    default:
        return -ENOTTY;
    }
}

static const struct file_operations fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = blinkctl_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = blinkctl_ioctl,
#endif
};

static int __init blinkctl_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&devt, 0, 1, "blinkctl");
    if (ret) return ret;

    cdev_init(&cdev_ctrl, &fops);
    ret = cdev_add(&cdev_ctrl, devt, 1);
    if (ret) goto err_chr;

    cls = class_create(THIS_MODULE, "blinkctl");
    if (IS_ERR(cls)) { ret = PTR_ERR(cls); goto err_cdev; }

    if (IS_ERR(device_create(cls, NULL, devt, NULL, "blinkctl"))) {
        ret = -ENODEV;
        goto err_class;
    }

    pr_info("blinkctl listo: /dev/blinkctl\n");
    return 0;

err_class:
    class_destroy(cls);
err_cdev:
    cdev_del(&cdev_ctrl);
err_chr:
    unregister_chrdev_region(devt, 1);
    return ret;
}

static void __exit blinkctl_exit(void)
{
    device_destroy(cls, devt);
    class_destroy(cls);
    cdev_del(&cdev_ctrl);
    unregister_chrdev_region(devt, 1);
}

module_init(blinkctl_init);
module_exit(blinkctl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jaime Garcia Coronel j.gcoronel@condumex.com.mx");
MODULE_DESCRIPTION("Control de blinkers por ioctl + demo kernel/user memory");
