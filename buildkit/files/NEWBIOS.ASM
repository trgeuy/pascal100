;+	TITLE	'Sorcim CompuPro DISK1 CP/M BIOS.'
;CBIOS - Sorcim CompuPro CP/M 2.2 CBIOS.
;+ File GBBIOS.ASM
;
;	+-----------------------+
;	|			|
;	|	C B I O S 	|
;	|			|
;	+-----------------------+
;
;
;	CompuPro			Sorcim Corp.
;	Oakland, CA			Santa Clara, CA
;
;	This product is a copyright program product of
;	Sorcim Corp. and is supplied for use with the CompuPro
;	Disk controllers.
;
;	Version number:	2.2K
; 80-10-27	Ver 2.2A
; 81-03-05	Ver 2.2E	Seek fixes. dwd.
; 81-03-30	Ver 2.2F	Add Bit Banger.  phm.
; 81-04-25	Ver 2.2G	Add System Support, two list devs. bdg.
; 81-05-21	Ver 2.2H	Add flushing fixes. dwd
; 81-09-08	Ver 2.2J	Add Interfacer 3 support routines. bdg.
; 81-10-26	Ver 2.2K	Add list device for interfacer 3. bdg.
;					Fix deblocking in SETTRK
;
;	The following code is supplied to customers who
;	purchase a hard/floppy disk system from CompuPro.
;	The intent of the following code is to illustrate
;	how to create a CBIOS for the user supplied
;+	CP/M 2.2. Lines of code beginning with ";+" were
;+	modified for assembly with Digital Research's ASM
;+	assembler. Syntax changes and comments can be found
;+	near the modified lines.
;+	In order to actually assemble this
;+	source, the Sorcim ACT assembler is required.
;	The following CBIOS was developed and
;	tested using the following hardware components.
;
;	CompuPro IEEE 696 Floppy Disk Controller.
;	CompuPro IEEE 696 chassis and motherboards.
;	CompuPro IEEE 696 Dual Processor board.
;	CompuPro IEEE 696 RAM 20 boards (2).
;	CompuPro IEEE 696 Interfacer I (assigned port 0 thru 3).

;+OPARM:	EQU	*o		;Capture O parameter
OPARM:	EQU	64		;Memory size in Kbytes, or Bios load address

VERS:	EQU	22
CBIOSV:	EQU	11		;CBIOS revision level (2.2K)

FALSE:	EQU	0
TRUE:	EQU	NOT FALSE
Z80:	EQU	TRUE		;Set to true if processor is Z80.
C8080:	EQU	not Z80		;Otherwise processor is 8080 type.
HARD:	EQU	FALSE		;Set to true if hard disk code
BANG:	EQU	FALSE		;False to eliminate Bit Banger.
SYSSUP:	EQU	FALSE		;Set to true to include System Support code.
INTER3:	EQU	FALSE		;Set to true to include Interfacer 3 support

BIOSLN:	EQU	00D00H		;Bios length

;	Link files.

;+	link	bios1		;Jump Table
;+	link	GBcbios2	;CP/M disk definitions
;+	link	GBcbios3	;Unit record I/O
;+	link	bios4		;Non data transfer disk
;+	link	GBcbios5	;Cold and Warm boot
;+	link	GBcbios6	;Disk data transfer I/O
	IF	HARD
;	(Link of hard disk support routines would go here.)
;+	link	gbcbios7.asm	;hard disk I/O routines
	ENDIF
;+	link	bios8		;Utility routines
;+	link	bios9		;CBIOS storage cells

;+ File BIOS1.ASM
;+	page
;BIOS1 - Sorcim CBios setups.

CR:	EQU	0Dh
LF:	EQU	0Ah
DELCNT:	EQU	3*1000		;Delay count for 3 Mhz CPU


K:	EQU	1024
;+	IF	OPARM < (64+2)	;If absolute
option:	set	false
	if	oparm/(64+1)
option:	set	true
	endif
	if 	not option

MSIZE:	EQU	OPARM		;Size of CP/M memory
BIOS:	EQU	MSIZE*K-BIOSLN	;Start of CP/M jump table
LWAMEM:	EQU	MSIZE*K-1
;+	MSG	'Assembling BIOS for LWA of ',LWAMEM,'h.'
;+ Msg function not available with ASM.COM
	ENDIF
;+	IF	OPARM > (64+1)	;If PRL generation
	if	option

MSIZE:	EQU	(OPARM+BIOSLN)/K	;Size of CP/M memory
BIOS:	EQU	OPARM		;Start of CP/M jump table
LWAMEM:	EQU	OPARM+BIOSLN-1
;+	MSG	'Assembling for relocation.'
;+ Msg function not available with ASM.COM
	ENDIF
BDOS:	EQU	BIOS-0E00h+6	;BDOS entry point
CCP:	EQU	BIOS-1600h
;+	space	4,10



;	Page Zero Definitions.
IOBYTE:	EQU	3		;Location of IOBYTE
CDISK:	EQU	4		;Location of current disk
BIORAM:	EQU	40h		;16 ram cells
OPTS:	EQU	BIORAM		;GBC DISK1 board switch options
;		BIORAM+1	;GBC (cell before TICK)
TICK:	EQU	BIORAM+2	;GBC Sample period
DBUF:	EQU	80h		;Default sector buffer
;+	space	4,10




;	CP/M to host disk constants

HSTSIZ:	EQU	1024		;Blocking/Deblocking buffer size
CPMSIB:	EQU	1024/128	;Standard sectors in block
FPYSIB:	EQU	2048/128	;Sectors in floppy disk block

;	CP/M disk control block equates which define the
;	disk types and maximum storage capability of each
;	disk type.

DSKS1:	EQU	0	;Single density, single sided.
DSKS2:	EQU	1	;Single density, double sided.
DSKD1:	EQU	2	;Double density, single sided.
DSKD2:	EQU	3	;Double density, double sided.
DSKD3:	EQU	4	;Double density, single sided
DSKD4:	EQU	5	;Double density, double sided
DSKD5:	EQU	6	;Double density, single sided
DSKD6:	EQU	7	;Double density, double sided
MAXFTP:	EQU	DSKD6	;Maximum floppy type
DSK8S1:	EQU	8	;SA 1002 and first half SA 1004
DSK8S2:	EQU	9	;last half SA 1004.
DSK8M1:	EQU	10	;First half of Memorex 8 inch drive
DSK8M2:	EQU	11	;Last half of Memorex 8 inch drive

S1DSM:	EQU	((77-2)*26)/CPMSIB
S2DSM:	EQU	((77-2)*2*26)/FPYSIB
D1DSM:	EQU	((77-2)*2*26)/FPYSIB
D2DSM:	EQU	((77-2)*2*2*26)/FPYSIB
D3DSM:	EQU	((77-2)*4*15)/FPYSIB
D4DSM:	EQU	((77-2)*2*4*15)/FPYSIB
D5DSM:	EQU	((77-2)*8*8)/FPYSIB
D6DSM:	EQU	((77-2)*2*8*8)/FPYSIB


;	BDOS constants on entry to write

WRALL:	EQU	0		;write to allocated
WRDIR:	EQU	1		;write to directory
WRUAL:	EQU	2		;write to unallocated
;+	page
;	Macro for generating Control Blocks for disk drives
;	The format of these disk control blocks are as follows:
;	16 bits = -> translation table.
;	48 bits = Work area for CP/M.
;	16 bits = -> DIRBUF.
;	16 bits = -> Parameter block.
;	16 bits = -> check vector.
;	16 bits = -> allocation vector.

NDSK:	SET	0		;Number of disk drives
NOHRD:	SET	0		;Number of hard disk drives
NOFDD:	SET	0		;Number of floppy disk drives
ALVS:	SET	0		;Allocation vector size
CSVS:	SET	0		;Check vector size

;+
;+ ASM.COM does not support macro definitions, so the following 
;+ macros were removed, and the equivalent code was inserted
;+ by hand.
;+
;+
;+DPHGEN	MACRO	TYPE,XLATE,DIRBUF,DPBADR
;+NDSK:	SET	NDSK+1
;+	DW	%2
;+	DW	0,0,0
;+	DW	%3
;+	DW	%4
;+	DW	CSV+CSVS
;+	DW	ALV+ALVS
;+.type	IF	%1 <= MAXFTP
;+NOFDD:	SET	NOFDD+1
;+CSVS:	SET	CSVS+(256/4)
;+ALVS:	SET	ALVS+((D6DSM+7)/8)
;+	else
;+	ERR	- Hard disk not supported.
;+.type	ENDIF
;+	ENDM
;+	space	4,10
;+
;	Macro for generating the Disk Parameter Blocks.
;
;	Disk type definition blocks for each particular mode.
;	The format of these areas are as follow:
;	8 bit = disk type code
;	16 bit = Sectors per track
;	8 bit  = Block shift
;	8 bit  = BS mask
;	8 bit  = Extent mask
;	16 bit = Disk size/1024 - 1.
;	16 bit = Directory size.
;	16 bit = Allocation for directory.
;	16 bit = check area size.
;	16 bit = offset to first track.

;+DPBGEN MACRO TYPE,SPT,BSH,BSM,EXM,DSM,DIRSIZ,ALVMSK,OFFSET
;+	DB	%1
;+	DW	%2
;+	DB	%3,%4,%5
;+	DW	%6-1,%7-1,REV (%8)
;+.hrd	IF	%1 <= MAXFTP
;+	DW	(%7+3)/4
;+	else
;+	DW	0
;+.hrd	ENDIF
;+	DW	%9
;+	ENDM
;+	page
;	The following jump table defines the entry points
;	into the CBIOS for use by CP/M and other external
;	routines; therfore the order of these jump cannot
;	be modified.  The location of these jumps can only
;	be modified by 400h locations, which is a restriction
;	of MOVCPM.

	ORG	BIOS

	JMP	CBOOT		;Cold boot
	JMP	WBOOT		;Warm boot
;+ ASM.COM does not like labels with embedded periods, changed to $
;+ J.CST	JMP	CONST		;Console status (input)
;+ J.CIN	JMP	CONIN		;Console input
;+ J.COUT	JMP	CONOUT		;Console output
J$CST	JMP	CONST		;Console status (input)
J$CIN	JMP	CONIN		;Console input
J$COUT	JMP	CONOUT		;Console output
J$LIST	JMP	LIST		;List output
	JMP	PUNCH		;Punch output
	JMP	READER		;Reader input
	JMP	HOME		;Set track to zero
	JMP	SELDSK		;Select disk unit
	JMP	SETTRK		;Set track
	JMP	SETSEC		;Set sector
	JMP	SETDMA		;Set Disk Memory Address
	JMP	READ		;Read from disk
	JMP	WRITE		;Write onto disk
