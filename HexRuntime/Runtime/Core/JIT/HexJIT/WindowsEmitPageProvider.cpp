#include "EmitPageProvider.h"
#include "..\..\Platform\Platform.h"

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
namespace RTJ
{
	UInt8* EmitPageProvider::Allocate(Int32 size)
	{
		return (UInt8*)VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_READWRITE);
	}
	void EmitPageProvider::Free(UInt8* origin)
	{
		VirtualFree(origin, 0, MEM_RELEASE);
	}
	UInt8* EmitPageProvider::SetExecutable(UInt8* origin, Int32 executableLength)
	{
		DWORD old;
		VirtualProtect(origin, executableLength, PAGE_EXECUTE, &old);
		return origin;
	}
	UInt8* EmitPageProvider::SetReadOnly(UInt8* origin, Int32 readonlyLength)
	{
		DWORD old;
		VirtualProtect(origin, readonlyLength, PAGE_READONLY, &old);
		return origin;
	}
}
#endif // PLATFORM_WINDOWS