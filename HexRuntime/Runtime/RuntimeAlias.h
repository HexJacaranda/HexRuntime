#pragma once
#pragma warning(disable:28251)
#include <numeric>
#include <new>

namespace Runtime
{
//Provide well-defined alias for runtime

	using Int8 = __int8;
	using Int16 = __int16;
	using Int32 = __int32;
	using Int64 = __int64;
	using UInt8 = unsigned __int8;
	using UInt16 = unsigned __int16;
	using UInt32 = unsigned __int32;
	using UInt64 = unsigned __int64;
	using Float = float;
	using Double = double;
	using Boolean = bool;

	using MDToken = UInt32;

	constexpr MDToken NullToken = std::numeric_limits<MDToken>::max();

#ifdef _DEBUG
#define RTDEBUG
#endif

#ifdef _M_AMD64
	using IntPtr = Int64;
	using Int = Int64;
#define _AMD64_
#define X64
#else
	using IntPtr = Int32;
	using Int = Int32;
#endif
	using RTString = const wchar_t*;
	using MutableRTString = wchar_t*;
#define Text(T) L##T

#define RT  Runtime
#define RTC Runtime::Core
#define RTO Runtime::Core::ManagedObject
#define RTM Runtime::Core::Meta
#define RTME Runtime::Core::Meta::EE

#define RTMM Runtime::Core::Memory

#define RTE Runtime::Exception
#define RTGC Runtime::Core::GC
#define RTP Runtime::Core::Platform
#define RTJ Runtime::Core::JIT
#define RTJE Runtime::Core::JIT::Emit
#define RTIOS2EE Runtime::Core::Interfaces::OSToEE
#define RTI Runtime::Core::Interfaces
	
#define ForcedInline __forceinline

//Argument as result annotation
#define _RE_
}

//Overrides for mimalloc

void* operator new(size_t size);
void* operator new[](size_t size);
void* operator new(size_t size, std::align_val_t align);
void* operator new[](size_t size, std::align_val_t align);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;