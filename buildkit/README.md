# The Pascal-100 build kit

The build kit and its guide take the simulated Pascal-100 from the three disks that came with
the boards to a working UCSD Pascal III.0 system. The guide is written for a person alone at
the simulator. You do not need any real hardware.

## What you get

The guide, [`HOWTO-Build-the-System.md`](HOWTO-Build-the-System.md), is in phases. Each phase
ends with something that works, so you can stop after any phase:

| Phase | What you do | What you get |
|---|---|---|
| 1 | Install the simulator, set up the work folder and learn about the simulator | The `pascal100` command, a work folder, and CP/M on the screen |
| 2 | Get CP/M running and build a work disk | `CPMWORK.DSK` |
| 3 | Get single-sided, single-density (SSSD) Pascal running | `PASSD.DSK` |
| 4 | Get single-sided, double-density (SSDD) Pascal working | `PASDD.DSK` |
| Bonus 1 | Make a double-sided (DSDD) CP/M work disk | `CPMDS.DSK` |
| Bonus 2 | Make a DSDD Pascal work disk | `PASDS.DSK` |

## What to download

1. **The simulator.** Get the archive for your computer from the
   [releases page](https://github.com/trgeuy/pascal100/releases). The guide needs a version
   later than 0.1.0. Step 1.1 of the guide installs it.
2. **The build kit,** [`buildkit.zip`](buildkit.zip). It unpacks to a folder named `buildkit`,
   with the guide, the CP/M starting disk, the machine file, the BIOS and boot sources, and a
   `README.txt`.
3. **The two Pascal-100 disks.** They are not in the zip. They are on the Internet Archive, with
   the Pascal-100 manual, at <https://archive.org/details/cpm-files>. Download these two files,
   rename each one, and put it in the `buildkit` folder, next to `CPM.DSK`. Step 1.2 of the guide
   gives the commands.

   | Download this file | Rename it to | SHA-1 |
   |---|---|---|
   | [`CPM_FIles.dsk`](https://archive.org/download/cpm-files/CPM_FIles.dsk) | `TOOLS.DSK` | `3fc3773d8fb5768fff03c4e3d039a18dc13030c8` |
   | [`UCSD_Pascal_III.dsk`](https://archive.org/download/cpm-files/UCSD_Pascal_III.dsk) | `PASCAL.DSK` | `bca58369bc31ebecff4f9372fdb66713ff399230` |

   `CPM_FIles.dsk` has a capital I in `FIles`. Each file is 256,256 bytes.

Then start at Step 1.1 of the guide.

The guide names four manuals. They are not in the kit. `README.txt` in the kit says where to get
each one.

## Testing

The guide was tested on the simulator with pascal100 `0.1.0-11-g80dda969`, from a new copy of
the kit. The disks that Phases 2 to 4 make were also tested on a real Pascal-100.

## The files in this folder

| File | What it is |
|---|---|
| [`HOWTO-Build-the-System.md`](HOWTO-Build-the-System.md) | The guide. The zip has the same file. |
| [`buildkit.zip`](buildkit.zip) | The kit to download |
| [`files/`](files/) | The files of the kit: the machine file `buildkit.toml`, the BIOS and boot sources, and `README.txt` |
| [`make-buildkit.sh`](make-buildkit.sh) | Makes `buildkit.zip` from `files/`, the guide and [`../disks/compupro-disk1/cpm22/CPM.DSK`](../disks/compupro-disk1/cpm22/README.md). Run it after you change any of them. |

## License

The MIT license of pascal100 does not cover `CPM.DSK`, which holds CP/M 2.2 by Digital Research
and the CompuPro programs and sources. It also does not cover the `.ASM` files, which are edits
of the CompuPro and Digicomp Research sources, and new code in the same structure.
