// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/atomic.h>

#include "blink_api.h"


struct hrtimer_blink {
	struct gpio_desc *led;
	struct hrtimer timer;
	struct work_struct work;
	ktime_t half_period;
	atomic_t state;
	bool can_sleep;

	/* char dev */
	dev_t devt;
	struct cdev cdev;
	struct class *cls;
	struct device *devnode;
	struct mutex lock;
};

static char *chip = (char *)"pinctrl-bcm2711";
module_param(chip, charp, 0444);
static int gpio = 16; module_param(gpio, int, 0444);
static bool active_low = false; module_param(active_low, bool, 0444);
static unsigned int start_ms = 100; module_param(start_ms, uint, 0644);
MODULE_PARM_DESC(start_ms, "Periodo inicial en ms para /dev/blink0");

static struct platform_device *pdev;
static struct platform_driver drv;
static struct gpiod_lookup_table *lt;
static struct hrtimer_blink *g_ctx; /* un solo dispositivo */


static void blink_work(struct work_struct *w)
{
	struct hrtimer_blink *ctx = container_of(w, struct hrtimer_blink, work);
	gpiod_set_value_cansleep(ctx->led, atomic_read(&ctx->state));
}

static enum hrtimer_restart blink_hrtimer(struct hrtimer *t)
{
	struct hrtimer_blink *ctx = container_of(t, struct hrtimer_blink, timer);
	int on = !atomic_read(&ctx->state);

	atomic_set(&ctx->state, on);

	if (ctx->can_sleep)
		schedule_work(&ctx->work);
	else
		gpiod_set_value(ctx->led, on);

	hrtimer_forward_now(&ctx->timer, ctx->half_period);
	return HRTIMER_RESTART;
}

/* --- char dev ops --- */
static ssize_t blink_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
	char tmp[32];
	unsigned long ms;

	if (!g_ctx) return -ENODEV;
	if (len >= sizeof(tmp)) return -EINVAL;
	if (copy_from_user(tmp, buf, len)) return -EFAULT;
	tmp[len] = '\0';

	if (kstrtoul(tmp, 10, &ms)) return -EINVAL;
	if (ms < 1) ms = 1;

	mutex_lock(&g_ctx->lock);
	g_ctx->half_period = ktime_set(0, (u64)ms * 1000000ULL / 2);
	hrtimer_cancel(&g_ctx->timer);
	hrtimer_start(&g_ctx->timer, g_ctx->half_period, HRTIMER_MODE_REL_PINNED);
	mutex_unlock(&g_ctx->lock);

	return len;
}

static int blink_open(struct inode *i, struct file *f) { return g_ctx ? 0 : -ENODEV; }
static int blink_release(struct inode *i, struct file *f) { return 0; }

static const struct file_operations blink_fops = {
	.owner   = THIS_MODULE,
	.open    = blink_open,
	.release = blink_release,
	.write   = blink_write,
};

static int hrtimer_blink_probe(struct platform_device *pdev)
{
	struct hrtimer_blink *ctx;
	int ret;
	unsigned int ms = start_ms ?: 1;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) return -ENOMEM;

	mutex_init(&ctx->lock);

	ctx->led = devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->led))
		return dev_err_probe(&pdev->dev, PTR_ERR(ctx->led), "gpiod_get\n");

	ctx->can_sleep = gpiod_cansleep(ctx->led);

	INIT_WORK(&ctx->work, blink_work);
	hrtimer_init(&ctx->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	ctx->timer.function = blink_hrtimer;
	atomic_set(&ctx->state, 0);
	ctx->half_period = ktime_set(0, (u64)ms * 1000000ULL / 2);
	hrtimer_start(&ctx->timer, ctx->half_period, HRTIMER_MODE_REL_PINNED);

	/* char device */
	ret = alloc_chrdev_region(&ctx->devt, 0, 1, "blink");
	if (ret) return ret;

	cdev_init(&ctx->cdev, &blink_fops);
	ret = cdev_add(&ctx->cdev, ctx->devt, 1);
	if (ret) goto err_unreg;

	ctx->cls = class_create(THIS_MODULE, "blink");
	if (IS_ERR(ctx->cls)) { ret = PTR_ERR(ctx->cls); goto err_cdev; }

	ctx->devnode = device_create(ctx->cls, NULL, ctx->devt, NULL, "blink0");
	if (IS_ERR(ctx->devnode)) { ret = PTR_ERR(ctx->devnode); goto err_class; }

	platform_set_drvdata(pdev, ctx);
	g_ctx = ctx;

	dev_info(&pdev->dev, "hrtimer blink: %u ms (escribe ms en /dev/blink0)\n", ms);
	return 0;

err_class:
	class_destroy(ctx->cls);
err_cdev:
	cdev_del(&ctx->cdev);
err_unreg:
	unregister_chrdev_region(ctx->devt, 1);
	return ret;
}

