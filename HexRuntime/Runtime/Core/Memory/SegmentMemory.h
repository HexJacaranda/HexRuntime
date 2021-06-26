#pragma once
#include "..\..\RuntimeAlias.h"
#include <type_traits>

namespace RTMM
{
	struct Segment
	{
		Segment* Previous = nullptr;
	};

	using ReleaseFun = void(*)(void*);

	//Common generator of destructors
	template<class T>
	static void ReleaseFunGenerator(void* target) {
		((T*)target)->~T();
	}

	struct NonPODEntry
	{
		Int32 Size;
		ReleaseFun Release;
	};

	/// <summary>
	/// Provide fast and safe bumping pointer allocator
	/// </summary>
	class SegmentMemory
	{
	public:
		static constexpr Int32 SegmentSize = 4096;
		static constexpr Int32 Align = 8;
	private:
		Int32 mSegmentSize = SegmentSize;
		Int32 mAlign = Align;
		Segment* mOversizeCurrent = nullptr;

		Segment* mPODCurrent = nullptr;
		Int8* mPODStart = nullptr;

		Segment* mNonPODCurrent = nullptr;
		Int8* mNonPODStart = nullptr;
	private:
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
		SegmentMemory(Int32 align, Int32 segmentSize);
		SegmentMemory();
		~SegmentMemory();

		void* AllocateNonPOD(Int32 size, ReleaseFun call);
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
			T* ret = (T*)AllocateNonPOD(sizeof(T), ReleaseFunGenerator<T>);
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
void* operator new(size_t size, RTMM::SegmentMemory* allocator);
void* operator new[](size_t size, RTMM::SegmentMemory* allocator);

void operator delete(void* target, RTMM::SegmentMemory* allocator);
void operator delete[](void* target, RTMM::SegmentMemory* allocator);