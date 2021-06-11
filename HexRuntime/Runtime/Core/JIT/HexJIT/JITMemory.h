#pragma once
#include "..\..\..\RuntimeAlias.h"
#include <type_traits>
#pragma warning(disable:4291)

namespace RTJ::Hex
{
	using NonPODReleaseCall = void(*)(void*);

	//Common generator of destructors
	template<class T>
	static void ReleaseCallGenerator(void* target) {
		((T*)target)->~T();
	}

	/// <summary>
	/// Provide an arena memory allocator
	/// </summary>
	class JITMemory
	{
		static constexpr Int32 SegmentSpace = 4096;
		struct Segment
		{
			Segment* Previous;
		};

		struct NonPODEntry
		{
			Int32 Size;
			NonPODReleaseCall ReleaseCall;
		};

		Segment* mOversizeCurrent = nullptr;
		Segment* mPODCurrent = nullptr;
		Int8* mPODStart = nullptr;

		Segment* mNonPODCurrent = nullptr;
		Int8* mNonPODStart = nullptr;

		void AllocateSegment();
		void AllocateNonPODSegment();
		void AllocateOversizeSegment(Int32 size);
		bool IsMoreSegmentNeeded(Int32 size);
		bool IsMoreNonPODSegmentNeeded(Int32 size);
		void MarkNonPODSegmentEnd();
	private:
		void ReleaseOversizeArea();
		void ReleaseNonPODArea();
		void ReleasePODArea();
	public:
		JITMemory();
		~JITMemory();
	public:
		void* AllocateNonPOD(Int32 size, NonPODReleaseCall call);
		void* Allocate(Int32 size);

		template<class T, class...Args>
		requires(std::is_pod_v<T>)
		inline T* New(Args&&...args) {
			T* ret = (T*)Allocate(sizeof(T));
			new (ret) T(std::forward<Args>(args)...);
			return ret;
		}
		template<class T, class...Args>
		requires(!std::is_pod_v<T>)
		inline T* New(Args&&...args) {
			T* ret = (T*)AllocateNonPOD(sizeof(T), ReleaseCallGenerator<T>);
			new (ret) T(std::forward<Args>(args)...);
			return ret;
		}
	};
}

/// <summary>
/// Overload new for arena memory allocator
/// </summary>
/// <param name="size"></param>
/// <param name="allocator"></param>
/// <returns></returns>
void* operator new(size_t size, RTJ::Hex::JITMemory* allocator);

void* operator new[](size_t size, RTJ::Hex::JITMemory* allocator);