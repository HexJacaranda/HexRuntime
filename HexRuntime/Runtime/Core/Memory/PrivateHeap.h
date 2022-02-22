#pragma once
#include "..\..\RuntimeAlias.h"
#include <memory_resource>
#include <mimalloc.h>
#include <atomic>

namespace RTMM
{
	/// <summary>
	/// Provide memory allocation for assembly
	/// </summary>
	class PrivateHeap
	{
		std::unique_ptr<std::pmr::synchronized_pool_resource> mResource;
		std::unique_ptr<std::pmr::polymorphic_allocator<std::byte>> mAllocator;
	public:
		PrivateHeap();
		~PrivateHeap();

		void* Allocate(Int32 size);
		void* Allocate(Int32 size, Int32 align);

		void Free(void* target, Int32 size);
		void FreeTracked(void* target);

		template<class U,class...Args>
		U* New(Args&&...args)
		{
			void* memory = Allocate(sizeof(U));
			new (memory) U(std::forward<Args>(args)...);
			return static_cast<U*>(memory);
		}

		template<class U>
		void Return(U* object)
		{
			if (object != nullptr)
			{
				object->~U();
				mAllocator->deallocate_bytes(object, sizeof(U));
			}
		}

		std::pmr::synchronized_pool_resource* GetResource()const;
	};

	enum class TrackTagT { Null };
	static constexpr TrackTagT TrackTag = TrackTagT::Null;

	extern PrivateHeap* GlobalHeap;
}

void* operator new(size_t size, RTMM::PrivateHeap* allocator);
void* operator new(size_t size, RTMM::PrivateHeap* allocator, RTMM::TrackTagT);
void* operator new[](size_t size, RTMM::PrivateHeap* allocator);
void* operator new[](size_t size, RTMM::PrivateHeap* allocator, RTMM::TrackTagT);