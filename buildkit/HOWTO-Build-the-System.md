# Build the Pascal-100 System in the Simulator

The Pascal-100 is an S-100 computer that runs both CP/M and UCSD Pascal III. This guide takes a simulated Pascal-100 machine from the three disks provided with the boards installed in the system to a fully functional system.

Following is a description of the Pascal-100 boardset taken from the Digicomp Research Pascal-100 manual.

> Pascal-100 is a 16-bit plug-in CPU module for the S-100 bus. It consists of two S-100 cards mated by a cable. The two board unit occupies adjacent slots in an S-100 frame.  
> 
> Pascal-100 incorporates two microprocessors: the Western Digital Microengine, which directly executes version III.0 UCSD Pascal P-code, and a Zilog Z80 ® processor. Switching between processors is software controlled via a switch-selectable output port.  
> 
> Pascal-100 runs version III of the UCSD Pascal system, including the screen-oriented editor, Pascal compiler, linker, filer and other utility programs. A version of this system is available from Digicomp to run on Pascal-100 in any environment that supports CP/M ™ and has at least 48K bytes of memory.   
> 
> Pascal-100 also supports programs written for the Z80 or 8080 on the S-100 bus, including the CP/M operating system and all associated software.

The emulated machine is a replica of the author's own vintage Pascal-100 system. It is made up of emulated versions of the Digicomp Research Pascal-100 CPU boardset, a CompuPro Disk 1 floppy disk controller, a CompuPro Interfacer 1 serial interface card and a CompuPro RAM23 128KB memory board.

Real Pascal-100 systems are extremely rare. We built the emulator so that others can learn how the system worked. You do not need any real hardware to complete this emulated build. All of these phases were tested on the simulator. The disks that Phases 2 through 4 make were also tested on the real Pascal-100.

The three starting disks are `CPM.DSK`, `TOOLS.DSK` and `PASCAL.DSK`. `CPM.DSK` is in the build kit, and you download the other two in Step 1.2. You make all the other disks from them.

The CP/M disk came with the author's CompuPro Disk 1, and the other two came with the Pascal-100. We imaged them from the original 8-inch floppy disks.

- **`CPM.DSK`** is the CompuPro CP/M 2.2 system disk that came with the Disk 1 controller. It starts as `64K CP/M 2.2LD`. It holds the CP/M programs and the CompuPro BIOS and Boot sources. We added two programs, `R.COM` and `W.COM`, that copy files between your computer and a CP/M disk in the simulator.

- **`TOOLS.DSK`** is a Digicomp Research disk that came with the Pascal-100. It is a CP/M disk with the Pascal-100 programs that run under CP/M, and the sources of the Pascal Boot and BIOS.

- **`PASCAL.DSK`** is the Digicomp Research UCSD Pascal III.0 system disk that came with the Pascal-100. It does not start as supplied. The Pascal-100 manual tells each owner to write the parts that connect Pascal to their own disk controller and serial board. Phase 3 makes these parts.

## The Project Phases

Work through each of the phases in sequence. Each phase builds on the disks from the phases before it. Each phase ends with something that works, so you can stop after any phase.

| Phase | What you do | What you get |
|---|---|---|
| 1 | Install the simulator, set up the work folder and learn about the simulator | The `pascal100` command, a work folder, and CP/M on the screen |
| 2 | Get CP/M running and build a work disk | `CPMWORK.DSK` |
| 3 | Get SSSD Pascal running | `PASSD.DSK` |
| 4 | Get SSDD Pascal working | `PASDD.DSK` |
| Bonus 1 | Create a DSDD CP/M work disk | `CPMDS.DSK` |
| Bonus 2 | Create a DSDD Pascal work disk | `PASDS.DSK` |

Each phase is made up of multiple steps. In each step you will see three parts:

- **Overview:** What the step does and why.

- **How to do it:** The actions you need to take to complete this step.

- **Result:** What you see when the step is completed.

## What you need

