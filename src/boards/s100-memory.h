#pragma once
//
// The `memory` board -- docs/boards/s100-memory.md.
//
// ONE BOARD IS ONE CARD. A card carrying 48K of RAM and three PROM sockets is
// one board, so this class holds a LIST OF REGIONS, each `ram` or `rom`. That
// collapse is what makes an empty PROM socket and an unpopulated RAM page the
// same case: neither is covered by a region, so the board does not decode it,
// nothing drives the bus, and it reads 0xFF.
//
// THERE IS NO WRITE-PROTECT ON THIS BOARD, and there must never be one. A `rom`
// region simply DOES NOT DECODE A WRITE CYCLE -- it does not reject the write,
// or ignore it, or log it. It never answers. Everything else (the byte
// vanishing, or falling through to RAM on another card) is emergent from what
// else is on the bus, and needs no rule anywhere.

#include "core/board.h"
#include "core/hex.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace altair {

enum class RegionKind { Ram, Rom };
enum class PhantomAssert { None, Read, All };

// WHAT I DO WHEN SOMEONE ELSE PULLS PHANTOM* -- and it has the same three
// positions as the assert strap, because it is the same kind of jumper.
//
//   none  I ignore the pin. I keep driving. (If a ROM is shadowing me, that is
//         two boards driving one cycle, and the bus reports the contention.)
//   read  I switch off for READS and keep answering WRITES. THIS IS THE TARBELL
//         CASE: its PROM shadows my reads while the byte still lands in my RAM
//         underneath, which is how the bootstrap deposits a sector into the very
//         addresses the PROM is covering.
//   all   I switch off entirely for the cycle.
//
// The read/write distinction lives HERE, on the HONORING board, and an earlier
// version of this file said in bold that it must never do so -- that the
// asserting card should gate PHANTOM* with its read strobe instead. That was
// wrong, and it was wrong because it was reasoned rather than sourced. PATRICK
// READ THE TARBELL SCHEMATIC (2026-07-12): the card holds PHANTOM* asserted like
// an interrupt, continuously, from RESET until A5 releases it -- it does not gate
// the pin with MEMR at all. The memory card is what is strapped to ignore it on
// writes.
enum class PhantomHonor { None, Read, All };
enum class Fill { Zero, Random };

// NO BANK SWITCHING HERE. This board used to carry a `bank_type=` strap with one
// SIMH-derived encoding for all five real cards; an audit against the period
// manuals and found it wrong for four of them, because they are genuinely different
// decoders (DESIGN.md 4.3). So `memory` is purely plain RAM/ROM again -- which is
// also the honest model of the SD Systems ExpandoRAM I, a static-strapped board with
// no I/O port at all. (Banking then lived on a board of its own, `bankmem`, removed in
// 2026-11. A real banked card, if one is added, gets its own board.)

struct Region {
    RegionKind kind = RegionKind::Ram;
    uint32_t at = 0;        // a bus address: A0-A15, or A0-A23 on a board with `ext`
    uint32_t size = 0;      // bytes, rounded up to a 0x100 page
    std::string mount;      // rom only: a host path, or "builtin:<name>"

    // ...AND WHERE THAT PATH ACTUALLY LEADS. The two differ only when the ROM was
    // named by a machine file in another directory (core/paths.h): `mount` is what
    // the file said, `mountFile` is where we found it.
    //
    // THIS CARD IS THE ONLY ONE THAT HAS TO KEEP BOTH, and the reason is power():
    // a ROM region is re-read from the host on EVERY power cycle, long after the
    // loader has packed up and gone home. A disk or a tape is opened once and the
    // handle survives, so those cards resolve and forget. This one cannot.
    std::string mountFile;

    // rom only: honor `at` for a HEX/S-record image instead of the address its own
    // records encode. Off by default -- a relocated ROM is the footgun the loader's
    // self-place check exists to catch, so it only happens when a region asks for it.
    // A `Bin` mount always honors `at`, so this key does nothing for one. See #468.
    bool relocate = false;

    // `ext`: the board decodes A0-A23 (MemoryBoard `ext`), so every address prints in six
    // digits, as the board's `pages` line does. Without it, six only past FFFF.
    std::string describe(bool ext = false) const;
};

class MemoryBoard : public Board {
public:
    MemoryBoard() : owner_(256, -1), planes_(1) {}  // every page unpopulated until a region says so
    // Not copyable: plane_[] points into this board's own planes_.
    MemoryBoard(const MemoryBoard&) = delete;
    MemoryBoard& operator=(const MemoryBoard&) = delete;
    std::string type() const override { return "memory"; }

    // ---- config ----
    bool addRegion(Region r, std::string& err);
    const std::vector<Region>& regions() const { return regions_; }

    // ---- bus (DESIGN.md 4.2) ----
    bool assertsPhantom(const BusCycle& c) const override;
    bool decodes(const BusCycle& c) const override;

    // Do I switch off for this cycle when someone else pulls PHANTOM*?
    bool honors(const BusCycle& c) const;
    uint8_t read(const BusCycle& c) override;
    void write(const BusCycle& c) override;

