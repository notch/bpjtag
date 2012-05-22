// **************************************************************************
//
//  WRT54G.H - Header file for the WRT54G/GS EJTAG DeBrick Utility  v4.1
//
//  Note:
//  This program is for De-Bricking the WRT54G/GS routers
//
//  New for v4.1 - software re-written to support 38 flash chips and
//                 auto-detect flash chip & flash size & adjust
//                 region info accordingly for reading/writing to the
//                 flash chips.  Also added support for compiling under
//                 Windows, Linux, and FreeBSD.
//
// **************************************************************************
//  Written by HairyDairyMaid (a.k.a. - lightbulb)
//  hairydairymaid@yahoo.com
// **************************************************************************
//
//  This program is copyright (C) 2004 HairyDairyMaid (a.k.a. Lightbulb)
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of version 2 the GNU General Public License as published
//  by the Free Software Foundation.
//  This program is distributed in the hope that it will be useful, but WITHOUT
//  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
//  more details.
//  To view a copy of the license go to:
//  http://www.fsf.org/copyleft/gpl.html
//  To receive a copy of the GNU General Public License write the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// **************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#ifndef WINDOWS_VERSION

   #include <unistd.h>
   #include <sys/ioctl.h>

   #ifdef __FreeBSD__
      #include <dev/ppbus/ppi.h>
      #include <dev/ppbus/ppbconf.h>
      #define PPWDATA PPISDATA
      #define PPRSTATUS PPIGSTATUS
   #else
      #include <linux/ppdev.h>
   #endif

#endif

#define true  1
#define false 1

#define RETRY_ATTEMPTS 16

// ------------------------------------------------------
// --- Choose only one cable specific section below
// ------------------------------------------------------
//
// --- Xilinx Type Cable ---
#define TDI     0
#define TCK     1
#define TMS     2
#define TDO     4
//
// --- Wiggler Type Cable ---
// #define TDI      3
// #define TCK      2
// #define TMS      1
// #define TDO      7
//
// ------------------------------------------------------

// --- Some BCM47XX Instructions ---
#define INSTR_IDCODE    0x01
#define INSTR_EXTEST    0x00
#define INSTR_SAMPLE    0x02
#define INSTR_PRELOAD   0x02
#define INSTR_BYPASS    0xFF
#define INSTR_CONTROL   0x0A
#define INSTR_DATA      0x09
#define INSTR_ADDRESS   0x08

// --- Some EJTAG Bit Masks ---
#define TOF             (1 << 1 )
#define BRKST           (1 << 3 )
#define DRWN            (1 << 9 )
#define DERR            (1 << 10)
#define DSTRT           (1 << 11)
#define SETDEV          (1 << 14)
#define PROBEN          (1 << 15)
#define DMAACC          (1 << 17)
#define PRACC           (1 << 18)
#define PRNW            (1 << 19)
#define DLOCK           (1 << 5 )
#define TIF             (1 << 2 )
#define SYNC            (1 << 23)
#define PRRST           (1 << 16)
#define PERRST          (1 << 20)
#define JTAGBRK         (1 << 12)
#define DNM             (1 << 28)
#define DMA_BYTE        0x00000000  //DMA tranfser size BYTE
#define DMA_HALFWORD    0x00000080  //DMA transfer size HALFWORD
#define DMA_WORD        0x00000100  //DMA transfer size WORD
#define DMA_TRIPLEBYTE  0x00000180  //DMA transfer size TRIPLEBYTE

// --- For 2MB Flash Chips ---
#define  CFE_START_2MB         0x1FC00000
#define  CFE_LENGTH_2MB        0x40000
#define  KERNEL_START_2MB      0x1FC40000
#define  KERNEL_LENGTH_2MB     0x1B0000
#define  NVRAM_START_2MB       0x1FDF0000
#define  NVRAM_LENGTH_2MB      0x10000
#define  WHOLEFLASH_START_2MB  0x1FC00000
#define  WHOLEFLASH_LENGTH_2MB 0x200000

// --- For 4MB Flash Chips ---
#define  CFE_START_4MB         0x1FC00000
#define  CFE_LENGTH_4MB        0x40000
#define  KERNEL_START_4MB      0x1FC40000
#define  KERNEL_LENGTH_4MB     0x3B0000
#define  NVRAM_START_4MB       0x1FFF0000
#define  NVRAM_LENGTH_4MB      0x10000
#define  WHOLEFLASH_START_4MB  0x1FC00000
#define  WHOLEFLASH_LENGTH_4MB 0x400000

// --- For 8MB Flash Chips ---
#define  CFE_START_8MB         0x1C000000
#define  CFE_LENGTH_8MB        0x40000
#define  KERNEL_START_8MB      0x1C040000
#define  KERNEL_LENGTH_8MB     0x7A0000
#define  NVRAM_START_8MB       0x1C7E0000
#define  NVRAM_LENGTH_8MB      0x20000
#define  WHOLEFLASH_START_8MB  0x1C000000
#define  WHOLEFLASH_LENGTH_8MB 0x800000

#define  size8K        0x2000
#define  size16K       0x4000
#define  size32K       0x8000
#define  size64K       0x10000
#define  size128K      0x20000
#define  size2MB       0x200000
#define  size4MB       0x400000
#define  size8MB       0x800000
#define  size16MB      0x1000000
#define  CMD_TYPE_BSC  0x01
#define  CMD_TYPE_SCS  0x02
#define  CMD_TYPE_AMD  0x03
#define  CMD_TYPE_SST  0x04


// --- Uhh, Just Because I Have To ---
static unsigned char clockout(void);
static unsigned int ReadData(void);
static unsigned int ReadWriteData(unsigned int in_data);
static unsigned int ejtag_dma_read(unsigned int addr);
static unsigned int ejtag_dma_read_h(unsigned int addr);
void ShowData(unsigned int value);
void WriteData(unsigned int in_data);
void capture_dr(void);
void capture_ir(void);
void chip_detect(void);
void chip_shutdown(void);
void clockin(int tms, int tdi);
void define_block(unsigned int block_count, unsigned int block_size);
void ejtag_dma_write(unsigned int addr, unsigned int data);
void ejtag_dma_write_h(unsigned int addr, unsigned int data);
void ejtag_issue_reset(void);
void ejtag_jtagbrk(void);
void identify_flash_part(void);
void lpt_closeport(void);
void lpt_openport(void);
void run_backup(char *filename, unsigned int start, unsigned int length);
void run_erase(char *filename, unsigned int start, unsigned int length);
void run_flash(char *filename, unsigned int start, unsigned int length);
void set_instr(int instr);
void sflash_config(void);
void sflash_erase_area(unsigned int start, unsigned int length);
void sflash_erase_block(unsigned int addr);
void sflash_probe(void);
void sflash_reset(void);
void sflash_write_word(unsigned int addr, unsigned int data);
void show_usage(void);
void test_reset(void);
