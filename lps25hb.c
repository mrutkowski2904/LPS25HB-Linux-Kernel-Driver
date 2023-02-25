#include <linux/module.h>
#include <linux/init.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/cdev.h>

/* ----------------------------- MACRO ---------------------------- */

#define LPS25HB_MAX_DEVICES 10

/* ----------------------------- TYPES ---------------------------- */

struct driver_data
{
	struct class *sysfs_class;
	dev_t device_num_base;
	int no_devices;
};

struct device_data
{
	dev_t dev_num;
	struct spi_device *client;
	struct device *device;
	struct cdev cdev;
	u16 last_reading;
};

/* --------------------- FUNCTION DECLARATIONS -------------------- */

static int lps25hb_probe(struct spi_device *client);
static int lps25hb_remove(struct spi_device *client);

static int lps25hb_driver_open(struct inode *inode, struct file *file);
static int lps25hb_driver_release(struct inode *inode, struct file *file);
static ssize_t lps25hb_driver_read(struct file *file, char __user *buffer, size_t count, loff_t *f_pos);

/* --------------------------- VARIABLES -------------------------- */

static struct of_device_id lps25hb_driver_of_ids[] = {
	{
		.compatible = "mr,lps25hb",
	},
	{/* NULL */},
};
MODULE_DEVICE_TABLE(of, lps25hb_driver_of_ids);

static struct spi_driver lps25hb_spi_driver = {
	.probe = lps25hb_probe,
	.remove = lps25hb_remove,
	.driver = {
		.name = "mr_lps25hb",
		.of_match_table = of_match_ptr(lps25hb_driver_of_ids),
	},
};

static struct file_operations file_ops = {
	.owner = THIS_MODULE,
	.open = lps25hb_driver_open,
	.release = lps25hb_driver_release,
	.read = lps25hb_driver_read,
};

struct driver_data driver_global_data;

/* --------------------- FUNCTION DEFINITIONS --------------------- */

static int lps25hb_driver_open(struct inode *inode, struct file *file)
{
	struct device_data *dev_data;

	/* read only */
	if (file->f_mode & FMODE_WRITE)
		return -EPERM;

	dev_data = container_of(inode->i_cdev, struct device_data, cdev);
	file->private_data = dev_data;
	return 0;
}

static int lps25hb_driver_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t lps25hb_driver_read(struct file *file, char __user *buffer, size_t count, loff_t *f_pos)
{
	struct device_data *dev_data;

	dev_data = file->private_data;
	pr_info("lps25hb read called, value: %d\n", dev_data->last_reading);
	return 0;
}

static int lps25hb_probe(struct spi_device *client)
{
	int status;
	struct device_data *dev_data;

	dev_info(&client->dev, "probe called\n");

	dev_data = devm_kzalloc(&client->dev, sizeof(struct device_data), GFP_KERNEL);
	if (!dev_data)
	{
		dev_err(&client->dev, "error while allocating device private data\n");
		return -ENOMEM;
	}
	spi_set_drvdata(client, dev_data);

	dev_data->client = client;
	dev_data->dev_num = driver_global_data.device_num_base + driver_global_data.no_devices;

	cdev_init(&dev_data->cdev, &file_ops);
	dev_data->cdev.owner = THIS_MODULE;
	status = cdev_add(&dev_data->cdev, dev_data->dev_num, 1);
	if (status < 0)
	{
		dev_err(&client->dev, "cdev add failed\n");
		return status;
	}

	dev_data->device = device_create(driver_global_data.sysfs_class, &client->dev, dev_data->dev_num, dev_data, "barometer%d", driver_global_data.no_devices);
	if (IS_ERR(dev_data->device))
	{
		status = PTR_ERR(dev_data->device);
		dev_err(&client->dev, "error while creating device\n");
		return status;
	}

	driver_global_data.no_devices++;

	return 0;
}

static int lps25hb_remove(struct spi_device *client)
{
	struct device_data *dev_data;

	pr_info("remove called\n");

	dev_data = spi_get_drvdata(client);
	device_destroy(driver_global_data.sysfs_class, dev_data->device->devt);
	return 0;
}

static int __init lps25hb_driver_init(void)
{
	int status;

	status = alloc_chrdev_region(&driver_global_data.device_num_base, 0, LPS25HB_MAX_DEVICES, "lps25hbs");
	if (status < 0)
	{
		pr_err("alloc chrdev region failed\n");
		return status;
	}

	driver_global_data.no_devices = 0;
	driver_global_data.sysfs_class = class_create(THIS_MODULE, "lps25hb");
	if (IS_ERR_OR_NULL(driver_global_data.sysfs_class))
	{
		pr_err("error while creating lps25hb class\n");
		return PTR_ERR(driver_global_data.sysfs_class);
	}

	status = spi_register_driver(&lps25hb_spi_driver);
	if (IS_ERR_VALUE(status))
	{
		pr_err("error while registering spi driver for lps25hb\n");
		return status;
	}

	printk("spi driver for lps25hb registered\n");
	return 0;
}

static void __exit lps25hb_driver_exit(void)
{
	spi_unregister_driver(&lps25hb_spi_driver);
	class_destroy(driver_global_data.sysfs_class);
	unregister_chrdev_region(driver_global_data.device_num_base, LPS25HB_MAX_DEVICES);
	printk("spi driver for lps25hb unregistered\n");
}

module_init(lps25hb_driver_init);
module_exit(lps25hb_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maciej Rutkowski");
MODULE_DESCRIPTION("SPI driver for LPS25HB");