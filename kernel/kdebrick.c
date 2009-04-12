/*
 *  Broadcom-MIPS EJTAG Debrick Utility
 *  Accelerator kernel module
 *
 *  Copyright (C) 2009 Michael Buesch <mb@bu3sch.de>
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
#include "../debrick.h"

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


#define CHRDEV		"kdebrick"


struct kdebrick {
	struct mutex mutex;
	struct pardevice *pdev;
	bool claimed;
	unsigned int curinstr;
	struct kdebrick_config config;
};


static inline void clockin(struct kdebrick *d, int tms, int tdi)
{
	unsigned char data;

	tms = tms ? 1 : 0;
	tdi = tdi ? 1 : 0;

	data = (0 << TCK) | (tms << TMS) | (tdi << TDI) | (1 << TRST_N);
	parport_write_data(d->pdev->port, data);

	data = (1 << TCK) | (tms << TMS) | (tdi << TDI) | (1 << TRST_N);
	parport_write_data(d->pdev->port, data);
}

static inline unsigned char clockin_tdo(struct kdebrick *d, int tms, int tdi)
{
	unsigned char data;

	clockin(d, tms, tdi);
	data = parport_read_status(d->pdev->port);
	data ^= (1 << TDO);
	data = !!(data & (1 << TDO));

	return data;
}

static void test_reset(struct kdebrick *d)
{
	clockin(d, 1, 0); /* Run through a handful of clock cycles with TMS high to make sure */
	clockin(d, 1, 0); /* we are in the TEST-LOGIC-RESET state. */
	clockin(d, 1, 0);
	clockin(d, 1, 0);
	clockin(d, 1, 0);
	clockin(d, 0, 0); /* enter runtest-idle */
}

static void set_instr(struct kdebrick *d, int instr)
{
	int i;

	if (instr == d->curinstr)
		return;
	d->curinstr = instr;

	clockin(d, 1, 0);		/* enter select-dr-scan */
	clockin(d, 1, 0);		/* enter select-ir-scan */
	clockin(d, 0, 0);		/* enter capture-ir */
	clockin(d, 0, 0);		/* enter shift-ir (dummy) */
	for (i = 0; i < d->config.instruction_length; i++)
		clockin(d, i == (d->config.instruction_length - 1), (instr >> i) & 1);
	clockin(d, 1, 0);		/* enter update-ir */
	clockin(d, 0, 0);		/* enter runtest-idle */

	cond_resched();
}

static unsigned int ReadWriteData(struct kdebrick *d, unsigned int in_data)
{
	int i;
	unsigned int out_data = 0;
	unsigned char out_bit;

	clockin(d, 1, 0);		/* enter select-dr-scan */
	clockin(d, 0, 0);		/* enter capture-dr */
	clockin(d, 0, 0);		/* enter shift-dr */
	for (i = 0; i < 32; i++) {
		out_bit = clockin_tdo(d, (i == 31), ((in_data >> i) & 1));
		out_data = out_data | (out_bit << i);
	}
	clockin(d, 1, 0);		/* enter update-dr */
	clockin(d, 0, 0);		/* enter runtest-idle */

	cond_resched();

	return out_data;
}

static inline unsigned int ReadData(struct kdebrick *d)
{
	return ReadWriteData(d, 0);
}

static void WriteData(struct kdebrick *d, unsigned int in_data)
{
	int i;

	clockin(d, 1, 0);		/* enter select-dr-scan */
	clockin(d, 0, 0);		/* enter capture-dr */
	clockin(d, 0, 0);		/* enter shift-dr */
	for (i = 0; i < 32; i++)
		clockin(d, (i == 31), ((in_data >> i) & 1));
	clockin(d, 1, 0);		/* enter update-dr */
	clockin(d, 0, 0);		/* enter runtest-idle */

	cond_resched();
}

static int ejtag_dma_read(struct kdebrick *d, struct kdebrick_dma *dma)
{
	int retries = RETRY_ATTEMPTS;

begin_ejtag_dma_read:

	// Setup Address
	set_instr(d, INSTR_ADDRESS);
	WriteData(d, dma->addr);

	// Initiate DMA Read & set DSTRT
	set_instr(d, INSTR_CONTROL);
	WriteData(d, DMAACC | DRWN | dma->control | DSTRT | PROBEN | PRACC);

	if (!(dma->flags & KDEBRICK_DMA_LUDICROUS_SPEED)) {
		// Wait for DSTRT to Clear
		while (ReadWriteData(d, DMAACC | PROBEN | PRACC) & DSTRT) {
			dma->flags |= KDEBRICK_DMA_LUDICROUS_SPEED_CORRUPTION;
			cond_resched();
			if (signal_pending(current))
				return -EINTR;
		}
	}

	// Read Data
	set_instr(d, INSTR_DATA);
	dma->data = ReadData(d);

	if (!(dma->flags & KDEBRICK_DMA_LUDICROUS_SPEED)) {
		// Clear DMA & Check DERR
		set_instr(d, INSTR_CONTROL);
		if (ReadWriteData(d, PROBEN | PRACC) & DERR) {
			dma->flags |= KDEBRICK_DMA_LUDICROUS_SPEED_CORRUPTION;
			if (retries--) {
				goto begin_ejtag_dma_read;
			} else {
				printk("kdebrick: DMA Read Addr = %08x  Data = (%08x)ERROR ON READ\n",
				       dma->addr, dma->data);
				return -EIO;
			}
		}
	}

	if ((dma->control & DMA_HALFWORD) && !(dma->control & DMA_WORD)) {
		/* Handle the bigendian/littleendian */
		if (dma->addr & 0x2)
			dma->data = (dma->data >> 16) & 0xffff;
		else
			dma->data = (dma->data & 0x0000ffff);
	}

	return 0;
}

