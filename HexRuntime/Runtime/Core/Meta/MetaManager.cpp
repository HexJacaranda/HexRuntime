#include "MetaManager.h"
#include "..\Exception\RuntimeException.h"
#include "..\Memory\PrivateHeap.h"
#include "..\..\LoggingConfiguration.h"
#include "..\..\Logging.h"
#include "CoreTypes.h"
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
	auto target = mContexts.find(reference->GUID.GetHashCode());
	if (target == mContexts.end())
		return nullptr;
	return target->second;
}

RTM::TypeDefEntry& RTM::MetaManager::GetTypeEntryFrom(Type* target)
{
	auto typeDefAssembly = target->GetAssembly();
	return typeDefAssembly->Entries[target->GetToken()];
}

RTM::TypeDescriptor* RTM::MetaManager::TryQueryType(AssemblyContext* context, MDToken typeDefinition)
{
	auto&& entry = context->Entries[typeDefinition];
	
	//In processing is not useable
	if (entry.Status.load(std::memory_order::acquire) != TypeStatus::Done)
		return nullptr;

	return entry.Type.load(std::memory_order::acquire);
}

RTM::TypeDescriptor* RTM::MetaManager::GetTypeFromTokenInternal(
	AssemblyContext* context,
	MDToken typeReference,
	INJECT(LOADING_CONTEXT),
	Int8 waitStatus,
	bool allowWaitOnEntry)
{
	auto&& typeRef = context->TypeRefs[typeReference];
	auto typeDefAssembly = GetAssemblyFromToken(context, typeRef.AssemblyToken);

	auto&& entry = typeDefAssembly->Entries[typeRef.TypeDefToken];
	auto status = entry.Status.load(std::memory_order::acquire);

	//Already done
	if (status == TypeStatus::Done)
		return entry.Type.load(std::memory_order::acquire);

	/*
	* The complexity of identity will get higher when we decide
	* to implement generics with shared instantiation, and
	* unavoidably introduce load limit of type for recursive
	* and generative pattern of generics
	*/
	
	TypeIdentity typeIdentity{ typeDefAssembly->Header.GUID, typeRef.TypeDefToken };
	if (HasVisitedType(visited, typeIdentity))
	{
		if (waitStatus != -1)
		{
			while (entry.Status.load(std::memory_order_acquire) < waitStatus)
				std::this_thread::yield();
		}
		return entry.Type.load(std::memory_order::acquire);
	}
	else
		visited.insert(typeIdentity);

	TypeDescriptor* type = nullptr;

	if (status == TypeStatus::NotYet)
	{
		type = new (context->Heap) TypeDescriptor();
		auto oldValue = entry.Type.load(std::memory_order_acquire);

		if (oldValue == nullptr &&
			entry.Type.compare_exchange_weak(oldValue, type, std::memory_order_release, std::memory_order_relaxed))
		{
			entry.Status.store(TypeStatus::Processing, std::memory_order_release);
			//The first one to resolve type
			bool nextShouldWait = false;
			ResolveType(typeDefAssembly, type, typeRef.TypeDefToken, entry, USE_LOADING_CONTEXT);

			if (nextShouldWait)
			{
				//Append to waiting list since it's a type loaded by our thread
				shouldWait = true;
				waitingList.push_back(type);
				entry.Status.store(TypeStatus::Almost, std::memory_order::release);
			}
			else
				entry.Status.store(TypeStatus::Done, std::memory_order::release);

			if (allowWaitOnEntry)
			{
				//Already root call, we need to promote type status to Done level in waiting list
				for (auto&& each : waitingList)
				{
					auto&& pendingEntry = each->GetAssembly()->Entries[each->GetToken()];
					pendingEntry.Status.store(TypeStatus::Done, std::memory_order::release);
				}

				//Then wait types loaded by other threads
				for (auto&& each : externalWaitingList)
				{
					auto&& pendingEntry = each->GetAssembly()->Entries[each->GetToken()];
					std::unique_lock lock{ pendingEntry.WaiterLock };
					pendingEntry.Waiter.wait(lock,
						[&]() { return pendingEntry.Status.load(std::memory_order::acquire) == TypeStatus::Done; });
				}

				//Wake up all waiting threads
				entry.Waiter.notify_all();
			}

			return type;
		}
		else
		{
			if (type != nullptr)
				::operator delete(type, context->Heap);

			type = oldValue;
			goto IntermediateStatus;
		}
	}

	if (status != TypeStatus::Done)
	{
		type = entry.Type.load(std::memory_order_acquire);
	IntermediateStatus:
		if (allowWaitOnEntry)
		{
			std::unique_lock lock{ entry.WaiterLock };
			entry.Waiter.wait(lock,
				[&]() { return entry.Status.load(std::memory_order::acquire) == TypeStatus::Done; });
			return type;
		}
		else
		{
			//Append to external waiting list (it's a type loaded by another thread) 
			//and mark that our whole loading chain need too.
			shouldWait = true;
			externalWaitingList.push_back(type);
			if (waitStatus != -1)
			{
				//Need to wait until specific intermediate state is reached
				while (entry.Status.load(std::memory_order_acquire) < waitStatus)
					std::this_thread::yield();
			}
			return type;
		}
	}

	THROW("Not allowed type status.");
	return nullptr;
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
	context->Entries = new (heap) TypeDefEntry[typeDefIndex.Count];

	mContexts.insert({ context->Header.GUID.GetHashCode(), context });

	return context;
}

