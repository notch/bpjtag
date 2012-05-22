// **************************************************************************
//
//  WRT54G.C - WRT54G/GS EJTAG DeBrick Utility  v4.1
//
//  Note:
//  This program is for De-Bricking the WRT54G/GS routers
//
//  New for v4.1 - software re-written to support 38 flash chips and
//                 auto-detect flash chip & flash size & adjust
//                 region info accordingly for reading/writing to the
//                 flash chips.  Also added support for compiling under
//                 Windows, Linux, and FreeBSD.  Also support the new
//                 Broadcom BCM5352 chip.
//
// **************************************************************************
//
//  wrt54g: read/write flash memory via EJTAG
//   usage: wrt54g [option] </noreset> </nobreak> </noerase> </notimestamp> </fc:XX>
//              -backup:cfe
//              -backup:nvram
//              -backup:kernel
//              -backup:wholeflash
//              -erase:cfe
//              -erase:nvram
//              -erase:kernel
//              -erase:wholeflash
//              -flash:cfe
//              -flash:nvram
//              -flash:kernel
//              -flash:wholeflash
//
//              /noreset     (prevents CPU reset of Broadcom Chip ..... optional)
//              /nobreak     (prevents issuing Debug Mode JTAGBRK ..... optional)
//              /noerase     (prevents forced erase before flashing ... optional)
//              /notimestamp (prevents timestamping of backups ........ optional)
//              /fc:XX       (specify flash chip manually ............. optional)
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

// Default is Compile for Linux (both #define's below should be commented out)
// #define WINDOWS_VERSION   // uncomment only this for Windows Compile / MS Visual C Compiler
// #define __FreeBSD__       // uncomment only this for FreeBSD

#ifdef WINDOWS_VERSION
   #include <windows.h>      // Only for Windows Compile
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "wrt54g.h"

static unsigned int ctrl_reg;

int pfd;
int instruction_length;
int issue_reset      = 1;
int issue_break      = 1;
int issue_erase      = 1;
int issue_timestamp  = 1;
int selected_fc      = 0;

char            flash_part[128];
unsigned int    flash_size = 0;

int             block_total = 0;
unsigned int    block_addr = 0;
unsigned int    blocks[1024];
unsigned int    cmd_type = 0;

char            REGION_NAME[128];
unsigned int    REGION_START;
unsigned int    REGION_LENGTH;
unsigned int    FLASH_MEMORY_START;
unsigned int    vendid;
unsigned int    devid;


typedef struct _flash_chip_type {
    unsigned int        vendid;         // Manufacturer Id
    unsigned int        devid;          // Device Id
    unsigned int        flash_size;     // Total size in MBytes
    unsigned int        cmd_type;       // Device CMD TYPE
    char*               flash_part;     // Flash Chip Description
    unsigned int        region1_num;    // Region 1 block count
    unsigned int        region1_size;   // Region 1 block size
    unsigned int        region2_num;    // Region 2 block count
    unsigned int        region2_size;   // Region 2 block size
    unsigned int        region3_num;    // Region 3 block count
    unsigned int        region3_size;   // Region 3 block size
    unsigned int        region4_num;    // Region 4 block count
    unsigned int        region4_size;   // Region 4 block size
} flash_chip_type;