static int ejtag_dma_write(struct kdebrick *d, struct kdebrick_dma *dma)
{
	int retries = RETRY_ATTEMPTS;

begin_ejtag_dma_write:

	// Setup Address
	set_instr(d, INSTR_ADDRESS);
	WriteData(d, dma->addr);

	// Setup Data
	set_instr(d, INSTR_DATA);
	WriteData(d, dma->data);

	// Initiate DMA Write & set DSTRT
	set_instr(d, INSTR_CONTROL);
	WriteData(d, DMAACC | dma->control | DSTRT | PROBEN | PRACC);

	if (!(dma->flags & KDEBRICK_DMA_LUDICROUS_SPEED)) {
		// Wait for DSTRT to Clear
		while (ReadWriteData(d, DMAACC | PROBEN | PRACC) & DSTRT) {
			dma->flags |= KDEBRICK_DMA_LUDICROUS_SPEED_CORRUPTION;
			cond_resched();
			if (signal_pending(current))
				return -EINTR;
		}

		// Clear DMA & Check DERR
		set_instr(d, INSTR_CONTROL);
		if (ReadWriteData(d, PROBEN | PRACC) & DERR) {
			dma->flags |= KDEBRICK_DMA_LUDICROUS_SPEED_CORRUPTION;
			if (retries--) {
				goto begin_ejtag_dma_write;
			} else {
				printk("kdebrick: DMA Write Addr = %08x  Data = ERROR ON WRITE\n",
				       dma->addr);
				return -EIO;
			}
		}
	}

	return 0;
}

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
	unsigned int minor = iminor(file->f_path.dentry->d_inode);
	struct kdebrick *d = file->private_data;
	struct parport *port;
	void __user *argp = (void __user *)arg;
	int err;

	/* First handle the cases that don't take arguments. */
	switch (cmd) {
	case KDEBRICK_IOCTL_CLAIM:
		if (d->claimed)
			return -EBUSY;

		/* Deferred device registration. */
		if (!d->pdev) {
			err = register_device(minor, d);
			if (err) {
				return err;
			}
		}

		err = parport_claim_or_block(d->pdev);
		if (err < 0)
			return err;

		d->claimed = 1;
		parport_set_timeout(d->pdev, parport_set_timeout(d->pdev, 0));

		return 0;
	}

	/* Everything else requires the port to be claimed, so check
	 * that now. */
	if (!d->claimed)
		return -EPERM;

	port = d->pdev->port;
	switch (cmd) {
	case KDEBRICK_IOCTL_RELEASE:
		parport_release(d->pdev);
		d->claimed = 0;
		return 0;
	case KDEBRICK_IOCTL_DMAREAD: {
		struct kdebrick_dma dma;
		struct kdebrick_dma __user *user_dma = argp;

		if (copy_from_user(&dma, user_dma, sizeof(dma)))
			return -EFAULT;
		err = ejtag_dma_read(d, &dma);
		if (err)
			return err;
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
		err = ejtag_dma_write(d, &dma);
		if (err)
			return err;
		if (put_user(dma.flags, (u32 __user *)&user_dma->flags))
			return -EFAULT;
		break;
	}
	case KDEBRICK_IOCTL_WRITEDATA: {
		u32 data;

		if (get_user(data, (u32 __user *)argp))
			return -EFAULT;
		WriteData(d, data);
		break;
	}
	case KDEBRICK_IOCTL_RWDATA: {
		u32 data;

		if (get_user(data, (u32 __user *)argp))
			return -EFAULT;
		data = ReadWriteData(d, data);
		if (put_user(data, (u32 __user *)argp))
			return -EFAULT;
		break;
	}
	case KDEBRICK_IOCTL_SETINSTR: {
		u32 instr;

		if (get_user(instr, (u32 __user *)argp))
			return -EFAULT;
		set_instr(d, instr);
		break;
	}
	case KDEBRICK_IOCTL_TESTRESET: {
		test_reset(d);
		break;
	}
	case KDEBRICK_IOCTL_SETCONFIG: {
		if (copy_from_user(&d->config, argp, sizeof(d->config)))
			return -EFAULT;
		break;
	}
	case KDEBRICK_IOCTL_GETCONFIG: {
		if (copy_to_user(argp, &d->config, sizeof(d->config)))
			return -EFAULT;
		break;
	}
	default:
		printk(KERN_DEBUG CHRDEV "%x: What? (cmd=0x%x)\n", minor, cmd);
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
	unsigned int minor = iminor(inode);
	struct kdebrick *d;

	if (minor >= PARPORT_MAX)
		return -ENXIO;

	d = kzalloc(sizeof(struct kdebrick), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	mutex_init(&d->mutex);
	d->curinstr = -1;

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
