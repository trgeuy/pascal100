# CompuPro Disk 1 — CP/M 2.2LD

`CPM.DSK` is the CompuPro CP/M 2.2 system disk that came with a CompuPro Disk 1 floppy
controller. It is the first of the three disks that the Pascal-100 build guide
([`builds/pascal-iii/`](../../../builds/pascal-iii/README.md)) starts from. The other two are on the Internet
Archive; [`disks/README.md`](../../README.md) says where.

| | |
|---|---|
| Format | 8″, single-sided, double density (MFM), 1024-byte sectors, 625,920 bytes |
| System | `64K CP/M 2.2LD`, by Digital Research, CompuPro and G & G Engineering |
| SHA-1 | `021d57b14be05db4c94dbcf62fab0baeab96a646` |
| Source | Imaged from the original 8″ disk of the maintainer's Disk 1. `R.COM` and `W.COM` added. |

## What is on it

- The Digital Research programs: `ASM`, `DDT`, `DUMP`, `ED`, `LOAD`, `MOVCPM`, `PIP`, `STAT`,
  `SUBMIT`, `SYSGEN`, `XSUB`.
- The CompuPro programs: `FORMAT`, `COPY`, `CLOCK`, `DISK2`, and the `CPMLD*` system images for
  the CompuPro disk and memory configurations.
- The CompuPro BIOS and boot sources: `BIOS.ASM`, `GBBIOS.ASM`, `GBBOOT.ASM`, the `GBCBIOS*`
  family, `GBCBOOT*`, `GBCFDC`, `GBCHDC` and `GBROM.ASM`.
- `R.COM` and `W.COM`, the pascal100 host bridge programs, which copy files between your
  computer and the disk.

It has no third-party programs. About 10K is free.

## Boot it

```
pascal100
pascal100> MOUNT fd0:drive0 "CPM.DSK"
pascal100> RUN 0
```

The built-in `default` machine prints `64K CP/M 2.2LD` and `A>`. The Pascal-100 machine
(`tests/media/pascal100/machine-pascal100.toml`) boots it too (both checked 2026-11-14). Copy the image
before you write to it: the machine writes to the file.

## Related disks

- `examples/cpm22/cpm22k-64k.dsk`: CompuPro CP/M 2.2K, the disk of the CP/M example. It was made
  from the maintainer's CP/M 2.2K working disk (the same system that Phase 2 of the build guide
  makes), with `MAC`, `Z80.LIB`, `LS`, `R`, `W` and `HDIR` added. It also has the FujiNet tools.
- `tests/media/compupro/CCPM22LD.DSK`: the same CP/M 2.2LD distribution without `R.COM` and
  `W.COM`, for the `disk1a` acceptance test.
