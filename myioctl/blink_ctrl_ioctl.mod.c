#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xb7391ba3, "module_layout" },
	{ 0x41029cd3, "device_destroy" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x6484396e, "cdev_del" },
	{ 0x3b2dbd16, "class_destroy" },
	{ 0xb6cbdd, "device_create" },
	{ 0x323459a0, "__class_create" },
	{ 0xe4056356, "cdev_add" },
	{ 0xed7647e8, "cdev_init" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x8da6585d, "__stack_chk_fail" },
	{ 0xe606f3e2, "hrtimer_blink_nodt_get_period" },
	{ 0xad3b6a4e, "timer_blink_nodt_get_period" },
	{ 0x4c7cbd79, "hrtimer_blink_nodt_set_period" },
	{ 0x2245a9a, "timer_blink_nodt_set_period" },
	{ 0x92997ed8, "_printk" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0x8d4e1ded, "kthread_blink_nodt_get_period" },
	{ 0x2ae0528e, "kthread_blink_nodt_set_period" },
	{ 0x37a0cba, "kfree" },
	{ 0x9291cd3b, "memdup_user" },
	{ 0x12a4e128, "__arch_copy_from_user" },
};

MODULE_INFO(depends, "hrtimer_blink_char_nodt,timer_blink_nodt,kthread_blink_nodt");


MODULE_INFO(srcversion, "D46D1D1E13FE5E1443E1755");
