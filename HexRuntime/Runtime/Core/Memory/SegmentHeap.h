#pragma once
#include "..\..\RuntimeAlias.h"
#include <type_traits>
#include <memory_resource>
#include <vector>

namespace RTMM
{
	//Common generator of destructors
	template<class T>
	static void CollectMethodGenerator(void* target) {
		((T*)target)->~T();
	}
	using CollectMethod = void(*)(void*);
	struct CollectPoint
	{
		CollectMethod Method;
		void* Address;
	};

	class SegmentHeap
	{
		std::vector<CollectPoint> mCollectPoints;
		std::unique_ptr<std::pmr::polymorphic_allocator<std::byte>> mAllocator;
	public:
		static constexpr Int32 Align = 8;

		SegmentHeap();
		~SegmentHeap();

		template<class T, class...Args>
		requires(std::is_pod_v<T>)
		T* New(Args&&...args) {
			T* ret = (T*)mAllocator->allocate_bytes(sizeof(T), Align);
			new (ret) T(std::forward<Args>(args)...);

			return ret;
		}
		template<class T, class...Args>
		requires(!std::is_pod_v<T>)
		T* New(Args&&...args) {
			T* ret = (T*)mAllocator->allocate_bytes(sizeof(T), Align);
			new (ret) T(std::forward<Args>(args)...);

			mCollectPoints.push_back(CollectPoint{ &CollectMethodGenerator<T>, ret });
			return ret;
		}

		void* Allocate(size_t bytes);
	};
}

/// <summary>
/// Overload new for arena memory allocator
/// </summary>
/// <param name="size"></param>
/// <param name="allocator"></param>
/// <returns></returns>
void* operator new(size_t size, RTMM::SegmentHeap* allocator);
void* operator new[](size_t size, RTMM::SegmentHeap* allocator);

void operator delete(void* target, RTMM::SegmentHeap* allocator);
void operator delete[](void* target, RTMM::SegmentHeap* allocator);