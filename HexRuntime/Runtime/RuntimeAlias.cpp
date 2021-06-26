#include "RuntimeAlias.h"
#include <mimalloc.h>

void* operator new(size_t size)
{
	return mi_malloc(size);;
}

void* operator new[](size_t size)
{
	return mi_malloc(size);
}

void* operator new(size_t size, std::align_val_t align)
{
	return mi_malloc_aligned(size, (std::size_t)align);
}

void* operator new[](size_t size, std::align_val_t align)
{
	return mi_malloc_aligned(size, (std::size_t)align);
}

void operator delete(void* ptr) noexcept
{
	mi_free(ptr);
}

void operator delete[](void* ptr) noexcept
{
	mi_free(ptr);
}
