// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>   // gpiod_lookup_table, GPIO_LOOKUP_IDX
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>

#include "blink_api.h"


struct kthread_blink {
	struct gpio_desc *led;
	struct task_struct *task;
	unsigned int period_ms;
};

static char *chip = (char *)"pinctrl-bcm2711";
module_param(chip, charp, 0444);
MODULE_PARM_DESC(chip, "Etiqueta del gpiochip (RPi4: 'pinctrl-bcm2711').");

static int gpio = 21;
module_param(gpio, int, 0444);
MODULE_PARM_DESC(gpio, "Número de línea dentro del chip (BCM).");

static bool active_low = false;
module_param(active_low, bool, 0444);
MODULE_PARM_DESC(active_low, "1 si el LED es activo en bajo.");

static unsigned int period_ms = 500;
module_param(period_ms, uint, 0644);
MODULE_PARM_DESC(period_ms, "Periodo de parpadeo en ms.");

static struct platform_device *pdev;
static struct platform_driver drv;
static struct gpiod_lookup_table *lt;


static struct kthread_blink *g_kb_ctx;  /* NUEVO */


/* === API exportada === */
int kthread_blink_nodt_set_period(unsigned int ms)
{
    if (!g_kb_ctx) return -ENODEV;
    if (ms < 1) ms = 1;
    g_kb_ctx->period_ms = ms;
    return 0;
}
EXPORT_SYMBOL_GPL(kthread_blink_nodt_set_period);

int kthread_blink_nodt_get_period(unsigned int *ms)
{
    if (!g_kb_ctx || !ms) return -EINVAL;
    *ms = g_kb_ctx->period_ms;
    return 0;
}
EXPORT_SYMBOL_GPL(kthread_blink_nodt_get_period);
static int blink_thread(void *arg)
{
	struct kthread_blink *ctx = arg;
	bool on = false;

	while (!kthread_should_stop()) {
		on = !on;
		gpiod_set_value_cansleep(ctx->led, on);
		if (msleep_interruptible(ctx->period_ms / 2))
			break;
	}
	gpiod_set_value_cansleep(ctx->led, 0);
	return 0;
}

static int kthread_blink_probe(struct platform_device *pdev)
{
	struct kthread_blink *ctx;



	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) return -ENOMEM;

	ctx->led = devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->led))
		return dev_err_probe(&pdev->dev, PTR_ERR(ctx->led), "gpiod_get\n");

	ctx->period_ms = period_ms ?: 1;

	ctx->task = kthread_run(blink_thread, ctx, "kthread_blink_nodt");
	if (IS_ERR(ctx->task))
		return dev_err_probe(&pdev->dev, PTR_ERR(ctx->task), "kthread_run\n");

	platform_set_drvdata(pdev, ctx);
	g_kb_ctx = ctx;   /* NUEVO */
	dev_info(&pdev->dev, "kthread blink: %u ms\n", ctx->period_ms);
	return 0;
}

static int kthread_blink_remove(struct platform_device *pdev)
{
	struct kthread_blink *ctx = platform_get_drvdata(pdev);
	if (ctx && ctx->task)
		kthread_stop(ctx->task);

    g_kb_ctx = NULL;  /* NUEVO */
	return 0;
}

static struct platform_driver drv = {
	.probe  = kthread_blink_probe,
	.remove = kthread_blink_remove,
	.driver = {
		.name = "kthread-blink-nodt",
	},
};

static int __init kthread_blink_init(void)
{
	int ret;
	size_t n = 2;

	/* 1) Registrar el driver */
	ret = platform_driver_register(&drv);
	if (ret) return ret;

	/* 2) Crear y registrar la tabla de lookup para este dev_id */
	lt = kzalloc(sizeof(*lt) + n * sizeof(struct gpiod_lookup), GFP_KERNEL);
	if (!lt) { ret = -ENOMEM; goto err_drv; }
	lt->dev_id = "kthread-blink-nodt.0";
	lt->table[0] = GPIO_LOOKUP_IDX(
		chip, gpio, "led", 0,
		active_low ? GPIO_ACTIVE_LOW : GPIO_ACTIVE_HIGH
	);
	gpiod_add_lookup_table(lt);

	/* 3) Registrar el platform_device que dispara el probe */
	pdev = platform_device_register_simple("kthread-blink-nodt", 0, NULL, 0);
	if (IS_ERR(pdev)) {
		ret = PTR_ERR(pdev);
		gpiod_remove_lookup_table(lt);
		kfree(lt);
		goto err_drv;
	}
	pr_info("kthread_blink_nodt: chip=%s gpio=%d %s period=%u ms\n",
		chip, gpio, active_low ? "ACTIVE_LOW" : "ACTIVE_HIGH", period_ms);
	return 0;

err_drv:
	platform_driver_unregister(&drv);
	return ret;
}

static void __exit kthread_blink_exit(void)
{
	if (pdev && !IS_ERR(pdev))
		platform_device_unregister(pdev);
	if (lt) {
		gpiod_remove_lookup_table(lt);
		kfree(lt);
	}
	platform_driver_unregister(&drv);
}

module_init(kthread_blink_init);
module_exit(kthread_blink_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tú");
MODULE_DESCRIPTION("Blink LED con kthread (autónomo sin DT)");
