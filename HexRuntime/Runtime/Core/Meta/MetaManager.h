#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Object\StringObject.h"
#include "MDRecords.h"
#include "MDImporter.h"
#include "MDHeap.h"
#include "MethodDescriptor.h"
#include "FieldDescriptor.h"
#include "TypeDescriptor.h"

#include <unordered_map>
#include <shared_mutex>

namespace RTM
{
	struct AssemblyContext
	{
		RTME::MDImporter* Importer;
		RTME::MDPrivateHeap* Heap;
		RTME::AssemblyRefMD* AssemblyRefs = nullptr;
		RTME::TypeRefMD* TypeRefs = nullptr;
		RTME::MemberRefMD* MemberRefs = nullptr;
		RTME::AssemblyHeaderMD Header;
	public:
		AssemblyContext(RTME::MDPrivateHeap* heap, RTME::MDImporter* importer) :
			Heap(heap),
			Importer(importer) {}
	};
	/// <summary>
	/// Manager that responsible for metadata management
	/// </summary>
	class MetaManager
	{
		std::shared_mutex mContextLock;
		std::unordered_map<UInt32, AssemblyContext*> mContexts;
	private:
		AssemblyContext* TryQueryContextLocked(RTME::AssemblyRefMD* reference);
		AssemblyContext* TryQueryContext(RTME::AssemblyRefMD* reference);

		TypeDescriptor* TryQueryType(AssemblyContext* context, MDToken typeDefinition);
		TypeDescriptor* TryQueryTypeLocked(AssemblyContext* context, MDToken typeDefinition);
		TypeDescriptor* ResolveType(AssemblyContext* context, MDToken typeDefinition);
		/// <summary>
		/// Unlocked
		/// </summary>
		/// <param name="assembly"></param>
		/// <returns></returns>
		AssemblyContext* LoadAssembly(RTString assembly);
		/// <summary>
		/// Very dangerous operation, can only be used when failing the load
		/// or exiting
		/// </summary>
		/// <param name="context"></param>
		void UnLoadAssembly(AssemblyContext* context);
		
	public:
		void StartUp(RTString assemblyName);
		void ShutDown();
	public:
		AssemblyContext* GetAssemblyFromToken(AssemblyContext* context, MDToken assemblyReference);
		TypeDescriptor* GetTypeFromToken(AssemblyContext* context, MDToken typeReference);
		RTO::StringObject* GetStringFromToken(AssemblyContext* context, MDToken stringToken);
		MethodDescriptor* GetMethodFromToken(AssemblyContext* context, MDToken methodReference);
		FieldDescriptor* GetFieldFromToken(AssemblyContext* context, MDToken fieldReference);
	};

	extern MetaManager* MetaData;
}