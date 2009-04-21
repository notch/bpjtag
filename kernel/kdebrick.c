/*
 *  Broadcom-MIPS EJTAG Debrick Utility
 *  Accelerator kernel module
 *
 *  Copyright (C) 2009 Michael Buesch <mb@bu3sch.de>
 *  Copyright (C) 2004 HairyDairyMaid (a.k.a. Lightbulb)
 *
 *  Derived from the Linux "ppdev" driver
 *  Copyright (C) 1998-2000, 2002 Tim Waugh <tim@cyberelk.net>
 *  Copyright (C) 1999-2001 Fred Barnes
 *  Copyright (C) 2000 Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of version 2 the GNU General Public License as published
 *  by the Free Software Foundation.
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *  To view a copy of the license go to:
 *  http://www.fsf.org/copyleft/gpl.html
 *  To receive a copy of the GNU General Public License write the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "kdebrick.h"
#include "../common/bitbang.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/parport.h>
#include <linux/ctype.h>
#include <linux/poll.h>
#include <linux/major.h>
#include <linux/smp_lock.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/compat.h>
#include <linux/delay.h>


#define CHRDEV			"kdebrick"
#define MAX_TCK_DELAY		1000
#define MAX_INSTR_LENGTH	32


struct kdebrick {
	struct mutex mutex;
	struct pardevice *pdev;
	bool claimed;
	struct kdebrick_config config;
	struct debrick_bitbang bitbang;
};


static inline void debrick_usec_delay(unsigned int usec)
{
	udelay(usec);
}

static inline int debrick_relax(void)
{
	cond_resched();
	return signal_pending(current);
}

static inline void debrick_parport_write_data(void *priv, uint8_t data)
{
	struct pardevice *pdev = priv;

	parport_write_data(pdev->port, data);
}

static inline uint8_t debrick_parport_read_status(void *priv)
{
	struct pardevice *pdev = priv;

	return parport_read_status(pdev->port);
}

#include "../common/bitbang.c"

static void pp_irq(void *private)
{
	/* Do nothing */
}

static int register_device(int minor, struct kdebrick *d)
{
	struct parport *port;
	struct pardevice *pdev = NULL;
	char *name;

	name = kmalloc(strlen(CHRDEV) + 3, GFP_KERNEL);
	if (name == NULL)
		return -ENOMEM;

	sprintf(name, CHRDEV "%x", minor);

	port = parport_find_number(minor);
	if (!port) {
		printk(KERN_WARNING "%s: no associated port!\n", name);
		kfree(name);
		return -ENXIO;
	}

	pdev = parport_register_device(port, name, NULL, NULL,
				       pp_irq, PARPORT_FLAG_EXCL, d);
	parport_put_port(port);

	if (!pdev) {
		printk(KERN_WARNING "%s: failed to register device!\n", name);
		kfree(name);
		return -ENXIO;
	}

	d->pdev = pdev;

	return 0;
}

static int kdebrick_do_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct kdebrick *d = file->private_data;
	void __user *argp = (void __user *)arg;
	int err;

	/* First handle the cases that don't take arguments. */
	switch (cmd) {
	case KDEBRICK_IOCTL_CLAIM: {
		unsigned int minor = iminor(file->f_path.dentry->d_inode);

		if (d->claimed)
			return -EBUSY;

		/* Deferred device registration. */
		if (!d->pdev) {
			err = register_device(minor, d);
			if (err) {
				return err;
			}
		}
		d->bitbang.parport_priv = d->pdev;

		err = parport_claim_or_block(d->pdev);
		if (err < 0)
			return err;

		d->claimed = 1;
		parport_set_timeout(d->pdev, parport_set_timeout(d->pdev, 0));

		return 0;
	} }

	/* Everything else requires the port to be claimed, so check
	 * that now. */
	if (!d->claimed)
		return -EPERM;

	switch (cmd) {
	case KDEBRICK_IOCTL_RELEASE:
		parport_release(d->pdev);
		d->claimed = 0;
		return 0;
	case KDEBRICK_IOCTL_DMAREAD: {
		struct kdebrick_dma dma;
		struct kdebrick_dma __user *user_dma = argp;
		unsigned int data;

		if (copy_from_user(&dma, user_dma, sizeof(dma)))
			return -EFAULT;

		err = bitbang_ejtag_dma_read(&d->bitbang, dma.control,
					     dma.addr, &data);
		if (err)
			return err;
		dma.data = data;
		if (d->bitbang.ludicrous_speed_corruption)
			dma.flags |= KDEBRICK_DMA_LUDICROUS_SPEED_CORRUPTION;

		if (put_user(dma.data, (u32 __user *)&user_dma->data))
			return -EFAULT;
		if (put_user(dma.flags, (u32 __user *)&user_dma->flags))
			return -EFAULT;
		break;
	}
	case KDEBRICK_IOCTL_DMAWRITE: {
		struct kdebrick_dma dma;
		struct kdebrick_dma __user *user_dma = argp;

		if (copy_from_user(&dma, user_dma, sizeof(dma)))
			return -EFAULT;

		err = bitbang_ejtag_dma_write(&d->bitbang, dma.control,
					      dma.addr, dma.data);
		if (err)
			return err;
		if (d->bitbang.ludicrous_speed_corruption)
			dma.flags |= KDEBRICK_DMA_LUDICROUS_SPEED_CORRUPTION;

		if (put_user(dma.flags, (u32 __user *)&user_dma->flags))
			return -EFAULT;
		break;
	}
	case KDEBRICK_IOCTL_WRITEDATA: {
		u32 data;

		if (get_user(data, (u32 __user *)argp))
			return -EFAULT;
		bitbang_wdata(&d->bitbang, data);
		break;
	}
	case KDEBRICK_IOCTL_RWDATA: {
		u32 data;

		if (get_user(data, (u32 __user *)argp))
			return -EFAULT;
		data = bitbang_rwdata(&d->bitbang, data);
		if (put_user(data, (u32 __user *)argp))
			return -EFAULT;
		break;
	}
	case KDEBRICK_IOCTL_SETINSTR: {
		u32 instr;

		if (get_user(instr, (u32 __user *)argp))
			return -EFAULT;
		bitbang_set_instr(&d->bitbang, instr);
		break;
	}
	case KDEBRICK_IOCTL_TESTRESET: {
		bitbang_test_reset(&d->bitbang);
		break;
	}
	case KDEBRICK_IOCTL_SETCONFIG: {
		if (copy_from_user(&d->config, argp, sizeof(d->config)))
			return -EFAULT;
		d->config.tck_delay = min(d->config.tck_delay, (u32)MAX_TCK_DELAY);

		d->bitbang.instruction_length = min(d->config.instruction_length,
						    (u32)MAX_INSTR_LENGTH);
		d->bitbang.tck_delay = min(d->config.tck_delay,
					   (u32)MAX_TCK_DELAY);
		d->bitbang.use_wiggler = !!(d->config.flags & KDEBRICK_CONF_WIGGLER);
		d->bitbang.use_ludicrous_speed =
			!!(d->config.flags & KDEBRICK_CONF_LUDICROUS_SPEED);
		break;
	}
	case KDEBRICK_IOCTL_GETCONFIG: {
		if (copy_to_user(argp, &d->config, sizeof(d->config)))
			return -EFAULT;
		break;
	}
	default:
		printk(KERN_DEBUG CHRDEV "Unknown IOCTL (cmd=0x%x)\n", cmd);
		return -EINVAL;
	}

	return 0;
}

