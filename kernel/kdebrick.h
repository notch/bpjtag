/****************************************************************************
 *
 *  Broadcom-MIPS EJTAG Debrick Utility
 *  Accelerator kernel module
 *
 *  Copyright (C) 2009 Michael Buesch <mb@bu3sch.de>
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
 *
 ****************************************************************************/

#ifndef LINUX_KDEBRICK_H_
#define LINUX_KDEBRICK_H_

#include <linux/types.h>
#include <linux/ioctl.h>


#define __KDEBRICK_IOCTL	('D'^'e'^'B'^'r'^'I'^'c'^'K')

enum kdebrick_dma_flags {
	KDEBRICK_DMA_LUDICROUS_SPEED		= (1 << 0),
	KDEBRICK_DMA_LUDICROUS_SPEED_CORRUPTION	= (1 << 1),
};

struct kdebrick_dma {
	__u32 addr;
	__u32 data;
	__u32 control;
	__u32 flags;
};

enum kdebrick_config_flags {
	KDEBRICK_CONF_WIGGLER		= (1 << 0),
};

struct kdebrick_config {
	__u32 instruction_length;
	__u32 tck_delay;
	__u32 flags;
};

#define KDEBRICK_IOCTL_CLAIM		_IO(__KDEBRICK_IOCTL,	0)
#define KDEBRICK_IOCTL_RELEASE		_IO(__KDEBRICK_IOCTL,	1)
#define KDEBRICK_IOCTL_GETCONFIG	_IOR(__KDEBRICK_IOCTL,	2, struct kdebrick_config)
#define KDEBRICK_IOCTL_SETCONFIG	_IOW(__KDEBRICK_IOCTL,	3, struct kdebrick_config)
#define KDEBRICK_IOCTL_DMAREAD		_IOWR(__KDEBRICK_IOCTL,	4, struct kdebrick_dma)
#define KDEBRICK_IOCTL_DMAWRITE		_IOWR(__KDEBRICK_IOCTL,	5, struct kdebrick_dma)
#define KDEBRICK_IOCTL_WRITEDATA	_IOW(__KDEBRICK_IOCTL,	6, __u32)
#define KDEBRICK_IOCTL_RWDATA		_IOWR(__KDEBRICK_IOCTL,	7, __u32)
#define KDEBRICK_IOCTL_SETINSTR		_IOW(__KDEBRICK_IOCTL,	8, __u32)
#define KDEBRICK_IOCTL_TESTRESET	_IO(__KDEBRICK_IOCTL,	9)


#endif /* LINUX_KDEBRICK_H_ */