- The `pascal100` program, the simulator that this guide uses. It comes as an archive for your computer from the [pascal100 releases page](https://github.com/trgeuy/pascal100/releases), and Step 1.1 installs it. This guide needs a version later than 0.1.0.

- A terminal window on your computer. The `cp` and `chmod` commands in this guide are for macOS and Linux.

- The build kit. Download [`buildkit.zip`](https://github.com/trgeuy/pascal100/raw/master/buildkit/buildkit.zip) from the [`buildkit` folder](https://github.com/trgeuy/pascal100/tree/master/buildkit) of the pascal100 repository. It unpacks to a folder named `buildkit`. When you finish Step 1.2, the folder holds these files:

| File                                                | Contents                                                            |
| --------------------------------------------------- | ------------------------------------------------------------------- |
| `buildkit.toml`                                     | The machine file                                                    |
| `CPM.DSK`                                           | CompuPro CP/M 2.2, DD, with `R.COM` and `W.COM` added               |
| `TOOLS.DSK`                                         | Pascal-100 files for CP/M, SD. Not in the zip: you download it in Step 1.2 |
| `PASCAL.DSK`                                        | UCSD Pascal III.0 system, SD. Not in the zip: you download it in Step 1.2 |
| `NEWBIOS.ASM`, `NEWBOOT.ASM`                        | The source files for the new CP/M BIOS and Boot                     |
| `PBIOS.ASM`, `PFBOOT.ASM`, `PSBQB.ASM`, `QGENC.ASM` | The source files for the new Pascal BIOS and Boot                   |
| `HOWTO-Build-the-System.md`                         | This guide                                                          |
| `README.txt`                                        | Where to get the two Pascal-100 disks and the four manuals          |

## Which manual to read

The manuals are not in the build kit. `README.txt` in the build kit tells you where to get each one.

| Manual                                        | Read it for                                                           |
| --------------------------------------------- | --------------------------------------------------------------------- |
| `Beginner's Guide for the UCSD Pascal System` | Learning to use the Pascal system, with the Editor and Filer commands |
| `Pascal-100 Manual`                           | The Pascal-100 boardset, and how Pascal connects to the BIOS          |
| `CompuPro Disk 1 Manual`                      | The disk controller and the CP/M BIOS                                 |
| `CompuPro Interfacer 1 Manual`                | The serial board                                                      |

## How to read this guide

### Where to type

The prompt shows you where you are. If a step does not name a place, look at the prompt.

| Prompt | Where it is | What you type there |
|---|---|---|
| The prompt of your computer | The terminal window | `cp`, `chmod`, `cd` and `pascal100` |
| `pascal100>` | The terminal window, while the simulator runs | `MOUNT`, `RUN`, `UNMOUNT` and `QUIT` |
| `A>` | The VT52 window, when CP/M runs | CP/M commands |
| `-` | The VT52 window, when the DDT debugger runs | DDT commands |
| `Command:` | The VT52 window, when Pascal runs | Pascal menu letters |

### What the instructions mean

- **Type:** Push the keys in sequence. Type the text exactly as the guide shows it, with all the spaces and punctuation.

- **Push RETURN:** Push the key marked Return or Enter. If a step does not say to push RETURN, do not push it.

- **Push CTRL-E:** Hold down the Control key, push E, then release the two keys.

- **Numbers with H:** `805H` is a hexadecimal number. Type only the characters that the command block shows.

### Terms

| Term | Meaning |
|---|---|
| Disk file | A file on your computer that holds the contents of one 8-inch floppy disk. The name ends in `.DSK`. |
| Drive 0, drive 1 | The first two disk drives of the machine. The machine starts from drive 0. The table below gives their names in CP/M and Pascal. |
| SD, DD | Single density and double density. A DD disk holds more data on each track. |
| SS, DS | Single-sided and double-sided. For example, SSDD means single-sided double density. |
| BIOS | The part of the system that operates the hardware of one type of machine. |
| Boot | The small program at the start of a disk. It loads the system when the machine starts. |
| Boot disk | A disk that can start the machine. |
| Work disk | A disk that you use each day. It has a boot, the system and free space. |
| Scratch disk | A disk that you use for one job. You can then delete it. |

This guide uses the simulator names for the drives. CP/M and Pascal each use their own names:

| Simulator | CP/M | Pascal |
|---|---|---|
| Drive 0 (`fd0:drive0`) | `A:` | `#4:` |
| Drive 1 (`fd0:drive1`) | `B:` | `#5:` |

## Notes

### Running at the speed of the real machine

In the default configuration, the machine runs as fast as possible, so the response is almost immediate. Some people prefer the feel of the real machine. This option slows the processor and the console down to the speed of the real machine. The option is not necessary, and this guide operates with or without it.

**How to do it:**

1. Open `buildkit.toml` in your work folder with a text editor.

2. Find `clock_hz = 0` and change it to `clock_hz = 3000000`.

3. Find the line that starts with `connect = "terminal"`. Add this line below it:


   ```
   baud = "9600"
   ```


4. Save the file.

With these changes, the processor runs at 3 MHz, the same speed as a real machine. In addition, text displays on the screen at 9600 baud. A `DIR` command takes about 1 second, instead of 0.02 seconds. This does not have any effect on the speed of the emulated floppy disks.

To go back to full speed, set `clock_hz = 0` and remove the `baud` line.

### Two instructions that this guide uses in every phase

We use the following two instructions throughout the process. In the example below we show these commands with the `BUILD.DSK` and `CPMWORK.DSK` disk images. They do not exist yet. You make them in Step 2.1. Do not run these commands at this time.

**"Start the machine with `BUILD.DSK` in drive 0 and `CPMWORK.DSK` in drive 1"**

This means that you need to follow these steps. Make sure to use the disk image file names that the step gives you.

1. In the work folder, use this command to start the simulator:


   ```
   pascal100 -f buildkit.toml
   ```


2. At the `pascal100>` prompt, mount the disk images in the drives:


   ```
   MOUNT fd0:drive0 "BUILD.DSK"
   MOUNT fd0:drive1 "CPMWORK.DSK"
   ```


3. Start the machine with the following command:


   ```
   RUN 0
   ```


4. Select the VT52 window.

5. Wait for the prompt to appear. A CP/M disk shows `A>`, and a Pascal disk shows `Command:`.

If the step gives no disk for drive 1, do not do the second `MOUNT` command.

**"Stop the machine"**

This means that you need to shut down the simulator using the following commands. Make sure you always stop the machine before you change or copy a disk image.

1. Select the launch window.

2. Push CTRL-E.

3. Unmount the disk images using these commands. Only unmount the disk images that you have mounted. The `SHOW MOUNT` command will tell you what is currently mounted.


   ```
   SHOW MOUNT
   UNMOUNT fd0:drive0
   UNMOUNT fd0:drive1
   QUIT
   ```


Stop the machine each time that you change the disk in any drive.

`RUN 0` does not boot from the disk in drive 0 after you halt the machine with CTRL-E. The ROM on the disk controller is disabled as part of the normal boot process and is only re-enabled after a reset or a cold start.

We have found that while `RESET` followed by `RUN 0` restarts CP/M, it does not reliably restart Pascal. We have observed this behavior on our real system as well. As a result, this guide always uses `QUIT`, and then starts the simulator up again.

### Always work from copies

The machine changes a disk file when it writes to the disk. You cannot undo a write. So you work from copies, at two levels:

- **Always work from a copy of the original folder `buildkit`.** Never start the machine in the original folder. Keep that folder unchanged.

- **We will often make new disks from old disks.** For example, `cp PASCAL.DSK PASSD.DSK` makes a new disk file, and in Phase 3 we make changes only to the copy. The three starting disks are not changed at any point.

Never make a copy of a disk that the running machine is using. The file does not have all the changes until you do `UNMOUNT`. If you need a copy as a backup, use the following process:

1. Stop the machine.

2. At the prompt of your computer, copy the disk file. Use any new name:


   ```
   cp PASDD.DSK PASDD-SAVE.DSK
   ```


3. To go back to that copy, stop the machine and copy the file back:


   ```
   cp PASDD-SAVE.DSK PASDD.DSK
   ```


This is a simulator, so as long as you do not work on the original files a mistake costs only time. If you corrupt a disk or delete the wrong file on a disk, you can go back and restart a phase or even rebuild everything from the three starting disks.

## Phase 1: Install the simulator, set up the work folder and learn about the simulator

### Step 1.1: Install the simulator

**Overview:** We will put the simulator in its own folder, `pascal100` in your home folder, and then make the `pascal100` command work from any folder. Your work stays in a separate folder, so a newer version of the simulator never changes it.

**How to do it:**

1. Open a terminal window.

2. Download the archive for your computer from the [pascal100 releases page](https://github.com/trgeuy/pascal100/releases). Use the newest release. On a Mac with Apple silicon, the name ends in `macos-arm64.tar.gz`. On Linux, it ends in `linux-x86_64.tar.gz`. The version number is in the middle of the name.

3. Go to the folder that holds the archive. If it is in your Downloads folder, type this command:


   ```
   cd ~/Downloads
   ```


   If you have an older version of the simulator in `~/pascal100`, remove it first with `rm -rf ~/pascal100`. Your work folder is not in it, so this does not change your work.

4. Unpack the simulator for your computer and move it to your home folder. On a Mac, type these commands, with the version number of your archive in place of `VERSION`:


   ```
   tar xzf pascal100-VERSION-macos-arm64.tar.gz
   mv pascal100-VERSION-macos-arm64 ~/pascal100
   ```


   On Linux, use `pascal100-VERSION-linux-x86_64` in place of `pascal100-VERSION-macos-arm64` in both commands.

5. On a Mac, remove the quarantine mark, because macOS blocks a downloaded program that is not notarized. Skip this step on Linux:


   ```
   xattr -dr com.apple.quarantine ~/pascal100/pascal100
   ```


6. Make the `pascal100` command work from any folder. The `sudo` command asks for the password of your computer:


   ```
   sudo mkdir -p /usr/local/bin
   sudo ln -sf ~/pascal100/pascal100 /usr/local/bin/pascal100
   ```


7. Check that the command works:


   ```
   pascal100 --version
   ```


**Result:** The terminal window shows `pascal100`, the version number of your archive and a code in brackets. This guide needs a version later than 0.1.0. The `pascal100` folder in your home folder also holds the User Manual, `QUICK-START.pdf` and the examples.

### Step 1.2: Set up the work folder

**Overview:** The `buildkit` folder holds the original files. First we will add the two Pascal-100 disks to it. Then we will make a copy of it, the work folder, and we will do all our work there.

**How to do it:**

1. Unpack `buildkit.zip`, and put the `buildkit` folder in a place that is easy to find, for example your Desktop.

2. Download the two Pascal-100 disks from the Internet Archive. They are on the page <https://archive.org/details/cpm-files>, with the Pascal-100 manual. These links download the two files:

   - [`CPM_FIles.dsk`](https://archive.org/download/cpm-files/CPM_FIles.dsk). The name has a capital I in `FIles`.
   - [`UCSD_Pascal_III.dsk`](https://archive.org/download/cpm-files/UCSD_Pascal_III.dsk)

3. Open a terminal window.

4. Go to the `buildkit` folder. If it is on your Desktop, type this command:


   ```
   cd ~/Desktop/buildkit
   ```


5. Move the two files into the `buildkit` folder, with the names that this guide uses. If they are in your Downloads folder, type these commands:


   ```
   mv ~/Downloads/CPM_FIles.dsk TOOLS.DSK
   mv ~/Downloads/UCSD_Pascal_III.dsk PASCAL.DSK
   ```


6. Check the two files:


   ```
   shasum TOOLS.DSK PASCAL.DSK
   ```


   The command must show these values. If a value is different, download that file again.

   | File | SHA-1 |
   |---|---|
   | `TOOLS.DSK` | `3fc3773d8fb5768fff03c4e3d039a18dc13030c8` |
   | `PASCAL.DSK` | `bca58369bc31ebecff4f9372fdb66713ff399230` |

7. Make the two files read-only, the same as `CPM.DSK`, so that nothing can change the originals:


   ```
   chmod a-w TOOLS.DSK PASCAL.DSK
   ```


8. Go back to the folder that holds `buildkit`, and make the work folder. The `cp` command copies `buildkit` and all its files to a new folder named `work`:


   ```
   cd ..
   cp -R buildkit work
   ```


9. Go to the work folder:


   ```
   cd work
   ```


10. Let the simulator write to the disks. The disks in the build kit are read-only, and the copies are read-only too until you do this command:


    ```
    chmod u+w *.DSK
    ```


**Result:** You have a folder named `work` with a copy of all the build kit files, with the three starting disks. Your terminal window is in that folder.

If you close out your terminal session, make sure you are in your work folder before starting a new session. For example, type `cd ~/Desktop/work`.

### Step 1.3: Starting and leaving the simulator

**Overview:** We will start the simulator and leave it again, to see that the program operates. We will not run the machine yet.

**How to do it:**

1. Run this command in your terminal window in the work folder:


   ```
   pascal100 -f buildkit.toml
   ```


2. To leave the simulator, type `QUIT` at the `pascal100>` prompt and then push RETURN.

**Result:** The simulator shows you `machine: pascal100` and then the `pascal100>` prompt. This prompt is the simulator's monitor, which you use to control the simulator. The terminal window that shows it is the launch window. After `QUIT`, you are back at the prompt of your computer.

If your computer shows `command not found`, do step 6 of Step 1.1 again.

### Step 1.4: Put a disk in a drive and start the machine

**Overview:** We will make a practice disk image, put it in the simulator's drive 0 and then start CP/M.

**How to do it:**

1. Make the practice disk:


   ```
   cp CPM.DSK TRY.DSK
   ```


2. Start the simulator:


   ```
   pascal100 -f buildkit.toml
   ```


3. At the `pascal100>` prompt, put the disk in drive 0:


   ```
   MOUNT fd0:drive0 "TRY.DSK"
   ```

   `fd0` is the disk controller, and `drive0` is the simulator's first drive. CP/M will see this as `A:`. `drive1` is the second drive, and CP/M will see it as `B:`. The quotation marks stay in the command. To see which disk images are mounted, type `SHOW MOUNT` at the `pascal100>` prompt.


4. Start the machine. We tell the processor to start at address 0. This is the address of the Disk 1 floppy controller boot ROM. The disk controller reads the disk image in drive 0 and boots CP/M.


   ```
   RUN 0
   ```


5. When you started `pascal100`, a second window opened. This is the VT52 window, which acts as the terminal of the machine. Select it so that you can type commands into the machine.

6. Type `DIR` and push RETURN.

**Result:** `MOUNT` shows `fd0:drive0: mounted` and the path of `TRY.DSK`. The VT52 window shows `64K CP/M 2.2LD` and the `A>` prompt. `DIR` shows the files on the disk.

### Step 1.5: Stop the machine

**Overview:** We will now stop the machine cleanly. `UNMOUNT` makes sure that the disk image file on your computer has all the changes.

**How to do it:**

1. Select the launch window.

2. Push CTRL-E. The `pascal100>` prompt will be displayed on the screen.

3. Run these commands to leave the simulator:


   ```
   UNMOUNT fd0:drive0
   QUIT
   ```


4. Now delete the practice disk:


   ```
   rm TRY.DSK
   ```


**Result:** You are back at the prompt on your computer, in the work folder.

### Phase 1 End Result

You have a work folder. You can start the simulator, put a disk in a drive, start the machine and stop it.

## Phase 2: Get CP/M running and build a work disk

The stock CompuPro BIOS has the code for many common CompuPro boards. In this phase we will replace it with an edited BIOS that just retains the code needed for our system. We have done this so that CP/M can run as high in memory as possible.

If you make a typing error in this phase that results in a different output than shown in this guide, stop the machine. You can then return to Step 2.1 and work through the process again. This approach applies to each phase of this project.

### Step 2.1: Make two copies of the CP/M disk

**Overview:** We will make two disk images from `CPM.DSK`. `BUILD.DSK` is a scratch disk that we will run the build on, and `CPMWORK.DSK` will become our work disk.

**How to do it:** Run these commands in the terminal window on your computer, in the work folder:

```
cp CPM.DSK BUILD.DSK
cp CPM.DSK CPMWORK.DSK
```

**Result:** The work folder now has two new disk images, `BUILD.DSK` and `CPMWORK.DSK`.

### Step 2.2: Start CP/M

**Overview:** We will start the stock CP/M from the scratch disk, with the future work disk in drive 1.

**How to do it:**

1. In the work folder, use this command to start the simulator:


   ```
   pascal100 -f buildkit.toml
   ```


2. At the `pascal100>` prompt, mount the disk images in the drives and start the machine:


   ```
   MOUNT fd0:drive0 "BUILD.DSK"
   MOUNT fd0:drive1 "CPMWORK.DSK"
   RUN 0
   ```


3. Select the VT52 window.

4. Wait for the `A>` prompt to appear.

From here on, when a step says "Start the machine with" and names the disk images, do these same actions with those disk images. See "Two instructions that this guide uses in every phase" in the Notes.

**Result:** The VT52 window shows `64K CP/M 2.2LD` and the `A>` prompt.

### Step 2.3: Get and assemble the new BIOS and Boot sources

**Overview:** The work folder has a copy of the new BIOS and Boot sources. We will copy them to the disk, and then assemble them. `R` copies a file from the work folder on your computer to the CP/M disk.

The CP/M disk has the CompuPro sources, `GBBIOS.ASM` and `GBBOOT.ASM`. The two new files are those sources, edited for this machine:

| File | What it is | What we changed |
|---|---|---|
| `NEWBIOS.ASM` | The CP/M BIOS | See the list below |
| `NEWBOOT.ASM` | The CP/M boot | Two values: the memory size is 64K, and the BIOS length is 0D00H |

The changes in `NEWBIOS.ASM` are:

- It sets the memory size to 64K, the processor to Z80 and the BIOS length to 0D00H.

- It sets the delay count for a 3 MHz processor.

- It removes the code for the Bit Banger, the System Support board and the Interfacer 3 board.

- It removes drives C and D.

- It removes the printer, punch and reader code. The BIOS discards printer and punch output, and the reader returns CTRL-Z.

The smaller BIOS is what lets CP/M go higher in memory.

We found the BIOS length in the usual way. We assembled the BIOS with the old length variable set to 1000H. In the listing file, `GBBIOS.PRN`, the code and work area end at the label `ESPACE`, 812H bytes after the start of the BIOS. The buffers at the top of memory take 480H bytes. The total is C92H. `MOVCPM` moves CP/M in whole pages of 256 bytes, so we rounded up to 0D00H, which is 13 pages. Then we set `BIOSLN` to 0D00H and assembled again. If you change the BIOS, do the same: assemble, read `ESPACE`, and make sure that it is below `DIRBUF`.

**How to do it:** Type these commands, and push RETURN after each line.

```
ERA CPM*.COM
ERA GBBIOS.ASM
ERA GBBOOT.ASM
R NEWBIOS.ASM GBBIOS.ASM
R NEWBOOT.ASM GBBOOT.ASM
ASM GBBIOS
ASM GBBOOT
```

`ERA CPM*.COM` erases the ready-made CP/M systems from `BUILD.DSK`.

`R NEWBIOS.ASM GBBIOS.ASM` copies `NEWBIOS.ASM` to the disk in drive 0, with the name `GBBIOS.ASM`.

`ASM` is the CP/M assembler. It reads the `.ASM` file and makes a `.HEX` file and a `.PRN` listing file. We only use the `.HEX` file in this guide.

**Result:** Each `R` command shows the number of records and bytes that it copied. Each `ASM` command ends with `END OF ASSEMBLY`. The address on the line before `USE FACTOR` is `FFFE` for `GBBIOS` and `01AC` for `GBBOOT`.

### Step 2.4: Change the length of the BIOS in MOVCPM

**Overview:** `MOVCPM` is the program that we use to make a CP/M system for a given memory size. It stores the length of the BIOS at address 805H. The new BIOS from Step 2.3 is 0D00H bytes long. We will set this new length and then save the changed program as `MOVD.COM`.

**How to do it:** The block below shows what the screen will look like after you finish the step.

- Do not type the `A>` prompt or the `-` prompt. Type the text after the prompt, then push RETURN.

- The lines that start with `0805`, `0806` and `0807` come from DDT. DDT shows the address and the old value. Type only the last item on the line, then push RETURN.

- The lines with no prompt and no address are messages from DDT. Do not type them.

```
A>DDT MOVCPM.COM
DDT VERS 2.2
NEXT  PC
3200 0100
-S805
0805 00 00
0806 14 0D
0807 00 .
-G0

A>SAVE 49 MOVD.COM
A>
```

| Command | What it does |
|---|---|
| `DDT MOVCPM.COM` | Loads `MOVCPM` into DDT, the CP/M debugger. The prompt changes to `-`. |
| `S805` | Starts to change memory at address 805H. DDT shows the old value of each byte. |
| `00`, `0D` | Set the new values of 805H and 806H. The low byte comes first. |
| `.` | Stops the change |
| `G0` | Leaves DDT and goes back to CP/M. The second character is a zero. |
| `SAVE 49 MOVD.COM` | Saves the changed program. The length is 49 pages of 256 bytes. |

**Result:** `DDT MOVCPM.COM` shows `NEXT  PC` and `3200 0100`. `S805` shows `0805 00`, and the next line shows `0806 14`, so the old length was 1400H. After `SAVE`, the `A>` prompt comes back with no message.

### Step 2.5: Make the new system

**Overview:** We will use `MOVD` to make a 64K CP/M system. Then we will use DDT to put the new Boot and the new BIOS in it, and save the result as `CPM64X.COM`.

**How to do it:** Type these commands, and push RETURN after each line.

```
MOVD 64 *
SAVE 43 CPM64.COM
DDT CPM64.COM
F0100,15FF,0
IGBBOOT.HEX
R800
IGBBIOS.HEX
R1800
G0
SAVE 43 CPM64X.COM
```

| Command | What it does |
|---|---|
| `MOVD 64 *` | Makes a 64K CP/M system. The `*` keeps the system in memory. |
| `SAVE 43 CPM64.COM` | Saves that system as a file. `MOVD` gives this command, with the length of 43 pages, in its last message. |
| `DDT CPM64.COM` | Loads the file into DDT |
| `F0100,15FF,0` | Clears the old boot and the old BIOS |
| `IGBBOOT.HEX`, `R800` | `I` names a file. `R` reads the file and adds 800H to each address. This puts the new boot in place. |
| `IGBBIOS.HEX`, `R1800` | These commands put the new BIOS in place |
| `G0`, `SAVE 43 CPM64X.COM` | Go back to CP/M and save the finished system |

**Result:** `MOVD 64 *` shows `CONSTRUCTING 64K CP/M Vers. 2.2`, then `READY FOR "SYSGEN" OR` and `"SAVE 43 CPM64.COM"`. After `R800` and after `R1800`, DDT shows `NEXT  PC` and `2C00 0000`.

### Step 2.6: Write the system to the work disk

**Overview:** `SYSGEN` writes a system to the first tracks of a disk. It does not change the files on the disk. We will write the new system to `CPMWORK.DSK`, in drive 1.

**How to do it:**

1. Type `SYSGEN CPM64X.COM` and push RETURN.

2. At `Destination drive name (or RETURN to terminate).`, type `B` to select `B:` (drive 1).

3. At `Destination on B, then type return.`, push RETURN.

4. At `Destination drive name (or RETURN to terminate).`, push RETURN to go back to CP/M.

5. Stop the machine.

**Result:** `SYSGEN` shows `SYSGEN Version 2.2D`. After step 3, the screen shows `Function complete.` After step 5, you are back at the prompt on your computer.

Do not skip step 5. `RUN 0` alone does not start the new disk. See "Stop the machine" in the Notes.

### Step 2.7: Start the new CP/M and remove the build files

**Overview:** We will now change the disks and start the machine from the work disk for the first time. The disk still has the files that the old system used. A work disk does not need them, so we will erase them.

**How to do it:**

1. Start the simulator and put `CPMWORK.DSK` in drive 0:


   ```
   pascal100 -f buildkit.toml
   MOUNT fd0:drive0 "CPMWORK.DSK"
   RUN 0
   ```


2. Select the VT52 window and wait for the `A>` prompt to appear.

3. Type these commands, and push RETURN after each line.


   ```
   ERA CPM*.COM
   ERA MOVCPM.COM
   ERA GB*.*
   ERA DISK2.*
   ERA BIOS.ASM
   ERA CLOCK.COM
   STAT
   ```


4. Stop the machine.

**Result:** The screen shows `CompuPro Systems` and `64K CP/M 2.2K` when the machine starts. `STAT` shows `A: R/W, Space: 536k`.

### Phase 2 End Result

You now have a CP/M work disk called `CPMWORK.DSK`. It contains the basic development tools that came with CP/M. It boots the machine from drive 0, and it reads both SSSD and SSDD disk images loaded in drive 1.

To move a file between CP/M and your computer, use `R` and `W`. `R` copies a file from the work folder to the disk. `W` copies a file from the disk to the work folder.

```
R NOTES.TXT
W STAT.COM
```

You can now delete `BUILD.DSK` from your work folder if you want, as it is no longer needed.

## Phase 3: Get SSSD Pascal running

The `PASCAL.DSK` disk image cannot boot this machine yet. The sample BIOS provided on the Tools disk is configured for different hardware: a Tarbell disk controller and a Cromemco TUART. In this phase we will make new BIOS and Boot files for this machine, and write them to a copy of the Pascal disk.

Pascal does not use the VT52 codes until Phase 4, so you may see some stray characters on the screen before then. Your typed commands will still operate.

These points apply to Pascal in all the phases:

- At the `Command:` prompt, Pascal shows a menu. Type one letter, with no RETURN. For example, `F` starts the Filer.

- The Filer has its own menu. Type `Q` to leave it.

- `#4:` is drive 0, and `#5:` is drive 1.

- In a Filer file name, `=` means all the files. In the new name, `$` means the same name.

- A volume is a Pascal disk. Each volume has a name.

### Step 3.1: Get the Pascal tools

**Overview:** The Tools disk contains the CP/M programs we need to install the BIOS and Boot code onto the Pascal boot disk. We will copy these files onto our CP/M work disk. We will also make `PASSD.DSK`, a copy of the Pascal disk.

**How to do it:**

1. At the prompt on your computer, make a copy of the Pascal disk:


   ```
   cp PASCAL.DSK PASSD.DSK
   ```


2. Start the machine with `CPMWORK.DSK` in drive 0 and `TOOLS.DSK` in drive 1.

3. Type `PIP A:=B:*.*[V]` and push RETURN. `PIP` copies all the files from `B:` (drive 1) to `A:` (drive 0). `[V]` makes it verify each copy.

4. Type `STAT` and push RETURN.

5. Stop the machine.

**Result:** `PIP` shows `COPYING -` and a list of 11 files. `STAT` shows `Space: 374k`.

### Step 3.2: Assemble the sources

**Overview:** We have provided edited versions of the original source files, to support the hardware in our machine. We will copy the four Pascal sources from the work folder to the CP/M disk, and then assemble them. Our files have new names, so the original sources from the Tools disk stay on the disk. `PASSD.DSK` goes in drive 1 now, because we will write to it in Step 3.4.

The Pascal-100 manual, section V.D, tells the owner to make these parts for their own hardware. We made them for the CompuPro Disk 1 and Interfacer 1:

| File | What it is | How we made it |
|---|---|---|
| `PFBOOT.ASM` | The hard boot, the first program on the disk. The ROM of the disk controller starts it, and it loads the other parts. | A new program. It replaces `FBOOT.ASM` on the Tools disk, which is for the Tarbell controller. |
| `PBIOS.ASM` | The Z80 BIOS for Pascal. It operates the console and the disks. It finds the density and the sides of each disk. | It keeps the structure of the sample `BIOS.ASM` on the Tools disk. The console and disk sections are new. |
| `PSBQB.ASM` | The soft boot, and the interface between the Z80 and the MicroEngine | `SBQB.ASM` from the Tools disk, with small changes. See the list below. |
| `QGENC.ASM` | A patch for the `QGEN` program | `QGEN.ASM` from the Tools disk, with two changes. See the second list below. |

The changes in `PSBQB.ASM` are:

- `FLAGS` is 1, so the disk format comes from the BIOS.

- The sector buffer is at 0800H, and it holds 512 bytes.

- The Z80 code area ends at 0A00H.

- It does not read part 1 of the Pascal boot, because `PFBOOT` reads it.

The changes in `QGENC.ASM` are:

- It subtracts 1 from each sector number, for the CP/M 2 BIOS.

- It writes track 0 as 26 sectors of 128 bytes on a DD disk. As a result, one boot works on SD and DD disks. Phase 4 uses this.

The Pascal BIOS must fit in a fixed space. It starts at 480H, and `PSBQB` puts the sector buffer at 0800H, so the BIOS must end below 0800H. We checked this as we did for the CP/M BIOS in Step 2.3: assemble, then read the end address. Here the address before `USE FACTOR` is the end of the BIOS, because `PBIOS` has no buffers at the top.

The first version that finds the sides of each disk ended at 081DH, 1DH bytes too long. A higher sector buffer would leave Pascal 512 bytes less memory, so we made the code smaller instead. Now it ends at 07F7H, 9 bytes below the limit. The other parts also fit: `PSBQB` ends at 0420H, below the BIOS, and `PFBOOT` ends at 017BH, inside the 4 sectors that the disk controller ROM reads.

**How to do it:**

1. Start the machine with `CPMWORK.DSK` in drive 0 and `PASSD.DSK` in drive 1.

2. Type these commands, and push RETURN after each line.


   ```
   R PFBOOT.ASM
   R PBIOS.ASM
   R PSBQB.ASM
   R QGENC.ASM
   ASM PFBOOT
   ASM PBIOS
   ASM PSBQB
   ASM QGENC
   ```


**Result:** Each `ASM` command ends with `END OF ASSEMBLY`, and it shows an address before `USE FACTOR`:

| File | Address |
|---|---|
| `PFBOOT` | `017B` |
| `PBIOS` | `07F7` |
| `PSBQB` | `0420` |
| `QGENC` | `08A3` |

### Step 3.3: Bind the boot into QGEN

**Overview:** `QGEN` is the boot maintenance program for the Pascal-100. It holds a copy of the Pascal boot, and it writes that boot to a disk. We will load `QGEN` into DDT and put our new parts in its copy. `SAVE` then makes a new program, `BOOTGEN.COM`.

**How to do it:** Type these commands, and push RETURN after each line.

```
DDT QGEN.COM
IQGENC.HEX
R
IPFBOOT.HEX
R800
IPBIOS.HEX
R700
IPSBQB.HEX
R700
G0
SAVE 25 BOOTGEN.COM
```

`PFBOOT` starts at 100H, so its offset is `R800`. The manual shows `R900` for a boot that starts at 0.

**Result:** Each `R` command shows `NEXT  PC`, then a line that starts with `1A00`. After `SAVE`, the `A>` prompt comes back with no message.

### Step 3.4: Write the boot to the disk in drive 1

**Overview:** We will use `BOOTGEN` to write the new boot to `PASSD.DSK`.

**How to do it:**

1. Type `STAT B:DSK:` and push RETURN.


   Do not skip this step. Without it, `BOOTGEN` stops with `DISK ERROR -- GIVING UP`. CP/M must first find the density of drive 1.


2. Type `BOOTGEN` and push RETURN.

3. At `Which drive to read Pascal-100 booter from?`, push RETURN to skip the read.

4. At `Want to see/modify boot parameters?`, type `N`.

5. At `Which drive to write Pascal-100 booter to?`, type `B`.

6. At `Hit <ret> to write booter.`, push RETURN.

7. At `Want to write it again?`, type `N`.

8. At `Remount CP/M diskette in A`, push RETURN to go back to CP/M.

9. Stop the machine.

**Result:** `STAT B:DSK:` shows `243: Kilobyte Drive  Capacity`. `BOOTGEN` shows `Digicomp Research, Pascal-100 Boot Maintainer 7/15/81`. The `A>` prompt comes back after step 8.

### Step 3.5: Start Pascal

**Overview:** We will start Pascal for the first time and use the Filer to list the disk.

**How to do it:**

1. Start the machine with `PASSD.DSK` in drive 0. The name after `Welcome` is the name of the boot volume, the disk in drive 0 (`#4:`).

2. Type `F` for the Filer.

3. Type `L`, then `#4:`, and push RETURN.

4. Push the space bar to see the remainder of the directory.

5. Stop the machine.

**Result:** The screen shows `Welcome  PASCAL,  to`, `U.C.S.D.  Pascal  System  III.0` and the `Command:` prompt. The directory ends with `26/26 files<listed/in-dir>, 483 blocks used, 1 unused`.

### Phase 3 End Result

`PASSD.DSK` starts Pascal. It is an SSSD disk and has only 1 free block, so it is a minimal boot disk and not a work disk. We will make the SSDD work disk in Phase 4.

## Phase 4: Get SSDD Pascal working

The CompuPro Disk 1 boot ROM expects track 0 of any boot disk to be an SD track with 26 sectors of 128 bytes. A Pascal DD disk also has this SD track 0. Only its other tracks have 15 sectors of 512 bytes. As a result, the same boot track works for SD and DD disks.

Do not use `BOOTCOPY` for these disks. `BOOTCOPY` expects 512-byte sectors on track 0.

`BOOTGEN` writes track 0 in the SD format on a DD disk, because of the second change in `QGENC` (Step 3.2).

### Step 4.1: Make and format a blank disk

**Overview:** The simulator makes a new blank disk image when you add `CREATE` to the `MOUNT` command. `media=8ss` makes a single-sided disk. A new blank disk has no format, so we will use the CP/M `FORMAT` program.

**How to do it:**

1. Start the simulator, put `CPMWORK.DSK` in drive 0, and make a blank disk image in drive 1. Then start the machine, select the VT52 window and wait for the `A>` prompt:


   ```
   pascal100 -f buildkit.toml
   MOUNT fd0:drive0 "CPMWORK.DSK"
   MOUNT fd0:drive1 "PASDD.DSK" CREATE media=8ss
   RUN 0
   ```


2. Type `FORMAT` and push RETURN.

3. Type `B` to select `B:` (drive 1).

4. Type the mode number from this table. Do not push RETURN. For this disk, use mode `2`.


   | Mode | Format | Use |
   |---|---|---|
   | `2` | 512-byte sectors, DD | Pascal DD disk |
   | `3` | 1024-byte sectors, DD | CP/M DD disk |


5. Type a lower case `y`. If you push RETURN here, `FORMAT` stops with `Format error`.

6. Wait for `FORMAT complete`.

7. Type `N` to go back to CP/M.

8. Stop the machine.

**Result:** `FORMAT` shows `Disk is NOT formatted`, then a line of `F` and a line of `V` for the 77 tracks, then `FORMAT complete`.

### Step 4.2: Make the directory and copy the files

**Overview:** We will start Pascal from the SD disk and use the Filer. With the Filer we will make an empty directory on the new disk, name the volume `WORK` and copy all the files to it.

The Filer asks for the size of the disk in blocks. A Pascal block is 512 bytes. Track 0 holds the boot. Each of the other 76 tracks holds 15 sectors of 512 bytes, so the size is 76 × 15 = 1140 blocks.

**How to do it:**

1. Start the machine with `PASSD.DSK` in drive 0 and `PASDD.DSK` in drive 1.

2. Type `F` for the Filer.

3. Type `Z`, then `#5:`, and push RETURN. `Z` makes an empty directory on the disk in drive 1.

4. Type `N` at `Duplicate dir ?`.

5. Type `1140` at `# of blocks ?` and push RETURN.

6. Type `WORK` at `New vol name ?` and push RETURN.

7. Type `Y` at `WORK: correct ?`.

8. Type `T`, then `#4:=`, and push RETURN. `T` copies files, and `#4:=` means all the files on drive 0.

9. Type `#5:$` and push RETURN. This means drive 1, with the same file names.

10. Type `Q`.

11. Stop the machine.

**Result:** The Filer shows `WORK: zeroed`. Then it shows one `transferred to` line for each of the 26 files.

Name the disk before you boot it. If you change the name of the disk that you booted, the system cannot find its tools.

### Step 4.3: Write the boot and start the disk

**Overview:** We will write the boot to the new disk, and then start Pascal from it.

**How to do it:**

1. Start the machine with `CPMWORK.DSK` in drive 0 and `PASDD.DSK` in drive 1.

2. Type `STAT B:DSK:` and push RETURN.


   Do not skip this step. Without it, `BOOTGEN` stops with `DISK ERROR -- GIVING UP`. CP/M must first find the density of drive 1.


3. Type `BOOTGEN` and push RETURN.

4. At `Which drive to read Pascal-100 booter from?`, push RETURN to skip the read.

5. At `Want to see/modify boot parameters?`, type `N`.

6. At `Which drive to write Pascal-100 booter to?`, type `B`.

7. At `Hit <ret> to write booter.`, push RETURN.

8. At `Want to write it again?`, type `N`.

9. At `Remount CP/M diskette in A`, push RETURN to go back to CP/M.

10. Stop the machine.

11. Start the machine with `PASDD.DSK` in drive 0 to verify that the new Pascal boot disk works.

12. Stop the machine.

**Result:** `STAT B:DSK:` shows `562: Kilobyte Drive  Capacity`. `BOOTGEN` shows `Digicomp Research, Pascal-100 Boot Maintainer 7/15/81`. The `A>` prompt comes back after step 9. Pascal starts with `Welcome  WORK,  to` and the `Command:` prompt.

### Step 4.4: Type the VT52 GOTOXY program

**Overview:** UCSD Pascal is not line-oriented like CP/M. It expects a terminal for its console. A small routine, `GOTOXY`, added to the system, is used to move the cursor. In addition, the file `SYSTEM.MISCINFO` holds the other screen and key codes. In this step we will use the Pascal Editor to type a `GOTOXY` for the VT52.

**How to do it:**

1. Start the machine with `PASDD.DSK` in drive 0. You will use this disk for the rest of this phase.

2. Type `E` to open the Editor. Push RETURN at the `No workfile` message.

3. Type `I` to insert text and type in this program. Push RETURN at the end of each line, also after the last line, `END.`


   ```
   {$R-}
   PROGRAM DUMMY;
   {GOTOXY FOR VT52}
   PROCEDURE FGOTOXY(X,Y:INTEGER);
   VAR P:PACKED ARRAY[0..3] OF CHAR;
   BEGIN
   IF Y>23 THEN Y:=23
   ELSE IF Y<0 THEN Y:=0;
   IF X>79 THEN X:=79
   ELSE IF X<0 THEN X:=0;
   P[0]:=CHR(27);
   P[1]:='Y';
   P[2]:=CHR(Y+32);
   P[3]:=CHR(X+32);
   UNITWRITE(1,P,4,,12);
   END;
   BEGIN
   END.
   ```


4. Once you have typed in the program, push CTRL-C to accept the text.

5. Type `Q`, then `W`. This starts the write of the text to a file.

6. When asked for a file name, type `FGOTOXY` and push RETURN.

7. Type `E` to leave the Editor.

**Result:** The screen shows `Your file is 333 bytes long.` If the size is different, you made a typing error. Use the editor to correct the file.

### Step 4.5: Compile the program and bind it

**Overview:** We will compile the `FGOTOXY` program, save the code, and then use `BINDER` to install the procedure in `SYSTEM.PASCAL`.

**How to do it:**

1. Type `C`, then `FGOTOXY`, and push RETURN.

2. Type `F` for the Filer.

3. Type `S`, then `FGOTOXY`, and push RETURN. Do not type `.CODE`.

4. Type `Q`.

5. Type `X`, then `BINDER`, and push RETURN.

6. Type `FGOTOXY.CODE` and push RETURN.

7. Push RETURN to read `SYSTEM.PASCAL`.

8. Push RETURN to write `SYSTEM.PASCAL`.

**Result:** The compiler shows that it compiled `17 lines`. The Filer shows `Code file saved`. `BINDER` shows `Binding completed.` If the line count is different, you made a typing error. Use the editor to correct the file, then do these steps again.

### Step 4.6: Change the terminal settings

**Overview:** We will use `SETUP` to change the screen and key codes to match those used by a VT52 terminal, and then write them into a new file, `NEW.MISCINFO`. This program shows you 48 fields in alphabetical order, one at a time. For each field it shows the name and the value, and then it asks `DO YOU WISH TO CHANGE THIS FIELD?`

**How to do it:**

1. Type `X`, then `SETUP`, and push RETURN.

2. Type `C`, then `P`. `C` changes the settings, and `P` shows the fields one at a time.

3. Type `N` for each field that is not in this table.

4. For a field in the table, type `Y`, then the value, then push RETURN. Then type `N`.


   | Field | Value |
   |---|---|
   | ERASE SCREEN | `J` |
   | KEY TO MOVE CURSOR DOWN | `B` |
   | KEY TO MOVE CURSOR LEFT | `D` |
   | KEY TO MOVE CURSOR RIGHT | `C` |
   | KEY TO MOVE CURSOR UP | `A` |
   | PREFIXED[ERASE SCREEN] | `T` |
   | PREFIXED[KEY FOR MOVING CURSOR DOWN] | `T` |
   | PREFIXED[KEY FOR MOVING CURSOR LEFT] | `T` |
   | PREFIXED[KEY FOR MOVING CURSOR RIGHT] | `T` |
   | PREFIXED[KEY FOR MOVING CURSOR UP] | `T` |


5. After the last field is displayed, type `Q` two times.

6. Type `D` to write `NEW.MISCINFO`.

7. Type `E` to leave `SETUP`.

**Result:** You are back at the `Command:` prompt with a new file `NEW.MISCINFO` on your disk.

### Step 4.7: Install the new settings

**Overview:** We will keep a copy of the old settings as `OLD.MISCINFO` and give the new file the name `SYSTEM.MISCINFO`.

**How to do it:**

1. Type `F` for the Filer.

2. Type `C`, then `SYSTEM.MISCINFO`, and push RETURN.

3. Type `OLD.MISCINFO` and push RETURN.

4. Type `C`, then `NEW.MISCINFO`, and push RETURN.

5. Type `SYSTEM.MISCINFO` and push RETURN.

6. Type `Q`.

7. Stop the machine.

**Result:** The Filer shows `WORK:SYSTEM.MISCINFO changed to OLD.MISCINFO`. Then it shows `WORK:NEW.MISCINFO changed to SYSTEM.MISCINFO`.

### Step 4.8: Test the disk

**Overview:** We will start the disk with the new settings and open the program in the Editor to see the screen codes operate.

**How to do it:**

1. Start the machine with `PASDD.DSK` in drive 0.

2. Type `E` for the Editor.

3. Type `FGOTOXY` and push RETURN. You can now move around in the file with the arrow keys.

4. Stop the machine.

**Result:** Your program appears on the screen with no stray characters and you can move around a file in the editor.

### Phase 4 End Result

You now have these disks:

| Disk          | Use                                                           |
| ------------- | ------------------------------------------------------------- |
| `CPMWORK.DSK` | A CP/M SSDD work disk, with the development tools          |
| `PASDD.DSK`   | A Pascal SSDD work disk configured to use a VT52 terminal |
| `PASSD.DSK`   | A Pascal SSSD boot disk. You used it to build `PASDD.DSK`     |

Both of the work disks read SSSD disks in drive 1, so you can use disks that originally shipped as SD. CP/M reads an SD CP/M disk, such as `TOOLS.DSK`. Pascal reads an SD Pascal disk, such as `PASCAL.DSK`.

The real system this is based on only supports single-sided disks. The two bonus sections below take this further, as the emulator supports double-sided disks.

#### Keys in the Editor

- The arrow keys send `ESC A`, `ESC B`, `ESC C` and `ESC D`, and they move the cursor.

- ESC starts each arrow key code, so push ESC two times to cancel an insert.

- Push CTRL-C to accept an insert.

#### Rules for disks in drive 1

- The BIOS finds the density when Pascal selects a drive.

- Pascal selects a drive only when the drive changes.

- Do not change the disk in drive 1 and then use drive 1 at once. Use drive 0 first. For example, list `#4:`, then list `#5:`.

- A disk that Pascal cannot read gives `No directory on volume`. This includes a CP/M DD disk.

- Do not use the same volume name in two drives. The Filer gives `Vol went off-line`. Change one name with the Filer `C` command.

## Bonus 1: Create a DSDD CP/M work disk

The simulator supplies the two-sided signal that the author's real machine does not. Do not try the bonus phases on the real machine. In the simulator, we can use this signal to create double-sided work disks that provide more free space for future projects.

CP/M needs no change for this phase, because the CP/M BIOS already reads and writes double-sided disks.

### Step B1.1: Make and format the disk

**Overview:** We will make a blank double-sided disk and format it for CP/M. `media=8ds` makes a double-sided disk.

**How to do it:**

1. Start the simulator, put `CPMWORK.DSK` in drive 0, and make a blank disk image in drive 1. Then start the machine, select the VT52 window and wait for the `A>` prompt:


   ```
   pascal100 -f buildkit.toml
   MOUNT fd0:drive0 "CPMWORK.DSK"
   MOUNT fd0:drive1 "CPMDS.DSK" CREATE media=8ds
   RUN 0
   ```


2. Type `FORMAT` and push RETURN.

3. Type `B` to select `B:` (drive 1).

4. Type `3` for 1024-byte sectors, DD. Do not push RETURN.

5. Type a lower case `y`. If you push RETURN here, `FORMAT` stops with `Format error`.

6. Wait for `FORMAT complete`.

7. Type `N` to go back to CP/M.

8. Type `STAT B:DSK:` and push RETURN.

**Result:** `STAT B:DSK:` shows `1200: Kilobyte Drive  Capacity`.

### Step B1.2: Write the system and copy the files

**Overview:** We will use `SYSGEN` to copy the system from the work disk to the new disk, and then use `PIP` to copy all the files.

**How to do it:**

1. Type `SYSGEN` and push RETURN.

2. At `Source drive name (or RETURN to skip).`, type `A`.

3. At `Source on A, then type return.`, push RETURN. `SYSGEN` reads the system from `A:` (drive 0).

4. At `Destination drive name (or RETURN to terminate).`, type `B`.

5. At `Destination on B, then type return.`, push RETURN. `SYSGEN` writes the system to `B:` (drive 1).

6. At `Destination drive name (or RETURN to terminate).`, push RETURN to go back to CP/M.

7. Type `PIP B:=A:*.*[V]` and push RETURN. `PIP` copies all the files from `A:` (drive 0) to `B:` (drive 1).

8. Stop the machine.

**Result:** `SYSGEN` shows `Function complete.` after steps 3 and 5. `PIP` shows `COPYING -` and the name of each file, then the `A>` prompt.

### Step B1.3: Start CP/M from the disk

**Overview:** We will now start the machine from the new disk.

**How to do it:**

1. Start the machine with `CPMDS.DSK` in drive 0.

2. Stop the machine.

**Result:** The screen shows `64K CP/M 2.2K` and the `A>` prompt.

### Bonus 1 End Result

`CPMDS.DSK` is now your CP/M DSDD work disk. It holds 1192k of files, about twice as much as an SSDD disk.

## Bonus 2: Create a DSDD Pascal work disk

The BIOS shows a double-sided disk to Pascal as one disk with more tracks. A cylinder is the pair of tracks, one on each side, at one head position. Track 0 is on side 0 of cylinder 0, and track 1 is on side 1 of cylinder 0. After that, the tracks go in turn on side 0 and side 1 of each cylinder from 1 to 76.

The BIOS that you installed in Phase 3 understands this disk geometry, so the boot files that you already have on your current work disk can read a DSDD disk. As a result, this phase needs no new code.

Do not use a DSDD Pascal boot disk with a different BIOS than the one we used here. Without the correct BIOS the Filer `T` command can write over the files with no error.

### Step B2.1: Make and format the disk

**Overview:** We will make a blank double-sided disk and format it for Pascal. We will use our CP/M work disk for this, because Pascal does not have a format command.

**How to do it:**

1. Start the simulator, put `CPMWORK.DSK` in drive 0, and make a blank disk image in drive 1. Then start the machine, select the VT52 window and wait for the `A>` prompt:


   ```
   pascal100 -f buildkit.toml
   MOUNT fd0:drive0 "CPMWORK.DSK"
   MOUNT fd0:drive1 "PASDS.DSK" CREATE media=8ds
   RUN 0
   ```


2. Type `FORMAT` and push RETURN.

3. Type `B` to select `B:` (drive 1).

4. Type `2` for 512-byte sectors, DD. Do not push RETURN.

5. Type a lower case `y`. If you push RETURN here, `FORMAT` stops with `Format error`.

6. Wait for `FORMAT complete`.

7. Type `N` to go back to CP/M.

8. Stop the machine.

**Result:** `FORMAT` shows `FORMAT complete`.

### Step B2.2: Make the directory and copy the files

**Overview:** We will make an empty directory on the new disk, name the volume `BIG` and copy all the files from `PASDD.DSK`. As a result, the new disk gets the VT52 settings.

A double-sided disk has 154 tracks. Track 0, on side 0, holds the boot. On a CompuPro disk, the other side of cylinder 0 is DD but CP/M does not use it. Our Pascal BIOS uses it for data, so 153 tracks hold 15 blocks each: 153 × 15 = 2295 blocks.

**How to do it:**

1. Start the machine with `PASDD.DSK` in drive 0 and `PASDS.DSK` in drive 1.

2. Type `F` for the Filer.

3. Type `Z`, then `#5:`, and push RETURN.

4. Type `N` at `Duplicate dir ?`.

5. Type `2295` at `# of blocks ?` and push RETURN.

6. Type `BIG` at `New vol name ?` and push RETURN.

7. Type `Y` at `BIG: correct ?`.

8. Type `T`, then `#4:=`, and push RETURN.

9. Type `#5:$` and push RETURN.

10. Type `Q`.

11. Stop the machine.

**Result:** The Filer shows `BIG: zeroed`, then one `transferred to` line for each file.

### Step B2.3: Write the boot and start the disk

**Overview:** We will write the boot to the new disk, and then start Pascal from it.

**How to do it:**

1. Start the machine with `CPMWORK.DSK` in drive 0 and `PASDS.DSK` in drive 1.

2. Type `STAT B:DSK:` and push RETURN.


   Do not skip this step. Without it, `BOOTGEN` stops with `DISK ERROR -- GIVING UP`. CP/M must first find the density of drive 1.


3. Type `BOOTGEN` and push RETURN.

4. At `Which drive to read Pascal-100 booter from?`, push RETURN to skip the read.

5. At `Want to see/modify boot parameters?`, type `N`.

6. At `Which drive to write Pascal-100 booter to?`, type `B`.

7. At `Hit <ret> to write booter.`, push RETURN.

8. At `Want to write it again?`, type `N`.

9. At `Remount CP/M diskette in A`, push RETURN to go back to CP/M.

10. Stop the machine.

11. Start the machine with `PASDS.DSK` in drive 0.

12. Stop the machine.

**Result:** `STAT B:DSK:` shows `1124: Kilobyte Drive  Capacity`. `BOOTGEN` shows `Digicomp Research, Pascal-100 Boot Maintainer 7/15/81`. The `A>` prompt comes back after step 9. Pascal starts with `Welcome  BIG,  to`, then the `Command:` prompt.

### Bonus 2 End Result

`PASDS.DSK` is now your Pascal DSDD work disk, with VT52 settings. It holds 2295 blocks, and an SSDD disk holds 1140. It can read SSSD, SSDD and DSDD Pascal disks in drive 1.

## If something goes wrong

| What you see | Cause | What to do |
|---|---|---|
| `command not found` when you start `pascal100` | The `pascal100` command is not installed | Do Step 1.1 again. |
| `has no property 'title'` when you start `pascal100` | The simulator is version 0.1.0, older than this guide | Install the newest release. Do Step 1.1 again. |
| `no such file` after `MOUNT` | The name is wrong, or the file is not in the work folder | Run `QUIT`, then look at the file names in the work folder. |
| `WRITE-PROTECTED` after `MOUNT`, or `Bdos Err On B: Bad Sector` | The disk file is read-only | Stop the machine. Run `chmod u+w *.DSK`. |
| `R: NAME: no such file` | The file is not in the work folder | Copy the file to the work folder. |
| `Format error` | You pushed RETURN at the `FORMAT` confirm prompt | Push RETURN to leave. Run `FORMAT` again, and type `y`. |
| `DISK ERROR -- GIVING UP` | CP/M does not know the density of drive 1 | Type `STAT B:DSK:` first. |
| `Vol went off-line` | Two disks have the same volume name | Change one name with the Filer `C` command. |
| `No directory on volume` | Pascal cannot read the disk, or you changed the disk | Use drive 0, then use drive 1 again. |
| The Editor does not load | You changed the name of the booted volume | Start again from Step 4.1. |
