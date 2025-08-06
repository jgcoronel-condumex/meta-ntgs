// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>   /* vmalloc/vfree */
#include <linux/slab.h>      /* kmalloc/kfree/kzalloc */

#define DRIVER_NAME "my_kmalloc_dummy"

struct my_dev {
	/* 4 KiB contiguos en RAM física —útil para DMA o registros MMIO */
	void        *dma_buf;

	/* 2 MiB virtuales (pueden estar fragmentados físicamente) */
	char        *big_array;
};

static struct my_dev *pdev_priv;

/* ---------- Probe: se ejecuta al detectar el dispositivo ---------- */
static int my_probe(struct platform_device *pdev)
{
	int ret = 0;

	/* kzalloc: estructura del dispositivo, inicializada a cero           */
	pdev_priv = kzalloc(sizeof(*pdev_priv), GFP_KERNEL);
	if (!pdev_priv)
		return -ENOMEM;

	/* kmalloc: bloque pequeño y físicamente contiguo (p. ej. para DMA)   */
	pdev_priv->dma_buf = kmalloc(4096, GFP_KERNEL);      /* 4 KiB */
	if (!pdev_priv->dma_buf) {
		ret = -ENOMEM;
		goto err_free_priv;
	}

	/* vmalloc: bloque grande; solo necesita continuidad virtual          */
	pdev_priv->big_array = vmalloc(2 * 1024 * 1024);     /* 2 MiB */
	if (!pdev_priv->big_array) {
		ret = -ENOMEM;
		goto err_free_kmalloc;
	}

	printk(KERN_INFO "Buffers asignados con éxito\n");
	return 0;

err_free_kmalloc:
	kfree(pdev_priv->dma_buf);
err_free_priv:
	kfree(pdev_priv);
	return ret;
}

/* ---------- Remove: se llama al desconectar el dispositivo ---------- */
static int my_remove(struct platform_device *pdev)
{
	vfree(pdev_priv->big_array);   /* inverso de vmalloc() */
	kfree(pdev_priv->dma_buf);     /* inverso de kmalloc() */
	kfree(pdev_priv);              /* inverso de kzalloc() */
	printk(KERN_INFO "Buffers liberados\n");
	return 0;
}

static struct platform_driver my_platform_driver = {
	.probe  = my_probe,
	.remove = my_remove,
	.driver = {
		.name = DRIVER_NAME,
	},
};

static struct platform_device *my_device;

static int __init my_init(void)
{
	my_device = platform_device_register_simple(DRIVER_NAME, -1, NULL, 0);
	return platform_driver_register(&my_platform_driver);
}

static void __exit my_exit(void)
{
	platform_driver_unregister(&my_platform_driver);
	platform_device_unregister(my_device);
}
module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tu Nombre");

