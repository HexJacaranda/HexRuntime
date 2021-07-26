#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Object\StringObject.h"
#include "..\..\Logging.h"
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
#include <memory>

namespace RTM
{
	struct TypeIdentity
	{
		RTME::GUID GUID;
		MDToken TypeDef;

		UInt32 GetHashCode()const
		{
			return RTME::ComputeHashCode(*this);
		}
	};

	struct TypeIdentityHash
	{
		inline UInt32 operator()(TypeIdentity const& identity)const 
		{
			return identity.GetHashCode();
		}
	};

	struct TypeIdentityEqual
	{
		inline bool operator()(TypeIdentity const& left, TypeIdentity const& right)const
		{
			return std::memcmp(&left, &right, sizeof(TypeIdentity)) == 0;
		}
	};

	using VisitSet = std::unordered_set<TypeIdentity, TypeIdentityHash, TypeIdentityEqual>;

	using WaitingList = std::vector<TypeDescriptor*>;

	/// <summary>
	/// Manager that responsible for metadata management
	/// </summary>
	class MetaManager
	{
		INJECT_LOGGER(MetaManager);
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

		void GenerateLayout(FieldTable* table, AssemblyContext* context);

		static bool HasVisitedType(VisitSet const& visited, TypeIdentity const& identity);
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