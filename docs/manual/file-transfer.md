# Moving files in and out

You will want to get a file into CP/M, and to get a file back out. There is a board for this.

## The Host Bridge is our own board, not a period board

**No S-100 maker made this board.** The `hostbridge` board is **our own**, made for this
simulator. The other way to move a file is a modem protocol over a simulated serial port. That is slow and difficult, and it is needed only because the machine
is simulated.

Every other board in this program is a real product that somebody sold. This one is not, and you
should know which boards are which. It is in the manual with the real boards, and it is the only
one that was never sold.

It is in the **`default` machine**. Unless you wrote a machine file that leaves it out, **you
already have it.**

The default port is **B0**. `BASE+0` is command and status, and `BASE+1` is data.

## The three utilities, and where they are

The utilities are **on the disk**, not in the board. They are ordinary CP/M `.COM` files. They
were assembled in the machine, with the machine's own assembler, from source that ships with it.

For this reason, a disk can be without them. A disk image that you got from another place is
one example, and a disk that you formatted yourself is another. *Getting the utilities onto a disk that does
not have them*, below, tells you what to do.

| | |
|---|---|
| `HDIR [pattern]` | List what is on the host. |
| `R <hostfile> [cpmfile]` | Read host → CP/M. |
| `W <cpmfile> [hostfile] [B\|T]` | Write CP/M → host. |

### `HDIR`

```
A>HDIR
    1792  07/14/26  R.COM
     640  07/14/26  HDIR.COM
   <DIR>  07/20/26  SRC/

3 files.
A>HDIR *.ASM
```

`HDIR` lists one file on each line, with its size, date and name, like `ls -l` or `DIR`. The
size is a number of bytes, or `<DIR>` for a folder. A folder also keeps its `/` at the end,
which shows that `R` cannot read it as a file. The list is sorted. The count at the end is the
number of files in the host folder, which can be much more than a CP/M disk holds.

**`HDIR` always prints the TRUE host names**, with their real case and their real length. It
does not print the 8.3 names that CP/M will see. That is the purpose of `HDIR`. When you cannot
find the name of a file, ask `HDIR`.

**A wildcard does not match a host file whose name starts with `.`**, such as `.DS_Store` or
`.gitignore`. This is the rule of a host shell. `HDIR` and `R *.*` skip these files. To see
them, type `HDIR .*`. To copy one, type its name and a CP/M name for it:
`R .gitignore GITIGNOR.TXT`.

### `R`: host to CP/M

```
A>R README.TXT
A>R *.ASM
A>R SRC/FOO.ASM
A>R SRC/FOO.ASM WORK.ASM
```

