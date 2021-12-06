#include "MetaManager.h"
#include "TypeStructuredName.h"
#include "..\Exception\RuntimeException.h"
#include "..\Memory\PrivateHeap.h"
#include "..\..\LoggingConfiguration.h"
#include "..\..\Logging.h"
#include "CoreTypes.h"
#include <memory>
#include <spdlog/async_logger.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/details/thread_pool.h>

namespace RT
{
	SPECIFIC_LOGGER_FOR(MetaManager)
	{
		GET_LOGGER_METHOD
		{
			{
				auto logger = spdlog::get("meta-manager");
				if (logger != nullptr)
					return std::static_pointer_cast<spdlog::async_logger>(logger);
			}

			auto logger = std::make_shared<spdlog::async_logger>(
				"meta-manager",
				sinks.begin(),
				sinks.end(),
				threadPool,
				spdlog::async_overflow_policy::block);

			logger->set_level(spdlog::level::debug);
			logger->set_pattern("[%n(%t)][%l]:%v");
			spdlog::register_logger(logger);
			return logger;
		}
	};
}

namespace RTM
{
	MetaManager* MetaData = nullptr;
}

RTM::AssemblyContext* RTM::MetaManager::TryQueryContextLocked(RTME::AssemblyRefMD* reference)
{
	std::shared_lock lock{ mContextLock };
	return TryQueryContext(reference);
}

RTM::AssemblyContext* RTM::MetaManager::TryQueryContext(RTME::AssemblyRefMD* reference)
{
	auto target = mContexts.find(reference->GUID);
	if (target == mContexts.end())
		return nullptr;
	return target->second;
}

