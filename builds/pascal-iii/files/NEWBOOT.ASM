;+	TITLE	'Sorcim ColdStart Boot for GBC DISK1.'
;BOOT	ColdStart Boot load for CP/m 2.2
;			on CompuPro DISK1.
;
;	+-----------------------+
;	|			|
;	|	D i s k		|
;	|	B O O T 	|
;	|			|
;	+-----------------------+
;	Copyright 1981, Sorcim Corp.
;
;	This product is a copyright program product of
;	SORCIM and is supplied for use with the GBC 171
;	series controllers.
;
;	CompuPro			Sorcim Corp.
;	Oakland, CA			Santa Clara, CA
;
;
;	Version number:	2.2F
;	Version date:	1981 March 31
;
;	The following code is supplied to customers who
;	purchase a hard/floppy disk system from CompuPro.
;	The following code is written onto track 0 sector 0
;	of the hard disk.  This routine is read into memory
;	at location 0 by the user's PROM.   This routine then
;	loads the rest of the system into memory.
;
;
;	The format of the Floppy Disk Boot sectors are as follows:
;
;			Routine
;	T  Sector	Name
;
;	0  0 thru 3	Boot program (this routine)
;
;	   4 thru 25	BIOS
;
;	1  0 thru 7	CCP
;
;	   8 thru 21	BDOS
;
;	  22 thru 25	reserved
	
;+OPARM:	EQU	*o		;Capture O parameter
OPARM:	EQU	64		;Memory size in Kbytes, or Bios load address
VERS:	EQU	22

FALSE:	EQU	0
TRUE:	EQU	NOT FALSE




K:	EQU	1024
biosln:	equ	0D00h

;+	IF	OPARM < (64+2)	;If absolute
option:	set	false
	if	oparm/(64+1)
option:	set	true
	endif
	if 	not option

MSIZE:	EQU	OPARM		;Size of CP/M memory
CBIOS:	EQU	MSIZE*K-biosln	;Start of CP/M jump table
	ENDIF

;+	IF	OPARM > (64+1)	;If PRL generation
	if	option
MSIZE:	EQU	(OPARM+0E00h)/K	;Size of CP/M memory
CBIOS:	EQU	OPARM		;Start of CP/M jump table
	ENDIF
BDOS:	EQU	CBIOS-0E00h+6	;Start of BDOS
CCP:	EQU	CBIOS-1600h	;Start of CCP

OPTS:	EQU	40h		;Option selections byte



;	Assembly Constants
FDPORT	EQU	0C0H		;Base port address for Controller
FDCS	EQU	FDPORT		;Status register
FDCD	EQU	FDPORT+1	;Data register
DMA	EQU	FDPORT+2	;Dma address (when write)
INTS	EQU	FDPORT+2	;Status Register (when read)
;				Input on port disables boot rom.
SER	EQU	FDPORT+3	;Serial port

;+ DELCNT	=	5000		;Delay count
DELCNT	EQU	5000		;Delay count

;	Controller function definitions
;	Specify (00) command
;+ NSEC	=	0		;Sect verify number
;+ F.RTK	=	02		;Read track
;+ F.SPEC	=	03		;Specify
;+ F.DSTS	=	04		;Drive status
;+ F.RDAT	=	06		;Read data
;+ F.RECA	=	07		;recalibrate
;+ F.RSTS	=	08		;Read status
;+ F.SEEK	=	0Fh		;Seek
NSEC	equ	0		;Sect verify number
F$RTK	equ	02		;Read track
F$SPEC	equ	03		;Specify
F$DSTS	equ	04		;Drive status
F$RDAT	equ	06		;Read data
F$RECA	equ	07		;recalibrate
F$RSTS	equ	08		;Read status
F$SEEK	equ	0Fh		;Seek

;+ SRT	=	16-8		;= Shuggart 800s
SRT	equ	16-8		;= Shuggart 800s
;		16-3		;= Shuggart 850s
;		16-3		;= Remex
;+ HUT:	=	240/16		;Head unload = 240 ms
;+ HLT:	=	(35+1)/2	;Head load = 35 ms
;+ ND:	=	00		;Set DMA mode
HUT:	equ	240/16		;Head unload = 240 ms
HDLT:	equ	(35+1)/2	;Head load = 35 ms
ND:	equ	00		;Set DMA mode
;+	space	4,10




;	Bootstrap load.
;	Do not change any addresses from here to START:
;	Entry	C= Board switches from ROM (0 .. 3)
	ORG	100h
