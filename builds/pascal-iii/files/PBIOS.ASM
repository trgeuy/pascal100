; PBIOS - Pascal-100 BIOS for CompuPro Interfacer 1 and Disk 1.
; Structure from the Digicomp sample (BIOS.ASM, disk 032): origin 480H,
; CP/M entry points, BOOT and WBOOT stubs, A=0 good and A=1 permanent
; error. Console and disk sections are new.
; Formats: UCSD standard (77 x 26 x 128, FM) and DD (FM track 0, then
; 76 x 15 x 512 MFM), 2 drives. Manual V.D.10. A two-sided DD disk has
; 153 tracks: track 0 FM on side 0, then the sides in turn (PHYS).
; Pascal calls SELDSK only when the drive changes (measured). It does
; one READ ID at the current track: no trial read, and no seek unless
; the head is on track 0 or lost. A disk swapped in the selected drive
; is seen after the other drive is used.
BIOSORG	EQU	480H
;
; Interfacer 1 channel A. Status at base+1. Channel B is FujiNet.
COND	EQU	0		;console data
CONS	EQU	1		;console status
TBMT	EQU	01H		;transmit buffer empty
DAV	EQU	02H		;data available
;
; Disk 1
FDCS	EQU	0C0H		;8272 main status, bit 7 = RQM
FDCD	EQU	0C1H		;8272 data
DMAP	EQU	0C2H		;DMA address (write)
INTS	EQU	0C2H		;bit 7 = INT (read)
RETRIES	EQU	10
NDRV	EQU	2
TOUT	EQU	8		;INT wait, about 1 s per count at 3 MHz
;
	ORG	BIOSORG
	JMP	BOOT
	JMP	WBOOT
	JMP	CONST
	JMP	CONIN
	JMP	CONOUT
	JMP	LIST
	JMP	PUNCH
	JMP	READER
	JMP	HOME
	JMP	SELDSK
	JMP	SETTRK
	JMP	SETSEC
	JMP	SETDMA
	JMP	READ
	JMP	WRITE
	JMP	LISTST
	JMP	SECTRAN
;
BOOT:
WBOOT:	RET			;never called (manual V.D.3)
;
; ---- Console ----
CONST:	IN	CONS
	ANI	DAV
	RZ
LISTST:	MVI	A,0FFH
	RET
CONIN:	IN	CONS
	ANI	DAV
	JZ	CONIN
	IN	COND
	ANI	7FH
	RET
CONOUT:	IN	CONS
	ANI	TBMT
	JZ	CONOUT
	MOV	A,C
	OUT	COND
	RET
;
; ---- No printer, punch or reader: channel B is FujiNet ----
LIST:
PUNCH:	RET
READER:	MVI	A,1AH
	RET
;
; ---- Disk entry points ----
HOME:	MVI	C,0
SETTRK:	MOV	A,C
	STA	TRACK
	XRA	A
	RET
SELDSK:	MOV	A,C
	CPI	NDRV
	JNC	BAD
	STA	DRIVE
	CALL	DSP
	MVI	M,0		;one side until TWOS says two
	CALL	DETECT
	JNZ	SELBAD
	CALL	DENP
	CPI	2
	LXI	H,SDBLK
	JNZ	SEL1
	CALL	TWOS
	LXI	H,DDBLK
	JNZ	SEL1
	CALL	DSP
	MVI	M,1
	LXI	H,DSBLK
SEL1:	XRA	A
	RET
SELBAD:	CALL	DENP
	MVI	M,0		;density not known
	LXI	H,0
	JMP	BAD
;
; DETECT: find the density of this drive. Z = good, DENTAB is set.
; Track 0 is FM on all disks, so read an ID from track 1 or higher.
; Known density first. A new drive tries FM first, as the CP/M TREAD.
DETECT:	CALL	TRKP
	CPI	0FFH
	JZ	DT1
	ORA	A
	JNZ	DT2
DT1:	LDA	TRACK		;keep the track that was set
	PUSH	PSW
	MVI	A,1
	STA	TRACK
	CALL	SEEK
	POP	B		;B = old track, flags stay
	MOV	A,B
	STA	TRACK
	RNZ
DT2:	CALL	DENP
	CPI	2
	JZ	DT3
	CALL	TFM
	RZ
	JMP	TMFM
DT3:	CALL	TMFM
	RZ
TFM:	MVI	A,0AH		;READ ID, FM
	CALL	RDID
	RNZ
	LDA	RES+6		;N
	ORA	A		;128 only
	RNZ
	MVI	A,1
	JMP	SETD
TMFM:	MVI	A,4AH		;READ ID, MFM
	CALL	RDID
	RNZ
	LDA	RES+6		;N
	CPI	2		;512 only: a CP/M DD disk has N=3
	RNZ
	MVI	A,2
