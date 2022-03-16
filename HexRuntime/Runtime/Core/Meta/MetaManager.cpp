#include "MetaManager.h"
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

RTM::TypeDescriptor* RTM::MetaManager::GetTypeFromReferenceTokenInternal(
	AssemblyContext* defineContext,
	MDToken reference,
	INJECT(LOADING_CONTEXT, INSTANTIATION_CONTEXT),
	Int8 waitStatus,
	bool allowWaitOnEntry)
{
	if (reference == NullToken)
		return nullptr;
	auto&& typeRef = defineContext->TypeRefs[reference];

	if (typeRef.DefKind == RTME::MDRecordKinds::GenericParameter)
	{
		//Return the type in instantiation map if possible
		if (auto type = genericMap.TryGetFromReference(reference); type.has_value())
			return type.value();

		/* If we didn't get the type from reference token, then we should
		* use its generic def-token to get the type in case we're instantiating
		* a type totally on the fly (no generic instantiation token in metadata)
		*/

		if (auto type = genericMap.TryGetFromDefinition(typeRef.GenericInstantiationDefToken);
			type.has_value())
			return type.value();

		THROW("Should never reach here");
	}

	auto typeDefAssembly = GetAssemblyFromToken(defineContext, typeRef.AssemblyToken);

	//Pass to def-token resolving
	return GetTypeFromDefinitionTokenInternal(
		typeDefAssembly,
		typeRef.DefKind,
		typeRef.TypeDefToken,	
		USE_LOADING_CONTEXT,
		USE_INSTANTIATION_CONTEXT,
		waitStatus,
		allowWaitOnEntry);
}

