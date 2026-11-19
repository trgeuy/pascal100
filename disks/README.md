# disks/

This directory holds disk images and their notes for the simulated disk controllers. It is a
working collection for someone who has the repository. It is not part of the release package.

Only the files that `.gitignore` names one at a time are tracked. A download that you put here
stays out of git until somebody adds a line for it.

## How it is arranged

The first level is the disk controller, and the second level is the operating system. Each
folder below that has its own `README.md`.

```
disks/<controller>/<operating system>/
```

| Folder | Controller | What is in it |
|---|---|---|
| [`compupro-disk1/cpm22/`](compupro-disk1/cpm22/README.md) | CompuPro Disk 1 | `CPM.DSK`, the CompuPro CP/M 2.2 disk that the Pascal-100 build guide starts from. |
| [`tarbell-sd/cpm22/buffered/`](tarbell-sd/cpm22/buffered/README.md) | Tarbell single-density | The BIOS source that the Tarbell board document cites. The disk is a test disk. |

## The Pascal-100 disks

The two Digicomp Research disks of the Pascal-100 are on the Internet Archive, with the
Pascal-100 manual: **<https://archive.org/details/cpm-files>**.

| Internet Archive file | Name in the build guide | What it is | SHA-1 |
|---|---|---|---|
| `CPM_FIles.dsk` | `TOOLS.DSK` | A CP/M disk with the Pascal-100 programs for CP/M, and the sources of the Pascal boot and BIOS. Single density. | `3fc3773d8fb5768fff03c4e3d039a18dc13030c8` |
| `UCSD_Pascal_III.dsk` | `PASCAL.DSK` | The UCSD Pascal III.0 system disk. It does not boot as supplied; the build guide makes the parts it needs. Single density. | `bca58369bc31ebecff4f9372fdb66713ff399230` |

Download each file and rename it to the name in the guide. The guide is
[`buildkit/HOWTO-Build-the-System.md`](../buildkit/HOWTO-Build-the-System.md), and
[`buildkit/README.md`](../buildkit/README.md) has direct download links.

## Where other disks are

| Location | What it holds |
|---|---|
| [`examples/`](../examples/) | The examples that ship in the release package, each with its machine file and its media. `examples/cpm22/cpm22k-64k.dsk` is our CompuPro CP/M 2.2K disk. |
| [`tests/media/`](../tests/media/) | The disks that the acceptance tests boot. The media for a new test goes there. |
