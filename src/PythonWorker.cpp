#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <reproc++/reproc.hpp>

#include "Image.hpp"
#include "PythonWorker.hpp"
#include "config.hpp"
#include "globals.hpp"

// Generated at configure time from src/pyworker.py.
extern const char* const PYWORKER_SOURCE;

namespace {

constexpr uint32_t STATUS_OK = 0;

constexpr size_t MAX_MESSAGE_SIZE = 1u << 20;
// A guard against a desynchronized stream handing us an absurd allocation.
constexpr size_t MAX_PIXELS = 1ull << 28; // ~1GB of float32 pixels

// Spawning the interpreter and importing numpy costs 80-150ms and has nothing to
// do with how slow the user's script is, so the handshake gets its own budget
// rather than eating into the per-request timeout.
constexpr int STARTUP_TIMEOUT_MS = 10000;

void put32(std::vector<uint8_t>& buf, uint32_t v)
{
    // The protocol is little-endian; both ends are always the same machine, but
    // being explicit keeps the worker script simple and portable.
    buf.push_back(v & 0xff);
    buf.push_back((v >> 8) & 0xff);
    buf.push_back((v >> 16) & 0xff);
    buf.push_back((v >> 24) & 0xff);
}

uint32_t get32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool wouldBlock(const std::error_code& ec)
{
    return ec == std::errc::resource_unavailable_try_again
        || ec == std::errc::operation_would_block;
}

reproc::milliseconds poll_timeout(int remaining)
{
    if (remaining < 0) {
        // A non-positive configured timeout means "wait forever". This lets a
        // user who knows their script is slow opt out, at the cost of being able
        // to hang the loading thread.
        return reproc::infinite;
    }
    return reproc::milliseconds(remaining);
}

}

PythonWorker& PythonWorker::instance()
{
    static PythonWorker worker(gPythonExe, gPythonTimeout);
    return worker;
}

PythonWorker::PythonWorker(const std::string& exe, int timeout)
    : exe(exe)
    , timeout(timeout)
{
}

PythonWorker::~PythonWorker()
{
    shutdown();
}

void PythonWorker::shutdown()
{
    std::lock_guard<std::mutex> _lock(mutex);
    stop();
}

void PythonWorker::stop()
{
    if (!process) {
        return;
    }
    process->close(reproc::stream::in);
    process->close(reproc::stream::out);
    reproc::stop_actions actions = {
        { reproc::stop::terminate, reproc::milliseconds(50) },
        { reproc::stop::kill, reproc::milliseconds(50) },
        {}
    };
    process->stop(actions);
    process.reset();
}

PythonWorker::Outcome PythonWorker::start(std::string& error)
{
    reproc::options options;
    options.nonblocking = true;
    // Inherit stderr so tracebacks and the user's print() output land in vpv's
    // own stderr instead of filling a pipe nobody drains.
    options.redirect.err.type = reproc::redirect::parent;
    options.redirect.in.type = reproc::redirect::pipe;
    options.redirect.out.type = reproc::redirect::pipe;
    options.stop = {
        { reproc::stop::terminate, reproc::milliseconds(50) },
        { reproc::stop::kill, reproc::milliseconds(50) },
        {}
    };

    std::string exe = this->exe.empty() ? "python" : this->exe;
    const char* args[] = { exe.c_str(), "-c", PYWORKER_SOURCE, nullptr };

    process = std::make_unique<reproc::process>();
    std::error_code ec = process->start(args, options);
    if (ec) {
        process.reset();
        if (ec == std::errc::no_such_file_or_directory) {
            error = "python interpreter '" + exe + "' not found; set PYTHON_INTERPRETER in your .vpvrc";
        } else {
            error = "cannot start '" + exe + "': " + ec.message();
        }
        // Missing or broken interpreter will not fix itself; do not respawn on
        // every single edit.
        fatal = true;
        fatalReason = error;
        return Outcome::Fatal;
    }

    // Read the hello frame so a missing numpy is reported once, clearly,
    // instead of once per edit.
    uint8_t header[12];
    if (!readAll(header, sizeof(header), error)) {
        stop();
        error = "python worker did not start: " + error;
        fatal = true;
        fatalReason = error;
        return Outcome::Fatal;
    }
    if (std::memcmp(header, "VPVH", 4) != 0) {
        stop();
        error = "unexpected greeting from python worker";
        fatal = true;
        fatalReason = error;
        return Outcome::Fatal;
    }

    const uint32_t status = get32(header + 4);
    const uint32_t msglen = get32(header + 8);
    if (msglen > MAX_MESSAGE_SIZE) {
        stop();
        error = "oversized error message from the python worker";
        fatal = true;
        fatalReason = error;
        return Outcome::Fatal;
    }

    std::string message;
    if (msglen > 0) {
        message.resize(msglen);
        if (!readAll((uint8_t*)message.data(), msglen, error)) {
            stop();
            fatal = true;
            fatalReason = error;
            return Outcome::Fatal;
        }
    }

    if (status != STATUS_OK) {
        stop();
        error = message.empty() ? "python worker refused to start" : message;
        fatal = true;
        fatalReason = error;
        return Outcome::Fatal;
    }

    return Outcome::Ok;
}

