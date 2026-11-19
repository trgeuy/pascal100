#pragma once
//
// AgentTap -- the MCP server's hands on a serial line that is not the console.
//
// WHY. Under --mcp the console is a ScriptedStream that the `send`, `recv` and `run` tools
// feed() and read. Every other serial unit is wired to whatever the machine file gave it:
// a terminal window, a socket, a printer file. An MP/M machine has a user on each of those
// lines, and an assistant that can reach only the console can drive only one of them.
//
// WHAT. A transparent decorator, the MirrorStream's shape with an in-memory far end:
//   read  -> bytes the assistant sent first, then the line's own input (a person typing in
//            the terminal window still types; the two merge, as two keyboards on one line).
//   write -> to the line, and a COPY of what the line accepted into a bounded buffer that
//            `recv` drains. The window still shows everything.
// Everything else -- flow control, pacing, modem pins, the title, the greeting, the log --
// is the inner line's, untouched, so the tap changes nothing the guest can measure.
//
// WHERE IT COMES FROM. resolveEndpoint() wraps each endpoint it builds in one of these when
// the process runs as an MCP server (setAgentTaps(true), set by main() BEFORE the machine
// loads). So every serial unit has a tap from the start, and `recv` on a unit returns what
// that line printed since the machine came up (the newest kKeep bytes). No stream is ever
// swapped out from under a running board.
//
// INVISIBLE TO THE OPERATOR. describe() is the inner's, so SHOW and CONFIG SAVE print the
// endpoint the operator wrote, and a saved file loads the same with or without --mcp.
//
// NO ECHO. As with the mirror, the guest is the echo authority: what the assistant sends is
// input, and the guest's echo of it comes back through write() like any other output.
#include "host/stream.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace altair {

class AgentTap : public ByteStream {
public:
    // The newest bytes kept for `recv`. A line nobody reads keeps scrolling: the oldest bytes
    // go, and dropped() counts them, so a reader knows that it missed some.
    static constexpr size_t kKeep = 64 * 1024;

    explicit AgentTap(std::unique_ptr<ByteStream> inner) : inner_(std::move(inner)) {}

    std::string doDescribe() const override { return inner_->describe(); }
    bool        realWire() const override { return inner_->realWire(); }

    size_t read(uint8_t* buf, size_t n) override {
        if (!inject_.empty()) {
            const size_t k = n < inject_.size() - injectPos_ ? n : inject_.size() - injectPos_;
            for (size_t i = 0; i < k; ++i) buf[i] = (uint8_t)inject_[injectPos_ + i];
            injectPos_ += k;
            if (injectPos_ >= inject_.size()) {
                inject_.clear();
                injectPos_ = 0;
            }
            return k;
        }
        return inner_->read(buf, n);
    }

    size_t doWrite(const uint8_t* buf, size_t n) override {
        const size_t took = inner_->write(buf, n);
        out_.append((const char*)buf, took);
        if (out_.size() > kKeep) {
            const size_t cut = out_.size() - kKeep;
            out_.erase(0, cut);
            dropped_ += cut;
        }
        return took;
    }

    // Injected bytes first, so a line with bytes waiting is never polled as empty (a
    // ScriptedStream counts empty polls -- the MCP run loop's idle test reads that).
    bool readable() const override { return !inject_.empty() || inner_->readable(); }
    bool writable() const override { return inner_->writable(); }
    bool pacesItself() const override { return inner_->pacesItself(); }
    bool pacedReceive() const override { return inner_->pacedReceive(); }
    void flush() override { inner_->flush(); }
    void pump() override { inner_->pump(); }
    LineStatus status() const override { return inner_->status(); }
    void       setControl(const LineControl& c) override { inner_->setControl(c); }
    bool       setParams(const LineParams& p, std::string& err) override {
        return inner_->setParams(p, err);
    }
    std::vector<std::string> drainLog() override { return inner_->drainLog(); }
    void greet(const std::string& owner) override { inner_->greet(owner); }
    void setTitle(const std::string& t) override { inner_->setTitle(t); }

    ByteStream* inner() { return inner_.get(); }

    // ---- the assistant's side (the MCP tools) ----
    void               feed(const std::string& s) { inject_ += s; }
    const std::string& out() const { return out_; }
    void               clearOut() { out_.clear(); }
    // Bytes let go since the last takeDropped(), because nobody read them in time.
    uint64_t takeDropped() {
        const uint64_t d = dropped_;
        dropped_ = 0;
        return d;
    }

private:
    std::unique_ptr<ByteStream> inner_;
    std::string                 inject_;
    size_t                      injectPos_ = 0;
    std::string                 out_;
    uint64_t                    dropped_ = 0;
};

// Turned on by main() for --mcp, before the machine loads. Off, resolveEndpoint() builds the
// bare endpoint, as it always has: the CLI pays nothing.
void setAgentTaps(bool on);
bool agentTaps();

// The tap on a unit's line, peeled out from under whatever wraps it (the console's filter,
// a shared handle). Null when the line has none -- a stream built before taps were on, or a
// board that holds a stream it did not get from resolveEndpoint().
AgentTap* findAgentTap(ByteStream* s);

} // namespace altair
