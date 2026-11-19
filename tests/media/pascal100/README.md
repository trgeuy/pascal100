# Pascal-100 test media

`pascal-dd.dsk` is the same image as `examples/pascal/pascal-dd.dsk`: UCSD Pascal III.0 for the
Pascal-100, double density, VT52 (the master `Pascal-DD-OneBIOS`, which passed on the real box on
2026-11-06), with the boot fixed on 2026-11-19: `PFBOOT` sets its own stack (see the changelog).
SHA-1 `a476aad45042519af47da31d7bcf52bcb2da454a`.

`acceptance-pascal100-boot` boots a copy of it. A test of the simulator does not read `examples/`
(`examples-isolation`), so this copy is here. The disk is not covered by the MIT license of
pascal100.