void PythonWorker::armDeadline(int ms)
{
    if (ms > 0) {
        deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    }
    infiniteDeadline = ms <= 0;
}

int PythonWorker::remainingMs() const
{
    if (infiniteDeadline) {
        return -1;
    }
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now())
                          .count();
    return left <= 0 ? 0 : (int)left;
}

bool PythonWorker::writeAll(const uint8_t* data, size_t size, std::string& error)
{
    size_t written = 0;
    while (written < size) {
        // Poll before writing: with a blocking pipe, sending a large image while
        // the child is writing its own output would deadlock both sides once the
        // 64KB pipe buffer fills.
        const int left = remainingMs();
        if (left == 0) {
            error = "timed out sending data to the python worker";
            timedOut = true;
            return false;
        }
        std::pair<int, std::error_code> polled = process->poll(reproc::event::in, poll_timeout(left));
        if (polled.second) {
            error = "write failed: " + polled.second.message();
            return false;
        }
        if (polled.first == 0) {
            error = "timed out sending data to the python worker";
            timedOut = true;
            return false;
        }

        std::pair<size_t, std::error_code> result = process->write(data + written, size - written);
        if (result.second) {
            if (wouldBlock(result.second)) {
                continue;
            }
            if (result.second == std::errc::broken_pipe) {
                error = "the python worker exited unexpectedly";
            } else {
                error = "write failed: " + result.second.message();
            }
            return false;
        }
        written += result.first;
    }
    return true;
}

bool PythonWorker::readAll(uint8_t* data, size_t size, std::string& error)
{
    size_t read = 0;
    while (read < size) {
        // The deadline is always armed by attempt() before anything, including
        // start() and its hello frame, can reach here.
        const int left = remainingMs();
        if (left == 0) {
            error = "the python script timed out";
            timedOut = true;
            return false;
        }
        std::pair<int, std::error_code> polled = process->poll(reproc::event::out, poll_timeout(left));
        if (polled.second) {
            error = "read failed: " + polled.second.message();
            return false;
        }
        if (polled.first == 0) {
            error = "the python script timed out";
            timedOut = true;
            return false;
        }

        std::pair<size_t, std::error_code> result = process->read(reproc::stream::out, data + read, size - read);
        if (result.second) {
            if (wouldBlock(result.second)) {
                continue;
            }
            if (result.second == std::errc::broken_pipe) {
                error = "the python worker exited unexpectedly (see stderr)";
            } else {
                error = "read failed: " + result.second.message();
            }
            return false;
        }
        read += result.first;
    }
    return true;
}

