#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Object\StringObject.h"
#include "..\..\Logging.h"
#include "MDRecords.h"
#include "MDImporter.h"
#include "FieldDescriptor.h"
#include "TypeDescriptor.h"
#include "AssemblyContext.h"
#include "MethodTable.h"
#include "InterfaceDispatchTable.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <shared_mutex>
#include <memory>

#define INJECT(...) __VA_ARGS__

//For consistency and convenience
#define LOADING_CONTEXT VisitSet& visited,\
		WaitingList& waitingList,\
		WaitingList& externalWaitingList, \
		bool& shouldWait

#define IMPORT_CONTEXT AssemblyContext* context,\
		RTME::TypeMD* meta,\
		RTME::IImportSession* session,\
		RTME::MDImporter* importer

#define USE_IMPORT_CONTEXT context, meta, session, importer

#define USE_LOADING_CONTEXT visited, waitingList, externalWaitingList, shouldWait

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
		USE_LOGGER(MetaManager);
		std::shared_mutex mContextLock;
		std::unordered_map<UInt32, AssemblyContext*> mContexts;
	private:
		AssemblyContext* TryQueryContextLocked(RTME::AssemblyRefMD* reference);
		AssemblyContext* TryQueryContext(RTME::AssemblyRefMD* reference);

		static TypeDefEntry& GetTypeEntryFrom(Type* target);

		TypeDescriptor* TryQueryType(AssemblyContext* context, MDToken typeDefinition);

		/*
		* Load the type, called internally. The waitStatus actually will only work in case where
		* the type you wish to load it's being loaded by other thread.
		*/

		TypeDescriptor* GetTypeFromTokenInternal(
			AssemblyContext* context, 
			MDToken typeReference, 
			INJECT(LOADING_CONTEXT),
			Int8 waitStatus = -1,
			bool allowWait = false);

		void ResolveType(
			AssemblyContext* context,
			TypeDescriptor* type,
			MDToken typeDefinition,
			INJECT(LOADING_CONTEXT));

		FieldTable* GenerateFieldTable(INJECT(IMPORT_CONTEXT, LOADING_CONTEXT));

		void GenerateLayout(FieldTable* table, AssemblyContext* context);

		MethodTable* GenerateMethodTable(Type* current, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT));

		InterfaceDispatchTable* GenerateInterfaceTable(Type* current, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT));

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
		MetaManager();
		AssemblyContext* StartUp(RTString assemblyName);
		void ShutDown();
	public:
		AssemblyContext* GetAssemblyFromToken(AssemblyContext* context, MDToken assemblyReference);
		TypeDescriptor* GetTypeFromToken(AssemblyContext* context, MDToken typeReference);
		RTO::StringObject* GetStringFromToken(AssemblyContext* context, MDToken stringToken);
		MethodDescriptor* GetMethodFromToken(AssemblyContext* context, MDToken methodReference);
		FieldDescriptor* GetFieldFromToken(AssemblyContext* context, MDToken fieldReference);

		TypeDescriptor* GetIntrinsicTypeFromCoreType(UInt8 coreType);
	};

	extern MetaManager* MetaData;
}