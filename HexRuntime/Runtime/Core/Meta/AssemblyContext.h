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

namespace RTO
{
	class StringObject;
}

namespace RTM
{
	struct TypeDefEntry
	{
		std::mutex WaiterLock;
		std::condition_variable Waiter;
		std::atomic<TypeDescriptor*> Type = nullptr;
	};

	using TypeEntryTableT = DynamicMetaTable<std::wstring_view, TypeDefEntry>;

	static std::pmr::synchronized_pool_resource* GetDefaultResource();

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
		RTO::StringObject* AssemblyName = nullptr;
		TypeEntryTableT Entries;
	public:
		AssemblyContext(RTMM::PrivateHeap* heap, RTME::MDImporter* importer);
	};
}