PythonWorker::Outcome PythonWorker::attempt(const std::string& prog,
    const std::vector<std::shared_ptr<Image>>& images,
    std::shared_ptr<Image>& result,
    std::string& error)
{
    if (!process) {
        // The handshake gets its own budget: interpreter startup is not the user's
        // script, and charging it to a short timeout would make every respawn fail.
        armDeadline(timeout > 0 ? STARTUP_TIMEOUT_MS : 0);

        Outcome outcome = start(error);
        if (outcome != Outcome::Ok) {
            return outcome;
        }
    }

    // From here on a single deadline covers the whole exchange, so a script that
    // dribbles output cannot extend it one poll at a time.
    armDeadline(timeout);

    // The protocol sends whole interleaved buffers. A lazy image is edited tile
    // by tile through EditChunkSource, and a tile is just a small resident
    // image, so the protocol itself did not have to change. See bigimages.md.
    for (const auto& image : images) {
        assert(image->pixels && "the python worker needs a resident buffer");
        if (!image->pixels) {
            error = "editing big images is not supported here";
            return Outcome::ScriptError;
        }
    }

    // Header and geometry first, so the worker knows exactly how much to read
    // before it writes anything back.
    std::vector<uint8_t> header;
    header.reserve(12 + 12 * images.size() + prog.size());
    header.insert(header.end(), { 'V', 'P', 'V', 'Q' });
    put32(header, (uint32_t)images.size());
    put32(header, (uint32_t)prog.size());
    for (const auto& image : images) {
        put32(header, (uint32_t)image->w);
        put32(header, (uint32_t)image->h);
        put32(header, (uint32_t)image->c);
    }
    header.insert(header.end(), prog.begin(), prog.end());

    if (!writeAll(header.data(), header.size(), error)) {
        return Outcome::WorkerLost;
    }
    for (const auto& image : images) {
        const size_t bytes = image->w * image->h * image->c * sizeof(float);
        if (!writeAll((const uint8_t*)image->pixels, bytes, error)) {
            return Outcome::WorkerLost;
        }
    }

    uint8_t response[8];
    if (!readAll(response, sizeof(response), error)) {
        return Outcome::WorkerLost;
    }
    if (std::memcmp(response, "VPVR", 4) != 0) {
        error = "desynchronized stream from the python worker";
        return Outcome::WorkerLost;
    }

    if (get32(response + 4) != STATUS_OK) {
        uint8_t lenbuf[4];
        if (!readAll(lenbuf, sizeof(lenbuf), error)) {
            return Outcome::WorkerLost;
        }
        const uint32_t msglen = get32(lenbuf);
        if (msglen > MAX_MESSAGE_SIZE) {
            error = "oversized error message from the python worker";
            return Outcome::WorkerLost;
        }
        std::string message(msglen, '\0');
        if (msglen && !readAll((uint8_t*)message.data(), msglen, error)) {
            return Outcome::WorkerLost;
        }
        error = message;
        return Outcome::ScriptError;
    }

    uint8_t geometry[12];
    if (!readAll(geometry, sizeof(geometry), error)) {
        return Outcome::WorkerLost;
    }
    const uint32_t w = get32(geometry);
    const uint32_t h = get32(geometry + 4);
    const uint32_t c = get32(geometry + 8);

    const uint64_t count = (uint64_t)w * h * c;
    if (count == 0 || count > MAX_PIXELS) {
        error = "the python script returned an image with an invalid size";
        return Outcome::WorkerLost;
    }

    // Image takes ownership and frees this in its destructor, so it has to come
    // from malloc rather than new.
    float* pixels = (float*)malloc(count * sizeof(float));
    if (!pixels) {
        error = "out of memory while reading the python result";
        return Outcome::WorkerLost;
    }
    if (!readAll((uint8_t*)pixels, count * sizeof(float), error)) {
        free(pixels);
        return Outcome::WorkerLost;
    }

    result = std::make_shared<Image>(pixels, w, h, c);
    return Outcome::Ok;
}

std::shared_ptr<Image> PythonWorker::run(const std::string& prog,
    const std::vector<std::shared_ptr<Image>>& images,
    std::string& error)
{
    std::lock_guard<std::mutex> _lock(mutex);

    if (fatal) {
        error = fatalReason;
        return nullptr;
    }

    std::shared_ptr<Image> result;

    // Two attempts: a worker killed by an OOM or a segfaulting extension module
    // leaves an unusable process behind, and respawning it is both cheap and
    // invisible to the user. A timeout is not retried: the script would just be
    // too slow a second time, and the user would wait twice as long to hear so.
    for (int tries = 0; tries < 2; tries++) {
        timedOut = false;
        Outcome outcome = attempt(prog, images, result, error);
        switch (outcome) {
        case Outcome::Ok:
            return result;
        case Outcome::ScriptError:
        case Outcome::Fatal:
            return nullptr;
        case Outcome::WorkerLost:
            // The worker is stuck running user code, or the stream is out of
            // sync; either way the process cannot be reused.
            stop();
            if (timedOut) {
                return nullptr;
            }
            break;
        }
    }

    return nullptr;
}

