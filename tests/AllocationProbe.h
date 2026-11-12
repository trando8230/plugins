#pragma once
#include <cstddef>
namespace allocationProbe
{
extern thread_local bool enabled;
extern thread_local size_t count;
struct Scope
{
    Scope() { count = 0; enabled = true; }
    ~Scope() { enabled = false; }
};
}