RTM::TypeDescriptor* RTM::MetaManager::GetTypeFromTokenInternal(
	AssemblyContext* context,
	MDToken typeReference,
	INJECT(LOADING_CONTEXT, INSTANTIATION_CONTEXT),
	Int8 waitStatus,
	bool allowWaitOnEntry)
{
	auto&& typeRef = context->TypeRefs[typeReference];
	auto typeDefAssembly = GetAssemblyFromToken(context, typeRef.AssemblyToken);

	//Wait method
	auto waitAndGetType = [&](TypeDefEntry& entry, TypeDescriptor* type) {
		//If type has already been loaded or there is no requirement
		if (waitStatus == -1 || 
			type->Status.load(std::memory_order_acquire) == TypeStatus::Done)
			return type;

		if (allowWaitOnEntry)
		{
			std::unique_lock lock{ entry.WaiterLock };
			/*
			* Currently it will only be notified when the type is fully loaded.
			* Not sure if we should add more notificiations in ResolveType and 
			* InstantiateType
			*/
			entry.Waiter.wait(lock,
				[&]() { return type->Status.load(std::memory_order::acquire) >= waitStatus; });
			return type;
		}
		else
		{
			//Append to external waiting list (it's a type loaded by another thread) 
			//and mark that our whole loading chain need too.
			shouldWait = true;
			externalWaitingList.push_back(type);

			//Need to wait until specific intermediate state is reached
			while (type->Status.load(std::memory_order_acquire) < waitStatus)
				std::this_thread::yield();

			return type;
		}
	};

	if (typeRef.DefKind == RTME::MDRecordKinds::GenericInstantiationDef)
	{
		/*
		* Always allocate meta memory from the assembly that defines
		* the canonical type, this is out of a further design: generic
		* instantiation sharing
		*/

		//Generic instantiation
		auto&& genericInstantiation = context->GenericDef[typeRef.GenericInstantiationDefToken];
		//Firstly make sure that we get the right type identity
		auto canonicalType = GetTypeFromTokenInternal(
			context,
			genericInstantiation.CanonicalTypeRefToken,
			USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
		//Construct type identity
		TypeIdentity identity{ canonicalType };
		identity.ArgumentCount = genericInstantiation.TypeParameterCount;
		if (identity.ArgumentCount == 1)
		{
			identity.SingleArgument = GetTypeFromTokenInternal(
				context,
				genericInstantiation.TypeParameterTokens[0],
				USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
		}
		else
		{
			identity.Arguments = new (typeDefAssembly->Heap) TypeDescriptor * [identity.ArgumentCount];
			for (Int32 i = 0; i < genericInstantiation.TypeParameterCount; ++i)
			{
				identity.Arguments[i] = GetTypeFromTokenInternal(
					context,
					genericInstantiation.TypeParameterTokens[i],
					USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
			}
		}

		/*
			* The complexity of identity will get higher when we decide
			* to implement generics with shared instantiation, and
			* unavoidably introduce load limit of type for recursive
			* and generative pattern of generics
		*/

		auto entry = new (typeDefAssembly->Heap) TypeDefEntry();
		auto instantiationType = new (typeDefAssembly->Heap) TypeDescriptor();
		entry->Type.store(instantiationType, std::memory_order_release);

		MDToken newDefinitionToken = NullToken;
		TypeDefEntry* finalValue = nullptr;
		if (!typeDefAssembly->Entries.DefineGenericInstantiation(identity, entry, finalValue, newDefinitionToken))
		{
			typeDefAssembly->Heap->Return(instantiationType);
			typeDefAssembly->Heap->Return(entry);
			/*
			* Mark type identity (if there is an allocated array inside) to be freed in heap
			* held by AssemblyContext to avoid memory leak when the loading is over
			*/
			if (identity.ArgumentCount > 1)
				identityReclaimList.push_back(std::make_pair(typeDefAssembly->Heap, identity));
		
			return waitAndGetType(*finalValue, finalValue->Type.load(std::memory_order_acquire));
		}
		else
		{
			InstantiationLayerMap layer{};

			if (identity.ArgumentCount == 1)
			{
				layer.insert(std::make_pair(genericInstantiation.TypeParameterTokens[0], identity.SingleArgument));
			}
			else
			{
				for (Int32 i = 0; i < identity.ArgumentCount; ++i)
					layer.insert(std::make_pair(genericInstantiation.TypeParameterTokens[i], identity.Arguments[i]));
			}

			{
				INSTANTIATION_SESSION(layer);
				InstantiateType(typeDefAssembly, canonicalType, instantiationType, newDefinitionToken, identity, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
				//Notify waiting threads after instantiation
				finalValue->Waiter.notify_all();
			}		
		}
	}
	else if (typeRef.DefKind == RTME::MDRecordKinds::GenericParameter)
	{
		//Return the type in instantiation map if possible
		TypeDescriptor* outType = nullptr;
		genericMap.TryGet(typeReference, outType);
		return outType;
	}

	//Canonical type loading
	return GetTypeFromDefTokenInternal(
		typeDefAssembly,
		typeRef.TypeDefToken,
		USE_LOADING_CONTEXT,
		USE_INSTANTIATION_CONTEXT,
		waitStatus,
		allowWaitOnEntry);
}

RTM::TypeDescriptor* RTM::MetaManager::GetTypeFromDefTokenInternal(
	AssemblyContext* context,
	MDToken typeDefinition, 
	INJECT(LOADING_CONTEXT, INSTANTIATION_CONTEXT), 
	Int8 waitStatus, 
	bool allowWait)
{
	//Wait method
	auto waitAndGetType = [&](TypeDefEntry& entry, TypeDescriptor* type) {
		//If type has already been loaded or there is no requirement
		if (waitStatus == -1 ||
			type->Status.load(std::memory_order_acquire) == TypeStatus::Done)
			return type;

		if (allowWait)
		{
			std::unique_lock lock{ entry.WaiterLock };
			/*
			* Currently it will only be notified when the type is fully loaded.
			* Not sure if we should add more notificiations in ResolveType and
			* InstantiateType
			*/
			entry.Waiter.wait(lock,
				[&]() { return type->Status.load(std::memory_order::acquire) >= waitStatus; });
			return type;
		}
		else
		{
			//Append to external waiting list (it's a type loaded by another thread) 
			//and mark that our whole loading chain need too.
			shouldWait = true;
			externalWaitingList.push_back(type);

			//Need to wait until specific intermediate state is reached
			while (type->Status.load(std::memory_order_acquire) < waitStatus)
				std::this_thread::yield();

			return type;
		}
	};

	//Canonical type loading

	auto&& entry = context->Entries[typeDefinition];
	auto type = entry.Type.load(std::memory_order::acquire);

	if (type == nullptr)
	{
		//Try to content for its resolve

		auto newTypeDescriptor = new (context->Heap) TypeDescriptor();
		if (entry.Type.compare_exchange_strong(
			type,
			newTypeDescriptor,
			std::memory_order_release,
			std::memory_order_relaxed))
		{
			//Won this contention
			newTypeDescriptor->Status.store(TypeStatus::Processing, std::memory_order_release);

			bool nextShouldWait = false;
			ResolveType(context, newTypeDescriptor, typeDefinition, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);

			if (nextShouldWait)
			{
				//Append to waiting list since it's a type loaded by our thread
				shouldWait = true;
				waitingList.push_back(type);
				newTypeDescriptor->Status.store(TypeStatus::Almost, std::memory_order::release);
			}
			else
				newTypeDescriptor->Status.store(TypeStatus::Done, std::memory_order::release);

			if (allowWait)
			{
				//Already root call, we need to promote type status to Done level in waiting list
				for (auto&& each : waitingList)
					each->Status.store(TypeStatus::Done, std::memory_order::release);

				//Then wait types loaded by other threads
				for (auto&& each : externalWaitingList)
				{
					auto&& pendingEntry = each->GetAssembly()->Entries[each->GetToken()];
					std::unique_lock lock{ pendingEntry.WaiterLock };
					pendingEntry.Waiter.wait(lock,
						[&]() { return each->Status.load(std::memory_order::acquire) == TypeStatus::Done; });
				}

				//Wake up all waiting threads
				entry.Waiter.notify_all();

				//Clean up type identity with allocated array to avoid memory leak
				for (auto&& [heap, identity] : identityReclaimList)
					heap->Free(identity.Arguments, sizeof(void*) * identity.ArgumentCount);
			}
			else
			{
				//We must have not visited this type: if we have visited, either it has been loaded by us
				//or it has been or is being loaded by other thread
				//For both, it should always be a valid pointer	
				visited.insert(TypeIdentity{ type });
			}

			//Return directly
			return newTypeDescriptor;
		}
		else
		{
			//Clean resource
			context->Heap->Return(newTypeDescriptor);
			visited.insert(TypeIdentity{ type });
		}
	}
	else
	{
		TypeIdentity identity{ type };
		//Decide whether we have visited
		if (!HasVisitedType(visited, identity))
			visited.insert(identity);
		//Its loaded by other thread. Go to wait case
	}

	return waitAndGetType(entry, type);
}

RTM::AssemblyContext* RTM::MetaManager::LoadAssembly(RTString assembly)
{
	auto heap = new RTMM::PrivateHeap();
	auto importer = new (heap) RTME::MDImporter(assembly, heap, RTME::ImportOption::Default);
	auto context = new (heap) AssemblyContext(heap, importer);

	auto startUp = importer->UseSession(
		[&](auto session)
		{
			IF_FAIL_RET(importer->ImportAssemblyHeader(session, &context->Header));
			IF_FAIL_RET(importer->ImportAssemblyRefTable(session, context->AssemblyRefs));
			IF_FAIL_RET(importer->ImportTypeRefTable(session, context->TypeRefs));
			IF_FAIL_RET(importer->ImportMemberRefTable(session, context->MemberRefs));
			return true;
		});

	if (!startUp)
	{
		//Clean up assembly
		UnLoadAssembly(context);
		THROW("Assembly loading failed.");
	}
	
	//Prepare type def entries
	auto&& typeDefIndex = importer->GetIndexTable()[(Int32)RTME::MDRecordKinds::TypeDef];	
	context->Entries = std::move(TypeEntryTableT(new (heap) TypeDefEntry[typeDefIndex.Count], typeDefIndex.Count));

	{
		//Prepare fully qualified name mapping
		RTME::TypeMD typeMD{};
		importer->UseSession(
			[&](auto session)
			{
				for (MDToken i = 0; i < typeDefIndex.Count; ++i)
				{
					IF_FAIL_RET(importer->ImportType(session, i, &typeMD));
					auto fqn = GetStringFromToken(context, typeMD.FullyQualifiedNameToken);
					context->Entries.DefineMetaMap({ fqn->GetContent(), (std::size_t)fqn->GetCount() }, i);
				}
				return true;
			});
	}

	//Prepare generic instantiaion def
	auto&& genericDefIndex = importer->GetIndexTable()[(Int32)RTME::MDRecordKinds::GenericInstantiationDef];                                                   
	context->GenericDef = new (heap) RTME::GenericInstantiationMD[genericDefIndex.Count];

	importer->UseSession(
		[&](auto session)
		{
			for (MDToken i = 0; i < genericDefIndex.Count; ++i)
			{
				IF_FAIL_RET(importer->ImportGenericInstantiation(session, i, &context->GenericDef[i]));
			}
			return true;
		});
	//Prepare generic parameter def
	auto&& genericParamIndex = importer->GetIndexTable()[(Int32)RTME::MDRecordKinds::GenericParameter];
	context->GenerciParamDef = new (heap) RTME::GenericParamterMD[genericParamIndex.Count];

	importer->UseSession(
		[&](auto session)
		{
			for (MDToken i = 0; i < genericParamIndex.Count; ++i)
			{
				IF_FAIL_RET(importer->ImportGenericParameter(session, i, &context->GenerciParamDef[i]));
			}
			return true;
		});

	mContexts.insert({ context->Header.GUID, context });
	return context;
}

void RTM::MetaManager::UnLoadAssembly(AssemblyContext* context)
{
	//Unload heap content
	if (context->Heap != nullptr)
	{
		context->Heap->Return(context->Importer);
		delete context->Heap;
		context->Heap = nullptr;
	}
}

RTO::StringObject* RTM::MetaManager::GetStringFromView(AssemblyContext* context, std::wstring_view view)
{
	auto string = (RTO::StringObject*)new (context->Heap) UInt8[sizeof(RTO::StringObject) + (view.size() + 1) * sizeof(UInt16)];
	new (string) RTO::StringObject(view.size(), view.data());
	return string;
}

void RTM::MetaManager::ResolveType(
	AssemblyContext* context,
	TypeDescriptor* type,
	MDToken typeDefinition,
	INJECT(LOADING_CONTEXT, INSTANTIATION_CONTEXT))
{
	auto&& importer = context->Importer;
	auto meta = new (context->Heap) RTME::TypeMD();

	//For performance, use a session across the whole meta data resolving
	auto session = importer->NewSession();

	if (!importer->ImportType(session, typeDefinition, meta))
	{
		importer->ReturnSession(session);
		THROW("Error occurred when importing type meta data");
	}

	type->mColdMD = meta;
	type->mContext = context;
	type->mSelf = typeDefinition;
	type->mTypeName = GetStringFromToken(context, meta->NameToken);
	type->mFullQualifiedName = GetStringFromToken(context, meta->FullyQualifiedNameToken);

	LOG_DEBUG("{} [{:#010x}] Resolution started",
		type->GetFullQualifiedName()->GetContent(),
		type->GetToken());

	//For pure canonical type, we will only load basic metadata.
	if (meta->IsGeneric())
		return;
		
	//Load parent
	if (meta->ParentTypeRefToken != NullToken)
	{
		LOG_DEBUG("{} [{:#010x}] Loading parent", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mParent = GetTypeFromTokenInternal(context, meta->ParentTypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
	}

	//Load implemented interfaces
	if (meta->InterfaceCount > 0)
	{
		LOG_DEBUG("{} [{:#010x}] Loading interfaces", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		if (meta->InterfaceCount == 1)
		{
			type->mInterfaceInline = GetTypeFromTokenInternal(context, meta->InterfaceTokens[0], USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
		}
		else
		{
			type->mInterfaces = new (context->Heap) TypeDescriptor * [meta->InterfaceCount];
			for (Int32 i = 0; i < meta->InterfaceCount; ++i)
				type->mInterfaces[i] = GetTypeFromTokenInternal(context, meta->InterfaceTokens[i], USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
		}
	}

	//Load canonical
	if (meta->CanonicalTypeRefToken != NullToken)
	{
		LOG_DEBUG("{} [{:#010x}] Loading canonical type", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mCanonical = GetTypeFromTokenInternal(context, meta->CanonicalTypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
	}

	//Load enclosing
	if (meta->EnclosingTypeRefToken != NullToken)
	{
		LOG_DEBUG("{} [{:#010x}] Loading enclosing type", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mEnclosing = GetTypeFromTokenInternal(context, meta->EnclosingTypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
	}
	type->Status.store(TypeStatus::Basic, std::memory_order_release);

	if (!CoreTypes::IsPrimitive(meta->CoreType))
	{
		//Loading field table
		LOG_DEBUG("{} [{:#010x}] Loading fields", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mFieldTable = GenerateFieldTable(USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
	}

	//Update to layout done
	type->Status.store(TypeStatus::LayoutDone, std::memory_order_release);

	//Loading method table
	LOG_DEBUG("{} [{:#010x}] Loading methods", type->GetFullQualifiedName()->GetContent(), type->GetToken());
	type->mMethTable = GenerateMethodTable(type, USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);

	//Update to method table done
	type->Status.store(TypeStatus::MethodTableDone, std::memory_order_release);

	//Loading(generating) interface table
	LOG_DEBUG("{} [{:#010x}] Loading interface table", type->GetFullQualifiedName()->GetContent(), type->GetToken());
	type->mInterfaceTable = GenerateInterfaceTable(type, USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);

	type->Status.store(TypeStatus::InterfaceTableDone, std::memory_order_release);

	importer->ReturnSession(session);

	LOG_DEBUG("{} [{:#010x}] Resolution done",
		type->GetFullQualifiedName()->GetContent(),
		type->GetToken());
}

void RTM::MetaManager::InstantiateType(
	AssemblyContext* context, 
	TypeDescriptor* canonical, 
	TypeDescriptor* type, 
	MDToken typeDefinition,
	TypeIdentity const& typeIdentity,
	INJECT(LOADING_CONTEXT, INSTANTIATION_CONTEXT))
{
	auto meta = canonical->mColdMD;
	//A question here, how should we handle Reflection?
	type->mColdMD = canonical->mColdMD;
	type->mContext = context;
	type->mSelf = typeDefinition;

	//Instantiation type should have instantiated name
	{
		auto fqn = GetStringFromToken(context, meta->FullyQualifiedNameToken);
		std::wstring_view fqnView{ fqn->GetContent(),  (std::size_t)fqn->GetCount() };
		auto canonicalTSN = TypeNameParser(fqnView).Parse(true);

		//Construct instantiation array
		std::vector<std::wstring_view> instantiationNameList(typeIdentity.ArgumentCount);

		auto appendFor = [&](Int32 index, TypeDescriptor* type) {
			auto name = type->GetFullQualifiedName();
			//Direct reference
			instantiationNameList[index] = std::wstring_view{ name->GetContent(), (std::size_t)name->GetCount() };
		};

		if (typeIdentity.ArgumentCount > 1)
		{
			for (Int32 i = 0; i < typeIdentity.ArgumentCount; ++i)
				appendFor(i, typeIdentity.Arguments[i]);
		}
		else
		{
			appendFor(0, typeIdentity.SingleArgument);
		}

		//Construct instantiated type name
		auto instantiatedTSN = canonicalTSN->InstantiateWith(instantiationNameList);
		type->mTypeName = GetStringFromView(context, instantiatedTSN->GetShortTypeName());
		type->mFullQualifiedName = GetStringFromView(context, instantiatedTSN->GetFullyQualifiedName());
	}

	//Load parent
	if (meta->ParentTypeRefToken != NullToken)
	{
		LOG_DEBUG("{} [{:#010x}] Loading parent", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mParent = GetTypeFromTokenInternal(context, meta->ParentTypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
	}

	//Load implemented interfaces
	if (meta->InterfaceCount > 0)
	{
		LOG_DEBUG("{} [{:#010x}] Loading interfaces", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		if (meta->InterfaceCount == 1)
		{
			type->mInterfaceInline = GetTypeFromTokenInternal(context, meta->InterfaceTokens[0], USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
		}
		else
		{
			type->mInterfaces = new (context->Heap) TypeDescriptor * [meta->InterfaceCount];
			for (Int32 i = 0; i < meta->InterfaceCount; ++i)
				type->mInterfaces[i] = GetTypeFromTokenInternal(context, meta->InterfaceTokens[i], USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
		}
	}

	//Load canonical
	if (meta->CanonicalTypeRefToken != NullToken)
	{
		LOG_DEBUG("{} [{:#010x}] Loading canonical type", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mCanonical = GetTypeFromTokenInternal(context, meta->CanonicalTypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
	}

	//Load enclosing
	if (meta->EnclosingTypeRefToken != NullToken)
	{
		LOG_DEBUG("{} [{:#010x}] Loading enclosing type", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mEnclosing = GetTypeFromTokenInternal(context, meta->EnclosingTypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
	}
	type->Status.store(TypeStatus::Basic, std::memory_order_release);

	{
		auto&& importer = context->Importer;
		//For performance, use a session across the whole meta data resolving
		auto session = importer->NewSession();

		//Loading field table
		LOG_DEBUG("{} [{:#010x}] Loading fields", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mFieldTable = GenerateFieldTable(USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);

		//Update to layout done
		type->Status.store(TypeStatus::LayoutDone, std::memory_order_release);

		//Loading method table
		LOG_DEBUG("{} [{:#010x}] Loading methods", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mMethTable = GenerateMethodTable(type, USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);

		//Update to method table done
		type->Status.store(TypeStatus::MethodTableDone, std::memory_order_release);

		//Loading(generating) interface table
		LOG_DEBUG("{} [{:#010x}] Loading interface table", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mInterfaceTable = GenerateInterfaceTable(type, USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);

		type->Status.store(TypeStatus::InterfaceTableDone, std::memory_order_release);

		importer->ReturnSession(session);
	}
}

RTM::FieldTable* RTM::MetaManager::GenerateFieldTable(INJECT(IMPORT_CONTEXT, LOADING_CONTEXT, INSTANTIATION_CONTEXT))
{
	//Load fields
	auto fieldTable = new (context->Heap) FieldTable();

	//Field descriptors
	fieldTable->mFieldCount = meta->FieldCount;
	fieldTable->mFields = new (context->Heap) FieldDescriptor[meta->FieldCount];
	auto fieldMDs = new (context->Heap) RTME::FieldMD[meta->FieldCount];

	for (Int32 i = 0; i < meta->FieldCount; ++i)
	{
		auto fieldMD = &fieldMDs[i];

		if (!importer->ImportField(session, meta->FieldTokens[i], fieldMD))
		{
			importer->ReturnSession(session);
			THROW("Error occurred when importing type meta data");
		}

		auto&& field = fieldTable->mFields[i];
		field.mColdMD = fieldMD;
		field.mName = GetStringFromToken(context, fieldMD->NameToken);
		field.mFieldType = GetTypeFromTokenInternal(context, fieldMD->TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
	}

	//Compute layout
	fieldTable->mLayout = GenerateLayout(fieldTable, context);
	
	return fieldTable;
}

RTM::FieldsLayout* RTM::MetaManager::GenerateLayout(FieldTable* table, AssemblyContext* context)
{
	auto fieldLayout = new (context->Heap) FieldsLayout();
	fieldLayout->Align = sizeof(IntPtr);

	fieldLayout->SlotCount = table->mFieldCount;
	fieldLayout->Slots = new (context->Heap) LayoutSlot[table->mFieldCount];

	auto getFieldSize = [](Type* type) 
	{
		auto coreType = type->GetCoreType();
		Int32 fieldSize = 0;
		if (CoreTypes::IsPrimitive(coreType) ||
			coreType == CoreTypes::InteriorRef ||
			coreType == CoreTypes::Ref)
			fieldSize = CoreTypes::GetCoreTypeSize(coreType);
		else if (coreType == CoreTypes::Struct)
		{
			//Should be very short
			while (type->Status.load(std::memory_order_acquire) < TypeStatus::LayoutDone)
				std::this_thread::yield();
			//Ensure the layout is done
			fieldSize = type->GetSize();
		}
		else
			THROW("Unknown core type");

		return fieldSize;
	};

	Int32 offset = 0;

	for (Int32 i = 0; i < table->mFieldCount; ++i)
	{
		auto&& fieldSlot = table->mFields[i];
		auto&& type = fieldSlot.GetType();

		Int32 leftToBoundary = sizeof(IntPtr) - (offset & (sizeof(IntPtr) - 1));
		Int32 fieldSize = getFieldSize(type);

		if (leftToBoundary < sizeof(IntPtr))
		{
			if (fieldSize <= leftToBoundary)
			{
				//Align small objects inside a big alignment
				if (offset & (fieldSize - 1))
				{
					offset &= ~(fieldSize - 1);
					offset += fieldSize;
				}
			}
			else
			{
				//Align to next boundary
				offset += leftToBoundary;
			}
		}

		fieldLayout->Slots[i].Offset = offset;
		offset += fieldSize;
	}

	fieldLayout->Size = offset;

	Int32 leftToBoundary = sizeof(IntPtr) - (offset & (sizeof(IntPtr) - 1));
	if (leftToBoundary != sizeof(IntPtr))
		fieldLayout->Size += leftToBoundary;

	return fieldLayout;
}

RTM::MethodTable* RTM::MetaManager::GenerateMethodTable(Type* current, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT, INSTANTIATION_CONTEXT))
{
	auto methodTable = new (context->Heap) MethodTable();

	//Dynamically
	std::vector<MethodDescriptor*> overridenMethods;

	auto parent = current->GetParentType();
	MethodTable* parentMT = nullptr;
	ObservableArray<MethodDescriptor*> overridenRegion{ nullptr, 0 };
	if (parent != nullptr)
	{
		//Should be very short, wait for method table to be done.
		while (parent->Status.load(std::memory_order_acquire) < TypeStatus::MethodTableDone)
			std::this_thread::yield();

		parentMT = parent->GetMethodTable();
		if (parentMT != nullptr)
		{
			overridenRegion = parentMT->GetOverridenRegion();
			overridenMethods = std::move(std::vector<MethodDescriptor*>{ overridenRegion.begin(), overridenRegion.end() });
		} 
	}

	methodTable->mMethods = new (context->Heap) MethodDescriptor*[meta->MethodCount];
	auto methodMDs = new (context->Heap) RTME::MethodMD[meta->MethodCount];

	auto generateSignature = [&](RTME::MethodSignatureMD& signatureMD)
	{
		auto arguments = new (context->Heap) MethodArgumentDescriptor[signatureMD.ArgumentCount];
		auto argumentMDs = new (context->Heap) RTME::ArgumentMD[signatureMD.ArgumentCount];
		auto signature = new MethodSignatureDescriptor();

		signature->mColdMD = &signatureMD;
		signature->mReturnType = GetTypeFromTokenInternal(context, signatureMD.ReturnTypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
		signature->mArguments = arguments;

		for (Int32 i = 0; i < signatureMD.ArgumentCount; ++i)
		{
			auto&& argumentMD = argumentMDs[i];
			auto&& argument = arguments[i];
			if (!importer->ImportArgument(session, signatureMD.ArgumentTokens[i], &argumentMD))
			{
				importer->ReturnSession(session);
				THROW("Error occurred when importing type meta data");
			}

			argument.mColdMD = &argumentMD;
			argument.mType = GetTypeFromTokenInternal(context, signatureMD.ReturnTypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
			argument.mManagedName = GetStringFromToken(context, argumentMD.NameToken);
		}
		return signature;
	};

	auto generateLocals = [&](RTME::MethodMD& md)
	{
		auto localCount = md.ILCodeMD.LocalVariableCount;
		auto locals = new (context->Heap) MethodLocalVariableDescriptor[localCount];
		for (Int32 i = 0; i < localCount; ++i)
		{
			auto&& local = locals[i];
			auto&& localMD = md.ILCodeMD.LocalVariables[i];

			local.mColdMD = &localMD;
			local.mType = GetTypeFromTokenInternal(context, localMD.TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
			local.mManagedName = GetStringFromToken(context, localMD.NameToken);
		}

		return locals;
	};

	auto tryOverrideMethodSlot = [&](RTME::MethodMD& md, MethodDescriptor* descriptor) {
		if (md.OverridesMethodRef == NullToken)
		{
			//it's a new virtual slot
			//record the index inside overriden region
			descriptor->mOverrideRedirectIndex = overridenMethods.size();
			overridenMethods.push_back(descriptor);
			return;
		}

		auto&& memberRef = parent->GetAssembly()->MemberRefs[md.OverridesMethodRef];
		auto&& overridenType = GetTypeFromTokenInternal(context, memberRef.TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT, TypeStatus::Basic);
		//Not applicable to interface
		if (overridenType->IsInterface())
			return;

		auto&& targetDescriptor = parentMT->GetMethodBy(memberRef.MemberDefToken);
		//Inside, then override with redirect index
		descriptor->mOverrideRedirectIndex = targetDescriptor->mOverrideRedirectIndex;
		overridenMethods[targetDescriptor->mOverrideRedirectIndex] = descriptor;
	};

	for (Int32 i = 0; i < meta->MethodCount; ++i)
	{
		auto&& methodMD = methodMDs[i];
		
		if (!importer->ImportMethod(session, meta->MethodTokens[i], &methodMD))
		{
			importer->ReturnSession(session);
			THROW("Error occurred when importing type meta data");
		}

		auto&& descriptor = methodTable->mMethods[i];
		descriptor = new (context->Heap) MethodDescriptor();

		descriptor->mColdMD = &methodMD;
		descriptor->mManagedName = GetStringFromToken(context, methodMD.NameToken);
		descriptor->mSignature = generateSignature(methodMD.Signature);
		descriptor->mLocals = generateLocals(methodMD);
		descriptor->mSelf = meta->MethodTokens[i];

		if (methodMD.IsVirtual())
		{
			methodTable->mVirtualMethodCount++;
			tryOverrideMethodSlot(methodMD, descriptor);
		}
		else if (methodMD.IsStatic())
			methodTable->mStaticMethodCount++;
		else
			methodTable->mInstanceMethodCount++;
	}
	if (methodTable->GetCount() > 0)
	{
		//Set base token
		methodTable->mBaseMethodToken = methodTable->mMethods[0]->GetDefToken();
	}

	//Set our overriden region
	if (overridenMethods.size() > 0)
	{
		methodTable->mOverridenRegionCount = overridenMethods.size();
		methodTable->mOverridenRegion = new (context->Heap) MethodDescriptor * [overridenMethods.size()];
		std::memcpy(methodTable->mOverridenRegion, &overridenMethods[0], sizeof(MethodDescriptor*)* overridenMethods.size());
	}

	return methodTable;
}

RTM::InterfaceDispatchTable* RTM::MetaManager::GenerateInterfaceTable(Type* current, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT, INSTANTIATION_CONTEXT))
{
	//No need for interface to build table
	if (current->IsInterface())
		return nullptr;

	auto table = new (context->Heap) InterfaceDispatchTable();
	auto interfaces = current->GetInterfaces();
	auto methodTable = current->GetMethodTable();

	auto findIndexInInterfaces = [&](Type* target) {
		Int32 index = -1;
		for (Int32 i = 0; i < interfaces.Count; ++i)
			if (interfaces[i] == target)
			{
				index = i;
				break;
			}
		return index;
	};
	//For all the methods, we
	//1. try to find the revirtualized one overriden by us and redirect it
	//2. try to arrange normal methods that directly implements the interface

	for (auto&& method : methodTable->GetInstanceMethods())
	{
		auto&& methodMD = method->GetMetadata();
		auto overrideToken = methodMD->OverridesMethodRef;
		if (overrideToken != NullToken)
		{
			//It's not a new slot
			auto&& memberRef = context->MemberRefs[overrideToken];
			//Find the type that defines method to be overriden
			auto type = GetTypeFromTokenInternal(context, memberRef.TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT, TypeStatus::Basic);

			if (!type->IsInterface())
			{
				//Overrides normal type
				//Requires higher level
				type = GetTypeFromTokenInternal(context, memberRef.TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT, TypeStatus::MethodTableDone);
				auto overridenMethod = type->GetMethodTable()->GetMethodBy(overrideToken);

				MDToken revirtualizedMethodRef = overridenMethod->mColdMD->OverridesMethodRef;
				//The root virtual call is a revirtualization for some interface method
				if (revirtualizedMethodRef != NullToken)
				{
					auto revirtualizedMethodMemberRef = type->GetAssembly()->MemberRefs[revirtualizedMethodRef];
					auto revirtualizedMethodOwner = GetTypeFromTokenInternal(type->GetAssembly(), revirtualizedMethodMemberRef.TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT, TypeStatus::MethodTableDone);

					if (!revirtualizedMethodOwner->IsInterface())
						THROW("Bad metadata for overriding method");

					//Update to the real interface target
					type = revirtualizedMethodOwner;
					overrideToken = revirtualizedMethodRef;
				}
				else
				{
					//Normal virtual method, continue
					continue;
				}
			}

			Int32 index = findIndexInInterfaces(type);
			if (index == -1)
				THROW("Interface type not found in implementation list");

			//Requires higher level
			type = GetTypeFromTokenInternal(context, memberRef.TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT, TypeStatus::MethodTableDone);
			auto overridenMethod = type->GetMethodTable()->GetMethodBy(overrideToken);
			//Set in interface table
			table->Put(index, overridenMethod->GetDefToken(), methodTable->GetMethodIndexBy(method->GetDefToken()));
		}
	}

	return table;
}

bool RTM::MetaManager::HasVisitedType(VisitSet const& visited, TypeIdentity const& identity)
{
	return visited.find(identity) != visited.end();
}

RTM::MetaManager::MetaManager()
{
	INJECT_LOGGER(MetaManager);
}

RTM::AssemblyContext* RTM::MetaManager::StartUp(RTString assemblyName)
{
	return LoadAssembly(assemblyName);
}

void RTM::MetaManager::ShutDown()
{
	//For safety
	std::unique_lock lock{ mContextLock };
	for (auto&& each : mContexts)
		UnLoadAssembly(each.second);
	mContexts.clear();
}

RTM::MetaManager::~MetaManager()
{
	ShutDown();
}

RTM::AssemblyContext* RTM::MetaManager::GetAssemblyFromToken(AssemblyContext* context, MDToken assemblyReference)
{
	//Self
	if (assemblyReference == RTME::AssemblyRefMD::SelfReference)
		return context;

	//Resolve to target assembly
	auto&& assemblyRef = context->AssemblyRefs[assemblyReference];
	auto targetAssembly = TryQueryContextLocked(&assemblyRef);
	if (targetAssembly == nullptr)
	{
		std::unique_lock lock{ mContextLock };
		//A more concurrent way should be raised
		targetAssembly = TryQueryContext(&assemblyRef);
		if (targetAssembly != nullptr)
			return targetAssembly;

		auto name = GetStringFromToken(context, assemblyRef.AssemblyName);
		auto rawNameString = (RTString)name->GetContent();
		targetAssembly = LoadAssembly(rawNameString);
	}

	return targetAssembly;
}

RTM::Type* RTM::MetaManager::GetTypeFromToken(AssemblyContext* context, MDToken typeReference)
{
	VisitSet visited{};
	WaitingList waitingList{};
	WaitingList externalWaitingList{};
	InstantiationMap genericMap{};
	TypeIdentityReclaimList identityReclaimList{};
	bool shouldWait = false;
	return GetTypeFromTokenInternal(context, typeReference, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT, true);
}

RTM::TypeDescriptor* RTM::MetaManager::GetTypeFromDefinitionToken(AssemblyContext* context, MDToken typeDefinition)
{
	VisitSet visited{};
	WaitingList waitingList{};
	WaitingList externalWaitingList{};
	InstantiationMap genericMap{};
	TypeIdentityReclaimList identityReclaimList{};
	bool shouldWait = false;
	return GetTypeFromDefTokenInternal(context, typeDefinition, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT, true);
}

RTO::StringObject* RTM::MetaManager::GetStringFromToken(AssemblyContext* context, MDToken stringReference)
{
	RTME::StringMD stringMD{};
	RTO::StringObject* string = nullptr;
	auto&& importer = context->Importer;
	importer->UseSession(
		[&](auto session)
		{
			importer->PreImportString(session, stringReference, &stringMD);
			string = (RTO::StringObject*)new (context->Heap) UInt8[sizeof(RTO::StringObject) + (stringMD.Count + 1) * sizeof(UInt16)];
			new (string) RTO::StringObject(stringMD.Count);
			stringMD.CharacterSequence = string->GetContent();
			((MutableRTString)stringMD.CharacterSequence)[stringMD.Count] = L'\0';
			return importer->ImportString(session, &stringMD);
		});
	return string;
}

RTM::MethodDescriptor* RTM::MetaManager::GetMethodFromToken(AssemblyContext* context, MDToken methodReference)
{
	auto&& memberRef = context->MemberRefs[methodReference];
	auto type = GetTypeFromToken(context, memberRef.TypeRefToken);
	return type->GetMethodTable()->GetMethodBy(memberRef.MemberDefToken);
}

RTM::FieldDescriptor* RTM::MetaManager::GetFieldFromToken(AssemblyContext* context, MDToken fieldReference)
{
	auto&& memberRef = context->MemberRefs[fieldReference];
	auto type = GetTypeFromToken(context, memberRef.TypeRefToken);
	return type->GetFieldTable()->GetFieldBy(memberRef.MemberDefToken);
}

RTM::TypeDescriptor* RTM::MetaManager::GetIntrinsicTypeFromCoreType(UInt8 coreType)
{
	AssemblyContext* coreLibrary = nullptr;

	{
		//Query or load assembly
		RTME::GUID guid{};
		std::memset(&guid, 0, sizeof(RTME::GUID));

		std::unique_lock lock{ mContextLock };
		auto placeEntry = mContexts.find(guid);
		if (placeEntry == mContexts.end())
		{
			coreLibrary = LoadAssembly(Text("Core"));
			mContexts.insert(std::make_pair(guid, coreLibrary));
		}
		else
		{
			coreLibrary = placeEntry->second;
		}
	}
	
	auto token = coreLibrary->Entries.GetTokenByFQN(CoreTypes::GetCoreTypeName(coreType));
	return GetTypeFromDefinitionToken(coreLibrary, token);
}
