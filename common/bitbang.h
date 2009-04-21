#ifndef DEBRICK_BITBANG_H_
#define DEBRICK_BITBANG_H_


/* --- Xilinx Type Cable --- */
#define TDI		0
#define TCK		1
#define TMS		2
#define TDO		4

/* --- Wiggler Type Cable --- */
#define WTDI		3
#define WTCK		2
#define WTMS		1
#define WTDO		7
#define WTRST_N		4


/* EJTAG Instruction Registers */
#define INSTR_EXTEST    0x00
#define INSTR_IDCODE    0x01
#define INSTR_SAMPLE    0x02
#define INSTR_IMPCODE   0x03
#define INSTR_ADDRESS   0x08
#define INSTR_DATA      0x09
#define INSTR_CONTROL   0x0A
#define INSTR_BYPASS    0xFF

/* EJTAG Bit Masks */
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
#define ROCC            (1 << 31)

#define DMA_BYTE        0x00000000	//DMA tranfser size BYTE
#define DMA_HALFWORD    0x00000080	//DMA transfer size HALFWORD
#define DMA_WORD        0x00000100	//DMA transfer size WORD
#define DMA_TRIPLEBYTE  0x00000180	//DMA transfer size TRIPLEBYTE


struct debrick_bitbang {
	unsigned int instruction_length;
	unsigned int tck_delay;
	int use_wiggler;
	int use_ludicrous_speed;
	int ludicrous_speed_corruption;
	void *parport_priv;

	/* internal */
	unsigned int curinstr;
};

static inline void debrick_bitbang_init(struct debrick_bitbang *b)
{
	memset(b, 0, sizeof(*b));
	b->curinstr = -1;
}


#endif /* DEBRICK_BITBANG_H_ */
