/*
 *  Broadcom-MIPS EJTAG Debrick Utility
 *  Common lowlevel bitbanging routines
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
 */

#include "bitbang.h"


/* The following functions have to be implemented in the code
 * using this bitbang library:
 *
 * Delay for "usec" microseconds.
 * void debrick_usec_delay(unsigned int usec)
 *
 * Relax a bit. Returns true, if the user wants to abort the operation.
 * int debrick_relax(void)
 *
 * Write parport "data" register.
 * void debrick_parport_write_data(void *priv, uint8_t data)
 *
 * Read parport "status" register.
 * uint8_t debrick_parport_read_status(void *priv)
 */


#ifdef __KERNEL__
# define printf		printk
# define ERRPFX		KERN_ERR "kdebrick: "
# define INFOPFX	KERN_INFO "kdebrick: "
#else
# define printf		fprintf
# define ERRPFX		stderr, "debrick: "
# define INFOPFX	stdout, "debrick: "
#endif


static inline void bitbang_tck_delay(struct debrick_bitbang *b)
{
	if (b->tck_delay)
		debrick_usec_delay(b->tck_delay);
}

static inline void bitbang_clockin(struct debrick_bitbang *b, int tms, int tdi)
{
	unsigned char data;

	tms = tms ? 1 : 0;
	tdi = tdi ? 1 : 0;

	if (b->use_wiggler)
		data = (0 << WTCK) | (tms << WTMS) | (tdi << WTDI) | (1 << WTRST_N);
	else
		data = (0 << TCK) | (tms << TMS) | (tdi << TDI);
	debrick_parport_write_data(b->parport_priv, data);
	bitbang_tck_delay(b);

	if (b->use_wiggler)
		data = (1 << WTCK) | (tms << WTMS) | (tdi << WTDI) | (1 << WTRST_N);
	else
		data = (1 << TCK) | (tms << TMS) | (tdi << TDI);
	debrick_parport_write_data(b->parport_priv, data);
	bitbang_tck_delay(b);
}

static inline unsigned char bitbang_clockin_tdo(struct debrick_bitbang *b, int tms, int tdi)
{
	unsigned char data;

	bitbang_clockin(b, tms, tdi);
	data = debrick_parport_read_status(b->parport_priv);
	if (b->use_wiggler) {
		data ^= (1 << WTDO);
		data = !!(data & (1 << WTDO));
	} else
		data = !!(data & (1 << TDO));

	return data;
}

static void bitbang_test_reset(struct debrick_bitbang *b)
{
	bitbang_clockin(b, 1, 0); /* Run through a handful of clock cycles with TMS high to make sure */
	bitbang_clockin(b, 1, 0); /* we are in the TEST-LOGIC-RESET state. */
	bitbang_clockin(b, 1, 0);
	bitbang_clockin(b, 1, 0);
	bitbang_clockin(b, 1, 0);
	bitbang_clockin(b, 0, 0); /* enter runtest-idle */
}

static void bitbang_set_instr(struct debrick_bitbang *b, unsigned int instr)
{
	int i;

	if (instr == b->curinstr)
		return;
	b->curinstr = instr;

	bitbang_clockin(b, 1, 0);		/* enter select-dr-scan */
	bitbang_clockin(b, 1, 0);		/* enter select-ir-scan */
	bitbang_clockin(b, 0, 0);		/* enter capture-ir */
	bitbang_clockin(b, 0, 0);		/* enter shift-ir (dummy) */
	for (i = 0; i < b->instruction_length; i++)
		bitbang_clockin(b, i == (b->instruction_length - 1), (instr >> i) & 1);
	bitbang_clockin(b, 1, 0);		/* enter update-ir */
	bitbang_clockin(b, 0, 0);		/* enter runtest-idle */

	debrick_relax();
}

static unsigned int bitbang_rwdata(struct debrick_bitbang *b, unsigned int in_data)
{
	int i;
	unsigned int out_data = 0;
	unsigned char out_bit;

	bitbang_clockin(b, 1, 0);		/* enter select-dr-scan */
	bitbang_clockin(b, 0, 0);		/* enter capture-dr */
	bitbang_clockin(b, 0, 0);		/* enter shift-dr */
	for (i = 0; i < 32; i++) {
		out_bit = bitbang_clockin_tdo(b, (i == 31), ((in_data >> i) & 1));
		out_data = out_data | (out_bit << i);
	}
	bitbang_clockin(b, 1, 0);		/* enter update-dr */
	bitbang_clockin(b, 0, 0);		/* enter runtest-idle */

	debrick_relax();

	return out_data;
}