SETD:	MOV	B,A
	CALL	DENP
	MOV	M,B
	XRA	A
	RET
; TWOS: SENSE DRIVE STATUS. Z = two-sided disk (ST3 bit 3).
TWOS:	MVI	A,04H
	CALL	CMD2
	RNZ
	MVI	B,1
	CALL	RESULT
	RNZ
	LDA	RES
	ANI	08H
	XRI	08H
	RET
; PHYS: TRACK to PCYL and PHEAD. Two sides: cylinder = track/2, side =
; bit 0. Track 0 stays FM side 0. TRKTAB holds cylinders.
PHYS:	LDA	TRACK
	MOV	B,A
	CALL	DSP
	ORA	A
	MOV	A,B
	MVI	C,0
	JZ	PH1
	ANI	1
	MOV	C,A
	MOV	A,B
	ORA	A
	RAR
PH1:	STA	PCYL
	MOV	A,C
	STA	PHEAD
	RET
; RDID: READ ID, command in A. Z = an ID was read.
RDID:	CALL	CMD2
	RNZ
	CALL	WAITI
	RNZ
	MVI	B,7
	CALL	RESULT
	RNZ
	LDA	RES
	ANI	0C0H
	RET
; CMD2: send command A and the drive. Z = good.
CMD2:	STA	SKC
	CALL	DRAIN
	RNZ
	LXI	H,SKC
	MVI	B,2
	JMP	SEND
SETSEC:	MOV	A,C
	STA	SECT
	RET
SETDMA:	MOV	H,B
	MOV	L,C
	SHLD	DMAADD
	RET
SECTRAN:
	XCHG			;HL = table
	DAD	B
	MOV	L,M
	MVI	H,0
	RET
READ:	MVI	A,06H
	JMP	RW
WRITE:	MVI	A,05H
RW:	STA	CMD
	MVI	A,RETRIES
	STA	TRIES
RW1:	CALL	SEEK
	JNZ	RETRY
	CALL	DRAIN
	JNZ	RETRY
	LHLD	DMAADD		;DMA: extended, high, low
	XRA	A
	OUT	DMAP
	MOV	A,H
	OUT	DMAP
	MOV	A,L
	OUT	DMAP
	LDA	PHEAD
	STA	CMD+3
	RLC
	RLC
	MOV	B,A
	LDA	DRIVE
	ORA	B
	STA	CMD+1
	LDA	PCYL
	STA	CMD+2
	LDA	SECT
	STA	CMD+4
	STA	CMD+6		;EOT = this sector
	CALL	DENP
	CPI	2
	JNZ	RWFM
	LDA	TRACK
	ORA	A
	JZ	RWFM		;track 0 is FM on a DD disk too
	LDA	CMD
	ORI	40H		;MFM
	STA	CMD
	MVI	A,2		;N: 512
	STA	CMD+5
	MVI	A,1BH		;GPL
	STA	CMD+7
	MVI	A,0FFH		;DTL
	JMP	RWGO
RWFM:	XRA	A		;N: 128
	STA	CMD+5
	MVI	A,7		;GPL
	STA	CMD+7
	MVI	A,128		;DTL
RWGO:	STA	CMD+8
	LXI	H,CMD
	MVI	B,9
	CALL	SEND
	JNZ	RETRY
	CALL	WAITI
	JNZ	RETRY
	MVI	B,7
	CALL	RESULT
	JNZ	RETRY
	LDA	RES
	ANI	0C0H
	CPI	40H
	JNZ	RETRY
	LDA	RES+1
	CPI	80H		;only end of cylinder
	JNZ	RETRY
	LDA	RES+2
	ORA	A
	JNZ	RETRY
	XRA	A		;good
	RET
RETRY:	CALL	LOST		;recalibrate before the next try
	LXI	H,TRIES
	DCR	M
	JNZ	RW1
BAD:	MVI	A,1		;permanent error
	ORA	A
	RET
;
; LOST: forget the head position of this drive.
LOST:	CALL	TRKP
	MVI	M,0FFH
	RET
; TRKP: HL = this drive's cell in TRKTAB, A = its value.
TRKP:	LXI	H,TRKTAB
	JMP	DRVP
; DSP: HL = this drive's cell in DSTAB, A = its value. 1 = two sides.
DSP:	LXI	H,DSTAB
	JMP	DRVP
; DENP: HL = this drive's cell in DENTAB, A = its value.
; 0 = not known, 1 = FM, 2 = MFM.
DENP:	LXI	H,DENTAB
DRVP:	LDA	DRIVE
	MOV	E,A
	MVI	D,0
	DAD	D
	MOV	A,M
	RET
