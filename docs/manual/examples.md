# Worked examples

These are complete sessions. **Every transcript below was captured from the program.** None of
them was typed from memory.

This chapter goes through the CP/M machine in `examples/` in detail. It shows something about
how the machine works, not only how to start it. Every folder in `examples/` has its own
README, in Markdown and as a PDF, that says what the machine is and what to type.

---

## 1. CompuPro CP/M from a floppy disk

```
$ pascal100 examples/cpm22/cpm22-console.toml
```

```
startup> RUN 0
[console -- ^E returns to the monitor]
CompuPro Systems
64K CP/M 2.2K

A>
```

### What is on the disk

`DIR` lists the files:

```
A>DIR
A: MAC      COM : Z80      LIB : LS       COM : R        COM
A: W        COM : HDIR     COM : FUJIDIR  COM : FUJIGET  COM
A: FUJIPUT  COM : WGET     COM : PIP      COM : ED       COM
A: SUBMIT   COM : XSUB     COM : ASM      COM : DDT      COM
A: LOAD     COM : STAT     COM : DUMP     COM : FORMAT   COM
A: COPY     COM : SYSGEN   COM
A>
```

There are 22 files: the Digital Research tools (`ASM`, `MAC` with `Z80.LIB`, `DDT`, `ED`,
`PIP`), the CompuPro `FORMAT` and `COPY`, `LS`, the host bridge tools `R`, `W` and `HDIR`, and
the FujiNet tools. `STAT *.*` gives the size of each file, and the space that is left:

```
A>STAT *.*

 Recs  Bytes  Ext Acc
   64     8k    1 R/W A:ASM.COM
   17     4k    1 R/W A:COPY.COM
   38     6k    1 R/W A:DDT.COM
   ...
   32     4k    1 R/W A:WGET.COM
    6     2k    1 R/W A:XSUB.COM
   84    12k    1 R/W A:Z80.LIB
Bytes Remaining On A: 488k
```

### Out, and back in

`Ctrl-E` stops the machine and gives you the monitor. Nothing is lost. `RUN` with no address
continues at the instruction that the machine was about to execute.

```
A>
STOP -- the machine is still at F4BD. RUN resumes.
C0Z1S0P1H1N0 A=00 BC=007F DE=DD06 HL=E80E SP=E837 IFF1=0 PC=F4BD  IN A,(01)
C0Z0S0P0H0N0 A'00 BC'0000 DE'0000 HL'0000 IX=0000 IY=0000 I=00 IM=00 IFF2=0
pascal100>
```

Look at the machine while it is stopped:

```
pascal100> BOARDS
pascal100> SHOW fd0
pascal100> DUMP 100
```

`BOARDS` shows the five boards of the default machine: the Z80 (`cpu0`), the Interfacer 1
console (`if1`), the Disk 1 (`fd0`) with its boot EPROM at `0000`, the host bridge (`hb0`) and
64K of memory (`mem0`).

To give the keyboard back to CP/M, type:

```
pascal100> RUN
```

Your `A>` prompt is where you left it.

### Before you write anything

The disk is mounted **read/write**, and there is no undo. Copy the folder first. The folder has
everything that the machine needs, and it boots from any place:

```
$ cp -R examples/cpm22 my-cpm
$ pascal100 my-cpm/cpm22-console.toml
```

When you finish writing files, **go back to the `A>` prompt before you quit.** Then the
programs that wrote to the disk have finished, and CP/M has written their files.

---

## Where to go next

- **The examples that this chapter does not describe:** `examples/`, and the README in each
  folder.
- **Move a file between CP/M and your computer:** the file-transfer chapter (`R`, `W`, `HDIR`).
- **Use telnet to connect to the guest, or connect it to a real serial port:** the serial
  chapter.
- **Look at the bus while the machine runs:** *The Debugger*.
