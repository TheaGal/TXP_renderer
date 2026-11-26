#if TRACY_ENABLE

#include <tracy/Tracy.hpp>

#include <mutex>


namespace
{

std::mutex g_mem_lock;

} // namespace

void* operator new(decltype(sizeof(0)) count) noexcept(false)
{
    std::lock_guard lock(g_mem_lock);
    auto ptr = malloc(count);
    TracyAlloc(ptr, count);
    return ptr;
}

void operator delete(void* ptr) throw()
{
    std::lock_guard lock(g_mem_lock);
    TracyFree(ptr);
    free(ptr);
}

#endif // TRACY_ENABLE