J$LST	JMP	LISTST		;List status (output)
	JMP	SECTRN		;Translate sector number
	JMP	SETNUM		;Set number of sectors to read
	JMP	SETXAD		;Set extended address bank.


;	Endx	BIOS1.asm

;+ File GBCBIOS2.ASM
;+	page
;	Extended bank addresses.

CXADR:	EQU	00h		;CP/M Bank
BXADR:	EQU	00h		;BIOS Bank


;	Disk Input / Output port assignments.

FDPORT	EQU	0C0h		;Base port address for Controller
FDCS	EQU	FDPORT		;Status register
FDCD	EQU	FDPORT+1	;Data register
DMA	EQU	FDPORT+2	;Dma address (when write)
INTS	EQU	FDPORT+2	;Status Register (when read)
SER	EQU	FDPORT+3	;Serial port

;	Controller function definitions

;+ F.SPEC	=	03	;Specify
;+ F.DSTS	=	04	;Drive status
;+ F.WRT	=	05	;Write data
;+ F.RDAT	=	06	;Read data
;+ F.RECA	=	07	;recalibrate
;+ F.RSTS	=	08	;Read status
;+ F.DRID	=	10	;Read ID
;+ F.SEEK	=	15	;Seek
F$SPEC	equ	03	;Specify
F$DSTS	equ	04	;Drive status
F$WRT	equ	05	;Write data
F$RDAT	equ	06	;Read data
F$RECA	equ	07	;recalibrate
F$RSTS	equ	08	;Read status
F$DRID	equ	10	;Read ID
F$SEEK	equ	15	;Seek


;	Disk drive constants

;+ STEPR:	=	8		;Shugart SA 800
;+ SRT:	=	16-STEPR	;Controller value
;+ HUT:	=	240/16		;Head unload = 240 ms
;+ HLT:	=	35		;Head load = 35 ms
;+ ND:	=	0b		;Set DMA mode
STEPR:	equ	8		;Shugart SA 800
SRT:	equ	16-STEPR	;Controller value
HUT:	equ	240/16		;Head unload = 240 ms
;+ ASM.COM doesn't like labels with same names os op codes (can't use HLT)
HDLT:	equ	35		;Head load = 35 ms
ND:	equ	0b		;Set DMA mode
;+	space	4,10
;		   7    6    5    4    3    2    1    0
;		+----+----+----+----+----+----+----+----+
;	ST0	|    IC   | SE | EC | NR | HD |    US   |
;		+----+----+----+----+----+----+----+----+
;	ST1	| EN |    | DE | OR |    | ND | NW | MA |
;		+----+----+----+----+----+----+----+----+
;	ST2	|    | CM | DD | WC | SH | SN | BC | MD |
;		+----+----+----+----+----+----+----+----+
;	ST3	| FT | WP | RY | T0 | TS | HD |    US   |
;		+----+----+----+----+----+----+----+----+
;
;	IC - Interrupt code.
;		00 - Normal termination by TC signal.
;		01 - Abnormal termination.
;		10 - Invalid command.
;		11 - Abnormal termination (READY dropped).
;
;	SE - Seek end, inticates end of seek.
;
;	EC - Equipment Check.
;
;	NR - Not ready.
;
;	HD - State of the head select.
;
;	US - State of the unit select.
;
;
;	EN - End of Cylinder, Read EOT sector.
;
;	DE - CRC error in ID or data fields.
;
;	OR - Over run.
;
;	ND - No Data.
;
;	NW - Not writable (write protect detected)
;
;	MA - Missing address mark.
;
;
;	CM - Control Mark (deleted data address mark).
;
;	DD - CRC error in data field.
;
;	WC - Wrong cylinder.
;
;	SH - Scan equal hit.
;
;	SN - Scan not satisfied.
;
;	BC - Bad cylinder.
;
;	MD - Missing data mark.
;
;	FT - Fault.
;
;	WP - Write protect signal.
;
;	RY - Ready.
;
;	T0 - Track zero.
;
;	TS - Two sided disk is inserted.
;+	space	4,10
	IF	HARD
;	Morrow Designs Hard Disk Controller Equates.

HDPORT:	EQU	050h		;Base port address
HDSTAT:	EQU	HDPORT+0	;Status and control
HDCMD:	EQU	HDPORT+1	;Command channel
HDFNC:	EQU	HDPORT+2	;Function
HDDATA:	EQU	HDPORT+3	;Data port

;	IN  HDSTAT	  7   6   5   4   3   2   1   0
;			  |   |   |   |   |   |   |   |
;	Halt <------------+   |   |   |   |   |   |   |
;	ILEVEL <--------------+   |   |   |   |   |   |
;	NREADY <------------------+   |   |   |   |   |
;	NFAULT <----------------------+   |   |   |   |
;	TIMOUT <--------------------------+   |   |   |
;	COMPLT <------------------------------+   |   |
;	OPDONE <----------------------------------+   |
;	TRACK0 <--------------------------------------+
;
;	IN  HDCMD	  7   6   5   4   3   2   1   0
;			                  |   |   |   |
;	R1 <------------------------------+   |   |   |
;	R0 <----------------------------------+
;	RETRY <-----------------------------------+   |
;	SDONE <---------------------------------------+
;
;	OUT  HDSTAT	  7   6   5   4   3   2   1   0
;			                  |   |   |   |
;	WPROT <---------------------------+   |   |   |
;	DSKCLK <------------------------------+   |   |
;	RUN <-------------------------------------+   |
;	FRENBL <--------------------------------------+
;
;	OUT  HDFNC	  7   6   5   4   3   2   1   0
;			  |   |   |   |   |   |   |   |
;	NHEAD <-----------+---+---+---+   |   |   |   |
;	DIR   <---------------------------+   |   |   |
;	NSTEP <-------------------------------+   |   |
;	DRIVE <-----------------------------------+---+

;	Hard disk commands  (OUT  HDCMD).

;+ H.RBDT:	EQU	00h		;reset buffer pointer (data)
;+ H.RSDT:	EQU	01h		;Read sector data
;+ H.RSHD:	EQU	03h		;Read sector header
;+ H.WSDT:	EQU	05h		;Write sector data
;+ H.WSHD:	EQU	07h		;Write sector header
;+ H.RHPT:	EQU	08h		;Reset header pointer
H$RBDT:	EQU	00h		;reset buffer pointer (data)
H$RSDT:	EQU	01h		;Read sector data
H$RSHD:	EQU	03h		;Read sector header
H$WSDT:	EQU	05h		;Write sector data
H$WSHD:	EQU	07h		;Write sector header
H$RHPT:	EQU	08h		;Reset header pointer
	ENDIF
;+	page
;+ STEPMS:	VFD	4\SRT,4\0  ;Step rate **** 
;+				    fixed location ****
STEPMS:	DB	(SRT SHL 4)+0

;	Sector Translation Tables.

XTABLE:	DW	XLTS		;Single 128
	DW	XLTD1		;Double 256
	DW	XLTD2		;Double 512
	DW	XLTD3		;Double 1024

XLTS:	DB	0,6,12,18,24,4,10,16,22,2,8,14,20
	DB	1,7,13,19,25,5,11,17,23,3,9,15,21

XLTD1:	DB	 0, 1,18,19,36,37, 2, 3,20,21,38,39
	DB	 4, 5,22,23,40,41, 6, 7,24,25,42,43
	DB	 8, 9,26,27,44,45,10,11,28,29,46,47
	DB	12,13,30,31,48,49,14,15,32,33,50,51
	DB	16,17,34,35

XLTD2:	DB	 0, 1, 2, 3,16,17,18,19
	DB	32,33,34,35,48,49,50,51
	DB	 4, 5, 6, 7,20,21,22,23
	DB	36,37,38,39,52,53,54,55
	DB	 8, 9,10,11,24,25,26,27
	DB	40,41,42,43,56,57,58,59
	DB	12,13,14,15,28,29,30,31
	DB	44,45,46,47

XLTD3:	DB	 0, 1, 2, 3, 4, 5, 6, 7
	DB	24,25,26,27,28,29,30,31
	DB	48,49,50,51,52,53,54,55
	DB	 8, 9,10,11,12,13,14,15
	DB	32,33,34,35,36,37,38,39
	DB	56,57,58,59,60,61,62,63
	DB	16,17,18,19,20,21,22,23
	DB	40,41,42,43,44,45,46,47


;	Disk selection masks.
;		A   B   C   D   E   F   G   H
DSKMSK:	DB	00h,01h,02h,03h,00h,00h,00h,00h,00h
;		I   J   K   L   M   N   O   P
;+	page
;	Control Blocks for disk drives

DPBASE:
;+	DPHGEN	DSKS1,XLTS,DIRBUF,DPBS1+1	;Drive A:
ndsk	set	ndsk+1
	dw	xlts,0,0,0,dirbuf,dpbs1+1,csv+csvs,alv+alvs
nofdd	set	nofdd+1
csvs	set	csvs+(256/4)
alvs	set	alvs+((d6dsm+7)/8)


;+	DPHGEN	DSKS1,XLTS,DIRBUF,DPBS1+1	;Drive B:
ndsk	set	ndsk+1
	dw	xlts,0,0,0,dirbuf,dpbs1+1,csv+csvs,alv+alvs
nofdd	set	nofdd+1
csvs	set	csvs+(256/4)
alvs	set	alvs+((d6dsm+7)/8)

	IF	HARD
;+	DPHGEN	DSK8M1,0,DIRBUF,DPBM81+1

;+	DPHGEN	DSK8M2,0,DIRBUF,DPBM82+1
	ENDIF
;+	space	4,10
;	Disk type definition blocks for each particular mode.

DPBS1:		;Single density, single sided.

;+	DPBGEN	DSKS1,26,3,7,0,S1DSM,64,1100000000000000b,2
	db	dsks1
	dw	26
	db	3,7,0
	dw	s1dsm-1,64-1
	db	11000000b,00000000b
	dw	(64+3)/4
	dw	2

DPBS2:		;Single density, double sided.

;+	DPBGEN	DSKS2,26,4,15,1,S2DSM,128,1100000000000000b,2*2
	db	dsks2
	dw	26
	db	4,15,1
	dw	s2dsm-1,128-1
	db	11000000b,00000000b
	dw	(128+3)/4
	dw	2*2

