#pragma once
//
// A TEST-ONLY BOARD WITH ONE STREAM UNIT AND NOTHING ELSE.
//
// Some tests check what the MACHINE and the MONITOR do for a board that leaves its stream to
// the Board defaults: no connectStream() override (so an MCP console hand-off falls back to a
// bare line) and no drainLog() override (so the machine drains the unit's stream itself, through
// unitStream()). The 88-C700 was the last real board of that shape; it was removed in 2026-11.
// Every board that stays overrides both. These code paths stay in Board, so a test board keeps
// them covered.
//
// No decode, no ports, no snapshot: one unit, `prn`, connected through the real endpoint
// resolver, exactly as a board that does not hold a pre-built stream is.
//
// Not in the registry. A test adds it with Machine::adopt(), under an id it picks.

#include "core/board.h"
#include "host/endpoint.h"
#include "host/stream.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace altair {

class StubLineBoard : public Board {
public:
    using Resolver = std::function<std::unique_ptr<ByteStream>(const std::string&, std::string&)>;

    explicit StubLineBoard(std::string boardId) { id = std::move(boardId); }

    std::string type() const override { return "stubline"; }
    std::vector<Property> properties() override { return {}; }

    std::vector<UnitDef> units() const override { return {{"prn", UnitKind::Serial, spec_}}; }

    bool connect(const std::string& unit, const std::string& endpoint, std::string& err) override {
        if (unit != "prn") {
            err = "stubline has one unit, 'prn'";
            return false;
        }
        auto s = resolver_ ? resolver_(endpoint, err) : resolveEndpoint(endpoint, err);
        if (!s) return false;
        stream_ = std::move(s);
        spec_   = endpoint;
        return true;
    }

    bool disconnect(const std::string& unit, std::string& err) override {
        if (unit != "prn") {
            err = "stubline has one unit, 'prn'";
            return false;
        }
        stream_ = std::make_unique<NullStream>();
        spec_   = "null";
        return true;
    }

    ByteStream* unitStream(const std::string& unit) override {
        return unit == "prn" ? stream_.get() : nullptr;
    }

    // A test that needs to hold the far end of the line hands its own resolver in here.
    // Empty means the real one (host/endpoint.h), as a running pascal100 uses.
    void setResolver(Resolver r) { resolver_ = std::move(r); }

private:
    std::unique_ptr<ByteStream> stream_ = std::make_unique<NullStream>();
    std::string                 spec_   = "null";
    Resolver                    resolver_;
};

} // namespace altair