static long kdebrick_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct kdebrick *d = file->private_data;
	long ret;

	mutex_lock(&d->mutex);
	ret = kdebrick_do_ioctl(file, cmd, arg);
	mutex_unlock(&d->mutex);

	return ret;
}

static long kdebrick_compat_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	return kdebrick_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}

static int kdebrick_open(struct inode *inode, struct file *file)
{
	struct kdebrick *d;

	if (iminor(inode) >= PARPORT_MAX)
		return -ENXIO;

	d = kzalloc(sizeof(struct kdebrick), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	mutex_init(&d->mutex);

	/* Defer the actual device registration until the first claim. */
	d->pdev = NULL;
	file->private_data = d;

	return 0;
}

static int kdebrick_release(struct inode *inode, struct file *file)
{
	struct kdebrick *d = file->private_data;

	if (d->claimed)
		parport_release(d->pdev);

	if (d->pdev) {
		const char *name = d->pdev->name;
		parport_unregister_device(d->pdev);
		kfree(name);
		d->pdev = NULL;
	}

	kfree(d);

	return 0;
}

static struct class *kdebrick_class;

static const struct file_operations kdebrick_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.unlocked_ioctl	= kdebrick_ioctl,
	.compat_ioctl	= kdebrick_compat_ioctl,
	.open		= kdebrick_open,
	.release	= kdebrick_release,
};

static void pp_attach(struct parport *port)
{
	device_create(kdebrick_class, port->dev, MKDEV(PP_MAJOR, port->number),
		      NULL, "kdebrick%d", port->number);
}

static void pp_detach(struct parport *port)
{
	device_destroy(kdebrick_class, MKDEV(PP_MAJOR, port->number));
}

static struct parport_driver kdebrick_driver = {
	.name = CHRDEV,
	.attach = pp_attach,
	.detach = pp_detach,
};

static int __init kdebrick_modinit(void)
{
	int err = 0;

	if (register_chrdev(PP_MAJOR, CHRDEV, &kdebrick_fops)) {
		printk(KERN_WARNING CHRDEV ": unable to get major %d\n", PP_MAJOR);
		return -EIO;
	}
	kdebrick_class = class_create(THIS_MODULE, CHRDEV);
	if (IS_ERR(kdebrick_class)) {
		err = PTR_ERR(kdebrick_class);
		goto out_chrdev;
	}
	if (parport_register_driver(&kdebrick_driver)) {
		printk(KERN_WARNING CHRDEV ": unable to register with parport\n");
		goto out_class;
	}

	printk(KERN_INFO "kdebrick: Debrick kernel accelerator\n");

	goto out;

out_class:
	class_destroy(kdebrick_class);
out_chrdev:
	unregister_chrdev(PP_MAJOR, CHRDEV);
out:
	return err;
}
module_init(kdebrick_modinit);

static void __exit kdebrick_modexit(void)
{
	parport_unregister_driver(&kdebrick_driver);
	class_destroy(kdebrick_class);
	unregister_chrdev(PP_MAJOR, CHRDEV);
}
module_exit(kdebrick_modexit);

MODULE_LICENSE("GPL");