DPBD1:		;Double density, single sided.

;+	DPBGEN	DSKD1,2*26,4,15,0,D1DSM,128,1100000000000000b,2
	db	dskd1
	dw	2*26
	db	4,15,0
	dw	d1dsm-1,128-1
	db	11000000b,00000000b
	dw	(128+3)/4
	dw	2

DPBD2:		;Double density, double sided.

;+	DPBGEN	DSKD2,2*26,4,15,0,D2DSM,256,1111000000000000b,2*2
	db	dskd2
	dw	2*26
	db	4,15,0
	dw	d2dsm-1,256-1
	db	11110000b,00000000b
	dw	(256+3)/4
	dw	2*2


DPBD3:		;Double density, single sided.

;+	DPBGEN	DSKD3,4*15,4,15,0,D3DSM,128,1100000000000000b,2
	db	dskd3
	dw	4*15
	db	4,15,0
	dw	d3dsm-1,128-1
	db	11000000b,00000000b
	dw	(128+3)/4
	dw	2


DPBD4:		;Double density, double sided.

;+	DPBGEN	DSKD4,4*15,4,15,0,D4DSM,256,1111000000000000b,2*2
	db	dskd4
	dw	4*15
	db	4,15,0
	dw	d4dsm-1,256-1
	db	11110000b,00000000b
	dw	(256+3)/4
	dw	2*2


DPBD5:		;Double density, single sided.

;+	DPBGEN	DSKD5,8*8,4,15,0,D5DSM,128,1100000000000000b,2
	db	dskd5
	dw	8*8
	db	4,15,0
	dw	d5dsm-1,128-1
	db	11000000b,00000000b
	dw	(128+3)/4
	dw	2

DPBD6:		;Double density, double sided.

;+	DPBGEN	DSKD6,8*8,4,15,0,D6DSM,256,1111000000000000b,2*2
	db	dskd6
	dw	8*8
	db	4,15,0
	dw	d6dsm-1,256-1
	db	11110000b,00000000b
	dw	(256+3)/4
	dw	2*2


	IF	HARD
DPBS81:		;Shugart SA 1000, first half.

;+	DPBGEN	DSK8S1,2*32,5,31,1,S8DSM,512,1111000000000000b,2


DPBS82:		;Shugart Sa 1000, last half.

;+	DPBGEN	DSK8S2,2*32,5,31,1,S8DSM,512,1111000000000000b,2


DPBM81:		;Memorex 8 inch. first half.

;+	DPBGEN	DSK8M1,4*21,5,31,1,M81DSM,512,1111111111111111b,1*4


DPBM82:		;Memorex 8 inch. last half.

;+	DPBGEN	DSK8M2,4*21,5,31,1,M82DSM,512,1111111111111111b,122*4
	ENDIF


;+ File GBCBIOS3.ASM
;+	page
;	CompuPro Interfacer board equates.

GBP0:	EQU	0		;Serial port zero
GBP1:	EQU	2		;Serial port one
GBP2:	EQU	4		;Serial port two
GBDATA:	EQU	0		;Data on even I/O unit
GBSTAT:	EQU	1		;Status on odd I/O unit

;+ GBTBMT:	EQU	0000_0001b	;Transmit buffer empty
;+ GBDAV:	EQU	0000_0010b	;Data available
;+ GBOPT:	EQU	0000_0100b	;Optional status line
;+ GBPE:	EQU	0000_1000b	;Parity error
;+ GBOR:	EQU	0001_0000b	;Overrun error
;+ GBFE:	EQU	0010_0000b	;Framing error
;+ GBCC:	EQU	0100_0000b	;RS 232 CC input
;+ GBCB:	EQU	1000_0000b	;RS 232 CB input

;+ GBRIE:	EQU	0000_0001b	;Receiver interrupt enable
;+ GBTIE:	EQU	0000_0010b	;Transmitter interrupt enable
;+ GBCD:	EQU	0000_0100b	;RS 232 CD output
;+ GBCA:	EQU	0000_1000b	;RS 232 CA output
;+ GBTSB:	EQU	0001_0000b	;Number of stop bits
;+ GBNP:	EQU	0010_0000b	;No parity
;+ GBEPS:	EQU	0100_0000b	;Even parity
;+ GBNBI:	EQU	1000_0000b	;number of bits/character
GBTBMT:	EQU	00000001b	;Transmit buffer empty
GBDAV:	EQU	00000010b	;Data available
GBOPT:	EQU	00000100b	;Optional status line
GBPE:	EQU	00001000b	;Parity error
GBOR:	EQU	00010000b	;Overrun error
GBFE:	EQU	00100000b	;Framing error
GBCC:	EQU	01000000b	;RS 232 CC input
GBCB:	EQU	10000000b	;RS 232 CB input

GBRIE:	EQU	00000001b	;Receiver interrupt enable
GBTIE:	EQU	00000010b	;Transmitter interrupt enable
GBCD:	EQU	00000100b	;RS 232 CD output
GBCA:	EQU	00001000b	;RS 232 CA output
GBTSB:	EQU	00010000b	;Number of stop bits
GBNP:	EQU	00100000b	;No parity
GBEPS:	EQU	01000000b	;Even parity
GBNBI:	EQU	10000000b	;number of bits/character
;+	page
;	C O N S O L   S T A T U S
;
;	This routine samples the Console status and returns the
;	following values in the A register.
;
;	EXIT	A = 0 (zero), means no character
;		currently ready to read.
;
;		A = FFh (255), means character
;		currently ready to read.

CONST:	IN	GBP0+GBSTAT	;Input from port
	ANI	GBDAV		;Mask data available
	RZ			;If data not available
	ORI	0FFh
	RET
;+	space	4,10
;	C O N S O L   I N P U T
;
;	Read the next character into the A register, clearing
;	the high order bit.  If no character currently ready to
;	read then wait for a character to arrive before returning.
;
;	EXIT	A = character read from terminal.

CONIN:	IN	GBP0+GBSTAT
	ANI	GBDAV
	JZ	CONIN		;If data not available
	IN	GBP0+GBDATA
	ANI	7Fh
	RET
;+	space	4,10
;	C O N S O L   O U T P U T
;
;	Send a character to the console.  If the console
;	is not ready to receive a character wait until
;	the console is ready.
;
;	ENTRY	C = ASCII character to output to console.

CONOUT:	IN	GBP0+GBSTAT
	ANI	GBTBMT
	JZ	CONOUT		;If transmit buffer not empty
	MOV	A,C
	OUT	GBP0+GBDATA
	RET
;+	space	4,10
;	P u n c h   O u t p u t.
;
;	Send a character to the punch device.  If no punch
;	device exists then immediately return.
;
;	ENTRY	C = ASCII character to output.

PUNCH:	RET			;No punch: channel B is FujiNet
;+	space	4,10
;	R e a d e r   I n p u t.
;
;	Read the next character from the currently assigned
;	reader device into the A register.
;
;	EXIT	A = character read from the reader device.

READER:	MVI	A,1AH		;No reader: return Ctrl-Z
	RET
;+	space	4,10
;	L i s t   O u t p u t.
;
;	Send a character to the list device.  If the list
;	device is not ready to receive a character wait
;	until the device is ready.
;
;	ENTRY	C = ASCII character to be output.

LIST:	RET			;No list device: discard
;	space	4,10
;	L i s t   S t a t u s.
;
;	Return the ready status for the list device.
;
;	EXIT	A = 0 (zero), list device is not ready to
;		accept another character.
;		A = FFh (255), list device is ready to accept
;		a character.

LISTST:	MVI	A,0FFH		;Always ready
	RET

	IF	BANG
;+	Link	GBcbiot3
;File GBCBIOT3.ASM
;+	TITLE	'Bit Banger for CompuPro DISK1'
;	Bit Banger for CompuPro DISK1.
;
; 81-03-28	phm


;	7 Data bits, no parity
;	For 2 MHz 8085, use 300 Baud.
;	For 5 MHz,	use 600 Baud.
;+	space	4,10
;	Normal bit timings.
;				___ TICK ___
;	Baud	uSec/bit	2MHz	5MHz
;	4800	208		<<<	25
;	2400	416		18	46
;	1200	833		38	92
;	600	1667		75	216
;	300	3333		102	>>>
;	110	can't		>>>	>>>
;
;	Restriction on higher rates is due to the fact
;	that we can only adjust speed by integral TICK
;	counts: Unless number > 100, cannot tune in closer
;	than one percent.
 
 
 
 
;	Timing for the Bit Banger is via programmed delays,
;	so be very careful if changes are contemplated.
;	Baud =	Bit per second of serial data.
;	Cycle =	Machine cycle (clock period, T-State).
;	Tick =	1/n of a bit time (PERIOD of sample).
;
;	1200 baud	= 833 uSec/bit
;	* 2 MHz		= 1667 cycles/bit
;	/ 16 sample rate= 104  cycles/Tic

yBANG:	EQU	SER	;Serial bit latch port


;+SAMP:	DB	-1	;dummy pre-store
samp:	db	0ffh
	DS	10	;buckets for Space counts
;+	DB	-1
	db	0ffh
;+	space	4,10
;	CONSOLE STATUS
;	BitBanger has no status available, so always
;		replies NO.  This means that Ctrl-S will
;		not work when using the BitBanger.
;	Exit	A= FFh	means character available.
KONST:
;+	XOR	A	;A=0, clear Carry
	xra	a
	RET
;+	space	4,10
;	Output 1 Character.
;	Entry	C= Character to output.
;		Line assumed marking.
;	Exit	Line marking, but stop time not elapsed.
;	Transmission format:
;		Data bits inverted;
;		Start(0), D0, D1, ..., D6, Stop(1), Leaves marking
;	Note: Cannot destroy DE or HL.
;	Uses	AF, BC.
;+KONOUT:	proc
KONOUT:
;+	PUSH	HL
	push	h
;+	PUSH	DE
	push	d
	MOV	A,C
;+	AND	7Fh	;use Bit7 as Start bit (0)
	ani	7fh	;use Bit7 as Start bit (0)
;+	XOR	0FFh	;invert data
	xri	0FFh	;invert data
	RLC		;adjust
	MOV	C,A
;+	LDK	B,7+1+1	;7 bit data, 1 Start, 1 fudge
	mvi	B,7+1+1	;7 bit data, 1 Start, 1 fudge

