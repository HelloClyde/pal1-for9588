#include <stddef.h>
#include <stdlib.h>

void *operator new(size_t size)
{
    void *memory = malloc(size == 0 ? 1 : size);
    if (!memory) abort();
    return memory;
}

void *operator new[](size_t size)
{
    return operator new(size);
}

void operator delete(void *memory) noexcept { free(memory); }
void operator delete[](void *memory) noexcept { free(memory); }
void operator delete(void *memory, size_t) noexcept { free(memory); }
void operator delete[](void *memory, size_t) noexcept { free(memory); }

extern "C" void __cxa_pure_virtual(void) { abort(); }
