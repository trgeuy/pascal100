#pragma once
//
// CompuPro DISK 3 (board 194) -- an intelligent ST506 Winchester controller with its own 8085.
// See Reference/compupro/disk3.md (the May 1985 manual, D-revision firmware); page numbers
// below are the manual's. The driver this model was checked against is CompuPro's CP/M 8-16
// V1.1PD BIOS (GBCBIOS2.A86 equates, GBCBIOSA.A86 D3INIT, GBCBIOS6.A86), 1983.
//
// ONE WRITE-ONLY PORT PAIR (p. 10), S1 = A7..A1, default 90H (CompuPro's D3PORT):
//   - a byte with the board's reset bit set (J10, `reset_bit`, default bit 0) holds the 8085
//     in reset. The 1983 BIOS writes 01h ("reset only board 1"); the 1985 manual says FFh.
//     A byte with other bits only is another board's reset, and this board ignores it.
//   - 00h is an ATTENTION. It also lets the 8085 out of reset.
//   An IN reads nothing: the board drives no data.
//
// EVERYTHING ELSE IS DMA (pp. 11-17). The host builds a 16-byte IOPB in memory and writes the
// attention port. The board pulls pHOLD at its TMA priority (J11, `dma_priority`, default 9,
// the manual's), and when the bus is granted it moves one byte per granted step with a real
// 24-bit memory cycle: it fetches the IOPB, moves the data, writes back what the command
// changed, and writes the STATUS byte LAST (the host polls it: 0 = busy, FF = done).
//
// THE FIRST ATTENTION AFTER A RESET only fetches the 16 bytes at 0:50h and keeps their LINK
// (bytes 13-15); it runs nothing and writes no status (p. 11, the D revision). Every later
// attention fetches the IOPB at the LINK the previous IOPB gave, and runs it. The 1983 BIOS
// sends 01h, then 00h ("warm up ATTN"), then 00h -- so the second 00h runs its NOOP, as here.
// Bit 6 of the command (continue) chains to the next IOPB without an attention; bit 7 asks for
// the interrupt at the end (J9, `interrupt`), which the next attention or a reset clears. An
// attention while a command runs is lost, as it is on the board (the 8085 is not listening).
//
// COMMANDS (pp. 14-17): NOOP, VERSION, GLOBAL, SPECIFY, SET-MAP, HOME, SEEK, READ-HEADER, R/W,
// FORMAT, FORMAT-BAD are modelled. RELOCATE, STATUS, SELECT, EXAMINE and MODIFY touch things the
// manual does not describe (the drive status bits, the 8085's own memory); they answer as
// below and say so in the log, so a program that uses them shows where the model is thin.
//
// THE DISK. An image is the drive's physical sectors, cylinder by cylinder, head by head,
// sector 0 up -- DREM's raw .DSK layout. Its shape is the drive's: `cylinders`, `heads`,
// `sectors`, `sector_size`, by default the manual's Quantum Q540 (512 x 8 x 9 x 1024). The
// firmware's view comes from SPECIFY (22 bytes: sector size, sectors per track, heads,
// cylinders, RES-TRACKS). A logical track is moved past the label track and the RES-TRACKS
// reserved tracks (the +1 is inferred: see locate()), or to
// the reserved track its entry in the SET-MAP relocation map names (Appendix, p. 19-20). A
// sector the drive does not have (a SPECIFY that does not fit the image) is TIME OUT, header
// not found. FORMAT writes the track's sectors with the fill byte; the header image must be
// the drive's own cylinder, head and sectors 0..n-1 in any order (the skew).
//
// NOT IN THE MANUAL, CHOSEN HERE:
//   - After a reset the firmware "defaults to ST506 parameters and absolute sector mode"
//     (p. 18) without saying what they are. Here they are the drive's own shape, no reserved
//     tracks, so sectors 0 and 1 read as the manual promises.
//   - VERSION's number is not printed anywhere we have; it answers 0D (the D revision).
//   - HOME, SEEK and the data are instant (no rotation or seek time), as on the Disk 1 here.

#include "core/board.h"
#include "host/disk.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace altair {

class CompuProDisk3Board : public Board {
public:
    CompuProDisk3Board();

    std::string type() const override { return "disk3"; }

    // ---- the bus ----
    bool    decodes(const BusCycle& c) const override;
    uint8_t read(const BusCycle& c) override;
    void    write(const BusCycle& c) override;

    bool    assertsInt() const override;
    uint8_t assertsVi() const override;

    // ---- pHOLD: the board's own DMA (HAS-A Mover, as the Disk 1) ----
    bool       requestsBus() const override { return job_.phase != Phase::Idle; }
    BusMaster* busMaster() override { return &mover_; }
    int        tmaPriority() const override { return priority_; }

