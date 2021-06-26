#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Memory\PrivateHeap.h"
#include "TypeDescriptor.h"
#include "MDRecords.h"
#include "MDImporter.h"
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace RTM
{
	class TypeStatus
	{
	public:
		static constexpr Int8 NotYet = 0;
		static constexpr Int8 Processing = 1;
		/// <summary>
		/// Used for cyclical type loading
		/// </summary>
		static constexpr Int8 Almost = 2;
		static constexpr Int8 Done = 3;
	};

	struct TypeDefEntry
	{
		std::mutex WaiterLock;
		std::condition_variable Waiter;
		std::atomic<Int16> InitializeTicket = 0;
		std::atomic<Int8> Status = TypeStatus::NotYet;
		std::atomic<TypeDescriptor*> Type = nullptr;
	};

	struct AssemblyContext
	{
		RTME::MDImporter* Importer;
		RTMM::PrivateHeap* Heap;
		RTME::AssemblyRefMD* AssemblyRefs = nullptr;
		RTME::TypeRefMD* TypeRefs = nullptr;
		RTME::MemberRefMD* MemberRefs = nullptr;
		RTME::AssemblyHeaderMD Header = {};
		TypeDefEntry* Entries = nullptr;
	public:
		AssemblyContext(RTMM::PrivateHeap* heap, RTME::MDImporter* importer);
	};
}