;
; SEEK: put the head on TRACK. Z = good.
SEEK:	CALL	PHYS
	CALL	TRKP
	CPI	0FFH
	JNZ	SK1
	MVI	A,07H		;RECALIBRATE
	STA	SKC
	MVI	B,2
	CALL	SKGO
	RNZ
	CALL	TRKP
	MVI	M,0
	XRA	A
SK1:	LXI	H,PCYL
	CMP	M
	RZ
	MVI	A,0FH		;SEEK
	STA	SKC
	MOV	A,M
	STA	SKC+2
	MVI	B,3
	CALL	SKGO
	RNZ
	CALL	TRKP
	LDA	PCYL
	MOV	M,A
	XRA	A
	RET
; SKGO: send B bytes of SKC, wait, SENSE INTERRUPT. Z = seek end, good.
SKGO:	PUSH	B
	CALL	DRAIN
	POP	B
	LXI	H,SKC
	CALL	SEND
	RNZ
	CALL	WAITI
	RNZ
	LXI	H,SNS
	MVI	B,1
	CALL	SEND
	RNZ
	MVI	B,2
	CALL	RESULT
	RNZ
	LDA	RES
	ANI	0F8H
	CPI	20H		;normal end, seek end
	RET
;
; DRAIN: clear a pending interrupt before a new command. A ready change
; on any drive (a disk swap) raises INT and holds a status for SENSE
; INTERRUPT. Without this, the wait for the next command ends at once,
; or the status of the wrong drive is read. Z = good.
DRAIN:	MVI	C,8		;at most 4 drives, with room
DR1:	IN	INTS
	ORA	A
	JP	WQ2		;no INT
	LXI	H,SNS
	MVI	B,1
	CALL	SEND
	RNZ
	CALL	WRQM
	RNZ
	IN	FDCD
	CPI	80H		;invalid: nothing was pending
	JZ	DR2
	CALL	WRQM
	RNZ
	IN	FDCD
DR2:	DCR	C
	JNZ	DR1
	XRA	A
	RET
;
; SEND: B bytes at HL to the 8272. Z = good.
SEND:	CALL	WRQM
	RNZ
	MOV	A,M
	OUT	FDCD
	INX	H
	DCR	B
	JNZ	SEND
	RET
; RESULT: B bytes from the 8272 to RES. Z = good.
RESULT:	LXI	H,RES
RES1:	CALL	WRQM
	RNZ
	IN	FDCD
	MOV	M,A
	INX	H
	DCR	B
	JNZ	RES1
	RET
; WRQM: wait for RQM. Z = ready, NZ = timed out.
WRQM:	LXI	D,0
WQ1:	IN	FDCS
	ORA	A
	JM	WQ2
	DCX	D
	MOV	A,D
	ORA	E
	JNZ	WQ1
	INR	A		;A=1, NZ
	RET
WQ2:	XRA	A
	RET
; WAITI: wait for INT. Z = done, NZ = timed out.
WAITI:	MVI	C,TOUT
	LXI	D,0
WI1:	IN	INTS
	ORA	A
	JM	WQ2
	DCX	D
	MOV	A,D
	ORA	E
	JNZ	WI1
	DCR	C
	JNZ	WI1
	INR	A
	RET
;
; ---- Data ----
TRACK:	DB	0
SECT:	DB	1
DMAADD:	DW	80H
TRIES:	DB	0
TRKTAB:	DB	0FFH,0FFH	;0FFH = position not known
DENTAB:	DB	0,0
DSTAB:	DB	0,0
PCYL:	DB	0
PHEAD:	DB	0
SNS:	DB	08H		;SENSE INTERRUPT
SKC:	DB	0,0,0		;op, drive, cylinder
DRIVE	EQU	SKC+1
CMD:	DB	0,0,0,0,0,0,0,7,128	;C H R N=0 EOT GPL DTL
RES:	DS	7
;
; ---- Disk Parameter Blocks, manual figure 5-8 ----
; QB keeps a word address, so a block must be at an even address.
	ORG	($+($ MOD 2))
SDBLK:	DW	0,76,1,1,26,128,0,6
	DB	0,22,5,1	;part 1, read by PFBOOT
	DB	1,1,8,2		;part 2
DDBLK:	DW	0,76,1,1,15,512,DTRANS,0
	DB	0,22,5,1	;part 1, read by PFBOOT
	DB	1,1,2,2		;part 2
DSBLK:	DW	0,153,1,1,15,512,DTRANS,0
	DB	0,22,5,1	;part 1, read by PFBOOT
	DB	1,1,2,2		;part 2
DTRANS:	DB	1,3,5,7,9,11,13,15,2,4,6,8,10,12,14
BIOSEND	EQU	$
	END