Wildcards work, and they skip host files whose names start with `.`. Folders work, and
**`/` and `\` both work on every host**, so you do not have to
remember which computer you use. A second argument gives the name of the file on the CP/M side,
instead of the automatic name.

### `W`: CP/M to host

```
A>W GAME.COM
A>W GAME.COM game.com
A>W NOTES.TXT
A>W *.*
A>W *.ASM SRC
```

Wildcards work. A second argument gives the name of the file on the host side.

**With a wildcard, the second argument is a host folder.** `W *.ASM SRC` writes each match into
the folder `SRC`, and each file keeps its CP/M name:

```
A>W *.ASM SRC
W: FOO.ASM -> SRC/FOO.ASM (text, by extension)
```

The folder must exist, because `W` does not make it. `W` checks the folder before it copies the
first file. If the folder is not there, `W` prints one error and copies nothing:

```
A>W *.* NOPE
W: NOPE: no such file
```

A `/` at the end of the folder name is optional. A lone `B` or `T` is a mode, not a folder. To
use a folder that has one of these names, type `B/` or `T/`.

## `W` and text files: **B** and **T**

CP/M stores a file in 128-byte records and does not keep its length in bytes. A text file ends
at the first `Ctrl-Z` (byte `1Ah`). CP/M does not fill the remainder of the last record. The
program that wrote the file decides what is there: some programs fill it with `Ctrl-Z`, and
some leave the bytes that were in their buffer. Thus `W` has two modes:

| Mode | What it does | Use it for |
|---|---|---|
| `B` (binary) | Writes every byte of every record. | A program or data. A `.COM` file arrives unchanged, and it runs. |
| `T` (text) | Stops at the first `Ctrl-Z`. | Text. The file arrives at its correct length. |

**When you give no mode, `W` chooses from the extension of the CP/M file.** These extensions
are text:

`ASM` `MAC` `Z80` `HEX` `PRN` `LST` `SYM` `INC` `SUB` `TXT`

Each other extension is binary, and `.COM` is one of them. When `W` chooses text mode, it tells
you:

```
A>W MON.HEX
W: MON.HEX -> MON.HEX (text, by extension)
3 records, 345 bytes
```

In a wildcard copy, `W` chooses for each file. `W *.*` brings back the source files as clean
text and the programs complete, in one command.

**A mode that you type is always used.** Put `B` or `T` after the names:

```
A>W NOTES.DOC notes.doc T
A>W LISTING.PRN B
```

`.BAS` and `.DOC` are not in the list, because these files are not always text. MBASIC saves a
program as tokens unless you tell it to save ASCII, and a word processor file can contain the
byte `1Ah`. If you know that such a file is text, type `T`.

Each mode has a cost when it is used on the wrong file:

- **`B` on a text file** gives you the remainder of the last record at the end: the `Ctrl-Z`,
  and up to 126 bytes after it that can be `Ctrl-Z` or old data. Your editor shows them.
- **`T` on a binary file that contains the byte `1Ah` TRUNCATES it.** Byte `1Ah` is an ordinary
  byte in a `.COM` file (the `LDAX D` instruction). The transfer stops at the first one, and you
  get a file with the correct name and the wrong length.

## The sandbox

The board has a **`hostdir`** property. It is the host folder that the guest can see, and it is
the **only** host folder that the guest can see.

```
pascal100> SET hb0 hostdir=/tmp/xfer
```

When `hostdir` is empty, the default, it is **the folder that you started `pascal100` from.**

### Where a relative `hostdir` points

`hostdir` is a path, so it follows the path rule in the machines chapter. A relative `hostdir`
starts from the machine's folder, whether you type it or a machine file gives it:

```
pascal100> SET hb0 hostdir=xfer          # the xfer folder beside the machine file
```

```toml
[[board]]
id      = "hb0"
hostdir = "xfer"                          # the same xfer folder
```

Both mean the same folder. It moves with the machine file, so an example folder that you copy to
another place still has its own transfer folder. For a built-in machine, which has no folder, a
relative `hostdir` starts from the folder that you started the program from.

**You do not have to work it out.** `SHOW hb0` prints the value as written and the folder that
it resolves to. The resolved folder is the limit:

```
pascal100> SHOW hb0
hb0  (hostbridge)

  property         value            legal
  ---------------- ---------------- -----------
  port             0xB0             0x0..0xFE
  hostdir          xfer
  hostdir_root     /home/you/pascal100/examples/cpm22/xfer (read-only)
  readonly         false            true|false
