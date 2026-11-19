# Running it

`pascal100` is one program file. Unzip the package, and run the program from a terminal.

```
$ ./pascal100
pascal100 X.Y.Z (commit) -- z80, full speed.
machine: default.  HELP for commands.
pascal100>
```

On **Windows**, the program is `pascal100.exe`:

```
> pascal100.exe
```

You can run the program from any folder. Put its folder on your `PATH`, and type `pascal100`.

The `pascal100>` prompt is **the monitor**. The machine has power, but it is stopped. No program
runs until you start one.

## macOS: the first run

Start the macOS program from a terminal. Do not double-click `pascal100` in Finder, because
Finder opens a plain program file as a document.

A package that you got with a browser has a quarantine mark. If the program is not notarized by
Apple, macOS refuses to start it while it has this mark. Remove the mark one time, in the
folder of the package:

```
$ xattr -dr com.apple.quarantine ./pascal100
```

The chapter *Troubleshooting* tells you more.

## Getting help, and getting out

| Type | To |
|---|---|
| `HELP` | list every command |
| `HELP DUMP` | show the usage and examples for one command |
| `QUIT` | leave the program |

**There is no `EXIT` command.** Type `QUIT`, or only `Q`.

You can type a short form of a command. `HELP` shows each command with brackets, for example
`D[UMP]`, `DE[POSIT]` and `RES[ET]`. The part before the bracket is the short form. You can type
the short form, the full name, or any length between them.

**Commands are not case-sensitive.** Board names, board types, settings and their values are
not case-sensitive either. The same is true for endpoints such as `socket:2323` and for
command-line options such as `--mcp`. A file name, a host name and a serial device keep their
case. This manual writes commands in capitals because they are easier to read.

## Which machine you get

When you run `pascal100` with no arguments, you get the machine called `default`. It is a
CompuPro workbench: a Z80, an Interfacer 1 console, a Disk 1 floppy controller with its boot
EPROM, and 64K of RAM. If the folder that you are in has a file called `pascal100.toml`, you get
that machine instead.

To get a different machine, give its name or its file:

```
$ pascal100 examples/cpm22/cpm22-console.toml     a machine file: this one boots CP/M
$ pascal100 examples/pascal/pascal.toml          a machine file: the Digicomp Pascal-100
$ pascal100 default                              the BUILT-IN machine, by name
```

The **built-in** machine is a machine file that is stored in the program. It uses the same
format as the machine files in `examples/`. Each example file is a complete machine: open it to
read the boards. To see what is in a running machine, look:

```
$ pascal100 examples/pascal/pascal-console.toml -x BOARDS
```

To get a running machine as a file that you can edit, type `CONFIG SAVE mine.toml`. This
command writes the machine that you are running to a file, and that file boots.

The machines chapter describes every command-line option. It also tells you how `pascal100`
decides if a word is the name of a built-in machine or the name of a file.