    void reset(Reset r) override;
    void power() override;

    std::vector<Property> properties() override;
    std::vector<MapEntry> ioMap() const override;

    std::vector<UnitDef> units() const override;
    bool mount(const std::string& unit, const std::string& path, bool ro, std::string& err) override;
    bool unmount(const std::string& unit, std::string& err) override;
    std::vector<Property> mountKeys(const std::string& unit) const override;
    bool mountWith(const std::string& unit, const std::string& path, bool ro, const KeyValues& keys,
                   std::string& err) override;

    std::vector<std::string> subUnitTables() const override { return {"drive"}; }
    std::vector<Property>    subUnitProperties(const std::string& table) const override;
    std::vector<SubUnit>     subUnits() const override;

    std::vector<std::string> drainLog() override;

    void serialize(StateWriter& w) const override;
    void deserialize(StateReader& r) override;

    // For tests.
    bool busy() const { return job_.phase != Phase::Idle; }
    bool held() const { return held_; }
    int  cylinder(int drive) const { return drive_[(size_t)drive].pos; }
    uint32_t link() const { return link_; }

    static constexpr int kDrives = 4;

protected:
    bool addSubUnit(const std::string& table, const KeyValues& kv, std::string& err) override;

private:
    // The firmware's picture of a drive: SPECIFY (p. 18-19) and the SET-MAP relocation map.
    struct Spec {
        int sectorSize = 1024, sectors = 9, heads = 8, cylinders = 512, reserved = 0;
    };

    struct Drive {
        std::unique_ptr<DiskImage> img;
        std::string path;  // as WRITTEN (round-trips through SAVE)
        int cylinders = 512, heads = 8, sectors = 9, sectorSize = 1024;  // the image's shape
        int pos = 0;                                                      // cylinder under the heads
        Spec spec;
        std::array<uint16_t, 128> map{};  // SET-MAP, words; all 0 = nothing relocated
    };

    enum class Phase : uint8_t {
        Idle,
        FetchLink,  // the first attention after a reset: 16 bytes at 50h, keep the LINK
        Fetch,      // 16 bytes of the IOPB at the LINK
        In,         // bytes from memory (DATA up) into buf
        Out,        // buf to memory (DATA up)
        WriteBack,  // the IOPB bytes the command changed, STATUS last
    };

    // What to do when an In or Out of buf is complete.
    enum class Then : uint8_t { None, Specify, SetMap, Format, ReadNext, WriteSector };

    struct Job {
        Phase    phase = Phase::Idle;
        Then     then  = Then::None;
        uint32_t iopbAt = 0;            // where this IOPB is
        std::array<uint8_t, 16> iopb{};  // as fetched, then as the command leaves it
        std::array<uint8_t, 16> orig{};  // as fetched: the write-back sends only changes
        uint32_t addr = 0;              // the DMA counter
        size_t   idx  = 0;              // the byte within the phase
        std::vector<uint8_t> buf;
        std::vector<int>     back;      // IOPB offsets to write back, STATUS last
        // R/W in progress
        uint32_t remaining = 0;         // sectors still to move
        int      sector = 0, track = 0; // the logical sector and track being moved
        uint32_t sectorAt = 0;          // DATA for this sector
    };

    void attention();
    StepResult stepOne(Bus& bus);
    void execute();
    void finish(uint8_t status);
    void startRw();
    void nextRw();
    bool locate(Drive& d, int track, int sector, int& cyl, int& head) const;
    void applySpecify();
    void applyFormat();
    Drive* driveFor(uint8_t& status, bool needDisk);
    uint16_t word(int off) const;
    void putWord(int off, uint16_t v);
    uint32_t data() const;
    void putData(uint32_t a);
    Property shapeKey(const char* name) const;

    struct Mover : public BusMaster {
        CompuProDisk3Board* d = nullptr;
        StepResult step(Bus& bus) override { return d->stepOne(bus); }
    } mover_;

    std::array<Drive, kDrives> drive_;

    // ---- straps ----
    uint8_t   port_     = 0x90;  // S1-1..7 = A7..A1
    int       priority_ = 9;     // J11 P0-P3
    int       resetBit_ = 0;     // J10
    IrqJumper irq_      = IrqJumper::None;

    // ---- runtime (snapshot) ----
    bool     held_      = false;  // the 8085 held in reset
    bool     firstAttn_ = true;   // the next attention only fetches the LINK at 50h
    bool     intOut_    = false;  // the completion interrupt
    uint32_t link_      = 0x50;
    uint8_t  mode_      = 0xFF;   // GLOBAL mode: FF = absolute sectors, 0 = sector + track
    uint8_t  retries_   = 0, drives_ = 0;
    bool     warnedVersion_ = false;
    Job      job_;

    std::vector<std::string> log_;
};

} // namespace altair