void RTM::MetaManager::UnLoadAssembly(AssemblyContext* context)
{
	//Unload heap content
	if (context->Heap != nullptr)
	{
		delete context->Heap;
		context->Heap = nullptr;
	}
}

void RTM::MetaManager::ResolveType(
	AssemblyContext* context,
	TypeDescriptor* type,
	MDToken typeDefinition,
	TypeDefEntry& entry,
	INJECT(LOADING_CONTEXT))
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
	type->mSelf = typeDefinition;
	type->mTypeName = GetStringFromToken(context, meta->NameToken);
	type->mNamespace = GetStringFromToken(context, meta->NamespaceToken);

	LOG_DEBUG("{}::{} [{:#10x}] resolution started",
		type->GetNamespace()->GetContent(),
		type->GetTypeName()->GetContent(),
		type->GetToken());

	//Load parent
	if (meta->ParentTypeRefToken != NullToken)
	{
		LOG_DEBUG("Loading [{:#10x}] parent", type->GetToken());
		type->mParent = GetTypeFromTokenInternal(context, meta->ParentTypeRefToken, USE_LOADING_CONTEXT);
	}

	//Load implemented interfaces
	if (meta->InterfaceCount > 0)
	{
		LOG_DEBUG("Loading [{:#10x}] interfaces", type->GetToken());
		type->mInterfaces = new (context->Heap) TypeDescriptor * [meta->InterfaceCount];
		for (Int32 i = 0; i < meta->InterfaceCount; ++i)
			type->mInterfaces[i] = GetTypeFromTokenInternal(context, meta->InterfaceTokens[i], USE_LOADING_CONTEXT);
	}

	//Load canonical
	if (meta->CanonicalTypeRefToken != NullToken)
	{
		LOG_DEBUG("Loading [{:#10x}] canonical type", type->GetToken());
		type->mCanonical = GetTypeFromTokenInternal(context, meta->CanonicalTypeRefToken, USE_LOADING_CONTEXT);
	}

	//Load enclosing
	if (meta->EnclosingTypeRefToken != NullToken)
	{
		LOG_DEBUG("Loading [{:#10x}] enclosing type", type->GetToken());
		type->mEnclosing = GetTypeFromTokenInternal(context, meta->EnclosingTypeRefToken, USE_LOADING_CONTEXT);
	}
	entry.Status.store(TypeStatus::Basic, std::memory_order_release);

	//Loading field table
	LOG_DEBUG("Loading [{:#10x}] fields", type->GetToken());
	type->mFieldTable = GenerateFieldTable(USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT);

	//Update to layout done
	entry.Status.store(TypeStatus::LayoutDone, std::memory_order_release);

	//Loading method table
	LOG_DEBUG("Loading [{:#10x}] methods", type->GetToken());
	type->mMethTable = GenerateMethodTable(type, USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT);

	//Update to method table done
	entry.Status.store(TypeStatus::MethodTableDone, std::memory_order_release);

	//Loading(generating) interface table
	LOG_DEBUG("Loading [{:#10x}] interface table", type->GetToken());
	type->mInterfaceTable = GenerateInterfaceTable(type, USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT);

	entry.Status.store(TypeStatus::InterfaceTableDone, std::memory_order_release);

	importer->ReturnSession(session);

	LOG_DEBUG("{}::{} [{:#10x}] resolution done",
		type->GetNamespace()->GetContent(),
		type->GetTypeName()->GetContent(),
		type->GetToken());
}