    // RAM and ROM can always be looked at without being disturbed -- that is what
    // makes them memory. (An unpopulated page is not covered by a region, so the
    // bus never asks this board about it in the first place.)
    //
    // A 16-bit address is segment 00 (A16-A23 = 0), as on the bus; peekX() takes A0-A23.
    //
    // peek() is ON THE HOT PATH: the CPU's instruction history peeks code bytes on every
    // instruction (Bus::peekBytes). So it must not call peekX() -- that is a second
    // virtual call -- and both go to the inline peekKey(). (Measured 2026-11-12.)
    bool peek(uint16_t addr, uint8_t& out) const override { return peekKey(addr, out); }
    bool peekX(uint32_t phys, uint8_t& out) const override { return peekKey(key(phys), out); }

    // ---- IEEE 696 settings (reference/IEEE_696_1983.md) ----
    //
    // `ext` = false (the default): STANDARD addressing (2.2.2), A0-A15 only. The board
    // does not look at A16-A23, so it answers in every 64K segment, and it holds at most
    // 64K. `ext` = true: EXTENDED addressing, A0-A23. Region `at` is a 24-bit bus
    // address and a region may be any size up to the top of the 16M space -- so one
    // board can be a 128K or a 1M block, or hold blocks in unrelated segments.
    // `sixteen_bit` = the board answers SIXTN* and moves a word in one cycle (2.6): an
    // even-byte bank and an odd-byte bank (Fig. 4).
    bool decodesExtended() const override { return ext_; }
    bool answersSixtn(const BusCycle&) const override { return sixteen_; }
    void read16(const BusCycle& c, uint8_t& even, uint8_t& odd) override;
    void write16(const BusCycle& c) override;

    // ---- lifecycle (DESIGN.md 6) ----
    // No reset() override: neither POC* nor RESET* touches RAM, and there is no bank
    // latch to clear any more, so the base no-op is exactly right.
    void power() override;

    // ---- state: SNAPSHOT / RESTORE (DESIGN.md 13) ----
    //
    // The store (RAM, and any ROM plane the guest or the burner mutated) and the
    // selected bank travel. The regions, straps, fill policy and mount paths are
    // config and are already correct in a matching machine. RESTORE writes the planes
    // directly -- it must NOT go through power(), which re-fills RAM and re-reads
    // ROM images and would wipe exactly the state being restored.
    void serialize(StateWriter& w) const override;
    void deserialize(StateReader& r) override;

    // ---- reflection ----
    std::vector<Property> properties() override;

    // ---- The PROM burner (DESIGN.md 10.2) ----
    //
    // peek()'s mirror, and deliberately its exact shape: a BUS address in, the live
    // bank applied, and the board's own owner() deciding whether it is ours to answer
    // for. The ONLY difference is that this one writes, and that a ROM region does not
    // stop it -- which is the entire point. A bus write cannot program a PROM (§4.2),
    // because on real hardware it cannot; you pull the chip and put it in a programmer,
    // and that is not a bus operation. This is that programmer.
    //
    // It takes a bus address and not a store offset ON PURPOSE. Every address the
    // operator types means the same thing everywhere in this monitor -- 0000-FFFF, what
    // the CPU would see. The store offset (bank * 64K + addr) is this board's private
    // business and stays that way; to reach another bank, SELECT it (SET mem0 bank=3)
    // and use ordinary addresses, exactly as the guest must.
    //
    // False means "not mine" -- this board does not decode that address. The caller
    // says so; this does not guess and does not silently drop the byte.
    bool poke(uint16_t addr, uint8_t v) { return pokeX(addr, v); }
    // The same with A16-A23.
    bool pokeX(uint32_t phys, uint8_t v) {
        const uint32_t k = key(phys);
        if (!owner(k)) return false;
        byteAt(k) = v;
        return true;
    }

    // ---- The store, flat, by offset. NOT AN ADDRESS SPACE ANYONE TYPES. ----
    //
    // This is the card's own view of its own silicon, by board address (key(): the bus
    // address, A0-A15, or A0-A23 with `ext`). It exists so that this board's invariants can be
    // asserted from outside -- that random fill really is random -- and for nothing
    // else. Nothing the operator or an agent can reach comes through here.
    //
    // It is deliberately NOT on Board. It was, once, as rawRead/rawWrite, and the price
    // was a whole second address space bolted to every card in the machine to serve one.
    // If you are reaching for this from anything but a test, you want peek() or poke(),
    // which speak bus addresses like the rest of the program.
    size_t storeSize() const { return ext_ ? 0x1000000u : 0x10000u; }
    // Past the end, or in a segment with no region, is a CALLER bug -- bounds-check
    // with storeSize(). It does NOT answer 0xFF: that is the BUS's floating value, and
    // a board has no business forging it (DESIGN.md 4.6.1), least of all where the
    // bus cannot see.
    uint8_t storeAt(size_t off) const {
        if (off >= storeSize()) return 0x00;
        const auto& pl = planes_[off >> 16];
        return pl.empty() ? 0x00 : pl[off & 0xFFFF];
    }

