#include "UTIL.h"

#include <new>
#include <cstdlib>
#include <algorithm>

double thread_cpu_time() {
    timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

double thread_real_time() {
    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

constexpr std::size_t GiB = 1024ULL * 1024 * 1024;

//namespace thread_mem {
//
///// thread-local storage
//    thread_local ThreadCounter tls_counter_instance;
//
//    ThreadCounter& tls_counter() {
//        return tls_counter_instance;
//    }
//
//    ThreadMemScope::ThreadMemScope()
//            : start_(tls_counter().allocated) {}
//
//    ThreadMemScope::~ThreadMemScope() = default;
//
//    double ThreadMemScope::used() const {
//        std::size_t now = tls_counter().allocated;
//        return (now >= start_) ? ((now - start_) / static_cast<double>(GiB)) : 0;
//    }
//
//} // namespace thread_mem
//
//// ================================
//// Global new / delete interception
//// ================================
//
//void* operator new(std::size_t size) {
//    using namespace thread_mem;
//
//    // allocate extra space to store size
//    std::size_t total = size + sizeof(std::size_t);
//    void* raw = std::malloc(total);
//    if (!raw) throw std::bad_alloc();
//
//    // store size at head
//    *reinterpret_cast<std::size_t*>(raw) = size;
//
//    // update counter
//    tls_counter().allocated += size;
//
//    return static_cast<char*>(raw) + sizeof(std::size_t);
//}
//
//void operator delete(void* ptr) noexcept {
//    if (!ptr) return;
//
//    using namespace thread_mem;
//
//    // recover raw pointer
//    void* raw = static_cast<char*>(ptr) - sizeof(std::size_t);
//    std::size_t size = *reinterpret_cast<std::size_t*>(raw);
//
//    // prevent underflow
//    if (tls_counter().allocated >= size) {
//        tls_counter().allocated -= size;
//    } else {
//        tls_counter().allocated = 0;
//    }
//
//    std::free(raw);
//}
//
//// C++14 sized delete
//void operator delete(void* ptr, std::size_t) noexcept {
//    operator delete(ptr);
//}

namespace thread_mem {

    // ============================
    // Thread-local counter
    // ============================

    thread_local ThreadCounter tls_counter_instance;

    ThreadCounter& tls_counter() {
        return tls_counter_instance;
    }

    // ============================
    // RAII scope
    // ============================

    ThreadMemScope::ThreadMemScope()
            : start_(tls_counter().allocated) {}

    ThreadMemScope::~ThreadMemScope() = default;

    double ThreadMemScope::used() const {
        std::size_t now = tls_counter().allocated;
        return (now >= start_) ? ((now - start_) / static_cast<double>(GiB)) : 0.0;
    }

    double ThreadMemScope::peak() const {
        return static_cast<double>(tls_counter().peak) / static_cast<double>(GiB);
    }

} // namespace thread_mem


// =====================================
// Global new/delete interception
// =====================================

namespace {

    struct Header {
        std::size_t size;
    };

    inline void* allocate_with_header(std::size_t size) {
        std::size_t total = size + sizeof(Header);

        void* raw = std::malloc(total);
        if (!raw) throw std::bad_alloc();

        auto* header = static_cast<Header*>(raw);
        header->size = size;

        thread_mem::tls_counter().allocated += size;
        thread_mem::tls_counter().peak = std::max(thread_mem::tls_counter().peak, thread_mem::tls_counter().allocated);

        return static_cast<void*>(header + 1);
    }

    inline void deallocate_with_header(void* ptr) noexcept {
        if (!ptr) return;

        auto* header = static_cast<Header*>(ptr) - 1;
        std::size_t size = header->size;

        auto& counter = thread_mem::tls_counter();
        if (counter.allocated >= size)
            counter.allocated -= size;
        else
            counter.allocated = 0;

        std::free(header);
    }

} // anonymous namespace


// ============================
// operator new
// ============================

void* operator new(std::size_t size) {
    return allocate_with_header(size);
}

void operator delete(void* ptr) noexcept {
    deallocate_with_header(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    deallocate_with_header(ptr);
}


// ============================
// operator new[]
// ============================

void* operator new[](std::size_t size) {
    return allocate_with_header(size);
}

void operator delete[](void* ptr) noexcept {
    deallocate_with_header(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    deallocate_with_header(ptr);
}


// ============================
// nothrow versions
// ============================

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return allocate_with_header(size);
    } catch (...) {
        return nullptr;
    }
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    deallocate_with_header(ptr);
}

