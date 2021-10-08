#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Object\StringObject.h"
#include "..\..\Logging.h"
#include "MDRecords.h"
#include "MDImporter.h"
#include "FieldDescriptor.h"
#include "TypeDescriptor.h"
#include "TypeIdentity.h"
#include "AssemblyContext.h"
#include "MethodTable.h"
#include "InterfaceDispatchTable.h"
#include "InstantiationMap.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <shared_mutex>
#include <memory>

#define INJECT(...) __VA_ARGS__

//For consistency and convenience
#define LOADING_CONTEXT RTM::VisitSet& visited,\
		RTM::WaitingList& waitingList,\
		RTM::WaitingList& externalWaitingList, \
		RTM::TypeIdentityReclaimList& identityReclaimList, \
		bool& shouldWait

#define IMPORT_CONTEXT RTM::AssemblyContext* context,\
		RTME::TypeMD* meta,\
		RTME::IImportSession* session,\
		RTME::MDImporter* importer

#define INSTANTIATION_CONTEXT InstantiationMap& genericMap

#define USE_IMPORT_CONTEXT context, meta, session, importer

#define USE_LOADING_CONTEXT visited, waitingList, externalWaitingList, identityReclaimList, shouldWait

#define USE_INSTANTIATION_CONTEXT genericMap

namespace RTM
{
	using VisitSet = std::unordered_set<TypeIdentity, TypeIdentityHash, TypeIdentityEqual>;
	using WaitingList = std::vector<TypeDescriptor*>;
	using TypeIdentityReclaimList = std::vector<std::pair<RTMM::PrivateHeap*, TypeIdentity>>;

	/// <summary>
	/// Manager that responsible for metadata management
	/// </summary>
	class MetaManager
	{
		USE_LOGGER(MetaManager);
		std::shared_mutex mContextLock;
		std::unordered_map<RTME::GUID, AssemblyContext*, RTME::GuidHash, RTME::GuidEqual> mContexts;
	private:
		AssemblyContext* TryQueryContextLocked(RTME::AssemblyRefMD* reference);
		AssemblyContext* TryQueryContext(RTME::AssemblyRefMD* reference);

		/*
		* Load the type, called internally. The waitStatus actually will only work in case where
		* the type you wish to load it's being loaded by other thread.
		*/

		TypeDescriptor* GetTypeFromTokenInternal(
			AssemblyContext* context, 
			MDToken typeReference, 
			INJECT(LOADING_CONTEXT, INSTANTIATION_CONTEXT),
			Int8 waitStatus = -1,
			bool allowWait = false);

		TypeDescriptor* GetTypeFromDefTokenInternal(
			AssemblyContext* context,
			MDToken typeDefinition,
			INJECT(LOADING_CONTEXT, INSTANTIATION_CONTEXT),
			Int8 waitStatus = -1,
			bool allowWait = false
		);

		void ResolveType(
			AssemblyContext* context,
			TypeDescriptor* type,
			MDToken typeDefinition,
			INJECT(LOADING_CONTEXT, INSTANTIATION_CONTEXT));

		void InstantiateType(
			AssemblyContext* context,
			TypeDescriptor* canonical,
			TypeDescriptor* type,
			MDToken typeDefinition,
			TypeIdentity const& typeIdentity,
			INJECT(LOADING_CONTEXT, INSTANTIATION_CONTEXT));

		FieldTable* GenerateFieldTable(INJECT(IMPORT_CONTEXT, LOADING_CONTEXT, INSTANTIATION_CONTEXT));

		void GenerateLayout(FieldTable* table, AssemblyContext* context);

		MethodTable* GenerateMethodTable(Type* current, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT, INSTANTIATION_CONTEXT));

		InterfaceDispatchTable* GenerateInterfaceTable(Type* current, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT, INSTANTIATION_CONTEXT));

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
		/// <summary>
		/// Allocate fixed managed string from wstring_view
		/// </summary>
		/// <param name="view"></param>
		/// <returns></returns>
		RTO::StringObject* GetStringFromView(AssemblyContext* context, std::wstring_view view);
	public:
		MetaManager();
		AssemblyContext* StartUp(RTString assemblyName);
		void ShutDown();
		~MetaManager();
	public:
		AssemblyContext* GetAssemblyFromToken(AssemblyContext* context, MDToken assemblyReference);
		TypeDescriptor* GetTypeFromToken(AssemblyContext* context, MDToken typeReference);
		TypeDescriptor* GetTypeFromDefinitionToken(AssemblyContext* context, MDToken typeDefinition);
		RTO::StringObject* GetStringFromToken(AssemblyContext* context, MDToken stringToken);
		MethodDescriptor* GetMethodFromToken(AssemblyContext* context, MDToken methodReference);
		FieldDescriptor* GetFieldFromToken(AssemblyContext* context, MDToken fieldReference);

		TypeDescriptor* GetIntrinsicTypeFromCoreType(UInt8 coreType);
	};

	extern MetaManager* MetaData;
}