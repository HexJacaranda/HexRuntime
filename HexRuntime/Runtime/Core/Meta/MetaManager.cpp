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
	* The complexity of identity will get higer when we decide
	* to implement generics with shared instantiation, and
	* unavoidably introduce load limit of type for recursive
	* and generative pattern of generics
	*/
	
	TypeIdentity typeIdentity{ typeDefAssembly->Header.GUID, typeRef.TypeDefToken };
	if (HasVisitedType(visited, typeIdentity))
		return entry.Type.load(std::memory_order::acquire);
	else
		visited.insert(typeIdentity);

	if (status == TypeStatus::NotYet)
	{
		if (entry.InitializeTicket.fetch_add(1, std::memory_order::acq_rel) == 0)
		{
			bool nextShouldWait = false;
			//The first one to resolve type
			entry.Status.store(TypeStatus::Processing);
			
			auto type = ResolveType(typeDefAssembly, typeRef.TypeDefToken, visited, waitingList, externalWaitingList, nextShouldWait);

			/* Attention!
			* The InitializeTicket may overflow and go back to 0 again in some extreme cases,
			* where many threads pour in and the first thread who gets 0-th ticket has not set the 	
			* status to Processing. Then there will be more than one thread getting 0-th ticket
			* and doing the resolve job. But it will be fine if we use compare_and_swap to ensure
			* that the final type descriptor you get is the same.
			*/
			auto entryValue = entry.Type.load(std::memory_order::relaxed);
			if (!entry.Type.compare_exchange_weak(entryValue, type, std::memory_order::release, std::memory_order::relaxed))
				type = entryValue;

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
			//For those who did not get the zero-th ticket
			//they should go processing status handling
			goto IntermediateStatus;
		}
	}

	if (status == TypeStatus::Processing || status == TypeStatus::Almost)
	{
	IntermediateStatus:

		if (allowWaitOnEntry)
		{
			std::unique_lock lock{ entry.WaiterLock };
			entry.Waiter.wait(lock,
				[&]() { return entry.Status.load(std::memory_order::acquire) == TypeStatus::Done; });
			return entry.Type.load(std::memory_order::acquire);
		}
		else
		{
			//Append to external waiting list (it's a type loaded by another thread) 
			//and mark that our whole loading chain need too.
			shouldWait = true;
			auto type = entry.Type.load(std::memory_order::acquire);
			externalWaitingList.push_back(type);
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

RTM::TypeDescriptor* RTM::MetaManager::ResolveType(
	AssemblyContext* context,
	MDToken typeDefinition,
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

	auto type = new (context->Heap) RTM::TypeDescriptor();

	type->mColdMD = meta;
	type->mSelf = typeDefinition;
	type->mTypeName = GetStringFromToken(context, meta->NameToken);
	type->mNamespace = GetStringFromToken(context, meta->NamespaceToken);

	LOG_DEBUG("{}::{} [{:#x}] resolution started",
		type->GetNamespace()->GetContent(),
		type->GetTypeName()->GetContent(),
		type->GetToken());

	//Load parent
	if (meta->ParentTypeRefToken != NullToken)
	{
		LOG_DEBUG("Loading [{:#x}] parent", type->GetToken());
		type->mParent = GetTypeFromTokenInternal(context, meta->ParentTypeRefToken, USE_LOADING_CONTEXT);
	}

	//Load implemented interfaces
	if (meta->InterfaceCount > 0)
	{
		LOG_DEBUG("Loading [{:#x}] interfaces", type->GetToken());
		type->mInterfaces = new (context->Heap) TypeDescriptor * [meta->InterfaceCount];
		for (Int32 i = 0; i < meta->InterfaceCount; ++i)
			type->mInterfaces[i] = GetTypeFromTokenInternal(context, meta->InterfaceTokens[i], USE_LOADING_CONTEXT);
	}

	//Load canonical
	if (meta->CanonicalTypeRefToken != NullToken)
	{
		LOG_DEBUG("Loading [{:#x}] canonical type", type->GetToken());
		type->mCanonical = GetTypeFromTokenInternal(context, meta->CanonicalTypeRefToken, USE_LOADING_CONTEXT);
	}

	//Load enclosing
	if (meta->EnclosingTypeRefToken != NullToken)
	{
		LOG_DEBUG("Loading [{:#x}] enclosing type", type->GetToken());
		type->mEnclosing = GetTypeFromTokenInternal(context, meta->EnclosingTypeRefToken, USE_LOADING_CONTEXT);
	}

	//Loading field table
	LOG_DEBUG("Loading [{:#x}] fields", type->GetToken());
	type->mFieldTable = GenerateFieldTable(USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT);

	//Loading method table
	LOG_DEBUG("Loading [{:#x}] methods", type->GetToken());
	type->mMethTable = GenerateMethodTable(USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT);

	//Loading(generating) interface table
	LOG_DEBUG("Loading [{:#x}] interface table", type->GetToken());
	type->mInterfaceTable = GenerateInterfaceTable(type, USE_IMPORT_CONTEXT, USE_LOADING_CONTEXT);

	importer->ReturnSession(session);

	LOG_DEBUG("{}::{} [{:#x}] resolution done",
		type->GetNamespace()->GetContent(),
		type->GetTypeName()->GetContent(),
		type->GetToken());
	return type;
}

RTM::FieldTable* RTM::MetaManager::GenerateFieldTable(INJECT(IMPORT_CONTEXT, LOADING_CONTEXT))
{
	//Load fields
	auto fieldTable = new (context->Heap) FieldTable();

	//Field descriptors
	fieldTable->FieldCount = meta->FieldCount;
	fieldTable->Fields = new (context->Heap) FieldDescriptor * [meta->FieldCount];
	auto fieldMDs = new (context->Heap) RTME::FieldMD[meta->FieldCount];

	for (Int32 i = 0; i < meta->FieldCount; ++i)
	{
		auto fieldMD = &fieldMDs[i];

		if (!importer->ImportField(session, meta->FieldTokens[i], fieldMD))
		{
			importer->ReturnSession(session);
			THROW("Error occurred when importing type meta data");
		}

		auto field = new (context->Heap) FieldDescriptor();
		field->mColdMD = fieldMD;
		field->mName = GetStringFromToken(context, fieldMD->NameToken);
		field->mFieldType = GetTypeFromTokenInternal(context, fieldMD->TypeRefToken, USE_LOADING_CONTEXT);

		fieldTable->Fields[i] = field;
	}

	//Compute layout
	GenerateLayout(fieldTable, context);
	return fieldTable;
}

void RTM::MetaManager::GenerateLayout(FieldTable* table, AssemblyContext* context)
{
	auto fieldLayout = new (context->Heap) FieldsLayout();
	fieldLayout->Align = sizeof(IntPtr);

	fieldLayout->SlotCount = table->FieldCount;
	fieldLayout->Slots = new (context->Heap) LayoutSlot[table->FieldCount];

	auto getFieldSize = [](Type* type) 
	{
		auto coreType = type->GetCoreType();
		Int32 fieldSize = 0;
		if (CoreTypes::IsPrimitive(coreType) ||
			coreType == CoreTypes::InteriorRef ||
			coreType == CoreTypes::Ref)
			fieldSize = CoreTypes::SizeOfCoreType[coreType];
		else if (coreType == CoreTypes::Struct)
			fieldSize = type->GetSize();
		else
			fieldSize = CoreTypes::Ref;

		return fieldSize;
	};

	Int32 offset = 0;

	for (Int32 i = 0; i < table->FieldCount; ++i)
	{
		auto&& fieldSlot = table->Fields[i];
		auto&& type = fieldSlot->GetType();

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

	table->Layout = fieldLayout;
}

RTM::MethodTable* RTM::MetaManager::GenerateMethodTable(INJECT(IMPORT_CONTEXT, LOADING_CONTEXT))
{
	auto methodTable = new (context->Heap) MethodTable();
	methodTable->Methods = new (context->Heap) MethodDescriptor[meta->MethodCount];
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

	for (Int32 i = 0; i < meta->MethodCount; ++i)
	{
		auto&& descriptor = methodTable->Methods[i];
		auto&& methodMD = methodMDs[i];

		if (!importer->ImportMethod(session, meta->MethodTokens[i], &methodMD))
		{
			importer->ReturnSession(session);
			THROW("Error occurred when importing type meta data");
		}

		if (methodMD.IsStatic())
			methodTable->StaticMethodCount++;
		else if (methodMD.IsVirtual())
			methodTable->VirtualMethodCount++;
		else
			methodTable->InstanceMethodCount++;

		descriptor.mColdMD = &methodMD;
		descriptor.mManagedName = GetStringFromToken(context, methodMD.NameToken);
		descriptor.mSignature = generateSignature(methodMD.Signature);
	}

	return methodTable;
}

RTM::InterfaceDispatchTable* RTM::MetaManager::GenerateInterfaceTable(Type* current, INJECT(IMPORT_CONTEXT, LOADING_CONTEXT))
{
	return nullptr;
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
	return GetTypeFromTokenInternal(context, typeReference, visited, waitingList, externalWaitingList, shouldWait, true);
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
	return nullptr;
}

RTM::FieldDescriptor* RTM::MetaManager::GetFieldFromToken(AssemblyContext* context, MDToken fieldReference)
{
	return nullptr;
}
