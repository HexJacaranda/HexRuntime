#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Object\StringObject.h"
#include "MDRecords.h"
#include "MDImporter.h"
#include "MethodDescriptor.h"
#include "FieldDescriptor.h"
#include "TypeDescriptor.h"
#include "AssemblyContext.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <shared_mutex>

namespace RTM
{
	using VisitSet = std::unordered_set<MDToken>;

	using WaitingList = std::vector<TypeDescriptor*>;

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

		TypeDescriptor* GetTypeFromTokenInternal(
			AssemblyContext* context, 
			MDToken typeReference, 
			VisitSet& visited,
			WaitingList& waitingList,
			WaitingList& externalWaitingList,
			bool& shouldWait,
			bool allowWait = false);

		TypeDescriptor* ResolveType(
			AssemblyContext* context,
			MDToken typeDefinition,
			VisitSet& visited,
			WaitingList& waitingList,
			WaitingList& externalWaitingList,
			bool& shouldWait);

		void GenerateLayout(FieldTable* table);

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
		AssemblyContext* StartUp(RTString assemblyName);
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