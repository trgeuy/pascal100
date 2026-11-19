pascal100 build kit: UCSD Pascal III on the Pascal-100
======================================================

This kit holds what HOWTO-Build-the-System.md needs to build a UCSD Pascal
III.0 system for the simulated Pascal-100 from its distribution disks. The
simulator comes separately: get the archive for your computer from
https://github.com/trgeuy/pascal100/releases

WHAT IS HERE

  HOWTO-Build-the-System.md       The guide. Start here.
  CPM.DSK                         The CompuPro CP/M 2.2 disk, the first of
                                  the three starting disks. It is read-only:
                                  the guide tells you to work on a copy.
  buildkit.toml                   The machine file.
  NEWBIOS.ASM, NEWBOOT.ASM        The sources of the new CP/M BIOS and boot.
  PBIOS.ASM, PFBOOT.ASM,          The sources of the new Pascal BIOS and
  PSBQB.ASM, QGENC.ASM            boot.

THE TWO PASCAL-100 DISKS

The other two starting disks are not in this kit. They are on the Internet
Archive, with the Pascal-100 manual:

  https://archive.org/details/cpm-files

Download these two files. Rename each one, and put it in this folder, next
to CPM.DSK. The names must be exact, in capital letters.

  https://archive.org/download/cpm-files/CPM_FIles.dsk
  https://archive.org/download/cpm-files/UCSD_Pascal_III.dsk

  Download this file      Rename it to   SHA-1 of the file
  ---------------------   ------------   ----------------------------------------
  CPM_FIles.dsk           TOOLS.DSK      3fc3773d8fb5768fff03c4e3d039a18dc13030c8
  UCSD_Pascal_III.dsk     PASCAL.DSK     bca58369bc31ebecff4f9372fdb66713ff399230

"CPM_FIles.dsk" has a capital I in "FIles". Each file is 256,256 bytes. On
macOS or Linux, "shasum TOOLS.DSK PASCAL.DSK" shows the SHA-1 of the two
files. Step 1.2 of the guide gives the commands.

THE MANUALS

The guide names four manuals. They are not in this kit.

  Pascal-100 Manual (Digicomp Research)
      On the Internet Archive, with the two Pascal-100 disks:
      https://archive.org/details/cpm-files

  CompuPro Disk 1 Manual (CompuPro/Godbout, board 171F)
  CompuPro Interfacer 1 Manual (CompuPro/Godbout, board 133H)
      In Mike Douglas's archive at deramp.com, with the other CompuPro
      board manuals.

  Beginner's Guide for the UCSD Pascal System (1980)
      On Hans Otten's Pascal site:
      http://pascal.hansotten.com/uploads/ucsd/ucsd/Beginner's_Guide_for_the_UCSD_Pascal_System_1980.pdf

LICENSE

CPM.DSK holds CP/M 2.2 by Digital Research and the CompuPro programs and
sources. The .ASM files are edits of the CompuPro and Digicomp Research
sources, and new code in the same structure. The MIT license of pascal100
does not cover the disk or the .ASM files.

Report problems at https://github.com/trgeuy/pascal100/issues