    std::vector<MapEntry> memMap() const override;
    // No ioMap() override: this board claims no I/O port.

    std::vector<std::string> subUnitTables() const override { return {"region"}; }
    std::vector<Property>    subUnitProperties(const std::string& table) const override;
    std::vector<SubUnit>     subUnits() const override;

    // A ROM region is a SOCKET, and a socket is a unit: `MOUNT mem0:rom0 dbl.hex`.
    //
    // A RAM region is NOT a unit. There is no chip to pull and nothing to mount
    // into it, and offering `mem0:ram0` would be offering something that can only
    // ever fail. (To put bytes in RAM you LOAD them, exactly as the CPU would.)
    std::vector<UnitDef> units() const override;
    bool mount(const std::string& unit, const std::string& path, bool ro,
               std::string& err) override;
    bool unmount(const std::string& unit, std::string& err) override;

    // "rom1" -> its index in regions_. regions_.size() means this card has no such
    // unit -- the caller reports it; this does not guess.
    size_t romRegionIndex(const std::string& unit) const;

    // Load an image into the store, behind the bus. Used by power-up ROM
    // loading and by `LOAD ... RAW`.
    bool blit(const Image& img, std::string& err);

    // Messages the board wants said out loud (a bank select it could not
    // decode, a ROM that failed to load). Drained by the monitor.
    std::vector<std::string> drainLog() override;

protected:
    // Reached only through Board::loadSubUnit(), which has already vetted the keys.
    bool addSubUnit(const std::string& table, const KeyValues& kv, std::string& err) override;

private:
    static constexpr uint32_t kPage = 0x100;
    static size_t page(uint32_t off) { return off >> 8; }

    // THE BOARD ADDRESS: the bus address as far as this board decodes it -- A0-A15
    // without `ext` (every segment is the same 64K), A0-A23 with it.
    uint32_t key(uint32_t phys) const { return ext_ ? (phys & 0xFFFFFFu) : (phys & 0xFFFFu); }
    uint32_t key(const BusCycle& c) const {
        return ext_ ? (((uint32_t)c.ext << 16) | c.addr) : c.addr;
    }

    // A 16-bit address is its own key in both modes (segment 00 with `ext`).
    bool peekKey(uint32_t k, uint8_t& out) const {
        if (!owner(k)) return false;
        out = byteAt(k);
        return true;
    }

    // The page table stores an INDEX, not a Region* -- pointers into regions_
    // dangle the moment the vector reallocates, which it does on the second
    // region of every combo card. One entry per 256-byte page: 256 pages without
    // `ext`, 65536 with it.
    const Region* owner(uint32_t k) const {
        int i = owner_[page(k)];
        return i < 0 ? nullptr : &regions_[(size_t)i];
    }

    // The bytes. One 64K plane per segment, made only for a segment a region uses,
    // so 128K at 000000 plus a ROM at FF0000 costs 192K, not 16M. Only reached for a
    // key that owner() covers, and a covered page always has its plane.
    //
    // THE HOT PATH reads through plane_[], a fixed table of raw pointers inside the board:
    // one load for the plane, one for the byte -- what the old flat 64K store cost. Going
    // through planes_ (a vector of vectors) is one load more, and measured 7-8% slower on
    // the CPU benchmark (2026-11-12). makePlanes() and power() keep the table in step.
    uint8_t  byteAt(uint32_t k) const { return plane_[k >> 16][k & 0xFFFF]; }
    uint8_t& byteAt(uint32_t k) { return plane_[k >> 16][k & 0xFFFF]; }
    void pointPlanes() {
        for (size_t i = 0; i < 256; ++i)
            plane_[i] = (i < planes_.size() && !planes_[i].empty()) ? planes_[i].data() : nullptr;
    }

    // The largest board address + 1: 64K without `ext`, 16M with it.
    uint32_t limit() const { return ext_ ? 0x1000000u : 0x10000u; }
    // Make the plane of every segment a region covers; drop none. A new plane is
    // zero until fillRegion() or a ROM load puts something in it.
    void makePlanes();

    void rebuildPageMap();
    bool loadRomRegion(size_t idx, std::string& err);
    void fillRegion(size_t idx);
    void fillRam();

    std::vector<Region> regions_;
    std::vector<int> owner_;                   // page table: region per page, or -1
    std::vector<std::vector<uint8_t>> planes_;  // per segment: empty, or 64K
    uint8_t* plane_[256] = {};                  // planes_[i].data(), or nullptr (pointPlanes)

    PhantomHonor honors_ = PhantomHonor::All;
    PhantomAssert phantom_ = PhantomAssert::All;

    Fill fill_ = Fill::Random;
    uint64_t seed_ = 1;

    bool ext_ = false;      // IEEE 696: extended addressing, decodes A16-A23
    bool sixteen_ = false;  // IEEE 696: answers SIXTN*

    std::vector<std::string> log_;
};

} // namespace altair