static inline unsigned int bitbang_rdata(struct debrick_bitbang *b)
{
	return bitbang_rwdata(b, 0);
}

static void bitbang_wdata(struct debrick_bitbang *b, unsigned int in_data)
{
	int i;

	bitbang_clockin(b, 1, 0);		/* enter select-dr-scan */
	bitbang_clockin(b, 0, 0);		/* enter capture-dr */
	bitbang_clockin(b, 0, 0);		/* enter shift-dr */
	for (i = 0; i < 32; i++)
		bitbang_clockin(b, (i == 31), ((in_data >> i) & 1));
	bitbang_clockin(b, 1, 0);		/* enter update-dr */
	bitbang_clockin(b, 0, 0);		/* enter runtest-idle */

	debrick_relax();
}

static int bitbang_ejtag_dma_read(struct debrick_bitbang *b,
				  unsigned int control, unsigned int addr,
				  unsigned int *data)
{
	unsigned int retries = 16;

begin_ejtag_dma_read:

	// Setup Address
	bitbang_set_instr(b, INSTR_ADDRESS);
	bitbang_wdata(b, addr);

	// Initiate DMA Read & set DSTRT
	bitbang_set_instr(b, INSTR_CONTROL);
	bitbang_wdata(b, DMAACC | DRWN | control | DSTRT | PROBEN | PRACC);

	if (!b->use_ludicrous_speed) {
		// Wait for DSTRT to Clear
		while (bitbang_rwdata(b, DMAACC | PROBEN | PRACC) & DSTRT) {
			b->ludicrous_speed_corruption = 1;
			if (debrick_relax())
				return -EINTR;
		}
	}

	// Read Data
	bitbang_set_instr(b, INSTR_DATA);
	*data = bitbang_rdata(b);

	if (!b->use_ludicrous_speed) {
		// Clear DMA & Check DERR
		bitbang_set_instr(b, INSTR_CONTROL);
		if (bitbang_rwdata(b, PROBEN | PRACC) & DERR) {
			b->ludicrous_speed_corruption = 1;
			if (retries--) {
				goto begin_ejtag_dma_read;
			} else {
				printf(ERRPFX "DMA Read Addr = %08x  Data = (%08x)ERROR ON READ\n",
				       addr, *data);
				return -EIO;
			}
		}
	}

	if ((control & DMA_HALFWORD) && !(control & DMA_WORD)) {
		/* Handle the bigendian/littleendian */
		if (addr & 0x2)
			*data = (*data >> 16) & 0xffff;
		else
			*data = (*data & 0x0000ffff);
	}

	return 0;
}

static int bitbang_ejtag_dma_write(struct debrick_bitbang *b,
				   unsigned int control, unsigned int addr,
				   unsigned int data)
{
	unsigned int retries = 16;

begin_ejtag_dma_write:

	// Setup Address
	bitbang_set_instr(b, INSTR_ADDRESS);
	bitbang_wdata(b, addr);

	// Setup Data
	bitbang_set_instr(b, INSTR_DATA);
	bitbang_wdata(b, data);

	// Initiate DMA Write & set DSTRT
	bitbang_set_instr(b, INSTR_CONTROL);
	bitbang_wdata(b, DMAACC | control | DSTRT | PROBEN | PRACC);

	if (!b->use_ludicrous_speed) {
		// Wait for DSTRT to Clear
		while (bitbang_rwdata(b, DMAACC | PROBEN | PRACC) & DSTRT) {
			b->ludicrous_speed_corruption = 1;
			if (debrick_relax())
				return -EINTR;
		}

		// Clear DMA & Check DERR
		bitbang_set_instr(b, INSTR_CONTROL);
		if (bitbang_rwdata(b, PROBEN | PRACC) & DERR) {
			b->ludicrous_speed_corruption = 1;
			if (retries--) {
				goto begin_ejtag_dma_write;
			} else {
				printf(ERRPFX "DMA Write Addr = %08x  Data = ERROR ON WRITE\n",
				       addr);
				return -EIO;
			}
		}
	}

	return 0;
}

#undef printf
