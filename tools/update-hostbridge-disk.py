#!/usr/bin/env python3
"""Put the current R, W and HDIR on a disk image, through the machine that boots it.

usage: tools/update-hostbridge-disk.py <machine.toml> <COM|CMD> <prompt> [options] <disk> ...

    --mpm            MP/M: SET [RW] first, then SET [SYS,RO] and SYNC, and let MP/M write its
                     cache before the simulator stops (SYNC prints its prompt first).
    --progs=R,W      only these programs (the default is R,W,HDIR).
    --power-on       boot from the processor's own reset address (an 8086 at FFFF0, the
                     CPU 86/87), not `run from 0`, which suits an 8080/8085/Z80 boot ROM.

Run it in the folder of the machine file; the disks are the files that it mounts.

    cd examples/cpm22   && python3 -I ../../tools/update-hostbridge-disk.py \\
                              cpm22-console.toml COM "A>" cpm22k-64k.dsk
    cd examples/cpm86   && python3 -I ../../tools/update-hostbridge-disk.py \\
                              cpm86-console.toml CMD "A>" --power-on cpm86-11.dsk
    cd examples/mpm8-16 && python3 -I ../../tools/update-hostbridge-disk.py \\
                              mpm816-console.toml CMD "0A>" --mpm mpm816-boot.dsk m20.dsk

WHY NOT install-hostbridge-utils.sh. That script pastes R.HEX through the console of the
88-DCDD machine. This one uses the machine that the disk belongs to (any controller, CP/M 2.2,
CP/M-86, CP/M 8-16 or MP/M 8-16), and the R that is already on the disk copies the new files
on. The machine needs a hostbridge board (hostdir "" = the folder it starts in).

WHAT IT PROVES. It works on copies in a temporary folder. After the install it boots the
written copy again, from cold, and W sends each program back out to the host. Only if every
one is byte-identical to cpm/hostbridge/ are the disks replaced. Otherwise the disks are not
touched, and the transcripts and the temporary folder are kept.

The machine runs under --mcp: PASCAL100 names the binary, or build/pascal100 is used.
Written 2026-11-15; it updated the cpm22, ai-mcp, cpm86, cpm8-16 and mpm8-16 example disks and
disks/compupro-disk1/cpm22/CPM.DSK.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIM = os.environ.get("PASCAL100") or os.path.join(ROOT, "build", "pascal100")
HB = os.path.join(ROOT, "cpm", "hostbridge")

args = sys.argv[1:]
if len(args) < 4 or args[1] not in ("COM", "CMD"):
    sys.exit(__doc__)
toml, ext, prompt = args[0], args[1], args[2]
mpm = "--mpm" in args
power_on = "--power-on" in args
progs = ["R", "W", "HDIR"]
for a in args:
    if a.startswith("--progs="):
        progs = a[len("--progs="):].split(",")
disks = [a for a in args[3:] if not a.startswith("--")]
if not disks:
    sys.exit(__doc__)
if not os.path.exists(SIM):
    sys.exit(f"update-hostbridge-disk: no simulator at {SIM} (set PASCAL100)")


def session(stage, cmds, boot_ms):
    """One cold boot, then each command after the prompt. Returns the console text."""
    lines = [
        {"jsonrpc": "2.0", "id": 1, "method": "initialize",
         "params": {"protocolVersion": "2024-11-05", "capabilities": {},
                    "clientInfo": {"name": "update-hostbridge-disk", "version": "1"}}},
        {"jsonrpc": "2.0", "method": "notifications/initialized"},
        {"jsonrpc": "2.0", "id": 2, "method": "tools/call",
         "params": {"name": "run", "arguments": dict({} if power_on else {"from": 0},
                                                     until=prompt, timeout_ms=boot_ms)}},
    ]
    for i, c in enumerate(cmds):
        lines.append({"jsonrpc": "2.0", "id": 3 + i, "method": "tools/call",
                      "params": {"name": "run", "arguments": {"input": c + "\r", "until": prompt,
                                                              "timeout_ms": 60000}}})
    if mpm:
        # SYNC prints 0A> before MP/M writes its cache; a stop at the prompt lost every write
        # in 8 of 10 runs. 100 ms more was enough in 9 of 9; 3 s is a wide margin.
        lines.append({"jsonrpc": "2.0", "id": 900, "method": "tools/call",
                      "params": {"name": "run", "arguments": {"until": "@@never@@", "timeout_ms": 3000}}})
    inp = "".join(json.dumps(x) + "\n" for x in lines)
    r = subprocess.run([SIM, os.path.basename(toml), "--mcp"], cwd=stage, input=inp,
                       capture_output=True, text=True, timeout=600)
    text = []
    for line in r.stdout.splitlines():
        try:
            m = json.loads(line)
        except ValueError:
            continue
        sc = m.get("result", {}).get("structuredContent", {})
        if "output" in sc:
            text.append(sc["output"] + ("" if sc.get("stopped") == "match" else f"\n[{sc.get('stopped')}]"))
    if r.stderr.strip():
        text.append("[stderr] " + r.stderr.strip())
    return "\n".join(text)


stage = tempfile.mkdtemp(prefix="hbdisk-")
shutil.copy(toml, stage)
for d in disks:
    shutil.copy(d, stage)
for p in progs:
    shutil.copy(os.path.join(HB, f"{p}.{ext}"), stage)

boot_ms = 120000 if mpm else 60000
install = []
if mpm:
    install += [f"SET {p}.{ext} [RW]" for p in progs]
install += [f"R {p}.{ext}" for p in progs]
if mpm:
    install += [f"SET {p}.{ext} [SYS,RO]" for p in progs] + ["SYNC"]
out1 = session(stage, install, boot_ms)

os.mkdir(os.path.join(stage, "back"))
verify = [f"W {p}.{ext} BACK/{p}.{ext}" for p in progs]
if mpm:
    verify += [f"DIR {p}.{ext} [SYS]" for p in progs] + ["SYNC"]
out2 = session(stage, verify, boot_ms)

ok = True
for p in progs:
    back = os.path.join(stage, "back", f"{p}.{ext}")
    want = os.path.join(HB, f"{p}.{ext}")
    if not os.path.exists(back):
        print(f"  FAIL {p}.{ext} did not come back off the disk")
        ok = False
    elif open(back, "rb").read() != open(want, "rb").read():
        print(f"  FAIL {p}.{ext} on the disk differs from cpm/hostbridge/{p}.{ext}")
        ok = False
    else:
        print(f"  ok   {p}.{ext} runs off the disk, byte-identical")
if not ok:
    print("---- install ----\n" + out1 + "\n---- verify ----\n" + out2)
    print("the disks were NOT changed. Temporary folder kept:", stage)
    sys.exit(1)
for d in disks:
    shutil.copy(os.path.join(stage, os.path.basename(d)), d)
shutil.rmtree(stage)
print("replaced:", " ".join(disks))
