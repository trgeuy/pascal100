# Builds

Each folder here is a build: a step-by-step guide that takes a simulated machine from its
period distribution disks to a working system, with a build kit to download. The guides are
written for a person alone at the simulator. You do not need any real hardware.

An example in [`examples/`](../examples/README.md) boots a finished system. A build shows you how
that system was made, one step at a time.

| Build | What you build | Guide | Download |
|---|---|---|---|
| [`pascal-iii/`](pascal-iii/README.md) | UCSD Pascal III.0 and CP/M 2.2 work disks for the Digicomp Pascal-100, from the three disks that came with the boards | [`HOWTO-Build-the-System.md`](pascal-iii/HOWTO-Build-the-System.md) | [`pascal-iii-buildkit.zip`](pascal-iii/pascal-iii-buildkit.zip) |

Each build folder has the same parts:

- `README.md`: what the build makes, and what to download.
- The guide.
- `files/`: the files of the kit.
- `make-buildkit.sh`: makes the zip from `files/`, the guide and any disk from this repository.
  Run it after you change any of them.
- The zip to download.
