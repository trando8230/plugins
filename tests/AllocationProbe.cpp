#include "AllocationProbe.h"
#include <cstdlib>
#include <new>
#include <cstdint>
#include <limits>

namespace allocationProbe
{
thread_local bool enabled = false;
thread_local size_t count = 0;
}

// Count standard C++ allocations on just the test's audio thread. JUCE/Catch
// allocations outside the measured scope and on other threads are unaffected.
void* operator new(std::size_t size)
{
    if (allocationProbe::enabled) ++allocationProbe::count;
    if (void* p = std::malloc(size == 0 ? 1 : size)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

void* operator new(std::size_t size, std::align_val_t alignmentValue)
{
    if (allocationProbe::enabled) ++allocationProbe::count;
    const auto alignment = static_cast<std::size_t>(alignmentValue);
    if (size > std::numeric_limits<std::size_t>::max() - alignment - sizeof(void*)) throw std::bad_alloc();
    void* raw = std::malloc(size + alignment + sizeof(void*));
    if (raw == nullptr) throw std::bad_alloc();
    const auto address = (reinterpret_cast<std::uintptr_t>(raw) + sizeof(void*) + alignment - 1) & ~(alignment - 1);
    auto* result = reinterpret_cast<void*>(address);
    static_cast<void**>(result)[-1] = raw;
    return result;
}
void* operator new[](std::size_t size, std::align_val_t alignment) { return ::operator new(size, alignment); }
void operator delete(void* p, std::align_val_t) noexcept { if (p) std::free(static_cast<void**>(p)[-1]); }
void operator delete[](void* p, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete(void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete[](void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
