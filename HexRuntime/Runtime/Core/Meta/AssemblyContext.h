#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Memory\PrivateHeap.h"
#include "..\..\DynamicTokenTable.h"
#include "TypeIdentity.h"
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
		static constexpr Int8 Basic = 2;
		//Intermediate status for better performance

		static constexpr Int8 LayoutDone = 3;
		static constexpr Int8 MethodTableDone = 4;
		static constexpr Int8 InterfaceTableDone = 5;
		/// <summary>
		/// Used for cyclical type loading
		/// </summary>
		static constexpr Int8 Almost = 6;
		static constexpr Int8 Done = 7;
	};

	struct TypeDefEntry
	{
		std::mutex WaiterLock;
		std::condition_variable Waiter;
		std::atomic<TypeDescriptor*> Type = nullptr;
	};


	using TypeEntryTableT = DynamicTokenTable<TypeIdentity, TypeDefEntry, TypeIdentityHash, TypeIdentityEqual>;

	struct AssemblyContext
	{
		RTME::MDImporter* Importer;
		RTMM::PrivateHeap* Heap;
		RTME::AssemblyRefMD* AssemblyRefs = nullptr;
		RTME::TypeRefMD* TypeRefs = nullptr;
		RTME::MemberRefMD* MemberRefs = nullptr;
		RTME::GenericInstantiationMD* GenericDef = nullptr;
		RTME::GenericParamterMD* GenerciParamDef = nullptr;
		RTME::AssemblyHeaderMD Header = {};
		TypeEntryTableT Entries;
	public:
		AssemblyContext(RTMM::PrivateHeap* heap, RTME::MDImporter* importer);
	};
}