RTM::FieldTable* RTM::MetaManager::GenerateFieldTable(INJECT(IMPORT_CONTEXT, LOADING_CONTEXT))
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
		field.mFieldType = GetTypeFromTokenInternal(context, fieldMD->TypeRefToken, USE_LOADING_CONTEXT);
	}
	//Compute layout
	GenerateLayout(fieldTable, context);
	return fieldTable;
}

void RTM::MetaManager::GenerateLayout(FieldTable* table, AssemblyContext* context)
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
			fieldSize = CoreTypes::SizeOfCoreType[coreType];
		else if (coreType == CoreTypes::Struct)
		{
			auto&& entry = GetTypeEntryFrom(type);
			//Should be very short
			while (entry.Status.load(std::memory_order_acquire) < TypeStatus::LayoutDone)
				std::this_thread::yield();
			//Ensure the layout is done
			fieldSize = type->GetSize();
		}
		else
			fieldSize = CoreTypes::Ref;

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

	table->mLayout = fieldLayout;
}

RTM::MethodTable* RTM::MetaManager::GenerateMethodTable(Type* current, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT))
{
	auto methodTable = new (context->Heap) MethodTable();

	//Dynamically
	std::vector<MethodDescriptor*> overridenMethods;

	auto parent = current->GetParentType();
	MethodTable* parentMT = nullptr;
	ObservableArray<MethodDescriptor*> overridenRegion{ nullptr, 0 };
	if (parent != nullptr)
	{
		auto&& entry = GetTypeEntryFrom(parent);
		//Should be very short, wait for method table to be done.
		while (entry.Status.load(std::memory_order_acquire) < TypeStatus::MethodTableDone)
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
		signature->mReturnType = GetTypeFromTokenInternal(context, signatureMD.ReturnTypeRefToken, USE_LOADING_CONTEXT);
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
			argument.mType = GetTypeFromTokenInternal(context, signatureMD.ReturnTypeRefToken, USE_LOADING_CONTEXT);
			argument.mManagedName = GetStringFromToken(context, argumentMD.NameToken);
		}
		return signature;
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
		auto&& overridenType = GetTypeFromTokenInternal(context, memberRef.TypeRefToken, USE_LOADING_CONTEXT, TypeStatus::Basic);
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

	//Set our overriden region
	methodTable->mOverridenRegionCount = overridenMethods.size();
	methodTable->mOverridenRegion = new (context->Heap) MethodDescriptor * [overridenMethods.size()];
	std::memcpy(methodTable->mOverridenRegion, &overridenMethods[0], sizeof(MethodDescriptor*) * overridenMethods.size());

	return methodTable;
}

RTM::InterfaceDispatchTable* RTM::MetaManager::GenerateInterfaceTable(Type* current, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT))
{
	//No need for interface to build table
	if (current->IsInterface())
		return nullptr;

	auto table = new InterfaceDispatchTable();
	auto interfaces = current->GetInterfaces();
	auto methodTable = current->GetMethodTable();
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
			auto type = GetTypeFromTokenInternal(context, memberRef.TypeRefToken, USE_LOADING_CONTEXT);
			if (methodMD->IsVirtual())
			{
				if (type->IsInterface())
				{
					//Directly implements
				}
				else
				{
					//Go for interface dispatch table

				}
			}
			else
			{
				/* If it's not a virtual method, then the type of that overriden method
				* must be interface
				*/


			}		
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
	VisitSet visited;
	WaitingList waitingList;
	WaitingList externalWaitingList;
	bool shouldWait = false;
	return GetTypeFromTokenInternal(context, typeReference, USE_LOADING_CONTEXT, true);
}

RTO::StringObject* RTM::MetaManager::GetStringFromToken(AssemblyContext* context, MDToken stringReference)
{
	RTME::StringMD stringMD;
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
