/****************************************************************************
 *
 *  Broadcom-MIPS EJTAG Debrick Utility
 *
 *  Copyright (C) 2009 Michael Buesch <mb@bu3sch.de>
 *  Copyright (C) 2004 HairyDairyMaid (a.k.a. Lightbulb)
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/ioctl.h>

#ifdef __FreeBSD__
# include <dev/ppbus/ppi.h>
# include <dev/ppbus/ppbconf.h>
# define PPWDATA PPISDATA
# define PPRSTATUS PPIGSTATUS
#else
# include <linux/ppdev.h>
#endif


#define size4K		0x1000
#define size8K		0x2000
#define size16K		0x4000
#define size32K		0x8000
#define size64K		0x10000
#define size128K	0x20000

#define size1MB		0x100000
#define size2MB		0x200000
#define size4MB		0x400000
#define size8MB		0x800000
#define size16MB	0x1000000

#define CMD_TYPE_BSC  0x01
#define CMD_TYPE_SCS  0x02
#define CMD_TYPE_AMD  0x03
#define CMD_TYPE_SST  0x04

// EJTAG DEBUG Unit Vector on Debug Break
#define MIPS_DEBUG_VECTOR_ADDRESS           0xFF200200

// Our 'Pseudo' Virtual Memory Access Registers
#define MIPS_VIRTUAL_ADDRESS_ACCESS         0xFF200000
#define MIPS_VIRTUAL_DATA_ACCESS            0xFF200004



static const unsigned int pracc_readword_code_module[] = {
	// #
	// # HairyDairyMaid's Assembler PrAcc Read Word Routine
	// #
	// start:
	// 
	// # Load R1 with the address of the pseudo-address register
	0x3C01FF20,		// lui $1,  0xFF20
	0x34210000,		// ori $1,  0x0000
	// 
	// # Load R2 with the address for the read
	0x8C220000,		// lw $2,  ($1)
	// 
	// # Load R3 with the word @R2
	0x8C430000,		// lw $3, 0($2)
	// 
	// # Store the value into the pseudo-data register
	0xAC230004,		// sw $3, 4($1)
	// 
	0x00000000,		// nop
	0x1000FFF9,		// beq $0, $0, start
	0x00000000
};				// nop

static const unsigned int pracc_writeword_code_module[] = {
	// #
	// # HairyDairyMaid's Assembler PrAcc Write Word Routine
	// #
	// start:
	// 
	// # Load R1 with the address of the pseudo-address register
	0x3C01FF20,		// lui $1,  0xFF20
	0x34210000,		// ori $1,  0x0000
	// 
	// # Load R2 with the address for the write
	0x8C220000,		// lw $2,  ($1)
	// 
	// # Load R3 with the data from pseudo-data register
	0x8C230004,		// lw $3, 4($1)
	// 
	// # Store the word at @R2 (the address)
	0xAC430000,		// sw $3,  ($2)
	// 
	0x00000000,		// nop
	0x1000FFF9,		// beq $0, $0, start
	0x00000000
};				// nop

static const unsigned int pracc_readhalf_code_module[] = {
	// #
	// # HairyDairyMaid's Assembler PrAcc Read HalfWord Routine
	// #
	// start:
	// 
	// # Load R1 with the address of the pseudo-address register
	0x3C01FF20,		// lui $1,  0xFF20
	0x34210000,		// ori $1,  0x0000
	// 
	// # Load R2 with the address for the read
	0x8C220000,		// lw $2,  ($1)
	// 
	// # Load R3 with the half word @R2
	0x94430000,		// lhu $3, 0($2)
	// 
	// # Store the value into the pseudo-data register
	0xAC230004,		// sw $3, 4($1)
	// 
	0x00000000,		// nop
	0x1000FFF9,		// beq $0, $0, start
	0x00000000
};				// nop

static const unsigned int pracc_writehalf_code_module[] = {
	// #
	// # HairyDairyMaid's Assembler PrAcc Write HalfWord Routine
	// #
	// start:
	// 
	// # Load R1 with the address of the pseudo-address register
	0x3C01FF20,		// lui $1,  0xFF20
	0x34210000,		// ori $1,  0x0000
	// 
	// # Load R2 with the address for the write
	0x8C220000,		// lw $2,  ($1)
	// 
	// # Load R3 with the data from pseudo-data register
	0x8C230004,		// lw $3, 4($1)
	// 
	// # Store the half word at @R2 (the address)
	0xA4430000,		// sh $3,  ($2)
	// 
	0x00000000,		// nop
	0x1000FFF9,		// beq $0, $0, start
	0x00000000
};				// nop