flash_chip_type  flash_chip_list[] = {
   { 0x0001, 0x2249, size2MB, CMD_TYPE_AMD, "AMD 29lv160DB 1Mx16 BotB   (2MB)"   ,1,size32K,    2,size8K,     1,size16K,  31,size64K },
   { 0x0001, 0x22c4, size2MB, CMD_TYPE_AMD, "AMD 29lv160DT 1Mx16 TopB   (2MB)"   ,31,size64K,   1,size16K,    2,size8K,   1,size32K  },
   { 0x0001, 0x22f9, size4MB, CMD_TYPE_AMD, "AMD 29lv320DB 2Mx16 BotB   (4MB)"   ,8,size8K,     63,size64K,   0,0,        0,0        },
   { 0x0001, 0x22f6, size4MB, CMD_TYPE_AMD, "AMD 29lv320DT 2Mx16 TopB   (4MB)"   ,63,size64K,   8,size8K,     0,0,        0,0        },
   { 0x0001, 0x2200, size4MB, CMD_TYPE_AMD, "AMD 29lv320MB 2Mx16 BotB   (4MB)"   ,8,size8K,     63,size64K,   0,0,        0,0        },
   { 0x0001, 0x227E, size4MB, CMD_TYPE_AMD, "AMD 29lv320MT 2Mx16 TopB   (4MB)"   ,63,size64K,   8,size8K,     0,0,        0,0        },
   { 0x0001, 0x2201, size4MB, CMD_TYPE_AMD, "AMD 29lv320MT 2Mx16 TopB   (4MB)"   ,63,size64K,   8,size8K,     0,0,        0,0        },
   { 0x0089, 0x0018,size16MB, CMD_TYPE_SCS, "Intel 28F128J3 8Mx16       (16MB)"  ,128,size128K, 0,0,          0,0,        0,0        },
   { 0x0089, 0x8891, size2MB, CMD_TYPE_BSC, "Intel 28F160B3 1Mx16 BotB  (2MB)"   ,8,size8K,     31,size64K,   0,0,        0,0        },
   { 0x0089, 0x8890, size2MB, CMD_TYPE_BSC, "Intel 28F160B3 1Mx16 TopB  (2MB)"   ,31,size64K,   8,size8K,     0,0,        0,0        },
   { 0x0089, 0x88C3, size2MB, CMD_TYPE_BSC, "Intel 28F160C3 1Mx16 BotB  (2MB)"   ,8,size8K,     31,size64K,   0,0,        0,0        },
   { 0x0089, 0x88C2, size2MB, CMD_TYPE_BSC, "Intel 28F160C3 1Mx16 TopB  (2MB)"   ,31,size64K,   8,size8K,     0,0,        0,0        },
   { 0x00b0, 0x00d0, size2MB, CMD_TYPE_SCS, "Intel 28F160S3/5 1Mx16     (2MB)"   ,32,size64K,   0,0,          0,0,        0,0        },
   { 0x0089, 0x8897, size4MB, CMD_TYPE_BSC, "Intel 28F320B3 2Mx16 BotB  (4MB)"   ,8,size8K,     63,size64K,   0,0,        0,0        },
   { 0x0089, 0x8896, size4MB, CMD_TYPE_BSC, "Intel 28F320B3 2Mx16 TopB  (4MB)"   ,63,size64K,   8,size8K,     0,0,        0,0        },
   { 0x0089, 0x88C5, size4MB, CMD_TYPE_BSC, "Intel 28F320C3 2Mx16 BotB  (4MB)"   ,8,size8K,     63,size64K,   0,0,        0,0        },
   { 0x0089, 0x88C4, size4MB, CMD_TYPE_BSC, "Intel 28F320C3 2Mx16 TopB  (4MB)"   ,63,size64K,   8,size8K,     0,0,        0,0        },
   { 0x0089, 0x0016, size4MB, CMD_TYPE_SCS, "Intel 28F320J3 2Mx16       (4MB)"   ,32,size128K,  0,0,          0,0,        0,0        },
   { 0x0089, 0x0014, size4MB, CMD_TYPE_SCS, "Intel 28F320J5 2Mx16       (4MB)"   ,32,size128K,  0,0,          0,0,        0,0        },
   { 0x00b0, 0x00d4, size4MB, CMD_TYPE_SCS, "Intel 28F320S3/5 2Mx16     (4MB)"   ,64,size64K,   0,0,          0,0,        0,0        },
   { 0x0089, 0x8899, size8MB, CMD_TYPE_BSC, "Intel 28F640B3 4Mx16 BotB  (8MB)"   ,8,size8K,     127,size64K,  0,0,        0,0        },
   { 0x0089, 0x8898, size8MB, CMD_TYPE_BSC, "Intel 28F640B3 4Mx16 TopB  (8MB)"   ,127,size64K,  8,size8K,     0,0,        0,0        },
   { 0x0089, 0x88CD, size8MB, CMD_TYPE_BSC, "Intel 28F640C3 4Mx16 BotB  (8MB)"   ,8,size8K,     127,size64K,  0,0,        0,0        },
   { 0x0089, 0x88CC, size8MB, CMD_TYPE_BSC, "Intel 28F640C3 4Mx16 TopB  (8MB)"   ,127,size64K,  8,size8K,     0,0,        0,0        },
   { 0x0089, 0x0017, size8MB, CMD_TYPE_SCS, "Intel 28F640J3 4Mx16       (8MB)"   ,64,size128K,  0,0,          0,0,        0,0        },
   { 0x0089, 0x0015, size8MB, CMD_TYPE_SCS, "Intel 28F640J5 4Mx16       (8MB)"   ,64,size128K,  0,0,          0,0,        0,0        },
   { 0x0004, 0x22F9, size4MB, CMD_TYPE_AMD, "MBM29LV320BE 2Mx16 BotB    (4MB)"   ,1,size16K,    2,size8K,     1,size32K,  63,size64K },
   { 0x0004, 0x22F6, size4MB, CMD_TYPE_AMD, "MBM29LV320TE 2Mx16 TopB    (4MB)"   ,63,size64K,   1,size32K,    2,size8K,   1,size16K  },
   { 0x00C2, 0x22A8, size4MB, CMD_TYPE_AMD, "MX29LV320B 2Mx16 BotB      (4MB)"   ,1,size16K,    2,size8K,     1,size32K,  63,size64K },
   { 0x00C2, 0x00A8, size4MB, CMD_TYPE_AMD, "MX29LV320B 2Mx16 BotB      (4MB)"   ,1,size16K,    2,size8K,     1,size32K,  63,size64K },
   { 0x00C2, 0x00A7, size4MB, CMD_TYPE_AMD, "MX29LV320T 2Mx16 TopB      (4MB)"   ,63,size64K,   1,size32K,    2,size8K,   1,size16K  },
   { 0x00C2, 0x22A7, size4MB, CMD_TYPE_AMD, "MX29LV320T 2Mx16 TopB      (4MB)"   ,63,size64K,   1,size32K,    2,size8K,   1,size16K  },
   { 0x00BF, 0x2783, size4MB, CMD_TYPE_SST, "SST39VF320 2Mx16           (4MB)"   ,64,size64K,   0,0,          0,0,        0,0        },
   { 0x0020, 0x22CB, size4MB, CMD_TYPE_AMD, "ST 29w320DB 2Mx16 BotB     (4MB)"   ,1,size16K,    2,size8K,     1,size32K,  63,size64K },
   { 0x0020, 0x22CA, size4MB, CMD_TYPE_AMD, "ST 29w320DT 2Mx16 TopB     (4MB)"   ,63,size64K,   1,size32K,    2,size8K,   1,size16K  },
   { 0x00b0, 0x00e3, size4MB, CMD_TYPE_BSC, "Sharp 28F320BJE 2Mx16 BotB (4MB)"   ,8,size8K,     63,size64K,   0,0,        0,0        },
   { 0x0098, 0x009C, size4MB, CMD_TYPE_AMD, "TC58FVB321 2Mx16 BotB      (4MB)"   ,1,size16K,    2,size8K,     1,size32K,  63,size64K },
   { 0x0098, 0x009A, size4MB, CMD_TYPE_AMD, "TC58FVT321 2Mx16 TopB      (4MB)"   ,63,size64K,   1,size32K,    2,size8K,   1,size16K  },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
   };



void test_reset(void)
{
    clockin(1, 0);
    clockin(1, 0);
    clockin(1, 0);
    clockin(1, 0);
    clockin(1, 0);

    clockin(0,0); // enter run-test/idle state
}


void capture_dr(void)
{
    clockin(1, 0);  // Select DR scan
    clockin(0, 0);  // Capture DR
}


void capture_ir(void)
{
    clockin(1, 0);  // Select DR scan
    clockin(1, 0);  // Select IR scan
    clockin(0, 0);  // Capture IR
}


void set_instr(int instr)
{
    int i;
    static int curinstr = 0xFFFFFFFF;

    if (instr == curinstr)
        return;

    capture_ir();

    clockin(0,0);
    for (i=0; i < instruction_length; i++)
    {
        clockin(i==(instruction_length - 1), (instr>>i)&1);
    }

    clockin(1, 0);  // Update IR
    clockin(0, 0);  // Run-Test/Idle

    curinstr = instr;
}


// -----------------------------------------
// ---- Start of Compiler Specific Code ----
// -----------------------------------------


