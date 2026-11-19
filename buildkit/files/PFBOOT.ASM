; PFBOOT - Pascal-100 hard boot for the CompuPro Disk 1.
; Track 0 sector 1. The Disk 1 EPROM has already done SPECIFY and
; RECALIBRATE, read sectors 1-4 to 0100H, and entered here at 0100H.
; This reads sectors 2-21 (SBQB + BIOS) to 280H, then sectors 22-26
; (part 1 of the Pascal boot) to 5000H, and jumps to 280H. Track 0 is
; FM on SD and DD disks, so part 1 is read here. PSBQB does not read it.
; The Disk 1 has no terminal count, so a good read ends with ST0=4xH
; and ST1=80H (end of cylinder).
FDCS	EQU	0C0H		;8272 main status
FDCD	EQU	0C1H		;8272 data
DMA	EQU	0C2H		;DMA address (write), INT status (read)
INTS	EQU	0C2H
LOADP	EQU	280H		;SBORG, the soft boot origin
PBORG	EQU	5000H		;load point of the Pascal boot
;
	ORG	100H
	JMP	START
DATA:	DB	0,LOADP SHR 8,LOADP AND 0FFH	;DMA: extended, high, low
CMD:	DB	06H,0,0,0,2,0,21,7,128		;READ T0 S2..S21
RES:	DS	7
START:	CALL	RD
	MVI	A,PBORG SHR 8
	STA	DATA+1
	XRA	A
	STA	DATA+2
	MVI	A,22
	STA	CMD+4
	MVI	A,26
	STA	CMD+6
	CALL	RD
	JMP	LOADP
;
RD:	LXI	H,DATA
	MVI	B,3
ADDR:	MOV	A,M
	OUT	DMA
	INX	H
	DCR	B
	JNZ	ADDR
	MVI	B,9
SEND:	IN	FDCS
	ORA	A
	JP	SEND
	MOV	A,M
	OUT	FDCD
	INX	H
	DCR	B
	JNZ	SEND
WAIT:	IN	INTS
	ORA	A
	JP	WAIT
	LXI	H,RES
	MVI	B,7
GET:	IN	FDCS
	ORA	A
	JP	GET
	IN	FDCD
	MOV	M,A
	INX	H
	DCR	B
	JNZ	GET
	LDA	RES
	ANI	0C0H
	CPI	40H
	JNZ	RD
	LDA	RES+1
	CPI	80H
	JNZ	RD
	RET
	END