BOOT:
	JMP	START		;invariance
;+	space	4,10




;	Function data for controller to boot
DATA	DB	0		;Extended
;+ ENTRY:	DB	high CBIOS
ENTRY:	DB	CBIOS shr 8
;+	DB	low CBIOS
	DB	CBIOS and 0ffh
;+ LDMA	EQU	*-DATA
LDMA	EQU	$-DATA

;+ SPEC	DB	F.SPEC
SPEC	DB	F$SPEC
;+	VFD	4\SRT,4\HUT
;+	VFD	7\HLT,1\ND
	db	srt shl (8-4) + hut
	db	(hdlt shl (8-7)) + nd
;+ LSPEC	=	*-SPEC
LSPEC	equ	$-SPEC


;+RECAL	DB	F.RECA,0
;+LRECAL	=	*-RECAL
RECAL	DB	F$RECA,0
LRECAL	equ	$-RECAL


;+ READ:	DB	F.RDAT
READ:	DB	F$RDAT
	DB	0		;hds,ds1,ds0
	DB	0		;C = sector ID info
	DB	0		;Head
	DB	5		;Record (sector)
	DB	0		;N
	DB	26		;Read to end of track
	DB	7		;GPL
	DB	128		;DTL
;+LREAD	=	*-READ
LREAD	equ	$-READ



START:
	MOV	A,C		;save board options
;+	STO	A,OPTS
	sta	OPTS
RETRY:
;+	LDK	DE,DATA
	lxi	D,DATA
;+	LDK	B,LDMA
	mvi	B,LDMA

;	Output beginning DMA address

;+ ADDR:	LD	A,[de]
ADDR:	ldax	d
	OUT	DMA		;set DMA
	INX	D
	DCR	B
	JNZ	ADDR		;if NOT all 3 bytes


;	Load Specify Command

;+	LDK	B,LSPEC
	mvi	B,LSPEC
SPEC1:	IN	FDCS
;+	OR	A
	ora	A
	JP	SPEC1		;if no master ready bit
;+	LD	A,[de]		;load command byte
	ldax	d		;load command byte
	OUT 	FDCD		;to controller
	INX	D
	DCR	B
	JNZ	SPEC1		;if more bytes

;	Recalibrate drive

;+	LDK	B,LRECAL
	mvi	B,LRECAL
RCAL1:	IN	FDCS
;+	OR	A
	ora	A
	JP	RCAL1		;if no master ready bit
;+	LD	A,[de]		;load command byte
	ldax	d		;load command byte
	OUT 	FDCD		;to controller
	INX	D
	DCR	B
	JNZ	RCAL1		;if more bytes

RCAL2:	IN	INTS
	ORA	A
	JP	RCAL2		;If not complete

;+	MVI	A,F.RSTS
	MVI	A,F$RSTS
	OUT	FDCD

RCAL3:	IN	FDCS
	ORA	A
	JP	RCAL3
	IN	FDCD
	SUI	20h
	MOV	C,A
RCAL4:	IN	FDCS
	ORA	A
	JP	RCAL4
	IN	FDCD
	ORA	C
	JNZ	START


;	Now set-up read command

;+	LDK	B,LREAD
	mvi	B,LREAD
READ1:	IN	FDCS
;+	OR	A
	ora	A
	JP	READ1		;if no master ready bit
;+	LD	A,[de]		;load command byte
	ldax	d		;load command byte
	OUT 	FDCD		;to controller
	INX	D
	DCR	B
	JNZ	READ1		;if more bytes

READ2:	IN	INTS
	ORA	A
	JP	READ2		;If not complete

READ3:	IN	FDCS
	ORA	A
	JP	READ3
	IN	FDCD
	SUI	40h
	MOV	L,A
READ4:	IN	FDCS
	ORA	A
	JP	READ4
	IN	FDCD
	SUI	80h
	MOV	H,A

;+	LDK	B,7-2
	mvi	B,7-2
READ5:	IN	FDCS
;+	OR	A
	ora	A
	JP	READ5		;if not ready
	IN	FDCD		;read status
;+	DEC	B
	dcr	B
	JNZ	READ5		;wait until all done

	MOV	A,L
	ORA	H
	JNZ	RETRY		;if problems
;+	LD	HL,ENTRY
	lhld	ENTRY
	MOV	D,H
	MOV	H,L		;reverse
	MOV	L,D
;+	JMP	[hl]		;enter CBIOS
	pchl			;enter CBIOS


	END
