// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>

#include "blink_api.h"

struct timer_blink {
	struct gpio_desc *led;
	struct timer_list timer;
	struct work_struct work;
	unsigned int period_ms;
	bool state;
};
static struct timer_blink *g_tb_ctx;  /* NUEVO */

/* === API exportada === */
int timer_blink_nodt_set_period(unsigned int ms)
{
    if (!g_tb_ctx) return -ENODEV;
    if (ms < 1) ms = 1;
    g_tb_ctx->period_ms = ms;
    mod_timer(&g_tb_ctx->timer, jiffies + msecs_to_jiffies(ms / 2));
    return 0;
}
EXPORT_SYMBOL_GPL(timer_blink_nodt_set_period);

int timer_blink_nodt_get_period(unsigned int *ms)
{
    if (!g_tb_ctx || !ms) return -EINVAL;
    *ms = g_tb_ctx->period_ms;
    return 0;
}
EXPORT_SYMBOL_GPL(timer_blink_nodt_get_period);



static char *chip = (char *)"pinctrl-bcm2711";
module_param(chip, charp, 0444);
static int gpio = 20; module_param(gpio, int, 0444);
static bool active_low = false; module_param(active_low, bool, 0444);
static unsigned int period_ms = 300; module_param(period_ms, uint, 0644);

static struct platform_device *pdev;
static struct platform_driver drv;
static struct gpiod_lookup_table *lt;

static void blink_work(struct work_struct *w)
{
	struct timer_blink *ctx = container_of(w, struct timer_blink, work);
	gpiod_set_value_cansleep(ctx->led, ctx->state);
}

static void blink_timer(struct timer_list *t)
{
	struct timer_blink *ctx = from_timer(ctx, t, timer);

	ctx->state = !ctx->state;
	schedule_work(&ctx->work);
	mod_timer(&ctx->timer, jiffies + msecs_to_jiffies(ctx->period_ms / 2));
}

static int timer_blink_probe(struct platform_device *pdev)
{
	struct timer_blink *ctx;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) return -ENOMEM;


	ctx->led = devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->led))
		return dev_err_probe(&pdev->dev, PTR_ERR(ctx->led), "gpiod_get\n");

	ctx->period_ms = period_ms ?: 1;
	INIT_WORK(&ctx->work, blink_work);
	timer_setup(&ctx->timer, blink_timer, 0);
	mod_timer(&ctx->timer, jiffies + msecs_to_jiffies(ctx->period_ms / 2));

	platform_set_drvdata(pdev, ctx);
	g_tb_ctx = ctx;   /* NUEVO */
	dev_info(&pdev->dev, "timer blink: %u ms\n", ctx->period_ms);
	return 0;
}

static int timer_blink_remove(struct platform_device *pdev)
{
	struct timer_blink *ctx = platform_get_drvdata(pdev);
	del_timer_sync(&ctx->timer);
	cancel_work_sync(&ctx->work);
	gpiod_set_value_cansleep(ctx->led, 0);
	g_tb_ctx = NULL; /* NUEVO */
	return 0;
}

static struct platform_driver drv = {
	.probe  = timer_blink_probe,
	.remove = timer_blink_remove,
	.driver = {
		.name = "timer-blink-nodt",
	},
};

static int __init timer_blink_init(void)
{
	int ret;
	size_t n = 2;

	ret = platform_driver_register(&drv);
	if (ret) return ret;

	lt = kzalloc(sizeof(*lt) + n * sizeof(struct gpiod_lookup), GFP_KERNEL);
	if (!lt) { ret = -ENOMEM; goto err_drv; }
	lt->dev_id = "timer-blink-nodt.0";
	lt->table[0] = GPIO_LOOKUP_IDX(
		chip, gpio, "led", 0,
		active_low ? GPIO_ACTIVE_LOW : GPIO_ACTIVE_HIGH
	);
	gpiod_add_lookup_table(lt);

	pdev = platform_device_register_simple("timer-blink-nodt", 0, NULL, 0);
	if (IS_ERR(pdev)) {
		ret = PTR_ERR(pdev);
		gpiod_remove_lookup_table(lt);
		kfree(lt);
		goto err_drv;
	}
	pr_info("timer_blink_nodt: chip=%s gpio=%d %s period=%u ms\n",
		chip, gpio, active_low ? "ACTIVE_LOW" : "ACTIVE_HIGH", period_ms);
	return 0;

err_drv:
	platform_driver_unregister(&drv);
	return ret;
}

static void __exit timer_blink_exit(void)
{
	if (pdev && !IS_ERR(pdev))
		platform_device_unregister(pdev);
	if (lt) {
		gpiod_remove_lookup_table(lt);
		kfree(lt);
	}
	platform_driver_unregister(&drv);
}

module_init(timer_blink_init);
module_exit(timer_blink_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tú");
MODULE_DESCRIPTION("Blink LED con timer_list + workqueue (autónomo sin DT)");
