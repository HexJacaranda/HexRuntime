#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\Guid.h"
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
#include "TypeStructuredName.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <shared_mutex>
#include <memory>
#include <string_view>

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
		std::unordered_map<Guid, AssemblyContext*, GuidHash, GuidEqual> mContexts;
		std::unordered_map<std::wstring_view, AssemblyContext*> mName2Context;
		std::atomic<AssemblyContext*> mCoreAssembly = nullptr;
		std::atomic<AssemblyContext*> mGenericAssembly = nullptr;

		static Guid CoreLibraryGuid;
		static Guid GenericLibraryGuid;
	private:
		AssemblyContext* TryQueryContextLocked(RTME::AssemblyRefMD* reference);
		AssemblyContext* TryQueryContext(RTME::AssemblyRefMD* reference);

		/*
		* Load the type, called internally. The waitStatus actually will only work in case where
		* the type you wish to load it's being loaded by other thread.
		*/

		TypeDescriptor* GetTypeFromReferenceTokenInternal(
			AssemblyContext* defineContext,
			MDToken reference,
			INJECT(LOADING_CONTEXT, INSTANTIATION_CONTEXT),
			Int8 waitStatus = -1,
			bool allowWait = false
		);

		TypeDescriptor* GetTypeFromDefinitionTokenInternal(
			AssemblyContext* defineContext,
			RTME::MDRecordKinds definitionKind,
			MDToken definition,
			INJECT(LOADING_CONTEXT, INSTANTIATION_CONTEXT),
			Int8 waitStatus = -1,
			bool allowWait = false
		);

		TypeDescriptor* InstantiateWith(
			AssemblyContext* defineContext,
			Type* canonical,
			std::vector<Type*> const& args);

		struct MetaOptions
		{
			ETY = UInt8;
			VAL Canonical = 0b00;
			VAL Generic = 0b01;
			VAL ShareMD = 0b10;
		};

		void ResolveOrInstantiateType(
			AssemblyContext* originContext,
			TypeIdentity const& identity,
			TypeDescriptor* type,
			UInt8 metaOption,
			MDToken definitionToken,
			INJECT(LOADING_CONTEXT, INSTANTIATION_CONTEXT)
		);

		TypeDescriptor* GetTypeFromFQN(std::shared_ptr<TypeStructuredName> const& tsn);

		FieldTable* GenerateFieldTable(AssemblyContext* allocatingContext, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT, INSTANTIATION_CONTEXT));

		FieldsLayout* GenerateLayout(FieldTable* table, AssemblyContext* allocatingContext, AssemblyContext* context, UInt8 coreType);

		MethodTable* GenerateMethodTable(Type* current, AssemblyContext* allocatingContext, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT, INSTANTIATION_CONTEXT));

		InterfaceDispatchTable* GenerateInterfaceTable(Type* current, AssemblyContext* allocatingContext, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT, INSTANTIATION_CONTEXT));

		static bool HasVisitedType(VisitSet const& visited, TypeIdentity const& identity);
		/// <summary>
		/// Dynamically create assembly for generic use
		/// </summary>
		/// <returns></returns>
		AssemblyContext* NewDynamicAssembly();
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

		AssemblyContext* GetGenericAssembly();
		AssemblyContext* GetCoreAssembly();
		
		std::shared_ptr<TypeStructuredName> GetTSN(TypeIdentity const& identity);
	public:
		MetaManager();
		AssemblyContext* StartUp(std::wstring_view const& name);
		void ShutDown();
		~MetaManager();
	public:
		AssemblyContext* GetAssemblyFromToken(AssemblyContext* context, MDToken assemblyReference);
		TypeDescriptor* GetTypeFromToken(AssemblyContext* context, MDToken typeReference);
		TypeDescriptor* GetTypeFromDefinitionToken(AssemblyContext* context, MDToken typeDefinition);
		RTO::StringObject* GetStringFromToken(AssemblyContext* context, MDToken stringToken);
		MethodDescriptor* GetMethodFromToken(AssemblyContext* context, MDToken methodReference);
		FieldDescriptor* GetFieldFromToken(AssemblyContext* context, MDToken fieldReference);

		TypeDescriptor* InstantiateRefType(TypeDescriptor* origin);
		TypeDescriptor* InstantiateArrayType(TypeDescriptor* origin);
		TypeDescriptor* Instantiate(TypeDescriptor* canonical, std::vector<TypeDescriptor*> const& args);
		
		TypeDescriptor* GetIntrinsicTypeFromCoreType(UInt8 coreType);
		TypeDescriptor* GetTypeFromFQN(std::wstring_view const& fqn);
		AssemblyContext* GetAssemblyFromName(std::wstring_view const& name);
	};

	static std::wstring_view ToStringView(RTO::StringObject* stringObject)
	{
		if (stringObject == nullptr) return {};
		return { stringObject->GetContent(), (std::size_t)stringObject->GetCount() };
	}

	extern MetaManager* MetaData;
}