```

`hostdir` is what was **written**. `hostdir_root` is where the limit **really is**. The
`(show-only)` mark in the legal column means that `SHOW` prints the value and `SET` cannot
change it. The value is a fact about this run, not a setting, and the program never writes it
into a machine file.

A value longer than 32 characters puts its legal text on the next line, under the `legal`
column, as `hostdir_root` does here.

`SHOW PATHS` prints the same folder, beside the base directory. If `R` cannot find a file that
you are sure you put in `xfer`, read that line to see which folder the guest uses.

### What the path rule means for `R` and `W`

The path rule stops at the board. **It does not reach the guest, and `R` and `W` never see it.**

At the `A>` prompt, you do not type a host path. You type a **name**, and the board finds it in
`hostdir_root`, and nowhere else:

```
A>R FOO.ASM          <- hostdir_root/FOO.ASM. Never your cwd, never the machine file's
A>W RESULT.TXT       <- hostdir_root/RESULT.TXT
```

The path rule affects `R` and `W` **one time only**. It decides which folder `hostdir` names.
After that, the guest has one folder, and it cannot name any other folder. The board refuses
`..`, an absolute path and a drive letter, as below.

The two mechanisms look alike, but they are different:

| | decides | confines |
|---|---|---|
| **the path rule** | where a path that you or a machine file gives points | nothing at all |
| **`hostdir`** | nothing you type | **everything the guest can reach** |

This is the *only* limit in the simulator. A machine file can mount a disk from any place on
your computer, but the CP/M program that runs from that disk still sees only `hostdir`.

**The guest cannot leave it.** The board refuses all of these, before anything touches your
files:

- an absolute path
- a drive letter
- any `..` component
- a symbolic link that points outside the root

This is a **fixed limit on purpose**, not a filter that does its best. The guest runs software
from 1977 that you found on the internet. You should not have to trust it.

`readonly` lets files go one way only:

```
pascal100> SET hb0 readonly=on
```

Files come **out of** the host. Nothing goes back in, and `W` fails.

## The guest can write to your working folder

This is the other half of the rule, and you must know it:

**By default, software in the guest can read and write the folder that you started `pascal100`
from.**

This is on unless you turn it off. It is a **decision**, not a mistake. `R FOO.ASM` works as
soon as you unzip the package, with no setup, and the folder that you are in is usually the
folder that you want. However, it is your folder, with your files in it, and a CP/M program that
you did not write can reach them.

If you do not want this, point `hostdir` at a different folder, or set `readonly=on`. Each one
is one command.

## Names: 8.3, and how a host name becomes one

CP/M names have eight characters and an extension of three, and host names do not. `R`
changes a host name in these steps:

1. It changes the host name to **upper case**.
2. It **removes the characters that CP/M cannot have in a name.**
3. It cuts the name to **8** characters.
4. It cuts the extension to **3** characters.

`R` removes these characters:

- a space
- `< > . , ; : = ? * [ ] | \ /`
- a control character, and a character that is not ASCII

All other characters stay. For example, `-`, `(` and `)` are legal in a CP/M name:

```
notes[old].txt    →   NOTESOLD.TXT
my-notes(2).txt   →   MY-NOTES.TXT
```

In the second name, `R` removes nothing. `MY-NOTES` is the first eight characters, and the
`(2)` comes after them.

**A second argument sets the whole name.** Use it when you do not like the result of the steps:

```
A>R my-notes(2).txt NOTES.TXT
```

**Each name that you type can have 64 characters or fewer.** This limit applies to `R` and `W`,
to the 8080 `.COM` and the 8086 `.CMD` versions, and to a host name and a CP/M name. `R` cuts a
CP/M name that you type to 8.3, in the same steps as above. A name of more than 64 characters
stops the utility before it copies anything:

```
R: a name is too long.
```

`HDIR` prints the **true** host names, so you can always see the name that you ask for.

## Case, and why the board does not guess

**The CP/M command line changes everything to upper case.** The CCP does this, before your
program sees the line. When `R` runs, `R readme.txt` and `R README.TXT` are **the same
command**. The case is lost.

For this reason, the board does these steps, in this order:

1. **An exact match wins.** If a file has exactly the name that you asked for, the board uses
   that file.
2. **Otherwise, it ignores case and looks again.** `README.TXT` finds `readme.txt`.
   The board does this for each folder in the name too. `WORK/FOO.ASM` finds `work/foo.asm`.
3. **If more than one file matches, it REFUSES.**

Step 3 is the important one. If your folder has `readme.txt`, `README.TXT` and `ReadMe.Txt`, the
board does not choose one. It does not choose the newest, and it does not choose the first. **It
tells you that it cannot decide**, and it stops. A file transfer that guesses wrong and says
that it worked is worse than one that fails.

## Getting the utilities onto a disk that does not have them

There is a problem with all of the above: **you use `R` to get a file onto a disk, and `R` is a
file on the disk.** If you boot a disk without it, such as a disk image from another place or a
new disk that you made, you cannot use the board to get the program that uses the board.

You solve this with **the console**, which works before any file-transfer program does. You
enter the program through the console. You do not type it. You paste it.

**You cannot paste `R.COM`.** It is binary, and a console carries text. The first `1Ah` in it
would end the paste, and the bytes above `7Fh` would not arrive correctly. For this reason, you
paste **`R.HEX`**, the file that `LOAD` changes into `R.COM`. It is Intel HEX: colons, hex
digits and line ends, about 5.3 KB of them, and every byte can be printed.

**It ships with the package.** The utilities are in `hostbridge/`, with the 8080 sources, the
`.HEX` files and the assembled `.COM` files. The CP/M disk in `examples/cpm22` already has
`R.COM`, `W.COM` and `HDIR.COM`. For this reason, this section is about the other kind of disk:
a new disk, or a disk image that you brought. For the paste below, open `hostbridge/R.HEX`.

1. **Tell PIP to write a file from the console.**

   ```
   A>PIP R.HEX=CON:
   ```

   PIP now copies what you type into `R.HEX`, until you tell it to stop.

2. **Paste all of `R.HEX` into the terminal.** Open `R.HEX` on your computer, select all of it,
   and paste it into the window where `pascal100` runs. PIP shows each line as it arrives, about
   118 lines. That shows you that the guest receives it.

   **The monitor can paste the file for you.** Press `Ctrl-E` to go to the monitor, give it the
   file, and continue:

   ```
   pascal100> PASTE "hostbridge/R.HEX"
   pasting hostbridge/R.HEX (5264 bytes)
   pascal100> RUN
   ```

   `PASTE` sends a file of any size to the keyboard of the guest, and no character is lost. It
   finds a relative file name in the folder of the machine file, the same as `LOAD`. To stop a
   paste that is not finished, press `Ctrl-E` and type `NOPASTE`.

3. **End it with `Ctrl-Z`.**

   ```
   ^Z
   A>
   ```

   `Ctrl-Z` is CP/M's end-of-file mark on the console, and it closes the file. You are at the
   `A>` prompt, and `R.HEX` is on the disk.

4. **Change the HEX file into a program.**

   ```
   A>LOAD R

   FIRST ADDRESS 0100
   LAST  ADDRESS 0848
   BYTES READ    0749
   RECORDS WRITTEN 0F
   ```

   `LOAD` reads `R.HEX` and writes `R.COM`. It adds the `.HEX` itself, so `LOAD R` is the whole
   command. Check `FIRST ADDRESS 0100`. A CP/M program starts at `100h`. If that line shows a
   different value, the paste lost something.

5. **Use `R`, and you do not need to paste again.**

   ```
   A>R W.COM
   A>R HDIR.COM
   ```

   `R.COM` now exists, so the other two utilities come across the board as ordinary binary
   files, with no HEX, no `LOAD` and no paste. `hostdir` must point at the folder that holds
   them. On a built-in machine, such as `default`, that you started in the package folder,
   type:

   ```
   pascal100> SET hb0 hostdir=hostbridge
   ```

   For a machine file in another folder, give the path from that folder, or a full path.

### Two problems of the real hardware that do not happen here

**No character is lost, however fast you paste.** On a real machine, 5 KB sent to a serial
board with no flow control can overrun the UART, and characters are lost in the middle of a
HEX record. That cannot happen here. The simulated UARTs deliver bytes at the line
rate, but **they take a byte only when the receive register is free**. The next byte waits on
the host side, and nothing is lost. A paste of any size arrives complete. This is different from
the hardware on purpose, because the host connection does not lose data.

**A bad paste tells you.** Every Intel HEX record has a checksum, and `LOAD` checks it. A
damaged line gives an error. It does not give a program that almost works.

### Build them from source in the same way

When you paste `R.HEX`, you get the program. When you paste `R.ASM`, you get the program *and*
the source, so that you can change it. The disk's own `ASM.COM` assembles it, and `LOAD`
finishes the job as above:

```
A>PIP R.ASM=CON:
   ...paste R.ASM, ^Z...