#include <doctest.h>

TEST_CASE("PythonWorker")
{
    auto makeImage = [](size_t w, size_t h, size_t c, float start) {
        float* pixels = (float*)malloc(w * h * c * sizeof(float));
        for (size_t i = 0; i < w * h * c; i++) {
            pixels[i] = start + i;
        }
        return std::make_shared<Image>(pixels, w, h, c);
    };

    PythonWorker worker("python3", 10000);
    std::string error;

    // If python or numpy is unavailable this is an environment problem, not a
    // regression; report it once and skip rather than failing the suite.
    std::shared_ptr<Image> probe = worker.run("x", { makeImage(2, 2, 1, 0.f) }, error);
    if (!probe) {
        WARN_MESSAGE(false, "skipping: " << error);
        return;
    }

    SUBCASE("identity preserves geometry and values")
    {
        auto in = makeImage(4, 3, 2, 1.f);
        auto out = worker.run("x", { in }, error);
        REQUIRE(out != nullptr);
        CHECK(out->w == 4);
        CHECK(out->h == 3);
        CHECK(out->c == 2);
        for (size_t i = 0; i < 4 * 3 * 2; i++) {
            CHECK(out->pixels[i] == in->pixels[i]);
        }
    }

    SUBCASE("arithmetic over two images")
    {
        auto out = worker.run("x + y", { makeImage(2, 2, 1, 1.f), makeImage(2, 2, 1, 10.f) }, error);
        REQUIRE(out != nullptr);
        CHECK(out->pixels[0] == 11.f);
        CHECK(out->pixels[3] == 17.f);
    }

    SUBCASE("a 2d result becomes a single channel")
    {
        auto out = worker.run("np.mean(x, axis=2)", { makeImage(2, 2, 3, 0.f) }, error);
        REQUIRE(out != nullptr);
        CHECK(out->c == 1);
        CHECK(out->w == 2);
        CHECK(out->h == 2);
    }

    SUBCASE("multi-line scripts and $-free multi statement bodies")
    {
        auto out = worker.run("t = x * 2\nu = t + 1\nu", { makeImage(2, 2, 1, 0.f) }, error);
        REQUIRE(out != nullptr);
        CHECK(out->pixels[0] == 1.f);
        CHECK(out->pixels[1] == 3.f);
    }

    SUBCASE("a script error is reported without killing the worker")
    {
        CHECK(worker.run("1/0", { makeImage(2, 2, 1, 0.f) }, error) == nullptr);
        CHECK(error.find("ZeroDivisionError") != std::string::npos);

        // The same worker must still be usable afterwards.
        CHECK(worker.run("x", { makeImage(2, 2, 1, 0.f) }, error) != nullptr);
    }

    SUBCASE("printing to stdout does not corrupt the protocol")
    {
        auto out = worker.run("print('noise')\nx * 2", { makeImage(2, 2, 1, 1.f) }, error);
        REQUIRE(out != nullptr);
        CHECK(out->pixels[0] == 2.f);
    }

    SUBCASE("a large image survives the pipe")
    {
        // Comfortably past the 64KB pipe buffer, in both directions.
        const size_t w = 512, h = 512, c = 3;
        auto out = worker.run("x + 1", { makeImage(w, h, c, 0.f) }, error);
        REQUIRE(out != nullptr);
        CHECK(out->w == w);
        CHECK(out->h == h);
        CHECK(out->c == c);
        CHECK(out->pixels[0] == 1.f);
        CHECK(out->pixels[w * h * c - 1] == (float)(w * h * c));
    }

    SUBCASE("the worker is respawned after it dies")
    {
        CHECK(worker.run("import os; os._exit(1)", { makeImage(2, 2, 1, 0.f) }, error) == nullptr);
        CHECK(worker.run("x", { makeImage(2, 2, 1, 0.f) }, error) != nullptr);
    }

    SUBCASE("a runaway script is interrupted and the worker recovers")
    {
        PythonWorker worker("python3", 100);

        CHECK(worker.run("import time; time.sleep(30)", { makeImage(2, 2, 1, 0.f) }, error) == nullptr);
        CHECK(error.find("timed out") != std::string::npos);

        CHECK(worker.run("x", { makeImage(2, 2, 1, 0.f) }, error) != nullptr);
    }
}
