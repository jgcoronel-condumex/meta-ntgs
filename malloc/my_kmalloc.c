// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>   /* vmalloc/vfree */
#include <linux/slab.h>      /* kmalloc/kfree/kzalloc */

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

	dev_info(&pdev->dev, "Buffers asignados con éxito\n");
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
	dev_info(&pdev->dev, "Buffers liberados\n");
	return 0;
}

/* Boilerplate del driver (omitido) */
MODULE_AUTHOR("Juanito babana");
MODULE_LICENSE("GPL");