RTM::TypeDescriptor* RTM::MetaManager::GetTypeFromDefinitionTokenInternal(
	AssemblyContext* defineContext,
	RTME::MDRecordKinds definitionKind,
	MDToken definition,
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

	auto promoteAndWait = [&](TypeDescriptor* type, TypeDefEntry* entry)
	{
		if (shouldWait)
		{
			//Append to waiting list since it's a type loaded by our thread
			waitingList.push_back(type);
			type->Status.store(TypeStatus::Almost, std::memory_order::release);
		}
		else
			type->Status.store(TypeStatus::Done, std::memory_order::release);

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
			entry->Waiter.notify_all();

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
		return type;
	};

	if (definitionKind == RTME::MDRecordKinds::GenericInstantiationDef)
	{
		/*
		* Always allocate meta memory from the assembly that defines
		* the canonical type, this is out of a further design: generic
		* instantiation sharing
		*/
		auto genericAssembly = GetGenericAssembly();

		//Generic instantiation
		auto&& genericInstantiation = defineContext->GenericDef[definition];
		//Firstly make sure that we get the right type identity
		auto canonicalType = GetTypeFromReferenceTokenInternal(
			defineContext,
			genericInstantiation.CanonicalTypeRefToken,
			USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);

		//Construct type identity
		TypeIdentity identity{ canonicalType };
		identity.ArgumentCount = genericInstantiation.TypeParameterCount;
		if (identity.ArgumentCount > 1)
			identity.Arguments = new (genericAssembly->Heap) TypeDescriptor * [identity.ArgumentCount];

		ForeachInlined(identity.Arguments, identity.ArgumentCount,
			[&](TypeDescriptor*& arg, Int32 index)
			{
				arg = GetTypeFromReferenceTokenInternal(
					defineContext,
					genericInstantiation.TypeParameterTokens[index],
					USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
			});

		/*
			* The complexity of identity will get higher when we decide
			* to implement generics with shared instantiation, and
			* unavoidably introduce load limit of type for recursive
			* and generative pattern of generics
		*/

		auto entry = new (genericAssembly->Heap) TypeDefEntry();
		auto instantiationType = new (genericAssembly->Heap) TypeDescriptor();
		entry->Type.store(instantiationType, std::memory_order_release);

		//Set fully qualified name and then establish new context with it
		auto fqn = GetTSN(identity);
		instantiationType->mFullQualifiedName = GetStringFromView(genericAssembly, fqn->GetFullyQualifiedName());
		instantiationType->mTypeName = GetStringFromView(genericAssembly, fqn->GetShortTypeName());
		auto [newDefinitionToken, finalValue] = genericAssembly->Entries.DefineNewMeta(ToStringView(instantiationType->mFullQualifiedName), entry);

		if (finalValue != entry)
		{
			genericAssembly->Heap->FreeTracked(instantiationType->mFullQualifiedName);
			genericAssembly->Heap->FreeTracked(instantiationType->mTypeName);
			genericAssembly->Heap->Return(instantiationType);
			genericAssembly->Heap->Return(entry);

			/*
			* Mark type identity (if there is an allocated array inside) to be freed in heap
			* held by AssemblyContext to avoid memory leak when the loading is over
			*/
			if (identity.ArgumentCount > 1)
				identityReclaimList.push_back(std::make_pair(genericAssembly->Heap, identity));

			return waitAndGetType(*finalValue, finalValue->Type.load(std::memory_order_acquire));
		}
		else
		{
			ScopeMap scope{};

			ForeachInlined(identity.Arguments, identity.ArgumentCount,
				[&](Type* argument, Int32 index)
				{
					scope.insert(
						{
							{ RTME::MDRecordKinds::TypeRef, genericInstantiation.TypeParameterTokens[index] },
							argument
						});
				});

			{
				INSTANTIATION_SCOPE(scope);
				ResolveOrInstantiateType(
					defineContext,
					identity,
					instantiationType,
					MetaOptions::Generic,
					newDefinitionToken,
					USE_LOADING_CONTEXT,
					USE_INSTANTIATION_CONTEXT);

				return promoteAndWait(instantiationType, finalValue);
			}
		}
	}
	
	if (definitionKind == RTME::MDRecordKinds::TypeDef)
	{
		auto&& entry = defineContext->Entries[definition];
		auto type = entry.Type.load(std::memory_order::acquire);

		if (type == nullptr)
		{
			//Try to content for its resolve
			auto newTypeDescriptor = new (defineContext->Heap) TypeDescriptor();
			if (entry.Type.compare_exchange_strong(
				type,
				newTypeDescriptor,
				std::memory_order_release,
				std::memory_order_relaxed))
			{
				//Won this contention
				newTypeDescriptor->Status.store(TypeStatus::Processing, std::memory_order_release);

				ResolveOrInstantiateType(
					defineContext,
					{},
					newTypeDescriptor,
					MetaOptions::Canonical,
					definition,
					USE_LOADING_CONTEXT,
					USE_INSTANTIATION_CONTEXT);

				return promoteAndWait(newTypeDescriptor, &entry);
			}
			else
			{
				//Clean resource
				defineContext->Heap->Return(newTypeDescriptor);
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

	THROW("Should not reach here");
}

RTM::TypeDescriptor* RTM::MetaManager::InstantiateWith(
	AssemblyContext* defineContext, 
	Type* canonical, 
	std::vector<Type*> const& args)
{
	/*Most of the logic is copied from GetTypeFromDefinitionTokenInternal*/

	auto genericAssembly = GetGenericAssembly();

	//Construct type identity
	TypeIdentity identity{ canonical };

	identity.ArgumentCount = args.size();
	if (identity.ArgumentCount > 1)
		identity.Arguments = (Type**)args.data();
	else
		identity.SingleArgument = args.front();

	auto entry = new (genericAssembly->Heap) TypeDefEntry();
	auto instantiationType = new (genericAssembly->Heap) TypeDescriptor();
	entry->Type.store(instantiationType, std::memory_order_release);

	//Set fully qualified name and then establish new context with it
	auto fqn = GetTSN(identity);
	instantiationType->mFullQualifiedName = GetStringFromView(genericAssembly, fqn->GetFullyQualifiedName());
	instantiationType->mTypeName = GetStringFromView(genericAssembly, fqn->GetShortTypeName());
	auto [newDefinitionToken, finalValue] = genericAssembly->Entries.DefineNewMeta(ToStringView(instantiationType->mFullQualifiedName), entry);

	if (finalValue != entry)
	{
		genericAssembly->Heap->FreeTracked(instantiationType->mFullQualifiedName);
		genericAssembly->Heap->FreeTracked(instantiationType->mTypeName);
		genericAssembly->Heap->Return(instantiationType);
		genericAssembly->Heap->Return(entry);

		//Wait and get
		auto type = finalValue->Type.load(std::memory_order_acquire);
		auto&& entry = *finalValue;
		std::unique_lock lock{ entry.WaiterLock };
		entry.Waiter.wait(lock,
			[&]() { return type->Status.load(std::memory_order::acquire) >= TypeStatus::Done; });

		return type;
	}
	else
	{
		ScopeMap scope{};

		ForeachInlined(identity.Arguments, identity.ArgumentCount,
			[&](Type* argument, Int32 index)
			{
				scope.insert(
					{
						{ RTME::MDRecordKinds::GenericParameter, canonical->GetMetadata()->GenericParameterTokens[index] },
						argument
					});
			});

		{
			VisitSet visited{};
			WaitingList waitingList{};
			WaitingList externalWaitingList{};
			InstantiationMap genericMap{};
			TypeIdentityReclaimList identityReclaimList{};
			bool shouldWait = false;

			auto promoteAndWait = [&](TypeDescriptor* type, TypeDefEntry* entry)
			{
				if (shouldWait)
				{
					//Append to waiting list since it's a type loaded by our thread
					waitingList.push_back(type);
					type->Status.store(TypeStatus::Almost, std::memory_order::release);
				}
				else
					type->Status.store(TypeStatus::Done, std::memory_order::release);

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
				entry->Waiter.notify_all();

				//Clean up type identity with allocated array to avoid memory leak
				for (auto&& [heap, identity] : identityReclaimList)
					heap->Free(identity.Arguments, sizeof(void*) * identity.ArgumentCount);

				//Return directly
				return type;
			};

			INSTANTIATION_SCOPE(scope);
			ResolveOrInstantiateType(
				defineContext,
				identity,
				instantiationType,
				MetaOptions::Generic,
				newDefinitionToken,
				USE_LOADING_CONTEXT,
				USE_INSTANTIATION_CONTEXT);

			promoteAndWait(instantiationType, entry);
		}
	}

	return instantiationType;
}

RTM::AssemblyContext* RTM::MetaManager::NewDynamicAssembly()
{
	auto heap = new RTMM::PrivateHeap();
	auto context = new (heap) AssemblyContext(heap, nullptr);
	TypeEntryTableT newTable{ nullptr, 0, heap->GetResource() };
	context->Entries = std::move(newTable);
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
	auto string = (RTO::StringObject*)new (context->Heap, RTMM::TrackTag) UInt8[sizeof(RTO::StringObject) + (view.size() + 1) * sizeof(UInt16)];
	new (string) RTO::StringObject(view.size(), view.data());
	return string;
}

void RTM::MetaManager::ResolveOrInstantiateType(
	AssemblyContext* context, 
	TypeIdentity const& identity,
	TypeDescriptor* type, 
	UInt8 metaOption,
	MDToken definitionToken, 
	INJECT(LOADING_CONTEXT,INSTANTIATION_CONTEXT))
{
	/* To support the generic instantiation, which requires allocating
	*  memory in global generic assembly
	*/
	AssemblyContext* allocatingContext = context;
	auto canonicalType = identity.Canonical;

	auto&& importer = context->Importer;
	//For performance, use a session across the whole meta data resolving
	auto session = importer->NewSession();

	RTME::TypeMD* meta = nullptr;
	if (metaOption & MetaOptions::Generic)
		meta = canonicalType->GetMetadata();
	else
	{
		meta = new (allocatingContext->Heap)RTME::TypeMD();
		if (!importer->ImportType(session, definitionToken, meta))
		{
			importer->ReturnSession(session);
			THROW("Error occurred when importing type meta data");
		}

		//The FQN of type to be instantiated has already been set in GetTypeFromDefinitionTokenInternal
		type->mTypeName = GetStringFromToken(context, meta->NameToken);
		type->mFullQualifiedName = GetStringFromToken(context, meta->FullyQualifiedNameToken);
	}

	//Set basic information
	type->mColdMD = meta;
	type->mContext = context;
	type->mSelf = definitionToken;
	type->mCanonical = canonicalType;

	//Set type arguments
	if (canonicalType != nullptr) {
		type->mTypeArguments = identity.Arguments;
	}

	//For canonical or half-open type, return directly
	if (type->IsGeneric() &&
		genericMap.GetCurrentScopeArgCount() < meta->GenericParameterCount)
	{
		return;
	}

	LOG_DEBUG("{} [{:#010x}] Resolution started",
		type->GetFullQualifiedName()->GetContent(),
		type->GetToken());

	//Load parent
	if (meta->ParentTypeRefToken != NullToken)
	{
		LOG_DEBUG("{} [{:#010x}] Loading parent", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mParent = GetTypeFromReferenceTokenInternal(context, meta->ParentTypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
	}

	//Load implemented interfaces
	if (meta->InterfaceCount > 0)
	{
		LOG_DEBUG("{} [{:#010x}] Loading interfaces", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		if (meta->InterfaceCount > 1)
			type->mInterfaces = new (allocatingContext->Heap) TypeDescriptor * [meta->InterfaceCount];

		ForeachInlined(type->mInterfaces, meta->InterfaceCount,
			[&](Type*& interface, Int32 index)
			{
				interface = GetTypeFromReferenceTokenInternal(context, meta->InterfaceTokens[index], USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
			});
	}

	//Load canonical
	if (meta->CanonicalTypeRefToken != NullToken)
	{
		LOG_DEBUG("{} [{:#010x}] Loading canonical type", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mCanonical = GetTypeFromReferenceTokenInternal(context, meta->CanonicalTypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
	}

	//Load enclosing
	if (meta->EnclosingTypeRefToken != NullToken)
	{
		LOG_DEBUG("{} [{:#010x}] Loading enclosing type", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mEnclosing = GetTypeFromReferenceTokenInternal(context, meta->EnclosingTypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
	}
	type->Status.store(TypeStatus::Basic, std::memory_order_release);

	if (!CoreTypes::IsPrimitive(meta->CoreType))
	{
		//Loading field table
		LOG_DEBUG("{} [{:#010x}] Loading fields", type->GetFullQualifiedName()->GetContent(), type->GetToken());
		type->mFieldTable = GenerateFieldTable(allocatingContext, USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
	}

	//Update to layout done
	type->Status.store(TypeStatus::LayoutDone, std::memory_order_release);

	//Loading method table
	LOG_DEBUG("{} [{:#010x}] Loading methods", type->GetFullQualifiedName()->GetContent(), type->GetToken());
	type->mMethTable = GenerateMethodTable(type, allocatingContext, USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);

	//Update to method table done
	type->Status.store(TypeStatus::MethodTableDone, std::memory_order_release);

	//Loading(generating) interface table
	LOG_DEBUG("{} [{:#010x}] Loading interface table", type->GetFullQualifiedName()->GetContent(), type->GetToken());
	type->mInterfaceTable = GenerateInterfaceTable(type, allocatingContext, USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);

	type->Status.store(TypeStatus::InterfaceTableDone, std::memory_order_release);

	importer->ReturnSession(session);

	LOG_DEBUG("{} [{:#010x}] Resolution done",
		type->GetFullQualifiedName()->GetContent(),
		type->GetToken());
}

RTM::FieldTable* RTM::MetaManager::GenerateFieldTable(AssemblyContext* allocatingContext, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT, INSTANTIATION_CONTEXT))
{
	//Load fields
	auto fieldTable = new (allocatingContext->Heap) FieldTable();

	//Field descriptors
	fieldTable->mFieldCount = meta->FieldCount;
	fieldTable->mFields = new (allocatingContext->Heap) FieldDescriptor[meta->FieldCount];
	auto fieldMDs = new (allocatingContext->Heap) RTME::FieldMD[meta->FieldCount];

	//Set base token
	if (meta->FieldCount > 0)
		fieldTable->mBaseToken = meta->FieldTokens[0];

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
		field.mFieldType = GetTypeFromReferenceTokenInternal(context, fieldMD->TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
		field.mOwningTable = fieldTable;
	}

	//Compute layout
	fieldTable->mLayout = GenerateLayout(fieldTable, allocatingContext, context);
	
	return fieldTable;
}

RTM::FieldsLayout* RTM::MetaManager::GenerateLayout(FieldTable* table, AssemblyContext* allocatingContext, AssemblyContext* context)
{
	auto fieldLayout = new (allocatingContext->Heap) FieldsLayout();
	fieldLayout->Align = sizeof(IntPtr);

	fieldLayout->SlotCount = table->mFieldCount;
	fieldLayout->Slots = new (allocatingContext->Heap) LayoutSlot[table->mFieldCount];

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

RTM::MethodTable* RTM::MetaManager::GenerateMethodTable(Type* current, AssemblyContext* allocatingContext, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT, INSTANTIATION_CONTEXT))
{
	auto methodTable = new (allocatingContext->Heap) MethodTable();

	//Set owning type
	methodTable->mOwningType = current;
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

	methodTable->mMethods = new (allocatingContext->Heap) MethodDescriptor*[meta->MethodCount];
	auto methodMDs = new (allocatingContext->Heap) RTME::MethodMD[meta->MethodCount];

	auto generateSignature = [&](RTME::MethodSignatureMD& signatureMD)
	{
		auto arguments = new (allocatingContext->Heap) MethodArgumentDescriptor[signatureMD.ArgumentCount];
		auto argumentMDs = new (allocatingContext->Heap) RTME::ArgumentMD[signatureMD.ArgumentCount];
		auto signature = new (allocatingContext->Heap) MethodSignatureDescriptor();

		signature->mColdMD = &signatureMD;
		signature->mReturnType = GetTypeFromReferenceTokenInternal(context, signatureMD.ReturnTypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
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
			argument.mType = GetTypeFromReferenceTokenInternal(context, argumentMD.TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
			argument.mManagedName = GetStringFromToken(context, argumentMD.NameToken);
		}
		return signature;
	};

	auto generateLocals = [&](RTME::MethodMD& md)
	{
		auto localCount = md.ILCodeMD.LocalVariableCount;
		auto locals = new (allocatingContext->Heap) MethodLocalVariableDescriptor[localCount];
		for (Int32 i = 0; i < localCount; ++i)
		{
			auto&& local = locals[i];
			auto&& localMD = md.ILCodeMD.LocalVariables[i];

			local.mColdMD = &localMD;
			local.mType = GetTypeFromReferenceTokenInternal(context, localMD.TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT);
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
		auto&& overridenType = GetTypeFromReferenceTokenInternal(context, memberRef.TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT, TypeStatus::Basic);
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
		descriptor = new (allocatingContext->Heap) MethodDescriptor();

		descriptor->mColdMD = &methodMD;
		descriptor->mManagedName = GetStringFromToken(context, methodMD.NameToken);
		descriptor->mSignature = generateSignature(methodMD.Signature);
		descriptor->mLocals = generateLocals(methodMD);
		descriptor->mSelf = meta->MethodTokens[i];
		descriptor->mOwningTable = methodTable;

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
		methodTable->mOverridenRegion = new (allocatingContext->Heap) MethodDescriptor * [overridenMethods.size()];
		std::memcpy(methodTable->mOverridenRegion, &overridenMethods[0], sizeof(MethodDescriptor*)* overridenMethods.size());
	}

	return methodTable;
}

RTM::InterfaceDispatchTable* RTM::MetaManager::GenerateInterfaceTable(Type* current, AssemblyContext* allocatingContext, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT, INSTANTIATION_CONTEXT))
{
	//No need for interface to build table
	if (current->IsInterface())
		return nullptr;

	auto table = new (allocatingContext->Heap) InterfaceDispatchTable();
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
			auto type = GetTypeFromReferenceTokenInternal(context, memberRef.TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT, TypeStatus::Basic);

			if (!type->IsInterface())
			{
				//Overrides normal type
				//Requires higher level
				type = GetTypeFromReferenceTokenInternal(context, memberRef.TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT, TypeStatus::MethodTableDone);
				auto overridenMethod = type->GetMethodTable()->GetMethodBy(overrideToken);

				MDToken revirtualizedMethodRef = overridenMethod->mColdMD->OverridesMethodRef;
				//The root virtual call is a revirtualization for some interface method
				if (revirtualizedMethodRef != NullToken)
				{
					auto revirtualizedMethodMemberRef = type->GetAssembly()->MemberRefs[revirtualizedMethodRef];
					auto revirtualizedMethodOwner = GetTypeFromReferenceTokenInternal(
						type->GetAssembly(), 
						revirtualizedMethodMemberRef.TypeRefToken, 
						USE_LOADING_CONTEXT, 
						USE_INSTANTIATION_CONTEXT, 
						TypeStatus::MethodTableDone);

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
			type = GetTypeFromReferenceTokenInternal(context, memberRef.TypeRefToken, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT, TypeStatus::MethodTableDone);
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

std::shared_ptr<RTM::TypeStructuredName> RTM::MetaManager::GetTSN(TypeIdentity const& identity)
{
	auto fqn = identity.Canonical->GetFullQualifiedName();
	std::wstring_view fqnView{ fqn->GetContent(), (std::size_t)fqn->GetCount() };
	auto canonicalTSN = TypeNameParser(fqnView).Parse();

	//Construct instantiation array
	std::vector<std::wstring_view> instantiationNameList(identity.ArgumentCount);

	auto appendFor = [&](Int32 index, TypeDescriptor* type) {
		auto name = type->GetFullQualifiedName();
		//Direct reference
		instantiationNameList[index] = std::wstring_view{ name->GetContent(), (std::size_t)name->GetCount() };
	};

	ForeachInlined(identity.Arguments, identity.ArgumentCount,
		[&](TypeDescriptor* const arg, Int32 index)
		{
			appendFor(index, arg);
		});

	//Construct instantiated type name
	return canonicalTSN->InstantiateWith(instantiationNameList);
}

RTM::MetaManager::MetaManager()
{
	INJECT_LOGGER(MetaManager);
}

RTM::AssemblyContext* RTM::MetaManager::StartUp(std::wstring_view const& name)
{
	return GetAssemblyFromName(name);
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
		auto name = GetStringFromToken(context, assemblyRef.AssemblyName);
		return GetAssemblyFromName(ToStringView(name));
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
	return GetTypeFromReferenceTokenInternal(context, typeReference, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT, true);
}

RTM::TypeDescriptor* RTM::MetaManager::GetTypeFromDefinitionToken(AssemblyContext* context, MDToken typeDefinition)
{
	VisitSet visited{};
	WaitingList waitingList{};
	WaitingList externalWaitingList{};
	InstantiationMap genericMap{};
	TypeIdentityReclaimList identityReclaimList{};
	bool shouldWait = false;
	return GetTypeFromDefinitionTokenInternal(context, RTME::MDRecordKinds::TypeDef, typeDefinition, USE_LOADING_CONTEXT, USE_INSTANTIATION_CONTEXT, true);
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
			//TODO: From GC
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

RTM::TypeDescriptor* RTM::MetaManager::InstantiateRefType(TypeDescriptor * origin)
{
	auto type = new (GetGenericAssembly()->Heap) TypeDescriptor;
	auto ref = GetIntrinsicTypeFromCoreType(CoreTypes::InteriorRef);
	return InstantiateWith(GetCoreAssembly(), ref, { origin });
}

RTM::TypeDescriptor* RTM::MetaManager::InstantiateArrayType(TypeDescriptor * origin)
{
	auto type = new (GetGenericAssembly()->Heap) TypeDescriptor;
	auto ref = GetIntrinsicTypeFromCoreType(CoreTypes::Array);
	return InstantiateWith(GetCoreAssembly(), ref, { origin });
}

RTM::TypeDescriptor* RTM::MetaManager::Instantiate(TypeDescriptor * canonical, std::vector<TypeDescriptor*> const & args)
{
	return InstantiateWith(canonical->GetAssembly(), canonical, args);
}

RTM::TypeDescriptor* RTM::MetaManager::GetIntrinsicTypeFromCoreType(UInt8 coreType)
{
	AssemblyContext* coreLibrary = GetCoreAssembly();
	auto token = coreLibrary->Entries[CoreTypes::GetCoreTypeName(coreType)];
	return GetTypeFromDefinitionToken(coreLibrary, token);
}

RTM::TypeDescriptor* RTM::MetaManager::GetTypeFromFQN(std::shared_ptr<TypeStructuredName> const& tsn)
{
	//The fqn may require instantiating type dynamically
	if (tsn->IsFullyCanonical())
	{
		auto assembly = GetAssemblyFromName(tsn->GetReferenceAssembly());
		auto defToken = assembly->Entries.GetTokenByFQN(tsn->GetFullyQualifiedName());
		if (defToken.has_value())
			return GetTypeFromDefinitionToken(assembly, defToken.value());
		else
			THROW("Undefined type for assembly");
	}
	else
	{
		{
			//Should check instantiated type to avoid unnecessary memory allocation
			auto genericAssembly = GetGenericAssembly();
			auto token = genericAssembly->Entries.GetTokenByFQN(tsn->GetFullyQualifiedName());
			if (token.has_value())
				return GetTypeFromDefinitionToken(genericAssembly, token.value());
		}

		TypeDescriptor* canonical = nullptr;
		auto canonicalName = tsn->GetCanonicalizedName();
		auto assembly = GetAssemblyFromName(tsn->GetReferenceAssembly());
		auto defToken = assembly->Entries.GetTokenByFQN(canonicalName);
		if (defToken.has_value())
			canonical = GetTypeFromDefinitionToken(assembly, defToken.value());
		else
			THROW("Undefined type for assembly");

		std::vector<TypeDescriptor*> args{};

		for (auto&& node : tsn->GetTypeNodes())
			for (auto&& arg : node.Arguments)
				args.push_back(GetTypeFromFQN(arg));

		return Instantiate(canonical, args);
	}
}

RTM::TypeDescriptor* RTM::MetaManager::GetTypeFromFQN(std::wstring_view const& fqn)
{
	TypeNameParser parser{ fqn };
	auto tsn = parser.Parse();
	return GetTypeFromFQN(tsn);
}

RTM::AssemblyContext* RTM::MetaManager::GetAssemblyFromName(std::wstring_view const& name)
{
	{
		std::shared_lock lock{ mContextLock };
		if (auto it = mName2Context.find(name); it != mName2Context.end())
			return it->second;
	}

	auto heap = new RTMM::PrivateHeap();
	std::wstring assembly{ name };
	auto importer = new (heap) RTME::MDImporter(assembly.c_str(), heap, RTME::ImportOption::Default);
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
		UnLoadAssembly(context);
		return nullptr;
	}

	{
		std::shared_lock lock{ mContextLock };
		if (auto it = mContexts.find(context->Header.GUID); it != mContexts.end())
			return it->second;
	}

	{
		//Prepare name 
		context->AssemblyName = GetStringFromToken(context, context->Header.NameToken);

		//Prepare type def entries
		auto&& typeDefIndex = importer->GetIndexTable()[(Int32)RTME::MDRecordKinds::TypeDef];

		TypeEntryTableT newTable{ new (heap) TypeDefEntry[typeDefIndex.Count], (MDToken)typeDefIndex.Count, heap->GetResource() };
		context->Entries = std::move(newTable);

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
						context->Entries.DefineMetaMapUnsafe({ fqn->GetContent(), (std::size_t)fqn->GetCount() }, i);
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
	}

	{
		std::unique_lock lock{ mContextLock };
		if (auto it = mContexts.find(context->Header.GUID); it != mContexts.end())
		{
			UnLoadAssembly(context);
			return it->second;
		}

		mContexts.insert({ context->Header.GUID, context });
		mName2Context.insert({ ToStringView(context->AssemblyName), context });
	}

	return context;
}

RTM::AssemblyContext* RTM::MetaManager::GetCoreAssembly()
{
	AssemblyContext* assembly = mCoreAssembly.load(std::memory_order_relaxed);
	if (assembly != nullptr)
		return assembly;

	Guid guid = CoreLibraryGuid;

	{
		std::shared_lock lock{ mContextLock };

		if (auto context = mContexts.find(guid); context != mContexts.end())
		{
			//In case other loaded this assembly
			if (assembly == nullptr)
				mCoreAssembly.store(assembly, std::memory_order_release);
			return context->second;
		}
	}

	assembly = GetAssemblyFromName(Text("Core"));
	mCoreAssembly.store(assembly, std::memory_order_release);

	return assembly;
}

RTM::AssemblyContext* RTM::MetaManager::GetGenericAssembly()
{
	AssemblyContext* assembly = mGenericAssembly.load(std::memory_order_relaxed);
	if (assembly != nullptr)
		return assembly;

	Guid guid = GenericLibraryGuid;

	{
		std::shared_lock lock{ mContextLock };
		if (auto context = mContexts.find(guid); context != mContexts.end())
			return context->second;
	}

	{
		std::unique_lock lock{ mContextLock };
		if (auto context = mContexts.find(guid); context != mContexts.end())
			return context->second;

		auto assembly = NewDynamicAssembly();
		mContexts[guid] = assembly;
		mGenericAssembly.store(assembly, std::memory_order_release);

		return assembly;
	}
}

RT::Guid RTM::MetaManager::CoreLibraryGuid = {};
RT::Guid RTM::MetaManager::GenericLibraryGuid = { 0xFFFFFFFF, 0xFFFF, 0xFFFF, { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF } };
