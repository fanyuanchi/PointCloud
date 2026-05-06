#ifndef PCPS_UTIL_H
#define PCPS_UTIL_H

#pragma once

#include <cstddef>
#include <atomic>
#include <ctime>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

double thread_cpu_time();
double thread_real_time();
/*
 * Thread-level memory tracker
 * Tracks logical allocation size via new/delete interception.
 *
 * Semantics:
 *   - Per-thread
 *   - Scope-based (RAII)
 *   - Counts requested allocation sizes (bytes)
 */

namespace thread_mem {

/// Thread-local counter
    struct ThreadCounter {
        std::size_t allocated = 0;
        std::size_t peak = 0;
    };

/// Get thread-local counter
    ThreadCounter& tls_counter();

/// RAII scope for memory tracking
    class ThreadMemScope {
    public:
        ThreadMemScope();
        ~ThreadMemScope();

        // Disable copy
        ThreadMemScope(const ThreadMemScope&) = delete;
        ThreadMemScope& operator=(const ThreadMemScope&) = delete;

        // Bytes allocated during this scope
        double used() const;
        //
//        double peak() const;

        double peak() const;

    private:
        std::size_t start_;
    };

} // namespace thread_mem



class Logger {
public:
    std::ofstream file_;
    std::mutex mtx_;

    bool verbose_ = true;
    bool metaSealed_ = false;
    bool runtimeHeaderWritten_ = false;
    bool resultHeaderWritten_ = false;
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    Logger() = default;
    ~Logger() { close(); }

    void writeLine(const std::string& line) {
        file_ << line << "\n";
    }

    void flush() {
        file_.flush();
    }

    void init(const std::string& fileName, bool verbose = true) {
//        std::lock_guard<std::mutex> lock(mtx_);
        verbose_ = verbose;
        file_.open(fileName, std::ios::out);
        if (!file_.is_open()) {
            throw std::runtime_error("Cannot open log file.");
        }
        file_ << std::fixed << std::setprecision(3);
        writeLine("# ================= META =================");
        metaSealed_ = false;
        runtimeHeaderWritten_ = false;
        resultHeaderWritten_ = false;
    }

    template<typename T>
    void writeMeta(const std::string& key, const T& value) {
//        std::lock_guard<std::mutex> lock(mtx_);
        if (metaSealed_) return;
        file_ << key << ": " << value << "\n\n";
    }

    void sealMeta() {
//        std::lock_guard<std::mutex> lock(mtx_);
        if (!runtimeHeaderWritten_) {
            writeLine("");
            writeLine("# ================= RUNTIME =================");
            runtimeHeaderWritten_ = true;
        }
        metaSealed_ = true;
        flush();
    }

    // every 100k requests processed call once
    void writeProgress(size_t curRequestNum, double progressTime, double totalTime){
        if (!verbose_) return;

//        std::lock_guard<std::mutex> lock(mtx_);

        file_ << "progress: 100k / "
              << std::to_string(curRequestNum / 100000) << "00k"
              << ", elapsed time (seconds): " << progressTime << " / " << totalTime
              << "\n\n";
        flush();
    }

    template<typename T>
    void writeResult(const std::string& key, const T& value) {
//        std::lock_guard<std::mutex> lock(mtx_);
        if (!resultHeaderWritten_) {
            writeLine("");
            writeLine("# ================= RESULT =================");
            resultHeaderWritten_ = true;
        }
        file_ << key << ": " << value << "\n\n";
        flush();
    }

    template<typename T>
    void writePhase(const std::string& key, const T& time) {
//        std::lock_guard<std::mutex> lock(mtx_);
        file_ << key << ": " << time << "\n";
        writeLine("");
        writeLine("# ================= NEXT PHASE =================");
        flush();
    }

    void close() {
//        std::lock_guard<std::mutex> lock(mtx_);
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
    }
};


#endif //PCPS_UTIL_H
