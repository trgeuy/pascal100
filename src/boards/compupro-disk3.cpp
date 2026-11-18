#include "boards/compupro-disk3.h"

#include "core/bus.h"
#include "core/statefile.h"
#include "core/value.h"
#include "host/media.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>

namespace altair {

namespace {

// A DMA byte is one memory cycle the board drives: three bus states, no waits.
constexpr uint32_t kDmaCycle = 3;

constexpr uint32_t kDefIopb = 0x50;  // the default IOPB (p. 11), CompuPro's DEFIOPB

// IOPB offsets (p. 11-13).
constexpr int kCmd = 0, kStat = 1, kDrv = 2, kArg1 = 3, kData = 10, kLink = 13;

constexpr uint8_t kBitInt  = 0x80;  // COMMAND bit 7: interrupt on completion
constexpr uint8_t kBitCont = 0x40;  // COMMAND bit 6: continue with the next IOPB

// STATUS codes (p. 12).
constexpr uint8_t kStRange = 1, kStNotReady = 2, kStTimeOut = 3, kStWriteFault = 5, kStDone = 0xFF;

// Operation codes (p. 14-17).
enum Op : uint8_t {
    kNoop = 0x00, kVersion, kGlobal, kSpecify, kSetMap, kHome, kSeek, kReadHeader, kRw,
    kRelocate, kFormat, kFormatBad, kStatus, kSelect, kExamine, kModify,
};

constexpr int kSpecifyLen = 22;   // p. 18
constexpr int kMapLen     = 256;  // p. 15
constexpr uint8_t kVersionNumber = 0x0D;  // not printed in the manual: chosen (header)
constexpr uint8_t kBadFill = 0x4E;        // FORMAT-BAD fills the track with 4Es (p. 16)
constexpr uint8_t kNewDiskFill = 0xE5;

int driveIndex(const std::string& unit) {
    if (unit.size() != 6 || unit.rfind("drive", 0) != 0) return -1;
    const char ch = unit[5];
    return (ch >= '0' && ch < '0' + CompuProDisk3Board::kDrives) ? ch - '0' : -1;
}

std::string hex2(unsigned v) {
    char b[8];
    std::snprintf(b, sizeof b, "%02X", v & 0xFF);
    return b;
}

} // namespace

CompuProDisk3Board::CompuProDisk3Board() { mover_.d = this; }

// ---------------------------------------------------------------------------
// The bus: one write-only port, decoded as a pair (p. 10). S1 compares A7-A1.
// ---------------------------------------------------------------------------
bool CompuProDisk3Board::decodes(const BusCycle& c) const {
    if (!enabled_ || c.type != Cycle::IoWrite) return false;
    return (c.port() & 0xFE) == port_;
}

uint8_t CompuProDisk3Board::read(const BusCycle&) { return 0xFF; }

void CompuProDisk3Board::write(const BusCycle& c) {
    if (c.type != Cycle::IoWrite) return;
    const uint8_t v = c.data;
    if (v & (1u << resetBit_)) {
        // RESET: the 8085 stops where it is. A command in flight is lost, the interrupt
        // drops, and the next attention is the first one again.
        if (job_.phase != Phase::Idle) {
            job_.phase = Phase::Idle;
            holdChanged();
        }
        held_      = true;
        firstAttn_ = true;
        if (intOut_) {
            intOut_ = false;
            intChanged();
        }
        return;
    }
    if (v != 0) return;  // another board's reset bit (J10): not ours
    held_ = false;
    attention();
}

bool CompuProDisk3Board::assertsInt() const {
    return irq_ == IrqJumper::Int && intOut_;
}

uint8_t CompuProDisk3Board::assertsVi() const {
    return intOut_ ? viBit(irq_) : 0;
}

// ---------------------------------------------------------------------------
// ATTENTION (p. 11, 14). It clears the interrupt and starts a fetch: the LINK at 50h if it
// is the first one after a reset, else the IOPB the last LINK named.
// ---------------------------------------------------------------------------
void CompuProDisk3Board::attention() {
    if (intOut_) {
        intOut_ = false;
        intChanged();
    }
    if (job_.phase != Phase::Idle) {
        log_.push_back("an attention while a command runs is lost (the host did not wait for "
                       "STATUS)");
        return;
    }
    job_        = Job{};
    job_.idx    = 0;
    if (firstAttn_) {
        job_.phase  = Phase::FetchLink;
        job_.iopbAt = kDefIopb;
    } else {
        job_.phase  = Phase::Fetch;
        job_.iopbAt = link_;
    }
    holdChanged();
}

// One granted bus step: one byte of whatever the board is doing.
StepResult CompuProDisk3Board::stepOne(Bus& bus) {
    Job& j = job_;
    switch (j.phase) {
    case Phase::Idle:
        return {0, RunStatus::Ok};

    case Phase::FetchLink:
    case Phase::Fetch: {
        j.iopb[j.idx] = bus.memReadX((j.iopbAt + (uint32_t)j.idx) & 0xFFFFFFu);
        if (++j.idx < 16) break;
        link_ = (uint32_t)j.iopb[kLink] | (uint32_t)j.iopb[kLink + 1] << 8 |
                (uint32_t)j.iopb[kLink + 2] << 16;
        if (j.phase == Phase::FetchLink) {
            // Only the LINK is kept; nothing runs, no status (p. 11).
            firstAttn_ = false;
            j.phase    = Phase::Idle;
            holdChanged();
            break;
        }
        j.orig = j.iopb;
        execute();
        break;
    }

    case Phase::In:
        j.buf[j.idx] = bus.memReadX(j.addr);
        j.addr = (j.addr + 1) & 0xFFFFFFu;
        if (++j.idx < j.buf.size()) break;
        switch (j.then) {
        case Then::Specify:     applySpecify(); break;
        case Then::Format:      applyFormat(); break;
        case Then::WriteSector: nextRw(); break;
        case Then::SetMap: {
            Drive& d = drive_[(size_t)(j.iopb[kDrv] & 3)];
            for (size_t i = 0; i < d.map.size(); ++i)
                d.map[i] = (uint16_t)(j.buf[2 * i] | j.buf[2 * i + 1] << 8);
            finish(kStDone);
            break;
        }
        default: finish(kStDone); break;
        }
        break;

    case Phase::Out:
        bus.memWriteX(j.addr, j.buf[j.idx]);
        j.addr = (j.addr + 1) & 0xFFFFFFu;
        if (++j.idx < j.buf.size()) break;
        if (j.then == Then::ReadNext) nextRw();
        else finish(kStDone);
        break;

    case Phase::WriteBack: {
        const int off = j.back[j.idx];
        bus.memWriteX((j.iopbAt + (uint32_t)off) & 0xFFFFFFu, j.iopb[(size_t)off]);
        if (++j.idx < j.back.size()) break;
        const uint8_t cmd = j.iopb[kCmd];
        if (cmd & kBitCont) {
            // Chained (p. 12): the next IOPB without an attention, no interrupt.
            const uint32_t next = link_;
            j          = Job{};
            j.phase    = Phase::Fetch;
            j.iopbAt   = next;
            break;
        }
        j.phase = Phase::Idle;
        holdChanged();
        if (cmd & kBitInt) {
            intOut_ = true;
            intChanged();
        }
        break;
    }
    }
    return {kDmaCycle, RunStatus::Ok};
}

// ---------------------------------------------------------------------------
// IOPB helpers. Two-byte values are low byte first (p. 13); addresses are three bytes.
// ---------------------------------------------------------------------------
uint16_t CompuProDisk3Board::word(int off) const {
    return (uint16_t)(job_.iopb[(size_t)off] | job_.iopb[(size_t)off + 1] << 8);
}

void CompuProDisk3Board::putWord(int off, uint16_t v) {
    job_.iopb[(size_t)off]     = (uint8_t)v;
    job_.iopb[(size_t)off + 1] = (uint8_t)(v >> 8);
}

uint32_t CompuProDisk3Board::data() const {
    return (uint32_t)job_.iopb[kData] | (uint32_t)job_.iopb[kData + 1] << 8 |
           (uint32_t)job_.iopb[kData + 2] << 16;
}

void CompuProDisk3Board::putData(uint32_t a) {
    job_.iopb[kData]     = (uint8_t)a;
    job_.iopb[kData + 1] = (uint8_t)(a >> 8);
    job_.iopb[kData + 2] = (uint8_t)(a >> 16);
}

// The drive the DRIVE byte names. Out of range is an argument range error (p. 13); with
// needDisk, no image is NOT READY.
CompuProDisk3Board::Drive* CompuProDisk3Board::driveFor(uint8_t& status, bool needDisk) {
    const uint8_t n = job_.iopb[kDrv];
    if (n >= kDrives) {
        status = kStRange;
        return nullptr;
    }
    Drive& d = drive_[n];
    if (needDisk && !d.img) {
        status = kStNotReady;
        return nullptr;
    }
    return &d;
}

// ---------------------------------------------------------------------------
// The command.
// ---------------------------------------------------------------------------
void CompuProDisk3Board::execute() {
    Job&          j  = job_;
    const uint8_t op = j.iopb[kCmd] & 0x3F;
    uint8_t       st = kStDone;
    switch (op) {
    case kNoop:
        finish(kStDone);
        return;

    case kVersion:
        j.iopb[kArg1] = kVersionNumber;
        if (!warnedVersion_) {
            log_.push_back("VERSION: the firmware number is not in the manual; answering " +
                           hex2(kVersionNumber));
            warnedVersion_ = true;
        }
        finish(kStDone);
        return;

    case kGlobal:
        mode_    = j.iopb[kArg1];
        retries_ = j.iopb[kArg1 + 1];
        drives_  = j.iopb[kArg1 + 2];
        finish(kStDone);
        return;

    case kSpecify:
    case kSetMap: {
        if (!driveFor(st, false)) break;
        j.buf.assign(op == kSpecify ? kSpecifyLen : kMapLen, 0);
        j.addr  = data();
        j.idx   = 0;
        j.then  = op == kSpecify ? Then::Specify : Then::SetMap;
        j.phase = Phase::In;
        return;
    }

    case kHome: {
        Drive* d = driveFor(st, true);
        if (!d) break;
        d->pos = 0;
        finish(kStDone);
        return;
    }

    case kSeek: {
        Drive* d = driveFor(st, true);
        if (!d) break;
        const int cyl = word(kArg1);
        if (cyl >= d->cylinders) {
            // Not in the manual. The drive has no such cylinder: the header is never found.
            st = kStTimeOut;
            log_.push_back("drive " + std::to_string(j.iopb[kDrv]) + ": SEEK to cylinder " +
                           std::to_string(cyl) + ", past the last (" +
                           std::to_string(d->cylinders - 1) + ")");
            break;
        }
        d->pos = cyl;
        finish(kStDone);
        return;
    }

    case kReadHeader: {
        Drive* d = driveFor(st, true);
        if (!d) break;
        // The manual names four "physical address bytes" and gives FORMAT's header as
        // (cylinder, cylinder, head, sector). Which cylinder byte is first is not printed:
        // high first is chosen here. Head 0, sector 0 (the board reads "any header").
        j.buf   = {(uint8_t)(d->pos >> 8), (uint8_t)d->pos, 0, 0};
        j.addr  = data();
        j.idx   = 0;
        j.then  = Then::None;
        j.phase = Phase::Out;
        return;
    }

    case kRw:
        startRw();
        return;

    case kFormat: {
        Drive* d = driveFor(st, true);
        if (!d) break;
        if (d->img->readOnly()) {
            st = kStWriteFault;
            log_.push_back("drive " + std::to_string(j.iopb[kDrv]) + " is write-protected");
            break;
        }
        // The header image first: four bytes per sector (p. 16).
        j.buf.assign((size_t)d->spec.sectors * 4, 0);
        j.addr  = data();
        j.idx   = 0;
        j.then  = Then::Format;
        j.phase = Phase::In;
        return;
    }

    case kFormatBad: {
        Drive* d = driveFor(st, true);
        if (!d) break;
        int track = word(kArg1 + 3);  // mode 0: ARG4-5
        if (mode_ != 0) {
            const uint32_t abs = (uint32_t)word(kArg1 + 1) | (uint32_t)word(kArg1 + 3) << 16;
            track = (int)(abs / (uint32_t)std::max(1, d->spec.sectors));
        }
        int cyl = 0, head = 0;
        if (!locate(*d, track, 0, cyl, head)) {
            st = kStTimeOut;
            break;
        }
        std::vector<uint8_t> fill((size_t)d->sectorSize, kBadFill);
        for (int s = 0; s < d->sectors; ++s) {
            size_t n = fill.size();
            d->img->writeSector(cyl, head, s, fill.data(), &n);
        }
        d->img->sync();
        // A real BAD track has no headers left, so a later read times out; the image cannot
        // hold that, so the 4E bytes read back. Said once per track.
        log_.push_back("drive " + std::to_string(j.iopb[kDrv]) + ": FORMAT-BAD logical track " +
                       std::to_string(track) + " (cylinder " + std::to_string(cyl) + " head " +
                       std::to_string(head) + ") filled with 4E; it still reads back");
        finish(kStDone);
        return;
    }

    case kRelocate:
    case kStatus:
    case kSelect:
    case kExamine:
    case kModify:
        // These reach the drive's status lines, the drive control ports, the relocation of
        // a failing sector or the 8085's own RAM: the manual does not describe enough of
        // them to answer truthfully. Refused, and said.
        log_.push_back("command " + hex2(op) + " is not modelled; answered with STATUS 01");
        st = kStRange;
        break;

    default:
        st = kStRange;  // "An invalid operation code ends with an argument range error"
        break;
    }
    finish(st);
}

// SPECIFY: 22 bytes, every entry two bytes, low first (p. 18-19).
void CompuProDisk3Board::applySpecify() {
    Drive&      d = drive_[(size_t)(job_.iopb[kDrv] & 3)];
    const auto& b = job_.buf;
    auto        w = [&b](int off) { return (int)(b[(size_t)off] | b[(size_t)off + 1] << 8); };
    Spec s;
    s.sectorSize = w(4);
    s.sectors    = w(6);
    s.heads      = w(8);
    s.cylinders  = w(10);
    s.reserved   = w(18);
    if (s.sectorSize <= 0 || s.sectors <= 0 || s.heads <= 0 || s.cylinders <= 0) {
        finish(kStRange);
        return;
    }
    d.spec = s;
    if (d.img && (s.sectorSize != d.sectorSize || s.sectors != d.sectors || s.heads != d.heads ||
                  s.cylinders != d.cylinders))
        log_.push_back("drive " + std::to_string(job_.iopb[kDrv]) + ": SPECIFY gives " +
                       std::to_string(s.cylinders) + " cylinders x " + std::to_string(s.heads) +
                       " heads x " + std::to_string(s.sectors) + " x " +
                       std::to_string(s.sectorSize) + ", the image is " +
                       std::to_string(d.cylinders) + " x " + std::to_string(d.heads) + " x " +
                       std::to_string(d.sectors) + " x " + std::to_string(d.sectorSize) +
                       ": sectors outside the image will time out");
    finish(kStDone);
}

// FORMAT: one track at the cylinder under the heads, head ARG3, every sector filled with
// ARG2 (p. 16). The header image is read to check the sector numbers; a skew is any order.
void CompuProDisk3Board::applyFormat() {
    Job&          j    = job_;
    Drive&        d    = drive_[(size_t)(j.iopb[kDrv] & 3)];
    const uint8_t fill = j.iopb[kArg1 + 1];
    const int     head = j.iopb[kArg1 + 2];
    const int     dn   = j.iopb[kDrv];
    if (d.spec.sectors != d.sectors || d.spec.sectorSize != d.sectorSize || head >= d.heads) {
        log_.push_back("drive " + std::to_string(dn) + ": FORMAT of head " + std::to_string(head) +
                       " with " + std::to_string(d.spec.sectors) + " sectors of " +
                       std::to_string(d.spec.sectorSize) + " does not fit the image (" +
                       std::to_string(d.heads) + " heads, " + std::to_string(d.sectors) + " x " +
                       std::to_string(d.sectorSize) + ")");
        finish(kStWriteFault);
        return;
    }
    std::vector<bool> seen((size_t)d.sectors, false);
    bool              ok = true;
    for (int i = 0; i < d.sectors; ++i) {
        const int s = j.buf[(size_t)i * 4 + 3];
        if (s >= d.sectors || seen[(size_t)s]) ok = false;
        else seen[(size_t)s] = true;
    }
    if (!ok)
        log_.push_back("drive " + std::to_string(dn) + ": FORMAT header image does not number "
                       "the sectors 0-" + std::to_string(d.sectors - 1) +
                       "; the image keeps the standard order");
    std::vector<uint8_t> buf((size_t)d.sectorSize, fill);
    for (int s = 0; s < d.sectors; ++s) {
        size_t n = buf.size();
        if (!d.img->writeSector(d.pos, head, s, buf.data(), &n)) {
            log_.push_back("drive " + std::to_string(dn) + ": the host write failed");
            finish(kStWriteFault);
            return;
        }
    }
    d.img->sync();
    finish(kStDone);
}

// A logical track and sector to a physical cylinder and head (Appendix, p. 19-20): past the
// label track and the RES-TRACKS reserved tracks, unless the relocation map sends the track to
// a reserved one (entry i = physical track 1 + i). False: the drive has no such place.
//
// THE +1 IS INFERRED. p. 19 says RES-TRACKS counts the label track too, but the Appendix's
// Q540 has 21 reserved tracks (0/1 .. 2/5) and its first data track at 2/6 = physical 22;
// CompuPro's DISK3.CMD writes RES-TRACKS = 21 and 21 map entries for that drive, and the
// V1.1PD BIOS says "22 tracks reserved by controller". So the data starts at RES-TRACKS + 1.
// With RES-TRACKS = 0 (a drive being formatted, p. 17) it starts at 0: the formatter writes
// absolute sectors 0 and 1 there.
bool CompuProDisk3Board::locate(Drive& d, int track, int sector, int& cyl, int& head) const {
    if (sector < 0 || sector >= d.spec.sectors || d.spec.sectors != d.sectors ||
        d.spec.sectorSize != d.sectorSize || d.spec.heads != d.heads)
        return false;
    int phys = track + (d.spec.reserved ? d.spec.reserved + 1 : 0);
    if (track != 0) {
        for (size_t i = 0; i < d.map.size(); ++i) {
            if (d.map[i] == 0xFFFF) break;
            if (d.map[i] == track) {
                phys = 1 + (int)i;
                break;
            }
        }
    }
    cyl  = phys / d.heads;
    head = phys % d.heads;
    return cyl < d.cylinders;
}

// ---------------------------------------------------------------------------
// R/W (p. 15): 1 to 64K sectors. ARG1 = 1 read, 0 write; ARG2-5 = the 32-bit absolute
// sector, or (GLOBAL mode 0) the sector then the logical track; ARG6-7 = the count.
// ---------------------------------------------------------------------------
void CompuProDisk3Board::startRw() {
    Job&    j  = job_;
    uint8_t st = kStDone;
    Drive*  d  = driveFor(st, true);
    if (!d) {
        finish(st);
        return;
    }
    const bool read = j.iopb[kArg1] != 0;
    const uint32_t count = word(kArg1 + 5);
    if (count == 0 || d->spec.sectors <= 0) {
        finish(kStRange);
        return;
    }
    if (!read && d->img->readOnly()) {
        log_.push_back("drive " + std::to_string(j.iopb[kDrv]) + " is write-protected");
        finish(kStWriteFault);
        return;
    }
    if (mode_ == 0) {
        j.sector = word(kArg1 + 1);
        j.track  = word(kArg1 + 3);
    } else {
        const uint32_t abs = (uint32_t)word(kArg1 + 1) | (uint32_t)word(kArg1 + 3) << 16;
        j.sector = (int)(abs % (uint32_t)d->spec.sectors);
        j.track  = (int)(abs / (uint32_t)d->spec.sectors);
    }
    j.remaining = count;
    j.sectorAt  = data();
    j.then      = Then::None;
    nextRw();
}

// Called to start the first sector, and again after each sector's bytes have moved.
void CompuProDisk3Board::nextRw() {
    Job&       j    = job_;
    Drive&     d    = drive_[(size_t)(j.iopb[kDrv] & 3)];
    const bool read = j.iopb[kArg1] != 0;
    const int  dn   = j.iopb[kDrv];

    // Leave the IOPB pointing at a sector: the one about to move, or (at the end) the last
    // one moved. Mode FF keeps the absolute number.
    auto mark = [&](int track, int sector, uint32_t at, uint32_t left) {
        if (mode_ == 0) {
            putWord(kArg1 + 1, (uint16_t)sector);
            putWord(kArg1 + 3, (uint16_t)track);
        } else {
            const uint32_t abs = (uint32_t)track * (uint32_t)d.spec.sectors + (uint32_t)sector;
            putWord(kArg1 + 1, (uint16_t)abs);
            putWord(kArg1 + 3, (uint16_t)(abs >> 16));
        }
        putWord(kArg1 + 5, (uint16_t)left);
        putData(at);
    };

    if (j.then == Then::ReadNext || j.then == Then::WriteSector) {
        // A sector has moved.
        if (j.then == Then::WriteSector) {
            int cyl = 0, head = 0;
            locate(d, j.track, j.sector, cyl, head);
            size_t n = j.buf.size();
            if (!d.img->writeSector(cyl, head, j.sector, j.buf.data(), &n)) {
                log_.push_back("drive " + std::to_string(dn) + ": the host write failed");
                mark(j.track, j.sector, j.sectorAt, j.remaining);
                finish(kStWriteFault);
                return;
            }
            // Every sector reaches the host file when it is written (host/cardimg.cpp).
            d.img->sync();
        }
        if (--j.remaining == 0) {
            mark(j.track, j.sector, j.sectorAt, 0);
            finish(kStDone);
            return;
        }
        j.sectorAt = (j.sectorAt + (uint32_t)d.spec.sectorSize) & 0xFFFFFFu;
        if (++j.sector == d.spec.sectors) {
            j.sector = 0;
            ++j.track;
        }
    }

    int cyl = 0, head = 0;
    if (!locate(d, j.track, j.sector, cyl, head)) {
        log_.push_back("drive " + std::to_string(dn) + ": no header for logical track " +
                       std::to_string(j.track) + " sector " + std::to_string(j.sector) +
                       ": TIME OUT");
        mark(j.track, j.sector, j.sectorAt, j.remaining);
        finish(kStTimeOut);
        return;
    }
    d.pos  = cyl;
    j.addr = j.sectorAt;
    j.idx  = 0;
    j.buf.assign((size_t)d.sectorSize, 0);
    if (read) {
        size_t n = j.buf.size();
        if (!d.img->readSector(cyl, head, j.sector, j.buf.data(), &n)) {
            log_.push_back("drive " + std::to_string(dn) + ": the host read failed");
            mark(j.track, j.sector, j.sectorAt, j.remaining);
            finish(kStTimeOut);
            return;
        }
        j.then  = Then::ReadNext;
        j.phase = Phase::Out;
    } else {
        j.then  = Then::WriteSector;
        j.phase = Phase::In;
    }
}

// The command is over: write back the IOPB bytes it changed, then STATUS (the host polls it,
// so it goes last).
void CompuProDisk3Board::finish(uint8_t status) {
    Job& j = job_;
    j.iopb[kStat] = status;
    j.back.clear();
    for (int i = 0; i < 16; ++i)
        if (i != kStat && j.iopb[(size_t)i] != j.orig[(size_t)i]) j.back.push_back(i);
    j.back.push_back(kStat);
    j.idx   = 0;
    j.phase = Phase::WriteBack;
}

// ---------------------------------------------------------------------------
// RESET* on the bus resets the 8085 too (the attention latch is cleared). The drives keep
// their heads where they are; the firmware's tables go back to its defaults.
// ---------------------------------------------------------------------------
void CompuProDisk3Board::reset(Reset r) {
    if (job_.phase != Phase::Idle) {
        job_.phase = Phase::Idle;
        holdChanged();
    }
    job_       = Job{};
    held_      = false;
    firstAttn_ = true;
    link_      = kDefIopb;
    mode_      = 0xFF;  // "absolute sector mode after a reset" (p. 18)
    retries_ = drives_ = 0;
    for (Drive& d : drive_) {
        d.spec = Spec{d.sectorSize, d.sectors, d.heads, d.cylinders, 0};
        d.map.fill(0);
        if (r == Reset::PowerOn) d.pos = 0;
    }
    intOut_ = false;
    intChanged();
}

void CompuProDisk3Board::power() { reset(Reset::PowerOn); }

// ---------------------------------------------------------------------------
// Properties: the switches and jumpers.
// ---------------------------------------------------------------------------
std::vector<Property> CompuProDisk3Board::properties() {
    std::vector<Property> p;
    {
        Property x;
        x.name  = "port";
        x.help  = "Attention port (S1-1..7 = A7..A1): a write-only pair, BASE and BASE+1. "
                  "Default 90, CompuPro's";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.min   = 0;
        x.max   = 0xFE;
        x.get   = [this] { return Value::ofInt(port_); };
        x.set   = [this](const Value& v, std::string& err) {
            if (v.i() & 1) {
                err = "the base is even: the switches set A7-A1 only";
                return false;
            }
            port_ = (uint8_t)v.i();
            decodeChanged();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name  = "dma_priority";
        x.help  = "DMA priority (J11 P0-P3): the TMA bus-arbitration priority, 0-15; the higher "
                  "one gets the bus first. Default 9, the manual's. Two bus masters must not "
                  "share a priority";
        x.kind  = Kind::Int;
        x.radix = 10;
        x.min   = 0;
        x.max   = 15;
        x.get   = [this] { return Value::ofInt(priority_); };
        x.set   = [this](const Value& v, std::string&) {
            priority_ = (int)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name  = "reset_bit";
        x.help  = "The data bit that resets this board (J10), 0-7. Default 0: CompuPro's BIOS "
                  "writes 01 to reset the first board, the manual FF for all";
        x.kind  = Kind::Int;
        x.radix = 10;
        x.min   = 0;
        x.max   = 7;
        x.get   = [this] { return Value::ofInt(resetBit_); };
        x.set   = [this](const Value& v, std::string&) {
            resetBit_ = (int)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    p.push_back(irqJumperProperty(
        "interrupt",
        "Where the completion interrupt goes (J9): vi0-vi7, int, or none (default). The "
        "manual says vi1 in a CompuPro multi-user system",
        irq_));
    return p;
}

std::vector<MapEntry> CompuProDisk3Board::ioMap() const {
    return {{(uint32_t)port_, (uint32_t)port_ + 1, "write",
             "attention (00) / reset (the J10 bit); all other traffic is DMA"}};
}

// ---------------------------------------------------------------------------
// Units, MOUNT, UNMOUNT, and [[board.drive]].
// ---------------------------------------------------------------------------
std::vector<UnitDef> CompuProDisk3Board::units() const {
    std::vector<UnitDef> u;
    for (int i = 0; i < kDrives; ++i) {
        const Drive& d = drive_[(size_t)i];
        UnitDef x;
        x.name  = "drive" + std::to_string(i);
        x.kind  = UnitKind::Disk;
        x.state = d.img ? d.path : "(empty)";
        if (d.img) {
            x.readOnly       = d.img->readOnly();
            x.readOnlyForced = d.img->readOnlyForced();
        }
        u.push_back(std::move(x));
    }
    return u;
}

// The image is the drive's physical sectors, cylinder by cylinder, head by head, sectors
// from 0 (DREM's raw .DSK). An empty file is a new drive, filled with E5.
bool CompuProDisk3Board::mount(const std::string& unit, const std::string& path, bool ro,
                               std::string& err) {
    const int i = driveIndex(unit);
    if (i < 0) {
        err = "no unit `" + unit + "` on " + id + " (it has drive0..drive3)";
        return false;
    }
    Drive& d     = drive_[(size_t)i];
    auto   media = openMedia(resolvePath(path), ro, err);
    if (!media) {
        err += pathNote(path);
        return false;
    }
    auto img = std::make_unique<DiskImage>(std::move(media));
    img->init(d.cylinders, d.heads, true);
    img->initFormat(0, d.cylinders - 1, 0, d.heads - 1, Density::DD, d.sectors, d.sectorSize, 0);
    const uint64_t want = (uint64_t)d.cylinders * (uint64_t)d.heads * (uint64_t)d.sectors *
                          (uint64_t)d.sectorSize;
    const uint64_t have = img->size();
    if (have == 0 && !img->readOnly()) {
        img->setExtendsOnWrite(true);
        std::vector<uint8_t> fill((size_t)d.sectorSize, kNewDiskFill);
        for (int c = 0; c < d.cylinders; ++c)
            for (int h = 0; h < d.heads; ++h)
                for (int s = 0; s < d.sectors; ++s) {
                    size_t n = fill.size();
                    if (!img->writeSector(c, h, s, fill.data(), &n)) {
                        err = "cannot write the new disk " + path;
                        return false;
                    }
                }
        img->setExtendsOnWrite(false);
        img->sync();
        log_.push_back(unit + ": new disk, " + std::to_string(want) + " bytes of E5");
    } else if (!sizeMatches(have, want)) {
        err = std::to_string(have) + " bytes is not this drive: " + std::to_string(d.cylinders) +
              " cylinders x " + std::to_string(d.heads) + " heads x " +
              std::to_string(d.sectors) + " sectors x " + std::to_string(d.sectorSize) +
              " bytes = " + std::to_string(want) +
              ". Set cylinders, heads, sectors and sector_size to the image's shape. An empty "
              "file becomes a new disk";
        return false;
    }
    d.img  = std::move(img);
    d.path = path;
    d.pos  = std::min(d.pos, d.cylinders - 1);
    if (d.img->readOnlyForced())
        log_.push_back(unit + " mounted WRITE-PROTECTED -- the host will not let us write " + path);
    return true;
}

bool CompuProDisk3Board::unmount(const std::string& unit, std::string& err) {
    const int i = driveIndex(unit);
    if (i < 0) {
        err = "no unit `" + unit + "` on " + id;
        return false;
    }
    Drive& d = drive_[(size_t)i];
    if (!d.img) {
        err = id + ":" + unit + " is empty";
        return false;
    }
    d.img->sync();
    d.img.reset();
    d.path.clear();
    return true;
}

Property CompuProDisk3Board::shapeKey(const char* name) const {
    Property x;
    x.name  = name;
    x.kind  = Kind::Int;
    x.radix = 10;
    x.min   = 1;
    const std::string n = name;
    if (n == "cylinders") {
        x.help = "Cylinders on the drive. Default 512 (Quantum Q540)";
        x.max  = 4096;
    } else if (n == "heads") {
        x.help = "Heads on the drive. Default 8 (Quantum Q540)";
        x.max  = 16;
    } else if (n == "sectors") {
        x.help = "Sectors per track as formatted. Default 9 (CompuPro: 9 x 1024 on ST506)";
        x.max  = 64;
    } else {
        x.help = "Bytes per sector as formatted. Default 1024";
        x.min  = 128;
        x.max  = 2048;
    }
    return x;
}

std::vector<Property> CompuProDisk3Board::mountKeys(const std::string& unit) const {
    if (driveIndex(unit) < 0) return {};
    return {shapeKey("cylinders"), shapeKey("heads"), shapeKey("sectors"),
            shapeKey("sector_size")};
}

// The shape goes with the disk: a mount that is refused leaves the old one.
bool CompuProDisk3Board::mountWith(const std::string& unit, const std::string& path, bool ro,
                                   const KeyValues& keys, std::string& err) {
    const int i = driveIndex(unit);
    if (i < 0) return mount(unit, path, ro, err);
    Drive&      d   = drive_[(size_t)i];
    const int   oc = d.cylinders, oh = d.heads, os = d.sectors, oz = d.sectorSize;
    for (const auto& [k, v] : keys) {
        if (v.empty()) continue;
        if (k == "cylinders") d.cylinders = std::stoi(v);
        else if (k == "heads") d.heads = std::stoi(v);
        else if (k == "sectors") d.sectors = std::stoi(v);
        else if (k == "sector_size") d.sectorSize = std::stoi(v);
    }
    if (mount(unit, path, ro, err)) {
        if (!busy()) d.spec = Spec{d.sectorSize, d.sectors, d.heads, d.cylinders, 0};
        return true;
    }
    d.cylinders  = oc;
    d.heads      = oh;
    d.sectors    = os;
    d.sectorSize = oz;
    return false;
}

std::vector<Property> CompuProDisk3Board::subUnitProperties(const std::string& table) const {
    if (table != "drive") return {};
    std::vector<Property> p;
    {
        Property x;
        x.name  = "unit";
        x.help  = "Which drive (0..3): the J1-J4 cable, the IOPB DRIVE byte";
        x.kind  = Kind::Int;
        x.radix = 10;
        x.min   = 0;
        x.max   = kDrives - 1;
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "mount";
        x.help = "The disk image to put in it. Relative to THIS FILE.";
        x.kind = Kind::Str;
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name    = "readonly";
        x.help    = "Write-protect the disk. A write gives STATUS 05 (write fault)";
        x.kind    = Kind::Bool;
        x.aliases = {"writeprotect"};
        p.push_back(std::move(x));
    }
    for (const char* k : {"cylinders", "heads", "sectors", "sector_size"}) p.push_back(shapeKey(k));
    {
        Property x;
        x.name = "create";
        x.help = "Make the disk file if it is not there: a new disk, filled with E5";
        x.kind = Kind::Bool;
        p.push_back(std::move(x));
    }
    return p;
}

bool CompuProDisk3Board::addSubUnit(const std::string& table, const KeyValues& kv,
                                    std::string& err) {
    if (table != "drive") {
        err = type() + " has no [[board." + table + "]] table";
        return false;
    }
    int         unit = -1;
    std::string path;
    KeyValues   shape;
    bool        ro = false, create = false;
    auto flag = [](const std::string& v) {
        Value       bv;
        std::string e;
        return parseValue(v, Kind::Bool, bv, e) && bv.b();
    };
    for (const auto& [k, v] : kv) {
        if (k == "unit") unit = std::stoi(v);
        else if (k == "mount") path = v;
        else if (k == "readonly") ro = flag(v);
        else if (k == "create") create = flag(v);
        else if (k == "cylinders" || k == "heads" || k == "sectors" || k == "sector_size")
            shape.push_back({k, v});
    }
    if (unit < 0) {
        err = "[[board.drive]] needs a `unit`";
        return false;
    }
    Drive& d = drive_[(size_t)unit];
    if (path.empty()) {
        for (const auto& [k, v] : shape) {
            if (k == "cylinders") d.cylinders = std::stoi(v);
            else if (k == "heads") d.heads = std::stoi(v);
            else if (k == "sectors") d.sectors = std::stoi(v);
            else if (k == "sector_size") d.sectorSize = std::stoi(v);
        }
        d.spec = Spec{d.sectorSize, d.sectors, d.heads, d.cylinders, 0};
        return true;
    }
    // `create`: an empty file at the place mount() opens, never over a file that is there.
    if (create) {
        const std::string rp = resolvePath(path);
        std::error_code   ec;
        if (!std::filesystem::exists(rp, ec)) {
            std::string cerr;
            if (!writeHostFile(rp, {}, cerr)) {
                err = cerr + pathNote(path);
                return false;
            }
        }
    }
    return mountWith("drive" + std::to_string(unit), path, ro, shape, err);
}

std::vector<Board::SubUnit> CompuProDisk3Board::subUnits() const {
    std::vector<SubUnit> out;
    for (int i = 0; i < kDrives; ++i) {
        const Drive& d      = drive_[(size_t)i];
        const bool   custom = d.cylinders != 512 || d.heads != 8 || d.sectors != 9 ||
                            d.sectorSize != 1024;
        if (!d.img && !custom) continue;
        SubUnit su;
        su.table = "drive";
        su.fields.push_back({"unit", std::to_string(i), false});
        if (d.cylinders != 512) su.fields.push_back({"cylinders", std::to_string(d.cylinders), false});
        if (d.heads != 8) su.fields.push_back({"heads", std::to_string(d.heads), false});
        if (d.sectors != 9) su.fields.push_back({"sectors", std::to_string(d.sectors), false});
        if (d.sectorSize != 1024)
            su.fields.push_back({"sector_size", std::to_string(d.sectorSize), false});
        if (d.img) {
            su.fields.push_back({"mount", d.path, true});
            if (d.img->readOnly()) su.fields.push_back({"readonly", "true", false});
        }
        out.push_back(std::move(su));
    }
    return out;
}

std::vector<std::string> CompuProDisk3Board::drainLog() {
    std::vector<std::string> out = std::move(log_);
    log_.clear();
    for (auto& s : out) s = id + ": " + s;
    return out;
}

// ---------------------------------------------------------------------------
// SNAPSHOT / RESTORE.
// ---------------------------------------------------------------------------
void CompuProDisk3Board::serialize(StateWriter& w) const {
    Board::serialize(w);
    for (bool v : {held_, firstAttn_, intOut_}) w.boolean(v);
    w.u32(link_);
    for (uint8_t v : {mode_, retries_, drives_}) w.u8(v);
    const Job& j = job_;
    w.u8((uint8_t)j.phase);
    w.u8((uint8_t)j.then);
    w.u32(j.iopbAt);
    w.blob(std::vector<uint8_t>(j.iopb.begin(), j.iopb.end()));
    w.blob(std::vector<uint8_t>(j.orig.begin(), j.orig.end()));
    w.u32(j.addr);
    w.u32((uint32_t)j.idx);
    w.blob(j.buf);
    std::vector<uint8_t> back(j.back.begin(), j.back.end());
    w.blob(back);
    w.u32(j.remaining);
    w.u32((uint32_t)j.sector);
    w.u32((uint32_t)j.track);
    w.u32(j.sectorAt);
    for (const Drive& d : drive_) {
        w.u32((uint32_t)d.pos);
        for (int v : {d.spec.sectorSize, d.spec.sectors, d.spec.heads, d.spec.cylinders,
                      d.spec.reserved})
            w.u32((uint32_t)v);
        std::vector<uint8_t> map;
        for (uint16_t e : d.map) {
            map.push_back((uint8_t)e);
            map.push_back((uint8_t)(e >> 8));
        }
        w.blob(map);
    }
}

void CompuProDisk3Board::deserialize(StateReader& r) {
    Board::deserialize(r);
    for (bool* v : {&held_, &firstAttn_, &intOut_}) *v = r.boolean();
    link_ = r.u32() & 0xFFFFFFu;
    for (uint8_t* v : {&mode_, &retries_, &drives_}) *v = r.u8();
    Job& j = job_;
    j       = Job{};
    j.phase = (Phase)std::min<uint8_t>(r.u8(), (uint8_t)Phase::WriteBack);
    j.then  = (Then)std::min<uint8_t>(r.u8(), (uint8_t)Then::WriteSector);
    j.iopbAt = r.u32() & 0xFFFFFFu;
    auto copy16 = [&r](std::array<uint8_t, 16>& a) {
        const std::vector<uint8_t> b = r.blob();
        a.fill(0);
        std::copy_n(b.begin(), std::min<size_t>(b.size(), 16), a.begin());
    };
    copy16(j.iopb);
    copy16(j.orig);
    j.addr = r.u32() & 0xFFFFFFu;
    j.idx  = r.u32();
    j.buf  = r.blob();
    for (uint8_t b : r.blob()) j.back.push_back(b & 0x0F);
    j.remaining = r.u32();
    j.sector    = (int)r.u32();
    j.track     = (int)r.u32();
    j.sectorAt  = r.u32() & 0xFFFFFFu;
    // A phase whose buffer does not fit what it moves cannot go on.
    const size_t need = j.phase == Phase::Fetch || j.phase == Phase::FetchLink ? 16
                        : j.phase == Phase::WriteBack                          ? j.back.size()
                        : j.phase == Phase::Idle                               ? 0
                                                                               : j.buf.size();
    if (j.phase != Phase::Idle && j.idx >= need) j = Job{};
    for (Drive& d : drive_) {
        d.pos             = (int)r.u32();
        d.spec.sectorSize = (int)r.u32();
        d.spec.sectors    = (int)r.u32();
        d.spec.heads      = (int)r.u32();
        d.spec.cylinders  = (int)r.u32();
        d.spec.reserved   = (int)r.u32();
        const std::vector<uint8_t> map = r.blob();
        d.map.fill(0);
        for (size_t i = 0; i + 1 < map.size() && i / 2 < d.map.size(); i += 2)
            d.map[i / 2] = (uint16_t)(map[i] | map[i + 1] << 8);
    }
    intChanged();
    holdChanged();
}

} // namespace altair
