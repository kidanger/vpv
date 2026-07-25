#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace reproc {
class process;
}

struct Image;

/*! Persistent `python3` child process used to evaluate user scripts.
 *
 * A worker is spawned lazily on the first edit and kept alive afterwards:
 * `import numpy` costs 80-150ms, and EditGUI revalidates on every frame of a
 * `$1` slider drag, so a per-edit spawn would be unusable.
 *
 * Input images are exposed to the script as `x`, `y`, `z`, `a`, `b`, ... (the
 * plambda convention) and as the list `I`; `np` is numpy.
 *
 * If the config key `PYTHON_PREAMBLE` is set to a non-empty string, its content
 * is prepended to every script before execution, useful for imports or
 * helper functions.
 *
 * Wire protocol (little-endian; both ends are the same machine). The worker
 * implementation lives in src/pyworker.py and must be kept in sync.
 *
 *   hello (worker -> vpv, once at startup):
 *     "VPVH" u32:status u32:msglen msglen*u8
 *
 *   request (vpv -> worker):
 *     "VPVQ" u32:nimages u32:proglen
 *     nimages * { u32:w u32:h u32:c }
 *     proglen*u8                        (UTF-8 source)
 *     nimages * (w*h*c*4 bytes)         (float32, row-major, interleaved)
 *
 *   response (worker -> vpv):
 *     "VPVR" u32:status
 *       status == 0: u32:w u32:h u32:c then w*h*c*4 bytes
 *       status != 0: u32:msglen then msglen*u8 (traceback)
 *
 * The child's stderr is deliberately inherited rather than piped: a script that
 * prints a lot would otherwise fill the pipe buffer and deadlock, since nothing
 * on this side drains it.
 */
class PythonWorker {
public:
    static PythonWorker& instance();

    PythonWorker(const std::string& exe, int timeout);
    ~PythonWorker();

    PythonWorker(const PythonWorker&) = delete;
    PythonWorker& operator=(const PythonWorker&) = delete;

    /*! Evaluate `prog` over `images`. Returns nullptr and fills `error` on
     * failure. Thread-safe; concurrent calls serialize on the worker. */
    std::shared_ptr<Image> run(const std::string& prog,
        const std::vector<std::shared_ptr<Image>>& images,
        std::string& error);

    /*! Terminate the worker if it is running. Safe to call repeatedly. */
    void shutdown();

private:
    enum class Outcome {
        Ok,
        /*! The script itself failed; the worker is still healthy. */
        ScriptError,
        /*! Communication broke down; the worker must be respawned. */
        WorkerLost,
        /*! Startup failed in a way that retrying cannot fix. */
        Fatal,
    };

    Outcome attempt(const std::string& prog,
        const std::vector<std::shared_ptr<Image>>& images,
        std::shared_ptr<Image>& result,
        std::string& error);

    Outcome start(std::string& error);
    void stop();

    bool writeAll(const uint8_t* data, size_t size, std::string& error);
    bool readAll(uint8_t* data, size_t size, std::string& error);

    /*! Start a fresh timeout window of `ms` for what follows; a non-positive `ms`
     * means "wait forever". */
    void armDeadline(int ms);

    /*! Milliseconds left in the current request: -1 to wait forever, 0 if the
     * deadline has already expired. */
    int remainingMs() const;

    std::string exe;
    int timeout;

    std::mutex mutex;
    std::unique_ptr<reproc::process> process;
    bool fatal = false;
    std::string fatalReason;
    /*! Set by readAll/writeAll when the deadline expired, so that run() does not
     * waste a second timeout re-running a script that is simply too slow. */
    bool timedOut = false;
    /*! Armed by attempt() and shared by every poll that follows, so the timeout
     * bounds a whole request instead of restarting on each poll. The handshake
     * and the exchange each get their own window. Only meaningful when
     * timeout > 0. */
    std::chrono::steady_clock::time_point deadline;
    bool infiniteDeadline = true;
};
