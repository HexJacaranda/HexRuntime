#include "MetaManager.h"
#include "..\Exception\RuntimeException.h"

RTM::AssemblyContext* RTM::MetaManager::TryQueryContextLocked(RTME::AssemblyRefMD* reference)
{
	std::shared_lock lock{ mContextLock };
	TryQueryContext(reference);
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
	return nullptr;
}

RTM::TypeDescriptor* RTM::MetaManager::TryQueryTypeLocked(AssemblyContext* context, MDToken typeDefinition)
{
	return nullptr;
}

RTM::AssemblyContext* RTM::MetaManager::LoadAssembly(RTString assembly)
{
	auto heap = new RTME::MDPrivateHeap();
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

	mContexts.insert({ context->Header.GUID.GetHashCode(), context });

	return context;
}

void RTM::MetaManager::UnLoadAssembly(AssemblyContext* context)
{
	auto heap = context->Heap;
	//Unload heap content
	heap->Unload();

	//Free heap object
	delete heap;
}

RTM::TypeDescriptor* RTM::MetaManager::ResolveType(AssemblyContext* context, MDToken typeDefinition)
{
	return nullptr;
}

void RTM::MetaManager::StartUp(RTString assemblyName)
{
	LoadAssembly(assemblyName);
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
	auto&& typeRef = context->TypeRefs[typeReference];
	auto assembly = GetAssemblyFromToken(context, typeRef.AssemblyToken);
	auto type = TryQueryTypeLocked(assembly, typeRef.TypeDefToken);
	if (type == nullptr)
	{
		std::unique_lock lock{ mContextLock };
		//A more concurrent way should be raised
		type = TryQueryType(assembly, typeRef.TypeDefToken);
		if (type != nullptr)
			return type;
		type = ResolveType(context, typeRef.TypeDefToken);
	}

	return type;
}

RTO::StringObject* RTM::MetaManager::GetStringFromToken(AssemblyContext* context, MDToken stringReference)
{
	return nullptr;
}

RTM::MethodDescriptor* RTM::MetaManager::GetMethodFromToken(AssemblyContext* context, MDToken methodReference)
{
	return nullptr;
}

RTM::FieldDescriptor* RTM::MetaManager::GetFieldFromToken(AssemblyContext* context, MDToken fieldReference)
{
	return nullptr;
}
