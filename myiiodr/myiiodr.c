// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>
#include <linux/slab.h>

#define DRIVER_NAME "my_iio_dummy"

static int injected_value;

static int my_read_raw(struct iio_dev *indio_dev,
		       struct iio_chan_spec const *chan,
		       int *val, int *val2, long mask)
{
	if (mask == IIO_CHAN_INFO_RAW) {
		*val = injected_value;
		return IIO_VAL_INT;
	}
	return -EINVAL;
}

static ssize_t inject_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t len)
{
	sscanf(buf, "%d", &injected_value);
	return len;
}

static DEVICE_ATTR_WO(inject);

static struct attribute *my_attributes[] = {
	&dev_attr_inject.attr,
	NULL,
};

static const struct attribute_group my_attr_group = {
	.attrs = my_attributes,
};

static const struct iio_chan_spec my_channels[] = {
	{
		.type = IIO_VOLTAGE,
		.indexed = 1,
		.channel = 0,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
	},
};

static const struct iio_info my_iio_info = {
	.read_raw = my_read_raw,
};

static int my_probe(struct platform_device *pdev)
{
	struct iio_dev *indio_dev;

	indio_dev = devm_iio_device_alloc(&pdev->dev, 0);
	if (!indio_dev)
		return -ENOMEM;

	indio_dev->name = DRIVER_NAME;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &my_iio_info;
	indio_dev->channels = my_channels;
	indio_dev->num_channels = ARRAY_SIZE(my_channels);

	platform_set_drvdata(pdev, indio_dev);
	iio_device_register(indio_dev);
	sysfs_create_group(&indio_dev->dev.kobj, &my_attr_group);

	return 0;
}

static int my_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	sysfs_remove_group(&indio_dev->dev.kobj, &my_attr_group);
	iio_device_unregister(indio_dev);
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
MODULE_DESCRIPTION("Dummy IIO driver con entrada por sysfs");