void lpt_openport(void)
{
   #ifdef WINDOWS_VERSION    // ---- Compiler Specific Code ----

      HANDLE h;

      h = CreateFile("\\\\.\\giveio", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      if(h == INVALID_HANDLE_VALUE) {
         printf("Couldn't access giveio device\n");
         CloseHandle(h);
         exit(0);
      }
      CloseHandle(h);

   #else                     // ---- Compiler Specific Code ----

      #ifdef __FreeBSD__     // ---- Compiler Specific Code ----

         pfd = open("/dev/ppi0", O_RDWR);
         if (pfd < 0)
         {   perror("Failed to open /dev/ppi0");   exit(0);   }

         if ((ioctl(pfd, PPEXCL) < 0) || (ioctl(pfd, PPCLAIM) < 0))
         {   perror("Failed to lock /dev/ppi0");   close(pfd);   exit(0);   }

      #else                  // ---- Compiler Specific Code ----

         pfd = open("/dev/parport0", O_RDWR);
         if (pfd < 0)
         {   perror("Failed to open /dev/parport0");   exit(0);   }

         if ((ioctl(pfd, PPEXCL) < 0) || (ioctl(pfd, PPCLAIM) < 0))
         {   perror("Failed to lock /dev/parport0");   close(pfd);   exit(0);   }

      #endif

   #endif
}


void lpt_closeport(void)
{
   #ifndef WINDOWS_VERSION   // ---- Compiler Specific Code ----

      #ifndef __FreeBSD__    // ---- Compiler Specific Code ----

         if (ioctl(pfd, PPRELEASE) < 0)  {  perror("Failed to release /dev/parport0");  close(pfd);  exit(0);  }

      #endif

      close(pfd);

   #endif
}


void clockin(int tms, int tdi)
{
   unsigned char data;

   tms = tms ? 1 : 0;
   tdi = tdi ? 1 : 0;

   #ifdef WINDOWS_VERSION   // ---- Compiler Specific Code ----

      data = (1 << TDO) | (0 << TCK) | (tms << TMS) | (tdi << TDI);
      _outp(0x378, data);

      data = (1 << TDO) | (1 << TCK) | (tms << TMS) | (tdi << TDI);
      _outp(0x378, data);

      data = (1 << TDO) | (0 << TCK) | (tms << TMS) | (tdi << TDI);
      _outp(0x378, data);

   #else   // ---- Compiler Specific Code ----

      data = (1 << TDO) | (0 << TCK) | (tms << TMS) | (tdi << TDI);
      if (ioctl(pfd, PPWDATA, &data) < 0)   {  perror("Failed to write parport data");  lpt_closeport();  exit(0);  }

      data = (1 << TDO) | (1 << TCK) | (tms << TMS) | (tdi << TDI);
      if (ioctl(pfd, PPWDATA, &data) < 0)   {  perror("Failed to write parport data");  lpt_closeport();  exit(0);  }

      data = (1 << TDO) | (0 << TCK) | (tms << TMS) | (tdi << TDI);
      if (ioctl(pfd, PPWDATA, &data) < 0)   {  perror("Failed to write parport data");  lpt_closeport();  exit(0);  }

   #endif
}


static unsigned char clockout(void)
{
   unsigned char data;

   #ifdef WINDOWS_VERSION   // ---- Compiler Specific Code ----

      data = (1 << TDO) | (0 << TCK);        // Data input on the LEADING EDGE of clock so we can do this!
      _outp(0x378, data);
      data = (unsigned char)_inp(0x379);

   #else   // ---- Compiler Specific Code ----

      data = (1 << TDO) | (0 << TCK);        // Data input on the LEADING EDGE of clock so we can do this!
      if (ioctl(pfd, PPWDATA, &data) < 0)    {  perror("Failed to write parport data");  lpt_closeport();  exit(0);  }
      if (ioctl(pfd, PPRSTATUS, &data) < 0)  {  perror("Failed to read  parport data");  lpt_closeport();  exit(0);  }

   #endif

   data ^= 0x80;
   data >>= TDO;
   data &= 1;

   return data;
}


// ---------------------------------------
// ---- End of Compiler Specific Code ----
// ---------------------------------------


static unsigned int ReadWriteData(unsigned int in_data)
{
    int i;
    unsigned int out_data;

    out_data = 0;
    capture_dr();
    for(i = 0 ; i < 32 ; i++)
    {
        clockin((i == 31), ((in_data >> i) & 1));
        out_data = out_data | (clockout() << i);
    }
    clockin(1,0);           // enter update DR state
    clockin(0,0);           // enter run-test/idle state
    return out_data;
}


static unsigned int ReadData(void)
{
    int i;
    unsigned int out_data;

    out_data = 0;
    capture_dr();
    clockin(0, 0);  // Shift DR
    for(i = 0 ; i < 32 ; i++)
    {
        out_data = out_data | (clockout() << i);
        clockin((i == 31), 0);
    }
    clockin(1,0);   // enter update DR state
    clockin(0,0);   // enter run-test/idle state
    return out_data;
}


void WriteData(unsigned int in_data)
{
    int i;

    capture_dr();
    clockin(0,0);           // enter shift state
    for(i = 0 ; i < 32 ; i++)
    {
        clockin((i == 31), ((in_data >> i) & 1) );
    }
    clockin(1,0);           // enter update DR state
    clockin(0,0);           // enter run-test/idle state
}


void ShowData(unsigned int value)
{
    int i;
    for (i=0; i<32; i++)
        printf("%d", (value >> (31-i)) & 1);
    printf(" (%08X)\n", value);
}


void ejtag_issue_reset(void)
{
    printf("Issuing Processor / Peripheral Reset...\n");

    set_instr(INSTR_CONTROL);
    ctrl_reg |= PRRST|PERRST;
    ctrl_reg = ReadWriteData(ctrl_reg);
    ctrl_reg &= ~(PRRST|PERRST);
    ctrl_reg = ReadWriteData(ctrl_reg);

    printf("Done\n\n");
}


void ejtag_jtagbrk(void)
{
    printf("Issuing JTAGBRK...\n");

    do {
       set_instr(INSTR_CONTROL);
       // Start ctrl_reg so that w0's are 1 and w1's are 0
       ctrl_reg &= 0x00FFFFFF;
       ctrl_reg &= ~(TIF|SYNC|DSTRT|PRRST|PERRST);
       ctrl_reg |= PROBEN|JTAGBRK|PRACC;
       ctrl_reg = ReadWriteData(ctrl_reg);
    } while ((ctrl_reg & BRKST) != BRKST);

    printf("Done\n\n");
}


static unsigned int ejtag_dma_read(unsigned int addr)
{
    unsigned int data;
    int retries = RETRY_ATTEMPTS;

begin_ejtag_dma_read:

    // Setup Address
    set_instr(INSTR_ADDRESS);
    WriteData(addr);

    // Initiate DMA Read & set DSTRT
    set_instr(INSTR_CONTROL);
    ctrl_reg &= 0x00FFFE7F&~(DLOCK|TIF|SYNC|PRRST|PERRST|JTAGBRK);
    ctrl_reg |= PROBEN|DMAACC|DSTRT|DMA_WORD|DRWN|TOF|PRACC;
    ctrl_reg = ReadWriteData(ctrl_reg);

    // Just clear the DSTRT bit since it is a slow interface
    ctrl_reg &= ~DSTRT;

    // Read Data
    set_instr(INSTR_DATA);
    data = ReadData();

    // Clear DMA & Check DERR
    set_instr(INSTR_CONTROL);
    ctrl_reg &= 0x00FFFE7F&~(DLOCK|TIF|SYNC|DMAACC|DSTRT|PRRST|PERRST|JTAGBRK);
    ctrl_reg |= PROBEN|PRACC|DMA_WORD;
    ctrl_reg = ReadWriteData(ctrl_reg);
    if (ctrl_reg & DERR)
    {
        if (retries--)  goto begin_ejtag_dma_read;
        else  printf("DMA Read Addr = %08x  Data = (%08x)ERROR ON READ\n", addr, data);
    }
    ctrl_reg &= 0x00FFFE7F&~(DLOCK|TIF|SYNC|DMAACC|DRWN|DSTRT|PRRST|PERRST|JTAGBRK);

    return(data);
}


static unsigned int ejtag_dma_read_h(unsigned int addr)
{
    unsigned int data;
    int retries = RETRY_ATTEMPTS;

begin_ejtag_dma_read_h:

    // Setup Address
    set_instr(INSTR_ADDRESS);
    WriteData(addr);

    // Initiate DMA Read & set DSTRT
    set_instr(INSTR_CONTROL);
    ctrl_reg &= 0x00FFFE7F&~(DLOCK|TIF|SYNC|PRRST|PERRST|JTAGBRK);
    ctrl_reg |= PROBEN|DMAACC|DSTRT|DMA_HALFWORD|DRWN|TOF|PRACC;
    ctrl_reg = ReadWriteData(ctrl_reg);

    // Just clear the DSTRT bit since it is a slow interface
    ctrl_reg &= ~DSTRT;

    // Read Data
    set_instr(INSTR_DATA);
    data = ReadData();

    // Clear DMA & Check DERR
    set_instr(INSTR_CONTROL);
    ctrl_reg &= 0x00FFFE7F&~(DLOCK|TIF|SYNC|DMAACC|DSTRT|PRRST|PERRST|JTAGBRK);
    ctrl_reg |= PROBEN|PRACC|DMA_WORD;
    ctrl_reg = ReadWriteData(ctrl_reg);
    if (ctrl_reg & DERR)
    {
        if (retries--)  goto begin_ejtag_dma_read_h;
        else  printf("DMA Read Addr = %08x  Data = (%08x)ERROR ON READ\n", addr, data);
    }
    ctrl_reg &= 0x00FFFE7F&~(DLOCK|TIF|SYNC|DMAACC|DRWN|DSTRT|PRRST|PERRST|JTAGBRK);

    // Handle the bigendian/littleendian
    if ( addr & 0x2 )
       data = (data>>16)&0xffff ;
    else
       data = (data&0x0000ffff) ;

    return(data);
}


void ejtag_dma_write(unsigned int addr, unsigned int data)
{
    int   retries = RETRY_ATTEMPTS;

begin_ejtag_dma_write:

    // Setup Address
    set_instr(INSTR_ADDRESS);
    WriteData(addr);

    // Setup Data
    set_instr(INSTR_DATA);
    WriteData(data);

    // Initiate DMA Write & set DSTRT
    set_instr(INSTR_CONTROL);
    ctrl_reg &= 0x00FFFE7F&~(DRWN|DLOCK|TIF|SYNC|PRRST|PERRST|JTAGBRK);
    ctrl_reg |= PROBEN|DMAACC|DSTRT|DMA_WORD|TOF|PRACC;
    ctrl_reg = ReadWriteData(ctrl_reg);

    // Just clear the DSTRT bit since it is a slow interface
    ctrl_reg &= ~DSTRT;

    // Clear DMA & Check DERR
    set_instr(INSTR_CONTROL);
    ctrl_reg &= 0x00FFFE7F&~(DLOCK|TIF|SYNC|DMAACC|DSTRT|PRRST|PERRST|JTAGBRK);
    ctrl_reg |= PROBEN|PRACC|DMA_WORD ;
    ctrl_reg = ReadWriteData(ctrl_reg);
    if (ctrl_reg & DERR)
    {
        if (retries--)  goto begin_ejtag_dma_write;
        else  printf("DMA Write Addr = %08x  Data = ERROR ON WRITE\n", addr);
    }
    ctrl_reg &= 0x00FFFE7F&~(DLOCK|TIF|SYNC|DMAACC|DRWN|DSTRT|PRRST|PERRST|JTAGBRK);
}


void ejtag_dma_write_h(unsigned int addr, unsigned int data)
{
    int   retries = RETRY_ATTEMPTS;

begin_ejtag_dma_write_h:

    // Setup Address
    set_instr(INSTR_ADDRESS);
    WriteData(addr);

    // Setup Data
    set_instr(INSTR_DATA);
    WriteData(data);

    // Initiate DMA Write & set DSTRT
    set_instr(INSTR_CONTROL);
    ctrl_reg &= 0x00FFFE7F&~(DRWN|DLOCK|TIF|SYNC|PRRST|PERRST|JTAGBRK);
    ctrl_reg |= PROBEN|DMAACC|DSTRT|DMA_HALFWORD|TOF|PRACC;
    ctrl_reg = ReadWriteData(ctrl_reg);

    // Just clear the DSTRT bit since it is a slow interface
    ctrl_reg &= ~DSTRT;

    // Clear DMA & Check DERR
    set_instr(INSTR_CONTROL);
    ctrl_reg &= 0x00FFFE7F&~(DLOCK|TIF|SYNC|DMAACC|DSTRT|PRRST|PERRST|JTAGBRK);
    ctrl_reg |= PROBEN|PRACC|DMA_HALFWORD;
    ctrl_reg = ReadWriteData(ctrl_reg);
    if (ctrl_reg & DERR)
    {
        if (retries--)  goto begin_ejtag_dma_write_h;
        else  printf("DMA Write Addr = %08x  Data = ERROR ON WRITE\n", addr);
    }
    ctrl_reg &= 0x00FFFE7F&~(DLOCK|TIF|SYNC|DMAACC|DRWN|DSTRT|PRRST|PERRST|JTAGBRK);
}


void chip_detect(void)
{
    unsigned int id;

    printf("\nProbing bus...\n\n");

    lpt_openport();

    printf("CHIP ID: ");

    // First, Attempt Detect of BCM4702 Chip
    test_reset();
    instruction_length = 5;
    set_instr(INSTR_IDCODE);
    id = ReadData();
    if (id == 0x0471017F)  // ID String: (Hi) 00000100011100010000000101111111 (Lo)
    {
        ShowData(id);  printf("*** Found a Broadcom BCM4702 Rev 1 chip ***\n\n");
        return;
    }

    // If Not Found, Then Attempt Detect of BCM4712 Chip (Rev 1)
    test_reset();
    instruction_length = 8;
    set_instr(INSTR_IDCODE);
    id = ReadData();
    if (id == 0x1471217F)  // ID String: (Hi) 00010100011100010010000101111111 (Lo)
    {
        ShowData(id);  printf("*** Found a Broadcom BCM4712 Rev 1 chip ***\n\n");
        return;
    }

    // If Not Found, Then Attempt Detect of BCM4712 Chip (Rev 2)
    test_reset();
    instruction_length = 8;
    set_instr(INSTR_IDCODE);
    id = ReadData();
    if (id == 0x2471217F)  // ID String: (Hi) 00100100011100010010000101111111 (Lo)
    {
        ShowData(id);  printf("*** Found a Broadcom BCM4712 Rev 2 chip ***\n\n");
        return;
    }

    // If Not Found, Then Attempt Detect of BCM5352 Chip
    test_reset();
    instruction_length = 8;
    set_instr(INSTR_IDCODE);
    id = ReadData();
    if (id == 0x0535217F)  // ID String: (Hi) 00000101001101010010000101111111 (Lo)
    {
        ShowData(id);  printf("*** Found a Broadcom BCM5352 Rev 1 chip ***\n\n");
        return;
    }

    ShowData(id);  printf("*** Unrecognized Chip ***\n");

    printf("*** This is not a Broadcom BCM47XX chip ***\n");
    printf("*** Possible Causes:\n");
    printf("    1) WRT54G/GS is not Connected.\n");
    printf("    2) WRT54G/GS is not Powered On.\n");
    printf("    3) Improper JTAG Cable.\n");
    printf("    4) Unrecognized Chip Version of WRT54G/GS.\n");

    chip_shutdown();;
    exit(0);
}


void chip_shutdown(void)
{
    fflush(stdout);
    test_reset();
    lpt_closeport();
}


void run_backup(char *filename, unsigned int start, unsigned int length)
{
    unsigned int addr, data;
    FILE *fd;
    int counter = 0;
    int percent_complete = 0;
    char newfilename[128] = "";

    time_t t = time(0);
    struct tm* lt = localtime(&t);
    char time_str[15];

    sprintf(time_str, "%04d%02d%02d_%02d%02d%02d",
        lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
        lt->tm_hour, lt->tm_min, lt->tm_sec
    );

    printf("*** You Selected to Backup the %s ***\n\n",filename);

    strcpy(newfilename,filename);
    strcat(newfilename,".SAVED");
    if (issue_timestamp)
    {
       strcat(newfilename,"_");
       strcat(newfilename,time_str);
    }

    fd = fopen(newfilename, "wb" );
    if (fd<=0)
    {
        fprintf(stderr,"Could not open %s for writing\n", newfilename);
        exit(1);
    }

    printf("=========================\n");
    printf("Backup Routine Started\n");
    printf("=========================\n");

    printf("\nSaving %s to Disk...\n",newfilename);
    for(addr=start; addr<(start+length); addr+=4)
    {
        counter += 4;
        percent_complete = (counter * 100 / length);
        if ((addr&0xF) == 0) {
            printf ("[%3d%% Backed Up]   %08x: ", percent_complete, addr);
        }

        data = ejtag_dma_read(addr);
        fwrite( (unsigned char*) &data, 1, sizeof(data), fd);

        printf ("%08x%c", data, (addr&0xF)==0xC?'\n':' ');
      }
    fclose(fd);

    printf("Done  (%s saved to Disk OK)\n\n",newfilename);

    printf("bytes written: %d\n", counter);

    printf("=========================\n");
    printf("Backup Routine Complete\n");
    printf("=========================\n");
}



void run_flash(char *filename, unsigned int start, unsigned int length)
{
    unsigned int addr, data ;
    FILE *fd ;
    int counter = 0;
    int percent_complete = 0;

    printf("*** You Selected to Flash the %s ***\n\n",filename);

    fd=fopen(filename, "rb" );
    if (fd<=0)
    {
        fprintf(stderr,"Could not open %s for reading\n", filename);
        exit(1);
    }

    printf("=========================\n");
    printf("Flashing Routine Started\n");
    printf("=========================\n");

    if (issue_erase) sflash_erase_area(start,length);

    printf("\nLoading %s to Flash Memory...\n",filename);
    for(addr=start; addr<(start+length); addr+=4)
    {
        counter += 4;
        percent_complete = (counter * 100 / length);
        if ((addr&0xF) == 0) {
            printf ("[%3d%% Flashed]   %08x: ", percent_complete, addr);
        }

        fread( (unsigned char*) &data, 1,sizeof(data), fd);
        // Erasing Flash Sets addresses to 0xFF's so we can avoid writing these (for speed)
        if (issue_erase) {
           if (!(data == 0xFFFFFFFF))
              sflash_write_word(addr, data);
        }
        else sflash_write_word(addr, data);  // Otherwise we gotta flash it all

        printf ("%08x%c", data, (addr&0xF)==0xC?'\n':' ');
        data = 0xFFFFFFFF;  // This is in case file is shorter than expected length
      }
    fclose(fd);
    printf("Done  (%s loaded into Flash Memory OK)\n\n",filename);

    sflash_reset();

    printf("=========================\n");
    printf("Flashing Routine Complete\n");
    printf("=========================\n");
}


void run_erase(char *filename, unsigned int start, unsigned int length)
{
    printf("*** You Selected to Erase the %s ***\n\n",filename);

    printf("=========================\n");
    printf("Erasing Routine Started\n");
    printf("=========================\n");

    sflash_erase_area(start,length);
    sflash_reset();

    printf("=========================\n");
    printf("Erasing Routine Complete\n");
    printf("=========================\n");
}


void identify_flash_part(void)
{
   flash_chip_type*   flash_chip;

   // Important for these to initialize to zero
   block_addr  = 0;
   block_total = 0;
   flash_size  = 0;
   cmd_type    = 0;
   strcpy(flash_part,"");

   // Funky AMD Chip
   if (((vendid & 0x00ff) == 0x0001) && (devid == 0x227E))  devid = ejtag_dma_read_h(FLASH_MEMORY_START+0x1E);  // Get real devid

   flash_chip = flash_chip_list;
   while (flash_chip->vendid)
   {
      if ((flash_chip->vendid == vendid) && (flash_chip->devid == devid))
      {
         flash_size = flash_chip->flash_size;
         cmd_type   = flash_chip->cmd_type;
         strcpy(flash_part, flash_chip->flash_part);

         if (flash_chip->region1_num)  define_block(flash_chip->region1_num, flash_chip->region1_size);
         if (flash_chip->region2_num)  define_block(flash_chip->region2_num, flash_chip->region2_size);
         if (flash_chip->region3_num)  define_block(flash_chip->region3_num, flash_chip->region3_size);
         if (flash_chip->region4_num)  define_block(flash_chip->region4_num, flash_chip->region4_size);

         sflash_reset();

         printf("\n");
         printf("FLASH VENDOR ID: ");  ShowData(vendid);
         printf("FLASH DEVICE ID: ");  ShowData(devid);
         if (selected_fc != 0)
            printf("*** MANUALLY SELECTED FLASH CHIP: %s ***\n\n", flash_part);
         else
            printf("*** AUTO-DETECTED FLASH CHIP: %s ***\n", flash_part);
         break;
      }
      flash_chip++;
   }

}


void define_block(unsigned int block_count, unsigned int block_size)
{

  unsigned int  i;

  if (flash_size == size2MB)  FLASH_MEMORY_START = CFE_START_2MB;
  if (flash_size == size4MB)  FLASH_MEMORY_START = CFE_START_4MB;
  if (flash_size == size8MB)  FLASH_MEMORY_START = CFE_START_8MB;

  if (block_addr == 0)  block_addr = FLASH_MEMORY_START;

  for (i = 1; i <= block_count; i++)
  {
     block_total++;
     blocks[block_total] = block_addr;
     block_addr = block_addr + block_size;
  }

}

void sflash_config(void)
{
   flash_chip_type*   flash_chip = flash_chip_list;
   int counter = 0;

   printf("Manual Flash Selection...\n");

   while (flash_chip->vendid)
   {
      counter++;
      if (counter == selected_fc)
      {
         vendid = flash_chip->vendid;
         devid  = flash_chip->devid;
         identify_flash_part();
         break;
      }
      flash_chip++;
   }

    if (strcmp(flash_part,"")==0)
       printf("*** UNKNOWN or NO FLASH CHIP SELECTED ***\n");

}


void sflash_probe(void)
{
   int retries = 300;

    // Default to Standard Flash Window for Detection
    FLASH_MEMORY_START = 0x1FC00000;

    printf("Probing Flash...\n");

again:

    strcpy(flash_part,"");

    // Probe using cmd_type for AMD
    if (strcmp(flash_part,"")==0)
    {
       cmd_type = CMD_TYPE_AMD;
       sflash_reset();
       ejtag_dma_write_h(FLASH_MEMORY_START + (0x555 << 1), 0x00AA00AA);
       ejtag_dma_write_h(FLASH_MEMORY_START + (0x2AA << 1), 0x00550055);
       ejtag_dma_write_h(FLASH_MEMORY_START + (0x555 << 1), 0x00900090);
       vendid = ejtag_dma_read_h(FLASH_MEMORY_START);
       devid  = ejtag_dma_read_h(FLASH_MEMORY_START+2);
       identify_flash_part();
    }

    // Probe using cmd_type for SST
    if (strcmp(flash_part,"")==0)
    {
       cmd_type = CMD_TYPE_SST;
       sflash_reset();
       ejtag_dma_write_h(FLASH_MEMORY_START + (0x5555 << 1), 0x00AA00AA);
       ejtag_dma_write_h(FLASH_MEMORY_START + (0x2AAA << 1), 0x00550055);
       ejtag_dma_write_h(FLASH_MEMORY_START + (0x5555 << 1), 0x00900090);
       vendid = ejtag_dma_read_h(FLASH_MEMORY_START);
       devid  = ejtag_dma_read_h(FLASH_MEMORY_START+2);
       identify_flash_part();
    }

    // Probe using cmd_type for BSC & SCS
    if (strcmp(flash_part,"")==0)
    {
       cmd_type = CMD_TYPE_BSC;
       sflash_reset();
       ejtag_dma_write_h(FLASH_MEMORY_START, 0x00900090);
       vendid = ejtag_dma_read_h(FLASH_MEMORY_START);
       devid  = ejtag_dma_read_h(FLASH_MEMORY_START+2);
       identify_flash_part();
    }

    if (strcmp(flash_part,"")==0)
    {
       if (retries--)
          goto again;
       else
          printf("*** UNKNOWN or NO FLASH CHIP DETECTED ***");

       return;
    }

    printf("\n\n");
    return;
}


void sflash_erase_area(unsigned int start, unsigned int length)
{
    int cur_block;
    int tot_blocks;
    unsigned int reg_start;
    unsigned int reg_end;


    reg_start = start;
    reg_end   = reg_start + length;

    tot_blocks = 0;

    for (cur_block = 1;  cur_block <= block_total;  cur_block++)
    {
       block_addr = blocks[cur_block];
       if ((block_addr >= reg_start) && (block_addr < reg_end))  tot_blocks++;
    }

    printf("Total Blocks to Erase: %d\n\n", tot_blocks);

    for (cur_block = 1;  cur_block <= block_total;  cur_block++)
    {
       block_addr = blocks[cur_block];
       if ((block_addr >= reg_start) && (block_addr < reg_end))
          {
             printf("Erasing block: %d (addr = %08x)...", cur_block, block_addr);
             sflash_erase_block(block_addr);
             printf("Done\n\n");
          }
    }

}


void sflash_erase_block(unsigned int addr)
{

    if (cmd_type == CMD_TYPE_AMD)
    {

        //Unlock Block
        ejtag_dma_write_h(FLASH_MEMORY_START+(0x555 << 1), 0x00AA00AA);
        ejtag_dma_write_h(FLASH_MEMORY_START+(0x2AA << 1), 0x00550055);
        ejtag_dma_write_h(FLASH_MEMORY_START+(0x555 << 1), 0x00800080);

        //Erase Block
        ejtag_dma_write_h(FLASH_MEMORY_START+(0x555 << 1), 0x00AA00AA);
        ejtag_dma_write_h(FLASH_MEMORY_START+(0x2AA << 1), 0x00550055);
        ejtag_dma_write_h(addr, 0x00300030);

        while (ejtag_dma_read_h(addr) != 0xFFFF) {}

    }

    if (cmd_type == CMD_TYPE_SST)
    {

        //Unlock Block
        ejtag_dma_write_h(FLASH_MEMORY_START+(0x5555 << 1), 0x00AA00AA);
        ejtag_dma_write_h(FLASH_MEMORY_START+(0x2AAA << 1), 0x00550055);
        ejtag_dma_write_h(FLASH_MEMORY_START+(0x5555 << 1), 0x00800080);

        //Erase Block
        ejtag_dma_write_h(FLASH_MEMORY_START+(0x5555 << 1), 0x00AA00AA);
        ejtag_dma_write_h(FLASH_MEMORY_START+(0x2AAA << 1), 0x00550055);
        ejtag_dma_write_h(addr, 0x00500050);

        while (ejtag_dma_read_h(addr) != 0xFFFF) {}

    }

    if ((cmd_type == CMD_TYPE_BSC) || (cmd_type == CMD_TYPE_SCS))
    {

        //Unlock Block
        ejtag_dma_write_h(addr, 0x00600060);     // Unlock Flash Block Command
        ejtag_dma_write_h(addr, 0x00D000D0);     // Confirm Command

        //Erase Block
        ejtag_dma_write_h(addr, 0x0020);         // Block Erase Command
        ejtag_dma_write_h(addr, 0x00D0);         // Confirm Command

        while (ejtag_dma_read_h(FLASH_MEMORY_START) != 0x0080) {}

    }

    sflash_reset();

}


void sflash_reset(void)
{

    if ((cmd_type == CMD_TYPE_AMD) || (cmd_type == CMD_TYPE_SST))
    {
        ejtag_dma_write_h(FLASH_MEMORY_START, 0x00F000F0);    // Set array to read mode
    }

    if ((cmd_type == CMD_TYPE_BSC) || (cmd_type == CMD_TYPE_SCS))
    {
        ejtag_dma_write_h(FLASH_MEMORY_START, 0x00500050);    // Clear CSR
        ejtag_dma_write_h(FLASH_MEMORY_START, 0x00ff00ff);    // Set array to read mode
    }

}


void sflash_write_word(unsigned int addr, unsigned int data)
{

    if (cmd_type == CMD_TYPE_AMD)
    {

      // Handle Half Of Word
      ejtag_dma_write_h(FLASH_MEMORY_START+(0x555 << 1), 0x00AA00AA);
      ejtag_dma_write_h(FLASH_MEMORY_START+(0x2AA << 1), 0x00550055);
      ejtag_dma_write_h(FLASH_MEMORY_START+(0x555 << 1), 0x00A000A0);
      ejtag_dma_write_h(addr, data);

      while (ejtag_dma_read_h(addr) != (data & 0xFFFF)) {}

      // Now Handle Other Half Of Word
      ejtag_dma_write_h(FLASH_MEMORY_START+(0x555 << 1), 0x00AA00AA);
      ejtag_dma_write_h(FLASH_MEMORY_START+(0x2AA << 1), 0x00550055);
      ejtag_dma_write_h(FLASH_MEMORY_START+(0x555 << 1), 0x00A000A0);
      ejtag_dma_write_h(addr+2, data);

      while (ejtag_dma_read_h(addr+2) != ((data >> 16) & 0xFFFF)) {}

    }

    if (cmd_type == CMD_TYPE_SST)
    {

      // Handle Half Of Word
      ejtag_dma_write_h(FLASH_MEMORY_START+(0x5555 << 1), 0x00AA00AA);
      ejtag_dma_write_h(FLASH_MEMORY_START+(0x2AAA << 1), 0x00550055);
      ejtag_dma_write_h(FLASH_MEMORY_START+(0x5555 << 1), 0x00A000A0);
      ejtag_dma_write_h(addr, data);

      while (ejtag_dma_read_h(addr) != (data & 0xFFFF)) {}

      // Now Handle Other Half Of Word
      ejtag_dma_write_h(FLASH_MEMORY_START+(0x5555 << 1), 0x00AA00AA);
      ejtag_dma_write_h(FLASH_MEMORY_START+(0x2AAA << 1), 0x00550055);
      ejtag_dma_write_h(FLASH_MEMORY_START+(0x5555 << 1), 0x00A000A0);
      ejtag_dma_write_h(addr+2, data);

      while (ejtag_dma_read_h(addr+2) != ((data >> 16) & 0xFFFF)) {}

    }

    if ((cmd_type == CMD_TYPE_BSC) || (cmd_type == CMD_TYPE_SCS))
    {

        // Handle Half Of Word
        ejtag_dma_write_h(addr, 0x00400040);           // Write Command
        ejtag_dma_write_h(addr, data);                 // Send HalfWord Data

        while (ejtag_dma_read_h(FLASH_MEMORY_START) != 0x0080) {}

        // Now Handle Other Half Of Word
        ejtag_dma_write_h(addr+2, 0x00400040);         // Write Command
        ejtag_dma_write_h(addr+2, data);               // Send HalfWord Data

        while (ejtag_dma_read_h(FLASH_MEMORY_START) != 0x0080) {}

    }

}


void show_usage(void)
{

   flash_chip_type*   flash_chip;
   int counter = 0;

   flash_chip = flash_chip_list;

   printf( " USAGE: wrt54g [option] </noreset> </nobreak> </noerase> </notimestamp> </fc:XX>\n\n"
           "            -backup:cfe\n"
           "            -backup:nvram\n"
           "            -backup:kernel\n"
           "            -backup:wholeflash\n"
           "            -erase:cfe\n"
           "            -erase:nvram\n"
           "            -erase:kernel\n"
           "            -erase:wholeflash\n"
           "            -flash:cfe\n"
           "            -flash:nvram\n"
           "            -flash:kernel\n"
           "            -flash:wholeflash\n\n"
           "            /noreset     (prevents CPU reset of Broadcom Chip ..... optional)\n"
           "            /nobreak     (prevents issuing Debug Mode JTAGBRK ..... optional)\n"
           "            /noerase     (prevents forced erase before flashing ... optional)\n"
           "            /notimestamp (prevents timestamping of backups ........ optional)\n"
           "            /fc:XX       (specify XX flash chip manually .......... optional)\n\n");

           while (flash_chip->vendid)
           {
              printf("            /fc:%02d ..... %-39.39s   (optional)\n", ++counter, flash_chip->flash_part);
              flash_chip++;
           }

   printf( "\n\n NOTES: 1) If 'flashing' - the source filename must exist as follows:\n"
           "           CFE.BIN or NVRAM.BIN or KERNEL.BIN or WHOLEFLASH.BIN\n\n"
           "        2) If you have difficulty auto-detecting a particular flash part\n"
           "           you can manually specify your exact part using the /fc:XX option.\n\n"
           "        3) If you have difficulty with the older bcm47xx chips or when no CFE\n"
           "           is currently active/operational you may want to try both the\n"
           "           /noreset and /nobreak command line options together.  Some bcm47xx\n"
           "           chips *may* always require both these switches to function properly.\n\n"
           "        4) When using this utility, usually it is best to type the command line\n"
           "           out, then plug in the router, and then hit <ENTER> quickly to avoid\n"
           "           the bcm47xx's watchdog interfering with the EJTAG operations.\n\n"
           " ***************************************************************************\n"
           " * Flashing the KERNEL or WHOLEFLASH will take a very long time using JTAG *\n"
           " * via this utility.  You are better off flashing the CFE & NVRAM files    *\n"
           " * & then using the normal TFTP method to flash the KERNEL via ethernet.   *\n"
           " ***************************************************************************\n\n");
}


int main(int argc, char** argv)
{
    char choice[128];
    int run_option;
    int i = 0;
    int j;

    printf("\n");
    printf("====================================\n");
    printf("WRT54G/GS EJTAG DeBrick Utility v4.1\n");
    printf("====================================\n\n");

    if (argc < 2)
    {
        printf(" ABOUT: This program reads/writes flash memory on the WRT54G/GS and\n");
        printf("        compatible routers via EJTAG.  Processor chips supported in\n");
        printf("        this version include the BCM4702, BCM4712, and BCM5352 chips.\n\n\n");
        show_usage();
        exit(1);
    }

    strcpy(choice,argv[1]);
    i = 0;
    while (choice[i])
    {
        choice[i] = tolower(choice[i]);
        i++;
    }

    run_option = 0;

    if (strcmp(choice,"-backup:cfe")==0)         run_option = 1;
    if (strcmp(choice,"-backup:nvram")==0)       run_option = 2;
    if (strcmp(choice,"-backup:kernel")==0)      run_option = 3;
    if (strcmp(choice,"-backup:wholeflash")==0)  run_option = 4;

    if (strcmp(choice,"-erase:cfe")==0)          run_option = 5;
    if (strcmp(choice,"-erase:nvram")==0)        run_option = 6;
    if (strcmp(choice,"-erase:kernel")==0)       run_option = 7;
    if (strcmp(choice,"-erase:wholeflash")==0)   run_option = 8;

    if (strcmp(choice,"-flash:cfe")==0)          run_option = 9;
    if (strcmp(choice,"-flash:nvram")==0)        run_option = 10;
    if (strcmp(choice,"-flash:kernel")==0)       run_option = 11;
    if (strcmp(choice,"-flash:wholeflash")==0)   run_option = 12;

    if (run_option == 0)
    {
        show_usage();
        printf("\n*** ERROR - Invalid [option] specified ***\n\n");
        exit(1);
    }

    if (argc > 2)
    {
       j = 2;
       while (j < argc)
       {
          strcpy(choice,argv[j]);
          i = 0;
          while (choice[i])
          {
              choice[i] = tolower(choice[i]);
              i++;
          }
          if (strcmp(choice,"/noreset")==0)
             issue_reset = 0;
          else
          if (strcmp(choice,"/nobreak")==0)
             issue_break = 0;
          else
          if (strcmp(choice,"/noerase")==0)
             issue_erase = 0;
          else
          if (strcmp(choice,"/notimestamp")==0)
             issue_timestamp = 0;
          else
          if (strncmp(choice,"/fc:",4)==0)
             sscanf(choice,"/fc:%d", &selected_fc);
          else
          {
             show_usage();
             printf("\n*** ERROR - Invalid <switch> specified ***\n\n");
             exit(1);
          }
          j++;
       }
    }


    // Detect & Initialize
    chip_detect();

    // For Good Measure
    test_reset();

    // Find Starting "ctrl_reg" Value
    set_instr(INSTR_CONTROL);
    ctrl_reg = ReadData();

    // New Init Sequence
    if (issue_reset)  ejtag_issue_reset();    // Reset Processor and Peripherals
    ejtag_dma_write(0xff300000,0);            // Clear DCR
    ejtag_dma_write(0xb8000080,0);            // Clear Watchdog
    if (issue_break)  ejtag_jtagbrk();        // Put into EJTAG Debug Mode

    // Flash Chip Detection
    if (selected_fc != 0)
       sflash_config();
    else
       sflash_probe();


    if (flash_size == size2MB)
    {
       FLASH_MEMORY_START = CFE_START_2MB;

       if (run_option == 1 )  run_backup("CFE.BIN",        CFE_START_2MB,         CFE_LENGTH_2MB);
       if (run_option == 2 )  run_backup("NVRAM.BIN",      NVRAM_START_2MB,       NVRAM_LENGTH_2MB);
       if (run_option == 3 )  run_backup("KERNEL.BIN",     KERNEL_START_2MB,      KERNEL_LENGTH_2MB);
       if (run_option == 4 )  run_backup("WHOLEFLASH.BIN", WHOLEFLASH_START_2MB,  WHOLEFLASH_LENGTH_2MB);

       if (run_option == 5 )  run_erase("CFE.BIN",         CFE_START_2MB,         CFE_LENGTH_2MB);
       if (run_option == 6 )  run_erase("NVRAM.BIN",       NVRAM_START_2MB,       NVRAM_LENGTH_2MB);
       if (run_option == 7 )  run_erase("KERNEL.BIN",      KERNEL_START_2MB,      KERNEL_LENGTH_2MB);
       if (run_option == 8 )  run_erase("WHOLEFLASH.BIN",  WHOLEFLASH_START_2MB,  WHOLEFLASH_LENGTH_2MB);

       if (run_option == 9 )  run_flash("CFE.BIN",         CFE_START_2MB,         CFE_LENGTH_2MB);
       if (run_option == 10)  run_flash("NVRAM.BIN",       NVRAM_START_2MB,       NVRAM_LENGTH_2MB);
       if (run_option == 11)  run_flash("KERNEL.BIN",      KERNEL_START_2MB,      KERNEL_LENGTH_2MB);
       if (run_option == 12)  run_flash("WHOLEFLASH.BIN",  WHOLEFLASH_START_2MB,  WHOLEFLASH_LENGTH_2MB);
    }

    if (flash_size == size4MB)
    {
       FLASH_MEMORY_START = CFE_START_4MB;

       if (run_option == 1 )  run_backup("CFE.BIN",        CFE_START_4MB,         CFE_LENGTH_4MB);
       if (run_option == 2 )  run_backup("NVRAM.BIN",      NVRAM_START_4MB,       NVRAM_LENGTH_4MB);
       if (run_option == 3 )  run_backup("KERNEL.BIN",     KERNEL_START_4MB,      KERNEL_LENGTH_4MB);
       if (run_option == 4 )  run_backup("WHOLEFLASH.BIN", WHOLEFLASH_START_4MB,  WHOLEFLASH_LENGTH_4MB);

       if (run_option == 5 )  run_erase("CFE.BIN",         CFE_START_4MB,         CFE_LENGTH_4MB);
       if (run_option == 6 )  run_erase("NVRAM.BIN",       NVRAM_START_4MB,       NVRAM_LENGTH_4MB);
       if (run_option == 7 )  run_erase("KERNEL.BIN",      KERNEL_START_4MB,      KERNEL_LENGTH_4MB);
       if (run_option == 8 )  run_erase("WHOLEFLASH.BIN",  WHOLEFLASH_START_4MB,  WHOLEFLASH_LENGTH_4MB);

       if (run_option == 9 )  run_flash("CFE.BIN",         CFE_START_4MB,         CFE_LENGTH_4MB);
       if (run_option == 10)  run_flash("NVRAM.BIN",       NVRAM_START_4MB,       NVRAM_LENGTH_4MB);
       if (run_option == 11)  run_flash("KERNEL.BIN",      KERNEL_START_4MB,      KERNEL_LENGTH_4MB);
       if (run_option == 12)  run_flash("WHOLEFLASH.BIN",  WHOLEFLASH_START_4MB,  WHOLEFLASH_LENGTH_4MB);
    }

    if ((flash_size == size8MB) || (flash_size == size16MB))  // Treat 16MB Flash Chip like it is an 8MB Flash Chip
    {
       FLASH_MEMORY_START = CFE_START_8MB;

       if (run_option == 1 )  run_backup("CFE.BIN",        CFE_START_8MB,         CFE_LENGTH_8MB);
       if (run_option == 2 )  run_backup("NVRAM.BIN",      NVRAM_START_8MB,       NVRAM_LENGTH_8MB);
       if (run_option == 3 )  run_backup("KERNEL.BIN",     KERNEL_START_8MB,      KERNEL_LENGTH_8MB);
       if (run_option == 4 )  run_backup("WHOLEFLASH.BIN", WHOLEFLASH_START_8MB,  WHOLEFLASH_LENGTH_8MB);

       if (run_option == 5 )  run_erase("CFE.BIN",         CFE_START_8MB,         CFE_LENGTH_8MB);
       if (run_option == 6 )  run_erase("NVRAM.BIN",       NVRAM_START_8MB,       NVRAM_LENGTH_8MB);
       if (run_option == 7 )  run_erase("KERNEL.BIN",      KERNEL_START_8MB,      KERNEL_LENGTH_8MB);
       if (run_option == 8 )  run_erase("WHOLEFLASH.BIN",  WHOLEFLASH_START_8MB,  WHOLEFLASH_LENGTH_8MB);

       if (run_option == 9 )  run_flash("CFE.BIN",         CFE_START_8MB,         CFE_LENGTH_8MB);
       if (run_option == 10)  run_flash("NVRAM.BIN",       NVRAM_START_8MB,       NVRAM_LENGTH_8MB);
       if (run_option == 11)  run_flash("KERNEL.BIN",      KERNEL_START_8MB,      KERNEL_LENGTH_8MB);
       if (run_option == 12)  run_flash("WHOLEFLASH.BIN",  WHOLEFLASH_START_8MB,  WHOLEFLASH_LENGTH_8MB);
    }

    printf("\n\n *** REQUESTED OPERATION IS COMPLETE ***\n\n");

    chip_shutdown();

    return 0;
}