static int hrtimer_blink_remove(struct platform_device *pdev)
{
	struct hrtimer_blink *ctx = platform_get_drvdata(pdev);

	hrtimer_cancel(&ctx->timer);
	cancel_work_sync(&ctx->work);
	gpiod_set_value_cansleep(ctx->led, 0);

	if (ctx->devnode) device_destroy(ctx->cls, ctx->devt);
	if (ctx->cls)     class_destroy(ctx->cls);
	cdev_del(&ctx->cdev);
	unregister_chrdev_region(ctx->devt, 1);
	g_ctx = NULL;
	return 0;
}

static struct platform_driver drv = {
	.probe  = hrtimer_blink_probe,
	.remove = hrtimer_blink_remove,
	.driver = {
		.name = "hrtimer-blink-nodt",
	},
};


int hrtimer_blink_nodt_set_period(unsigned int ms)
{
    if (!g_ctx) return -ENODEV;
    if (ms < 1) ms = 1;

    mutex_lock(&g_ctx->lock);
    g_ctx->half_period = ktime_set(0, (u64)ms * 1000000ULL / 2);
    hrtimer_cancel(&g_ctx->timer);
    hrtimer_start(&g_ctx->timer, g_ctx->half_period, HRTIMER_MODE_REL_PINNED);
    mutex_unlock(&g_ctx->lock);
    return 0;
}
EXPORT_SYMBOL_GPL(hrtimer_blink_nodt_set_period);

int hrtimer_blink_nodt_get_period(unsigned int *ms)
{
    u64 ns;
    if (!g_ctx || !ms) return -EINVAL;
    /* half_period a ms = (ns*2)/1e6 */
    ns = ktime_to_ns(g_ctx->half_period) * 2ULL;
    *ms = (unsigned int)(ns / 1000000ULL);
    return 0;
}
EXPORT_SYMBOL_GPL(hrtimer_blink_nodt_get_period);

static int __init hrtimer_blink_init(void)
{
	int ret;
	size_t n = 2;

	ret = platform_driver_register(&drv);
	if (ret) return ret;

	lt = kzalloc(sizeof(*lt) + n * sizeof(struct gpiod_lookup), GFP_KERNEL);
	if (!lt) { ret = -ENOMEM; goto err_drv; }
	lt->dev_id = "hrtimer-blink-nodt.0";
	lt->table[0] = GPIO_LOOKUP_IDX(
		chip, gpio, "led", 0,
		active_low ? GPIO_ACTIVE_LOW : GPIO_ACTIVE_HIGH
	);
	gpiod_add_lookup_table(lt);

	pdev = platform_device_register_simple("hrtimer-blink-nodt", 0, NULL, 0);
	if (IS_ERR(pdev)) {
		ret = PTR_ERR(pdev);
		gpiod_remove_lookup_table(lt);
		kfree(lt);
		goto err_drv;
	}
	pr_info("hrtimer_blink_nodt: chip=%s gpio=%d %s start=%u ms\n",
		chip, gpio, active_low ? "ACTIVE_LOW" : "ACTIVE_HIGH", start_ms);
	return 0;

err_drv:
	platform_driver_unregister(&drv);
	return ret;
}

static void __exit hrtimer_blink_exit(void)
{
	if (pdev && !IS_ERR(pdev))
		platform_device_unregister(pdev);
	if (lt) {
		gpiod_remove_lookup_table(lt);
		kfree(lt);
	}
	platform_driver_unregister(&drv);
}

module_init(hrtimer_blink_init);
module_exit(hrtimer_blink_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tú");
MODULE_DESCRIPTION("Blink LED con hrtimer + char (autónomo sin DT)");