A>ASM R
A>LOAD R
```

It is the same method with a longer paste, about 21 KB instead of 5.3 KB. The `.COM` files in
`hostbridge/` were built in this way. If you only want to *run* the utility, paste the HEX file.
It is about four times shorter, and it needs no assembler on the disk.

## The CPU 8085/88: CP/M-86, CP/M 8-16 and MP/M 8-16

These three systems run programs on the 8088, so they have their own versions of the utilities:
**`R.CMD`, `W.CMD` and `HDIR.CMD`**. The commands, the messages and the rules are the same as for
the 8080 versions in this chapter. There are two small differences:

- `W` also takes `.A86` and `.H86` (the ASM86 source and hex files) as text.
- The byte count that `R` and `W` print has 32 bits. The 8080 versions count to 16,777,215.

The disks in [`examples/cpm86`](../../examples/cpm86/), [`examples/cpm8-16`](../../examples/cpm8-16/)
and [`examples/mpm8-16`](../../examples/mpm8-16/) have the three programs, and each machine file
has the `hb0` board. On MP/M they are on the hard disk, drive `A:`, with the SYS attribute, so
every user number finds them. Concurrent DOS 8-16 on the CPU 286 runs them too: the disks in
[`examples/cdos8-16`](../../examples/cdos8-16/) have them on `A:` with the SYS attribute.

CP/M 8-16 and MP/M 8-16 can also run the 8080 `.COM` versions through CompuPro's `SW!`. Use the
`.CMD` versions. `SW!` is for a CP/M program that has no 8086 version.

**On MP/M, one user at a time, and the utilities wait their turn.** The board has one transfer
stream. Under MP/M, `R.CMD`, `W.CMD` and `HDIR.CMD` share a lock on it: an MP/M mutual exclusion
queue named `MXhostbr`. When another user has the board, the utility tells you and waits:

```
R: another user has the host bridge. Waiting...
```

It continues when the other user's transfer is done. To stop the wait, type `^C`, then `Y` when
MP/M asks `Abort (Y/N)?`. If a utility is stopped that way while it has the board, MP/M gives the
lock back, and the next user gets the board at once (*MP/M-86 Programmer's Guide*, 1.3.2). The 8080
`.COM` versions through `SW!` do not take the lock, so on MP/M use the `.CMD` versions. Under
CP/M-86 and CP/M 8-16 there is one user, and the utilities do not lock.

**A disk without them.** Paste the `.H86` file with `PIP` from the console, as for the `.HEX`
above, then make the program with `GENCMD`. `GENCMD.CMD` must be on the disk.

```
A>PIP R.H86=CON:
   ...paste hostbridge/R.H86, then ^Z
A>GENCMD R
```

Then copy the other two with `R`, as in step 6 above. The sources are `R.A86`, `W.A86` and
`HDIR.A86` in `hostbridge/`. They were assembled under CP/M 8-16 with `ASM86` and `GENCMD`.

## Why one `R.COM` works on every disk

**Every file operation goes through CP/M's BDOS.** It does not use the BIOS or a disk parameter
block. It makes no assumption about how many sectors are on a track, or where the directory is.

`R` and `W` open, read, write and close files with the same BDOS calls as any CP/M program, and
the BDOS knows about your disk. For this reason, the same `R.COM`, with no changes, runs on:

- the CompuPro Disk 1
- the Tarbell floppy controllers
- **any BIOS that anybody writes later**, for a controller that does not exist yet

That was the goal of the design. The board is not a period board, but the software that uses it
is an ordinary CP/M program, and it will work on a machine that you have not built yet.