;	Write 8 bits.
; outer loop time:
;	N = 8
;	NT = (N * MT+6) + (N+1)*(37) - 3
;	N = 8
;	NT = 8*MT + 48+333-3
;		= 378 + 8*(43*TICK-3)
;		= 378 - 24 + (344*TICK)
;		= 354 + 344*TICK
;		= 10 + 344*(TICK+1)
; for each bit:
;	N1 = 43*M-3 + 6 + 37 - 3
;		= 37 + 43*M
;		= -6 + 43*(TICK+1)
;+ :N:
proc1$n:
;+	LD	A,TICK	;(7	7
	lda	tick	;(7	7
	MOV	E,A	;(5	4
;r	STO	D,[hl]	;(7	7
;r	LDK	D,0	;(7	7
	MOV	A,C	;(5	4
	RRC		;(4	4
	MOV	C,A	;(5	4
;+	DEC	B	;(5	4
	dcr	B	;(5	4
;+	JZ	:6	;if enuf bits read
	JZ	proc1$6	;if enuf bits read
;+	INC	HL	;(5	6
	inx	h	;(5	6

;	Begin output loop for this bit.
;+ :M:
proc1$m:
	OUT	yBANG	;(10	10	11)
;+	AND	80h	;(7	7
	ani	80h	;(7	7
	RLC		;(4	4
	RRC		;(4	4
	MOV	D,A	;(5	4
;+	DEC	E	;(5	4	4)
	dcr	E	;(5	4	4)
;+	JNZ	:M	;(10	10(7)	10)
	JNZ	proc1$m	;(10	10(7)	10)
;			;(22*M
; inner loop=			43*M-3
;					21*M)
;+	JMP	:N
	JMP	proc1$n
;+ :6:
proc1$6:
;+	LDK	B,2	;2 Stop bits
	mvi	B,2	;2 Stop bits
;+ :62:
proc1$62:
;+	LD	A,TICK
	lda	TICK	
	MOV	E,A
;+	INC	E
	inr	e
;+ :65:
proc1$65:
;+	LDK	A,0	;Stop bit= Mark= 1
	mvi	A,0	;Stop bit= Mark= 1
	OUT	yBANG	;(10	10	11)
;+	AND	80h	;(7	7
	ani	80h	;(7	7
	RLC		;(4	4
	RRC		;(4	4
	MOV	D,A	;(5	4
;+	DEC	E	;(5	4	4)
	dcr	E	;(5	4	4)
;+	JNZ	:65	;(10	10(7)	10)
	JNZ	proc1$65	;(10	10(7)	10)
;+	DEC	B
	dcr	B
;+	JNZ	:62	;if more stoppers
	JNZ	proc1$62	;if more stoppers
;+	POP	DE
	POP	D
;+	POP	HL
	POP	H
	RET
;+	space	4,10
;	Bit Banger Input.
;
;	Exit	A= Character read.
;			Bit7 clear.
;	Uses	AF, BC, DE, HL.

;	Timing for	8080	8085	z80
;+KONIN:	proc
KONIN:
;+	LDK	HL,SAMP
	lxi	H,SAMP
;+	LDK	B,9+1
	mvi	B,9+1

;	Wait for Start bit.
;+ :L0:
proc2$l0:
	IN	yBANG
	RLC
;+	JNC	:L0	;if line still Marking
	JNC	proc2$L0	;if line still Marking

;	Now take 7 uniform samples.
;	The number of peeks in each determines the sample width.
;+ :N:
proc2$n:
;+	LD	A,TICK	;(13	13
	lda	TICK	;(13	13
	MOV	E,A	;(5	4
;+	STO	D,[hl]	;(7	7
	mov	m,d	;(7	7
;+	LDK	D,0	;(7	7
	mvi	d,0	;(7	7
;+	DEC	B	;(5	4
	dcr	B	;(5	4
;+	JZ	:6	;(10)	if enuf bits read
	JZ	proc2$6	;(10)	if enuf bits read
;+	INC	HL	;(5	6
	inx	H	;(5	6

;	Begin sampling loop for this bit.
;+ :M:
proc2$m:
	IN	yBANG	;(10	10	11)
;+	AND	80h	;(7	7
	ani	80h	;(7	7
	RLC		;(4	4
;+	ADD	A,D	;(4	4
	ADD	D	;(4	4
	MOV	D,A	;(5	4
;+	DEC	E	;(5	4	4)
	dcr	E	;(5	4	4)
;+	JNZ	:M	;(10	10(7)	10)
	JNZ	proc2$M	;(10	10(7)	10)
;+	JMP	:N	;(10	10	10)
	JMP	proc2$n	;(10	10	10)
;			;(22*M
; inner loop=			43*M-3
;					21*M)


;	Reduce sample counts to data bits.
;	Note that due to DISK1 inversion,
;	0  Space =	Count[i]  >  Threshold
;	1  Mark  =	Count[i]  <  Threshold
;	Actually, all counts "near" mid-range are probably
;	errors.
;+ :6:
proc2$6:
;+	LDK	HL,SAMP+8	;-> Data bit 6
	lxi	h,SAMP+8	;-> Data bit 6
;+	LD	A,TICK-1
	lda	TICK-1
	MOV	D,A	;C= Threshold for Mark versus Space
;+	LDK	BC,rev 7
	lxi	B,0700h
;+ :64:
proc2$64:
	MOV	A,C
	RLC
	MOV	C,A
;+	LD	A,[hl]
	mov	a,m
	CMP	D
;+	JNC	:66	;if large count
	JNC	proc2$66	;if large count
;+	INC	C	;set bit for Mark
	inr	C	;set bit for Mark
;+ :66:
proc2$66:
;+	DEC	HL
	dcx	H
;+	DEC	B
	dcr	B
;+	JNZ	:64	;if more bits to reduce
	JNZ	proc2$64	;if more bits to reduce
	MOV	A,C
	RET
;+	space	4,10
;	Determine speed of terminal.
;	User must input a 'U'
;	Relies on being able to measure the width of the
;	Start bit.  Therefore, needs an odd-numbered Ascii
;	to be input.
;	Consecutive samples look like this:
;		...11111111111110000..001xxxxxxx...
;		Mark		Space	Mark
;		...Idle		Start	Data0	(ignore...)
;+ BAUD:	proc
baud:
;+	LDK	HL,SAMP
	lxi	h,SAMP
;+	LDK	B,3
	mvi	B,3
;+	LDK	DE,1
	lxi	D,1

;	Wait for Start bit.
;+ :L0:
proc3$l0:
	IN	yBANG
	RLC
;+	JNC	:L0	;if line still Marking
	JNC	proc3$L0	;if line still Marking

;	Now measure width of next several pulses.
;	The number of peeks in each determines the sample width.
;+ :N:
proc3$n:
	MOV	C,E	;(5	4
;+	LD	A,TICK	;(13	13
	lda	TICK	;(13	13
;+	STO	D,[hl]	;(7	7
	mov	m,d	;(7	7
;+	LDK	D,0	;(7	7
	mvi	D,0	;(7	7
;+	DEC	B	;(5	4
	dcr	B	;(5	4
;+	JZ	:6	;(10)	if enuf bits read
	JZ	proc3$6	;(10)	if enuf bits read
;+	INC	HL	;(5	6
	inx	H	;(5	6


;	Begin sampling loop for this bit.
;+ :M:
proc3$m:
;+	INC	D	;(5	4	4)
	inr	D	;(5	4	4)
	IN	yBANG	;(10	10	11)
;+	AND	80h	;(7	7
	ani	80h	;(7	7
	RLC		;(4	4
	CMP	C	;(4	4
	MOV	E,A	;(5	4
;+	JZ	:M	;(10	10(7)	10)
	JZ	proc3$M	;(10	10(7)	10)
;+	JMP	:N	;(10	10	10)
	JMP	proc3$N	;(10	10	10)
;			;(22*M
; inner loop=			43*M-3
;					21*M)


;	Reduce sample counts to data bits.
;+ :6:
proc3$6:
;+	LD	A,SAMP+1
	lda	SAMP+1
;+	STO	A,TICK	;set nominal bit width
	sta	TICK	;set nominal bit width
;+	CLC
	ora	a	;clear carry
	RAR		;width / 2 = threshold
;+	STO	A,TICK-1
	sta	TICK-1
	RET

;	Endx	GBcbiot3.asm

	ENDIF

	IF	SYSSUP
;+	Link	GBcbiou3.asm		;If System Support included

;	page
;	CompuPro System Support 1 equates.

GBSS:	EQU	50h		;System Support starting port
GBMP0:	EQU	GBSS+0		;Master PIC port 0
GBMP1:	EQU	GBSS+1		;Master PIC port 1
GBSP0:	EQU	GBSS+2		;Slave PIC port 0
GBSP1:	EQU	GBSS+3		;Slave PIC port 1
GBT0:	EQU	GBSS+4		;Timer number 0
GBT1:	EQU	GBSS+5		;Timer number 1
GBT2:	EQU	GBSS+6		;Timer number 2
GBTC:	EQU	GBSS+7		;Timer control port
GBFPPD:	EQU	GBSS+8		;Floating point processor data port
GBFPPC:	EQU	GBSS+9		;Floating point processor command port
GBCLKC:	EQU	GBSS+10		;Clock command port
GBCLKD:	EQU	GBSS+11		;Clock data port
GBUD:	EQU	GBSS+12		;Uart data port
GBUS:	EQU	GBSS+13		;Uart status port
GBUM:	EQU	GBSS+14		;Uart modem port
GBUC:	EQU	GBSS+15		;Uart command port

;+SSDAV:	EQU	0000_0010b	;System Support Data Available
;+SSTBMT:	EQU	0000_0001b	;System Support Transmit Buffer Empty
SSDAV:	EQU	00000010b	;System Support Data Available
SSTBMT:	EQU	00000001b	;System Support Transmit Buffer Empty

;	page
;	C O N S O L   I N I T I A L I Z A T I O N
;
;	This routine performs the initialization required by
;	the System Support UART.
;

sTINIT:
	MVI	A,11101110b	;Async, 16x, 8 bits, no parity, even, 2 stops
	OUT	GBUM		;Set up mode register 1
	MVI	A,01111110b	;9600 baud
	OUT	GBUM		;Set up mode register 2
	MVI	A,00100111b	;Trans. on, dtr low, rec. on, no break,
				;	no reset, rts low
	OUT	GBUC		;Set up command port
	RET
;	space	4,10
;	C O N S O L   S T A T U S
;
;	This routine samples the Console status and returns the
;	following values in the A register.
;
;	EXIT	A = 0 (zero), means no character
;		currently ready to read.
;
;		A = FFh (255), means character
;		currently ready to read.

sCONST:
	IN	GBUS		;Input from port
	ANI	SSDAV		;Mask data available
	RZ			;If data not available
	ORI	0FFH
	RET
;	space	4,10
;	C O N S O L   I N P U T
;
;	Read the next character into the A register, clearing
;	the high order bit.  If no character currently ready to
;	read then wait for a character to arrive before returning.
;
;	EXIT	A = character read from terminal.

sCONIN:
	IN	GBUS		;Get status from uart
	ANI	SSDAV
	JZ	sCONIN
	IN	GBUD
	ANI	7Fh
	RET
;	space	4,10
;	C O N S O L   O U T P U T
;
;	Send a character to the console.  If the console
;	is not ready to receive a character wait until
;	the console is ready.
;
;	ENTRY	C = ASCII character to output to console.

sCONOUT:
	IN	GBUS		;Get uart status
	ANI	SSTBMT		;Test if buffer empty
	JZ	sCONOUT
	MOV	A,C
	OUT	GBUD
	RET

;	Endx	GBcbiou3.asm

	ENDIF

	IF	INTER3

;	CompuPro Interfacer 3 support routines

GBI3:	EQU	10h		;Interfacer 3 Base address
GBI3D:	EQU	GBI3+0		;Uart data location
GBI3S:	EQU	GBI3+1		;Uart status
GBI3M:	EQU	GBI3+2		;Uart mode register
GBI3C:	EQU	GBI3+3		;Uart command register
GBI3U:	EQU	GBI3+7		;Uart select register

GBI3DV:	EQU	00000010b	;Interfacer 3 Data Available
GBI3MT:	EQU	00000001b	;Interfacer 3 Transmit Buffer Empty
GBI3DS:	EQU	10000000b	;Interfacer 3 Data Set Ready
CON:	EQU	7		;Interfacer 3 Console Select
PRN:	EQU	6		;Interfacer 3 Printer Select
ULS:	EQU	5		;Interfacer 3 UL1 Select

;	C O N S O L   I N I T I A L I Z A T I O N
;
;	This routine performs the initialization required by
;	the Interfacer 3.
;

I3INIT:
	MVI	A,CON		;Console select
	OUT	GBI3U		;Select Uart 7
	MVI	A,11101110b	;Async, 16x, 8 bits, no parity, even, 2 stops
	OUT	GBI3M		;Set up mode register 1
	MVI	A,01111110b	;9600 baud
	OUT	GBI3M		;Set up mode register 2
	MVI	A,00100111b	;Trans. on, dtr low, rec. on, no break,
				;	no reset, rts low
	OUT	GBI3C		;Set up command port

	MVI	A,PRN		;Printer Select
	OUT	GBI3U		;Select Uart 0
	MVI	A,11101110b	;Async, 16x, 8 bits, no parity, even, 2 stops
	OUT	GBI3M		;Set up mode register 1
	MVI	A,01111110b	;9600 baud
	OUT	GBI3M		;Set up mode register 2
	MVI	A,00100111b	;Trans. on, dtr low, rec. on, no break,
				;	no reset, rts low
	OUT	GBI3C		;Set up command port

	MVI	A,ULS		;User list 1 Select
	OUT	GBI3U		;Select Uart 0
	MVI	A,11101110b	;Async, 16x, 8 bits, no parity, even, 2 stops
	OUT	GBI3M		;Set up mode register 1
	MVI	A,01111110b	;9600 baud
	OUT	GBI3M		;Set up mode register 2
	MVI	A,00100111b	;Trans. on, dtr low, rec. on, no break,
				;	no reset, rts low
	OUT	GBI3C		;Set up command port
	RET



;	C O N S O L   S T A T U S
;
;	This routine samples the Console status and returns the
;	following values in the A register.
;
;	EXIT	A = 0 (zero), means no character
;		currently ready to read.
;
;		A = FFh (255), means character
;		currently ready to read.

I3CONST:
	MVI	A,CON
	OUT	GBI3U
	IN	GBI3S		;Input from port
	ANI	GBI3DV		;Mask data available
	RZ			;If data not available
	ORI	0FFH
	RET


;	C O N S O L   I N P U T
;
;	Read the next character into the A register, clearing
;	the high order bit.  If no character currently ready to
;	read then wait for a character to arrive before returning.
;
;	EXIT	A = character read from terminal.

I3CONIN:
	MVI	A,CON
	OUT	GBI3U
	IN	GBI3S		;Get status from uart
	ANI	GBI3DV
	JZ	I3CONIN
	IN	GBI3D
	ANI	7Fh
	RET



;	C O N S O L   O U T P U T
;
;	Send a character to the console.  If the console
;	is not ready to receive a character wait until
;	the console is ready.
;
;	ENTRY	C = ASCII character to output to console.

I3CONOUT:
	MVI	A,CON
	OUT	GBI3U
	IN	GBI3S		;Get uart status
	ANI	GBI3MT		;Test if buffer empty
	JZ	I3CONOUT
	MOV	A,C
	OUT	GBI3D
	RET
;+	space	4,10
;	L i s t   O u t p u t.
;
;	Send a character to the list device.  If the list
;	device is not ready to receive a character wait
;	until the device is ready.
;
;	ENTRY	C = ASCII character to be output.

I3LIST:	LDA	IOBYTE		;Get IOBYTE status
	ANI	0C0H		;Check for UL1:
	SUI	0C0H
	MVI	A,ULS
	JZ	I3UL1
	MVI	A,PRN
I3UL1:	OUT	GBI3U
I3LST1:	IN	GBI3S
	ANI	GBI3MT+GBI3DS
	SUI	GBI3MT+GBI3DS
	JNZ	I3LST1
	MOV	A,C
	OUT	GBI3D
	RET
;	space	4,10
;	L i s t   S t a t u s.
;
;	Return the ready status for the list device.
;
;	EXIT	A = 0 (zero), list device is not ready to
;		accept another character.
;		A = FFh (255), list device is ready to accept
;		a character.

I3LST:	LDA	IOBYTE
	ANI	0C0H		;Ceck for UL1:
	SUI	0C0H
	MVI	A,ULS
	JZ	I3LS1
	MVI	A,PRN
I3LS1:	OUT	GBI3U
	IN	GBI3S
	ANI	GBI3MT+GBI3DS
	SUI	GBI3MT+GBI3DS
	MVI	A,0FFH
	RZ
	XRA	A
	RET

;	Endx	GBcbiov3.asm
	ENDIF

;	Endx	GBcbios3.asm


;	File is BIOS4.asm
;	S E L E C T   D I S K   D R I V E
;
;	Select the disk drive for subsequent disk transfers and
;	return the appropriate DPB address.   This routine
;	diverges from the normal CP/M implementation of just
;	saving the disk selection value until the transfer is
;	performed.  This divergence is required because floppy
;	disks are a removable media and come in more than on
;	format.  This routine determines the correct format and
;	initializes the DPH with the appropriate values for the
;	format type.
;
;	ENTRY	C = disk delection value.
;		DE and 1 = 0, must determine disk type.
;		         = 1, drive type has been determined.
;
;	EXIT	HL = 0, if drive not selectable.
;		HL = DPH address if drive is selectable.
;
;		DPH is intialized for the appropriate floppy
;		disk format.

SELDSK:	MOV	A,C
	CPI	NDSK
	JNC	SELD1		;If invalid drive
	PUSH	D		;Save drive selection mask
	MVI	B,0

	LXI	H,DSKMSK
	DAD	B
	MOV	A,M
	STA	SEKDSK		;Save selection code

	MOV	L,C		;Compute DPH address
	MOV	H,B
	DAD	H		;*2
	DAD	H		;*4
	DAD	H		;*8
	DAD	H		;*16
	LXI	D,DPBASE
	DAD	D		;HL = DPH address

	LXI	D,5*2
	XCHG
	DAD	D		;HL = DPH(DPB)
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	XCHG			;DE = DPB address
	DCX	D
	LDAX	D
	STA	SEKTYP		;Save disk type
	POP	D		;Restore Drive selction mask
	CPI	MAXFTP+1
	RNC			;If hard disk

	MOV	A,E
	ANI	1		;Mask selected bit
	RNZ			;If drive previously selected

	PUSH	H		;Save DPH address
	CALL	TREAD		;Determine disk type
	POP	H		;Restore DPH address
	JNZ	SELD1		;If disk type not determined

	STA	SEKTYP		;Save disk type
	XCHG
	MOV	L,A		;Move type
	MVI	H,0
	DAD	H		;*2
	DAD	H		;*4
	DAD	H		;*8
	DAD	H		;*16
	LXI	B,DPBS1+1
	DAD	B
	XCHG			;DE = DPB address
	PUSH	H
	LXI	B,5*2
	DAD	B		;Compute DPH DPB address
	MOV	M,E		;Set DPB address into DPH
	INX	H
	MOV	M,D
	ANI	0FEh		;Remove sided bit
	MOV	E,A
	MVI	D,0
	LXI	H,XTABLE
	DAD	D
	XCHG
	POP	H
	LDAX	D		;Set translation table
	MOV	M,A		;address into DPH
	INX	H
	INX	D
	LDAX	D
	MOV	M,A
	DCX	H
	RET

SELD1:	LXI	H,0
	MOV	A,L
	STA	CDISK
	RET
;	page
;	H O M E
;
;	Return disk to home.  This routine sets the track number
;	to zero.  The current host disk buffer is flushed to the
;	disk.

HOME:
	CALL	FLUSH		;Flush host buffer
	XRA	A
	STA	HSTACT		;Clear host active flag
	STA	UNACNT		;Clear sector count
	STA	SEKTRK
	STA	SEKTRK+1
	RET
	;space	4,10
;	S E T   T R A C K.
;
;	Set track number.  The track number is saved for later
;	use during a disk transfer operation.
;
;	ENTRY	BC = track number.

SETTRK:
	IF	Z80
;	STO	BC,SEKTRK	;Set track
	db	0edh,43h
	dw	sektrk
	ENDIF

	IF	C8080
	MOV	L,C
	MOV	H,B
	SHLD	SEKTRK
	ENDIF

	LHLD	UNATRK
	MOV	A,L
	XRA	C
	MOV	C,A
	MOV	A,H
	XRA	B
	ORA	C
	RZ			;If same track
;	JMP	CUNACT
;	space	4,10
;	Clear Unallocated block count (force pre-reads).

CUNACT:	XRA	A		;A = 0
	STA	UNACNT		;Clear unallocated block count
	RET
	;space	4,10
;	Set the sector for later use in the disk transfer.  No
;	actual disk operations are perfomed.
;
;	Entry	BC = sector number.

SETSEC:	MOV	A,C
	STA	SAVSEC		;sector to seek
	RET
;	space	4,10
;	Set Disk memory address for subsequent disk read or
;	write routines.  This address is saved in DMAADR until
;	the disk transfer is performed.
;
;	ENTRY	BC = Disk memory address.
;
;	EXIT	DMAADR = BC.

SETDMA:
	IF	Z80
;	STO	BC,DMAADR
	db	0edh,43h
	dw	dmaadr
	RET
	ENDIF

	IF	C8080
	MOV	H,B
	MOV	L,C
	SHLD	DMAADR
	RET
	ENDIF
	;space	4,15
;	Translate sector number from logical to physical.
;
;	ENTRY	DE = 0, no translation required.
;		DE = translation table address.
;		BC = sector number to translate.
;
;	EXIT	HL = translated sector.

SECTRN:
	LDA	UNASEC
	CMP	C
	CNZ	CUNACT		;If sectors do not match
	MOV	A,C
	STA	LOGSEC
	MOV	L,C
	MOV	H,B
	MOV	A,D
	ORA	E
	RZ			;If no translation
	DAD	D
	MOV	L,M
	MVI	H,0
	RET
;	space	4,10
;	S E T   S E C T O R   C O U N T
;
;	Set the number of continuous sectors to transfer.
;
;	ENTRY	C = Number of sectors to transfer.
;
;	EXIT	NUMSEC = C

SETNUM:
	MOV	A,C
	STA	NUMSEC
	RET
;	space	4,10
;	S E T   E X T E N D E D   B A N K
;
;	Set the extended bank data tranfer address.
;
;	ENTRY	C = Extended address bank.
;
;	EXIT	DMAADE = C.

SETXAD:
	MOV	A,C
	STA	DMAADE
	RET
;end of bios4.asm
;FILE: GBCBIOS5.ASM
;	page
;	B o o t   C P / M   f r o m   d i s k.
;
;	The CBOOT entry point gets control from the cold start
;	loader and is responsible for the basic system initial-
;	ization.  This includes outputting a sign-on message and
;	initializing the following page zero locations:
;
;	   0,1,2: Set to the warmstart jump vector.
;	       3: Set to the initial IOBYTE value.
;	       4: Default and logged on drive.
;	   5,6,7: Set to a jump to BDOS.
;	      40: (Set by BOOT) Board switch options.
;
;	If BANG is true (DISK1 bit serial latch is to be
;	supported), then board switch option 1 means to
;	use the BitBanger for console I/O.
;	Register C must contain the selected drive, which is
;	zero to select the A drive.  The exit address is to
;	the CCP routine.
;
;
;	The WBOOT entry point gets control when a warm start
;	occurs, a ^C from the console, a jump to BDOS (function
;	0), or a jump to location zero.  The WBOOT routine reads
;	the CCP and BDOS from the appropriate disk sectors.
;	WBOOT must also re-initialize locations 0,1,2 and 5,6,7.
;	The WBOOT routines exits with the C register set to the
;	appropriate drive selection value.  The exit address
;	is to the CCP routine.
;
;
;	Disk layout Definition.
;	Cylinder 0   Head 0
;	  0 thru 3	Boot program
;	  4 thru 26	Reserved for CBIOS
;
;	256 byte sectors -- Cylinder 1  Head 0:
;	  0 thru 7	CCP
;	  8 thru 21	BDOS
;	 22 thru 26	**Reserved for CP/M expansion**
;
;	512 byte sectors -- Cylinder 1  Head 0:
;	  0 thru 3	CCP
;	  4 thru 11	BDOS
;	 12 thru 15	**Reserved for CP/M expansion**
;
;	1024 byte sectors -- Cylinder 1  Head 0:
;	  0 thru 1	CCP
;	  2 thru 5,7	BDOS
;	  6		**Reserved for CP/M expansion**

CBOOT:	LXI	SP,DBUF
	lda	opts
	IF	BANG
	CPI	1
	JNZ	CBOOT4		;if BitBanger not selected
	lxi H,KONST	! shld J$CST+1
	lxi H,KONIN	! shld J$CIN+1
	lxi H,KONOUT	! shld J$COUT+1
	CALL	BAUD		;get terminal speed
	JMP	CBOOTX
CBOOT4:
	ENDIF
	IF	SYSSUP
	CPI	2
	JNZ	CBOOT5		;If System support not selected
	LXI H,sCONST	! SHLD	J$CST+1
	LXI H,sCONIN	! SHLD	J$CIN+1
	LXI H,sCONOUT	! SHLD	J$COUT+1
	CALL	sTINIT		;Initialize System Support
	JMP	CBOOTX
CBOOT5:
	ENDIF
	IF	INTER3
	CPI	3
	JNZ	CBOOT6		;If Interfacer 3 not selected
	LXI H,I3CONST	! SHLD	J$CST+1
	LXI H,I3CONIN	! SHLD	J$CIN+1
	LXI H,I3CONOUT	! SHLD	J$COUT+1
	LXI H,I3LIST	! SHLD  J$LIST+1
	LXI H,I3LST	! SHLD  J$LST+1
	CALL	I3INIT		;Initialize Interfacer 3
;	JMP	CBOOTX
CBOOT6:
	ENDIF
CBOOTX:	LXI	H,SIGNON
	CALL	PRINT		;Output Banner
	XRA	A
	STA	CDISK		;Force A drive
	STA	IOBYTE		;Clear I/O byte

WBOOT:				;Warm boot
	LXI	SP,DBUF
	CALL	BCPM		;Boot CP/M
	JNZ	WBOOT		;If error

	LXI	B,DBUF		;Set default data transfer address
	CALL	SETDMA
	MVI	A,0C3h		;Store jumps in low memory
	STA	0
	STA	5
	LXI	H,BIOS+3
	SHLD	1
	LXI	H,BDOS
	SHLD	6
	LDA	CDISK
	MOV	C,A
	JMP	CCP		;Go to CPM

SIGNON:
	DB	CR,LF,LF
	DB	'CompuPro Systems '
	DB	CR,LF,MSIZE/10+'0',MSIZE mod 10 + '0'
	DB	'K CP/M ',VERS/10+'0','.',VERS mod 10 + '0'
	DB	CBIOSV+'@',CR,LF,0
;	space	4,10
;	Boot CPM from disk
;
;	Exit	A = 0, load sucessful.
;		Z bit = 1, load successful.

BCPM:	CALL	HOME		;Force buffer flush
	mvi	C,'A'-'A'	;Select Disk 'A'
	lxi	D,0		;Force disk type determination
	CALL	SELDSK
	MOV	A,L
	ORA	H
	JZ	BCPME		;If drive not selected
	LDA	SEKTYP		;Get disk type
	RAR			;Remove sided bit
	DCR	A
	JM	BCPME		;If invalid boot type

	MOV	C,A
	lxi	H,BSECT	;Boot sector table
	DAD	B
	mov	A,M		;Get number of sectors
	STA	NUMSEC
	lxi	H,CCP		;Set buffer address
	shld	DMAADR
	MVI	A,CXADR		;Set extended address
	sta	DMAADE
	LDA	SEKTYP
	ani	1b		;Mask sided bit
	Adi	1
	mvi	H,0
	MOV	L,A
	shld	SEKTRK	;Set track
	MVI	A,0		;Set boot sector
	sta	SAVSEC

	CALL	SETACT		;Move SEK to ACT
	mvi	A,F$RDAT+040h	;Read data
	CALL	FINAL
	JNZ	BCPME		;If read erros
	mvi	A,1
	sta	NUMSEC
	LDA	SEKTYP
;+	CMP	DSKD5
	cpi	dskd5
	JC	BCPM1		;If not 1024 byte
	lxi	H,CCP+5*1024-512
	SHLD	BUFADR
	mvi	A,7
	STA	ACTSEC
	mvi	A,F$RDAT+040h	;Read data
	JMP	FINAL

BCPM1:	XRA	A		;Clear error indicator
	RET

BCPME:	lxi	B,500
	CALL	DELAY		;Delay 500 milli-seconds
	ORI	1		;Set error indicator
	RET

BSECT:	DB	22*256/256	;double 256 byte sectors
	DB	22*256/512	;double 512 byte sectors
	DB	22*256/1024	;double 1024 byte sectors
	IF	HARD
	DB	22*256/256	;SA 1000 hard disk
	DB	22*256/512	;Memorex 8 inch
	ENDIF
;end of gbcbios5.asm
;File is GBCBIOS6.ASM
;	page
;	Read a CP/M 128 byte sector.
;
;	EXIT	A = 0, successful read operation.
;		A = 1, unsucessful read operation.
;		Z bit = 1, successful read operation.
;		Z bit = 0, unsuccessful read operation.

READ:	CALL	CHKBKD		;Check for blocked drive
	MVI	A,F$RDAT	;Read from single density floppy
	JC	FINAL		;If non-blocked transfer

	XRA	A		;Set flag to force a read
	STA	UNACNT		;Clear sector counter
	CALL	FILL		;Fill buffer with data
	POP	H
	POP	D

	IF	C8080
	MVI	C,128
	CALL	MOVDTA		;Move 128 bytes
	endif
	IF	Z80
	LXI	B,128		;Move 128 bytes
;	LDIR
	db	0edh,0b0h
	endif

	LDA	ERFLAG
	ORA	A
	RZ			;If no error
	XRA	A
	STA	HSTACT		;Clear host active
	ORI	001h		;Set error flag
	RET
;	space	4,20
;	Write the selected 128 byte CP/M sector.
;
;	ENTRY	C = 0, write to a previously allocated block.
;		C = 1, write to the directory.
;		C = 2, write to the first sector of unallocated
;		data block.
;
;	EXIT	A = 0, write was successful.
;		A = 1, write was unsucessful.
;		Z bit = 1, write was successful.
;		Z bit = 0, write was unsucessful.

WRITE:	CALL	CHKBKD		;Check for blocked drive
	MVI	A,F$WRT		;Write to single density floppy
	JC	FINAL		;If non-blocked transfer

	MOV	A,C		;Write type in c
	STA	WRTYPE
	CPI	WRUAL
	JNZ	WRIT2		;If write to allocated
	LDA	SEKTYP
	CPI	MAXFTP
	MVI	A,2048/128
	JC	WRIT1		;If floppy disk
	MVI	A,4096/128

WRIT1:	STA	UNACNT
	LHLD	SEKTRK
	SHLD	UNATRK		;UNATRK = SEKTRK
	LDA	LOGSEC
	INR	A
	JMP	WRIT3

WRIT2:	LDA	UNACNT
	ORA	A
	JZ	WRIT4		;If no unallocated records
	DCR	A
	STA	UNACNT
	LDA	SEKTYP
	RAR
	MOV	L,A
	MVI	H,0
	LXI	D,LSITT-1
	DAD	D
	LDA	UNASEC		;Increment logical sector
	INR	A
	CMP	M		;Last sector in track table
	JNZ	WRIT3		;If not end of track
	LHLD	UNATRK
	INX	H
	SHLD	UNATRK
	XRA	A

WRIT3:	STA	UNASEC
	MVI	A,0FFh

WRIT4:	CALL	FILL
	POP	D
	POP	H

	IF	C8080
	MVI	C,128
	CALL	MOVDTA		;Move 128 bytes
	endif
	IF	Z80
	LXI	B,128
;	LDIR
	db	0edh,0b0h
	endif

	MVI	A,1
	STA	HSTWRT		;HSTWRT = 1
	LDA	ERFLAG
	ORA	A
	RNZ			;If any errors occurred

	LDA	WRTYPE		;write type
	CPI	WRDIR		;to directory?
	CZ	FLUSH		;Force write of directory
	LDA	ERFLAG
	ORA	A
	RET

LSITT:	DB	2*26		;Double 256 byte
	DB	4*15		;Double 512 byte
	DB	 8*8		;Double 1024 byte
	DB	2*32		;Shugart 8 inch (256 byte)
	DB	4*21		;Memorex 8 inch (512 bye)
;	page
;	TREAD - Determine floppy disk type.
;
;	ENTRY	C = Selected drive.
;
;	Exit	Zbit set = no error
;		A = disk type (0-3)

TREAD:	MOV	A,C
	ADI	'A'
	STA	NRDYM2		;Set drive into message
	CALL	SPECIFY		;Set disk parameters
	lxi	b,240		;Time delay for selecting sides
	call	delay
	LDA	SEKDSK		;Move drive to command buffer
	STA	ACTDSK		;Set into ACTDSK
	lxi	H,DSTS
	mvi	B,DSTSL
	CALL	EXECP		;Perform command
	mvi	B,1
	CALL	GCMPS		;Get the one status byte
	ANI	020h		;Mask ready bit
	JNZ	TRD1		;If drive is ready
	lxi	H,NRDYM1
	CALL	PRINT
	ORI	0FFh		;Clear zero flag
	RET

TRD1:	LDA	TEMPBF		;Get status byte
	ANI	008h		;Mask TS bit
	RRC
	RRC
	RRC
	STA	SEKTYP		;Save sided flag
	lxi	H,RECAL	;Do a test seek
	mvi	B,LRECAL
	CALL	MOVETO		;Process command
	RNZ			;If error

	mvi	A,2		;Seek to track two
	CALL	DOSEEK		;Do seek
	RNZ			;If error

	mvi	A,F$DRID
	STA	DRID
TRD2:	lxi	H,DRID
	mvi	B,DRIDL
	mvi	C,7
	CALL	EXECX		;Process command
	JZ	TRD3		;If read valid
	LDA	DRID
	XRI	040h		;Compliment MFM bit
	STA	DRID
	ANI	040h
	JNZ	TRD2		;If MFM not tried
	ORI	0FFh
	RET

TRD3:	LDA	TEMPBF+6	;Get number of bytes
	ADD	A
	MOV	B,A
	LDA	SEKTYP
	ORA	B		;Combine N with sided flag
	CMP	A		;Set zero flag
	RET

DSTS:	DB	F$DSTS,0
DSTSL:	equ	$-DSTS

RECAL:	DB	F$RECA,0
LRECAL:	equ	$-RECAL

DRID:	DB	F$DRID,0
DRIDL:	equ	$-DRID

NRDYM1:	DB	CR,LF,'Drive '
NRDYM2:	DB	'x'
	DB	' not ready.',0
;	page
;	FILL - fill host buffer with approprite host sector.
;
;	ENTRY	A = 0, Read required if not in buffer.
;		0therwise read not required.
;
;	EXIT	On exit the stack will contain the following
;		values:
;	      POP     x         ;x = host record address.
;	      POP     y		;y = caller's buffer address.

FILL:	STA	RDFLAG		;Save read flag
	LDA	SEKTYP		;Get disk type
	CPI	MAXFTP+1
	JC	FILL1		;If floppy disk
	SUI	DSK8S1-2

FILL1:	RRC			;divide by 2
	ANI	3h
	MOV	B,A		;B = log base 2 (sector size) - 7
	LXI	D,HSTBUF	;initial offset
	LXI	H,128		;128 byte records
	LDA	SEKSEC		;Get logical sector
FILL2:	XCHG
	RRC
	JNC	FILL3		;If low bit not set
	DAD	D		;Add bias to offset
FILL3:	XCHG
	DAD	H
	ANI	07Fh		;Mask sector
	DCR	B
	JNZ	FILL2		;If not all bits checked
	STA	SEKSEC
	LHLD	DMAADR
	XTHL			;Set return parameters
	PUSH	D
	PUSH	H		;Set return address

	LXI	H,HSTACT	;host active flag
	MOV	A,M
	MVI	M,1		;always becomes 1
	ORA	A
	JZ	FILL6		;If host buffer inactive
	LXI	H,HSTSEC
	LXI	D,SEKSEC
	MVI	C,SEKTYP-SEKSEC+1
FILL4:	LDAX	D
	CMP	M
	JNZ	FILL5		;If mis-match
	INX	H
	INX	D
	DCR	C
	JNZ	FILL4		;If all bytes not checked
	RET

FILL5:	CALL	FLUSH		;Flush host buffer

FILL6:	LHLD	SEKDSK		;Move disk and type
	SHLD	HSTDSK
	SHLD	ACTDSK
	LHLD	SEKTRK
	SHLD	HSTTRK
	SHLD	ACTTRK
	LDA	SEKSEC
	STA	HSTSEC
	STA	ACTSEC
	LDA	RDFLAG
	ORA	A
	RNZ			;If no read required

	MVI	A,F$RDAT+040h	;Read double density
	JMP	BLKXFR
;	space	4,10
;	FLUSH - Write out active host buffer onto disk.

FLUSH:
	LXI	H,HSTWRT
	MOV	A,M
	ORA	A
	RZ			;If host buffer already on disk
	MVI	M,0
	LHLD	HSTDSK		;Move disk and type
	SHLD	ACTDSK
	LHLD	HSTTRK
	SHLD	ACTTRK
	LDA	HSTSEC
	STA	ACTSEC
	MVI	A,F$WRT+040h	;Write double density
;	JMP	BLKXFR
;	space	4,10
;	BLKXFR -- blocked mode transfer.
;
;	ENTRY	A = command.

BLKXFR:	MOV	C,A
	LXI	H,HSTBUF	;Set buffer address
	SHLD	BUFADR
	MVI	A,BXADR
	STA	BUFADE
	MOV	A,C
;	JMP	FINAL
;	space	4,10
;	F I N A L   --  Preform final transfer processing.
;
;	ENTRY	A = Command.

FINAL:	CALL	PRCDCH		;Process command, drive, cylinder
	lxi	H,CIOPB+0	;Set buffer address
	mov	m,C		;Set command
	INX	H
	mov	m,B		;Set drive
	INX	H
	mov	m,E		;Set cylinder
	INX	H
	mov	m,D		;Set head
	INX	H
	MOV	E,A		;Save N field
	LDA	ACTSEC		;Get sector
	MOV	C,A
	INR	A
	mov	m,A		;Set beginning sector
	INX	H
	MOV	A,E		;Get type
	CPI	4
	JP	HDFNL		;If hard disk
	mov	m,A		;Set N field
	INX	H
	ADD	A		;N*2
	ADI	CMDTYP and 0ffh
	MOV	E,A
	MVI	A,0
	ACI	CMDTYP/256
	MOV	D,A
	LDA	NUMSEC		;Compute ending sector number
	ADD	C
	mov	m,A		;Set EOT
	INX	H
	LDAX	d
	mov	m,A		;Set GPL field
	INX	D
	INX	H
	LDax	d
	mov	m,A		;Set DTL

	MVI	A,MRTRY		;Set retry count
FNL1:	STA	RTRY		;Clear retry count
	LDA	CIOPB+2		;Get cylinder number
	CALL	DOSEEK		;Seek to proper track
	JNZ	FNL3		;If seek error

	lxi	H,BUFADE
	mvi	B,3
FNL2:	mov	A,m		;get ext adr
	OUT	DMA
	Dcx	H		;data is backward in memory
	dcr	B
	JNZ	FNL2		;If not all 3 bytes

	lxi	H,CIOPB
	mvi	B,CIOPL		;Set command buffer length
	mvi	C,7
	CALL	EXEC		;perform operation
	Cpi	40h
	JNZ	FNL3		;If error
	LDA	TEMPBF+1
	SUI	80h
	STA	ERFLAG
	RZ			;If no errors

FNL3:	LDA	RTRY		;Get retry counter
	DCR	A
	JNZ	FNL1		;If not permanent error
	ORI	01h
	STA	ERFLAG		;Set error flag
	RET

;	HDFNL -- Hard disk final command processing.
;

HDFNL:
	IF	NOHRD 
	CALL	HDSEL
	STA	ERFLAG
	RNZ			;If select error
	MVI	A,MRTRY		;Set retry count
HDFNL1:	STA	RTRY
	CALL	HDSEEK		;Seek to correct track
	CALL	HDXFER		;Perform hard disk transfer
	STA	ERFLAG
	RZ			;If no errors
	LDA	RTRY
	DCR	A
	JNZ	HDFNL1		;If attempts left
	LDA	ACTDSK
	MVI	D,0
	MOV	E,A
	LXI	H,HDCYL
	DAD	D
	MVI	M,(-1)		;Force track zero seek
	endif
	XRA	A
	ORI	001h
	STA	ERFLAG
	RET

RTRY:	DB	0
MRTRY:	EQU	10		;Maximum retry count

;	Command buffer disk type dependent values.

CMDTYP:	;	GPL  DTL
	DB	007h,128	;Single density
	DB	00Eh,255	;Double density 256 bytes
	DB	01Bh,255	;Double density 512 bytes
	DB	035h,255	;Double density 1024 bytes
;	space	4,10
;	PRCDCH -- Process Command, Drive, Cylinder, and Head.
;
;	ENTRY	A = command.
;
;	EXIT	A = N field (0..4).
;		B = drive.
;		C = command.
;		D = head.
;		E = cylinder.

PRCDCH:	MOV	C,A		;Save Command
	LDA	ACTDSK
	MOV	B,A
	LHLD	ACTTRK		;Get track number
	LDA	ACTTYP		;Get type
	CPI	MAXFTP+1
	JNC	CDCH2		;If hard disk
	XCHG
	MOV	H,A		;Save type
	ANI	1
	JZ	CDCH1		;If single sided
	MOV	A,E
	ANI	1
	MOV	D,A		;Set head
	RLC
	RLC
	ORA	B		;Combine head with drive
	MOV	B,A
	MOV	A,E		;Adjust track for cylinder
	RAR
	MOV	E,A

CDCH1:	MOV	A,H
	ANI	0FEh		;Remove sided bit
	RRC
	RET

CDCH2:
	IF	NOHRD 
	MOV	A,L
	ANI	003h
	MOV	D,A		;Save head
	DAD	H		;*2
	DAD	H		;*4
	DAD	H		;*8
	DAD	H		;*16
	MOV	A,L		;head * 16
	CMA
	ANI	030h
	ORA	B		;Combine with drive
	MOV	B,A
	DAD	H		;*32
	DAD	H		;*64
	MOV	E,H		;track*64/256 = track/4
	MOV	A,C
	ANI	00Fh
	CPI	F$RDAT
	MVI	A,4
	MVI	C,H$RSDT
	RZ			;If read command
	MVI	C,H$WSDT
	ENDIF
	RET
;	space	4,10
;	Seek to specified Track/Sector
;
;	Entry	A = Track

DOSEEK:
	sta	DSEKC+2
	lxi	H,DSEKC
	mvi	B,DSEKL
;	JMP	MOVETO


;	Move head according to command.
;
;	ENTRY	HL = address of command buffer.
;		B = length of command buffer.
;
;	Exit	Z bit set if no error.

MOVETO:
	CALL	EXECP		;Perform seek
MVTO1:	IN	INTS
	ORA	A
	JP	MVTO1		;if not complete
	mvi	A,F$RSTS
	OUT	FDCD		;request status
	mvi	B,2
	CALL	GCMPS		;Get status
	Cpi	20h
	RZ   			;If seek complete
	LDA	TEMPBF		;Get true status byte
	ANI	3h		;Mask disk unit
	MOV	C,A
	LDA	ACTDSK
	CMP	C
	JNZ	MVTO1		;If not proper unit
	ORI	001h		;Clear zero flag
	RET
;	space	4,10
;	SPECIFY - Specify disk drive characteristics.

SPECIFY:
	lxi	H,SPEC+1
	mvi	B,LSPEC
	mvi	C,0
	LDA	STEPMS
	ORI	HUT
	mov	m,A
	dcx	H
	JMP	EXEC		;Specify disk command

DSEKC	DB	F$SEEK,0,0
DSEKL:	EQU	$-DSEKC

SPEC	DB	F$SPEC
;+	VFD	4\SRT,4\HUT
	db	(srt shl 4)+hut
;+	VFD	7\HLT,1\ND
	db	(hdlt shl 1)+nd
LSPEC	equ	$-SPEC
;	page
;	E X E C
;	Entry	HL = FWA of command buffer.
;		B  = # of bytes to output
;		C  = # of bytes for status
;
;	Exit	If C <> 0 then see GCMPS.

EXECP:	mvi	C,0		;Set no status byte

EXECX:	INX	H
	LDA	ACTDSK		;Set drive into command buffer
	MOV	M,A
	DCX	H

EXEC:
EXEC1:	IN	FDCS
	ORA	A
	JP	EXEC1		;if no master ready bit
	mov	A,m		;command byte
	OUT 	FDCD		;to controller
	INx	H
	DCR	B
	JNZ	EXEC1		;if more bytes
	MOV	A,C		;# of status bytes+1
	ORA	A
	RZ			;if no status bytes
	MOV	B,C		;# of status bytes

EXEC2:	IN	INTS
	ORA	A
	JP	EXEC2		;If operation not complete
;	space	4,10
;	Get completion status.
;
;	Entry	B= # of status bytes to read
;
;	Exit	TEMPBF = status bytes read in.
;		A = [TEMPBF] and 0F8h.
;		Flags set according to above value in A.

GCMPS:
	lxi	H,TEMPBF	;Set status buffer address
GCMPS2:	IN	FDCS
	ORA	A
	JP	GCMPS2		;if not ready
	IN	FDCD		;Get status byte
	mov	m,A
	INx	H
	Dcr	B		;decrement counter
	JNZ	GCMPS2		;wait until all done
	LDA	TEMPBF		;Get first status byte
	ANI	0F8h
	RET
;end of gbcbios6.asm
;File is BIOS8.asm
;	page
	If	C8080
;	MOVDTA	- Move data in memory.
;
;	ENTRY	C = number of bytes to move
;		DE = destination address.
;		HL = source address.

MOVDTA:
	MOV	A,M		;Source character
	STAX	D		;to destination
	INX	H
	INX	D
	DCR	C		;loop 128 times
	JNZ	MOVDTA		;If transfer not complete
	RET
	ENDIF
;	space	4,10
;	Check blocked disk transfer.
;
;	EXIT	Cbit set, unblocked device.
;		Cbit clear, blocked device.

CHKBKD:
	XRA	A
	STA	ERFLAG		;Clear error flag
	LDA	SEKTYP
	CPI	DSKD1
	JC	CBKD2		;If not blocked device
	CPI	MAXFTP+1
	JNC	CBKD1		;If hard disk
	LHLD	SEKTRK
	MOV	A,H
	ORA	L
	MVI	A,DSKS1		;A = Single density
	JZ	CBKD2		;If zero force non-blocked

CBKD1	LDA	SAVSEC
	STA	SEKSEC
	XRA	A		;Clear carry flag
	RET

SETACT:	LDA	SEKTYP
CBKD2:	STA	ACTTYP		;Set actual disk type
	LHLD	DMAADR
	SHLD	BUFADR
	LDA	DMAADE
	STA	BUFADE

	LDA	SEKDSK
	STA	ACTDSK
	LHLD	SEKTRK
	SHLD	ACTTRK
	LDA	SAVSEC
	STA	ACTSEC
	STC			;Set carry flag
	RET
;	space	4,10
;	Delay	Delay the millisecond count contained in BC.
;
;	Destroys  A and flags.

DELAY:	MVI	A,DELCNT/26
DLAY1:	DCX	B
	INX	B
	DCR	A
	JNZ	DLAY1		;If not 1 millisecond
	DCX	B
	MOV	A,C
	ORA	B
	JNZ	DELAY		;If not desired count
	RET
;	space	4,10
;	Print message terminated by zero byte.
;
;	ENTRY	HL -> message buffer, terminated by zero.
;
;	EXIT	HL -> zero byte + 1.
;		A = 0.
;		Z bit set.
;
;	Destroys only HL, Flags, and A registers.

PRINT:	MOV	A,M		;Get a character
	ORA	A
	INX	H
	RZ			;If zero the terminate
	PUSH	B
	MOV	C,A
	CALL	J$COUT		;Output to the console
	POP	B
	JMP	PRINT
;end of bios8.asm
;FILE is BIOS9.asm
	page
;	Physical data buffer address ((DMAADR) or HSTBUF)

BUFADR:	DW	0		;Lower 16 bits (least, middle)
BUFADE:	DB	0		;Extended address

;	User data buffer address

DMAADR:	DW	0		;Lower 16 bits (least, middle)
DMAADE:	DB	0		;Extended address
;	space	4,10
;	BIOS blocking / deblocking flags.

HSTACT:	DB	0		;host active flag
HSTWRT:	DB	0		;host written flag
UNACNT:	DB	0		;unalloc rec CNT
UNATRK:	DW	0		;Track
UNASEC:	DB	255		;Sector
LOGSEC	DB	0		;Logical sector
;	space	4,10
;	Area for storage of hard disk cylinders.

HDCYL:
;+	ECHO	NOHRD/2
;+	DB	-1
;+	ENDM
	if	hard
	db	(-1),(-1),(-1),(-1)
	endif
;	space	4,10
;	CP/M disk work space.

ALV:	DS	ALVS
CSV:	DS	CSVS
;	page
;	Disk access information.
;	This area is organized into the following groups
;		sector number
;		track number
;		disk drive
;		drive type
;	Each of these groups has three cells for the
;	current disk request, ACTual disk transfer,
;	and active host disk.

SEKSEC:	DS	1		;Current request
SEKTRK:	DS	2		;Current request
SEKDSK:	DS	1		;Current request
SEKTYP:	DS	1		;Current disk's type

ACTSEC:	DS	1		;Actual transfer operation
ACTTRK:	DS	2		;Actual transfer operation
ACTDSK:	DS	1		;Actual transfer operation
ACTTYP:	DS	1		;Actual disk's type

HSTSEC:	DS	1		;Active host disk
HSTTRK:	DS	2		;Active host disk
HSTDSK:	DS	1		;Active host disk
HSTTYP:	DS	1		;Active disk's type
;	space	4,10
;	Disk transfer flags and counters.

RDFLAG:	DS	1		;Read flag
ERFLAG:	DS	1		;Error reporting
WRTYPE:	DS	1		;Write operation type

SAVSEC:	DS	1		;Save sector
NUMSEC:	DS	1		;Number of sectors

CIOPL:	EQU	9
CIOPB:	DS	CIOPL		;Disk command buffer
TEMPBF:	DS	8		;Result status cells
ESPACE:


	ORG	LWAMEM-HSTSIZ-128

DIRBUF:	DS	128		;Directory buffer
HSTBUF:	DS	HSTSIZ-1		;Host buffer


;	MSG	'Available exspansion = $',DIRBUF-ESPACE,' bytes.'

;	TITLE	'Cross Reference Listing.'
