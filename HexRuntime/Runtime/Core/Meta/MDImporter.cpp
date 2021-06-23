#include "MDImporter.h"
#include "MappedImportSession.h"
#include "ImportSession.h"

#define IF_FAIL_RET(EXPR) if (!(EXPR)) return false;

#define IF_SESSION_FAIL_RET(EXPR) if (!(session->EXPR)) return false;

bool RTM::MDImporter::ReadCode(IImportSession* session, Int32& countTarget, UInt8*& target)
{
	Int32 readBytes = session->ReadInto((UInt8*)&countTarget, sizeof(Int32));
	if (readBytes != sizeof(Int32))
		return false;
	target = mHeap->AllocateForCode(countTarget);
	readBytes = session->ReadInto(target, countTarget * sizeof(UInt8));
	return readBytes == countTarget * sizeof(UInt8);
}

void RTM::MDImporter::PrepareFile(ImportOption option)
{
	//Open assembly file for read only
	mAssemblyFile = OSFile::Open(mAssemblyFileName, UsageOption::Read, SharingOption::SharedRead);
	mAssemblyLength = OSFile::SizeOf(mAssemblyFile);
	//Open file mapping for fast mode
	if (option == ImportOption::Fast)
	{
		mAssemblyMapping = OSFile::OpenMapping(mAssemblyFile, UsageOption::Read);
		mAssemblyMappedAddress = OSFile::MapAddress(mAssemblyMapping, UsageOption::Read);
	}
}

bool RTM::MDImporter::PrepareImporter()
{
	//Prepare index table
	mIndexTable = new (mHeap) MDIndexTable[(Int32)MDRecordKinds::KindLimit];
	return UseSession(
		[&](auto session)
		{
			//Read ref table header
			IF_SESSION_FAIL_RET(ReadInto(mRefTableHeader.TypeRefTableOffset));
			IF_SESSION_FAIL_RET(ReadInto(mRefTableHeader.TypeRefCount));
			IF_SESSION_FAIL_RET(ReadInto(mRefTableHeader.MemberRefTableOffset));
			IF_SESSION_FAIL_RET(ReadInto(mRefTableHeader.MemberRefCount));

			//Read regular table stream
			for (Int32 i = 0; i < (Int32)MDRecordKinds::KindLimit; ++i)
			{
				IF_SESSION_FAIL_RET(ReadInto(mIndexTable[i].Kind));
				IF_SESSION_FAIL_RET(ReadIntoSeries(mIndexTable[i].Count, mIndexTable[i].Offsets));
			}
			return true;
		},
		AssemblyHeaderMD::CompactSize);
}

RTM::MDImporter::MDImporter(RTString assemblyName, MDToken assembly, ImportOption option):
	mAssemblyFileName(assemblyName),
	mImportOption(option)
{
	mHeap = MDHeap::GetPrivateHeap(assembly);
	PrepareFile(option);
	PrepareImporter();
}

RTM::MDImporter::~MDImporter()
{
	if (mImportOption == ImportOption::Fast)
	{
		OSFile::UnmapAddress(mAssemblyMappedAddress);
		OSFile::CloseMapping(mAssemblyMapping);
	}
	OSFile::Close(mAssemblyFile);
}

inline bool RTM::MDImporter::IsCanonicalToken(MDToken typeRefToken)
{
	return typeRefToken & 0x80000000;
}

bool RTM::MDImporter::ImportAssemblyHeader(IImportSession* session, AssemblyHeaderMD* assemblyMD)
{
	session->Relocate(0, LocateOption::Start);
	IF_SESSION_FAIL_RET(ReadInto(assemblyMD->NameToken));
	IF_SESSION_FAIL_RET(ReadInto(assemblyMD->MajorVersion));
	IF_SESSION_FAIL_RET(ReadInto(assemblyMD->MinorVersion));
	IF_SESSION_FAIL_RET(ReadInto(assemblyMD->GroupNameToken));
	IF_SESSION_FAIL_RET(ReadInto(assemblyMD->GUID));
	return true;
}

bool RTM::MDImporter::ImportAttribute(IImportSession* session, MDToken token, AtrributeMD* attributeMD)
{
	//Rather complicated to resolve for meta manager
	Int32 offset = mIndexTable[(Int32)MDRecordKinds::AttributeDef][token];
	session->Relocate(offset, LocateOption::Start);
	IF_SESSION_FAIL_RET(ReadInto(attributeMD->ParentKind));
	IF_SESSION_FAIL_RET(ReadInto(attributeMD->ParentToken));
	IF_SESSION_FAIL_RET(ReadInto(attributeMD->TypeRefToken));
	IF_SESSION_FAIL_RET(ReadIntoSeries(attributeMD->AttributeSize, attributeMD->AttributeBody));
	return true;
}

bool RTM::MDImporter::ImportArgument(IImportSession* session, MDToken token, ArgumentMD* argumentMD)
{
	Int32 offset = mIndexTable[(Int32)MDRecordKinds::Argument][token];
	session->Relocate(offset, LocateOption::Start);

	IF_SESSION_FAIL_RET(ReadInto(argumentMD->TypeRefToken));
	IF_SESSION_FAIL_RET(ReadInto(argumentMD->CoreType));
	IF_SESSION_FAIL_RET(ReadInto(argumentMD->DefaultValue));
	IF_SESSION_FAIL_RET(ReadIntoSeries(argumentMD->AttributeCount, argumentMD->AttributeTokens));

	return true;
}

bool RTM::MDImporter::ImportMethod(IImportSession* session, MDToken token, MethodMD* methodMD)
{
	Int32 offset = mIndexTable[(Int32)MDRecordKinds::MethodDef][token];
	session->Relocate(offset, LocateOption::Start);

	IF_SESSION_FAIL_RET(ReadInto(methodMD->ParentTypeRefToken));
	IF_SESSION_FAIL_RET(ReadInto(methodMD->NameToken));
	IF_SESSION_FAIL_RET(ReadInto(methodMD->Flags));

	ImportMethodSignature(session, &methodMD->SignatureMD);

	IF_SESSION_FAIL_RET(ReadInto(methodMD->Entry.Type));
	IF_SESSION_FAIL_RET(ReadInto(methodMD->Entry.CallingConvetion));
	
	IF_FAIL_RET(ReadCode(session, methodMD->Entry.ILLength, methodMD->Entry.IL));
	IF_FAIL_RET(ReadCode(session, methodMD->Entry.NativeLength, methodMD->Entry.Native));

	return true;
}

bool RTM::MDImporter::ImportMethodSignature(IImportSession* session, MethodSignatureMD* signatureMD)
{
	IF_SESSION_FAIL_RET(ReadInto(signatureMD->ReturnTypeRefToken));
	IF_SESSION_FAIL_RET(ReadIntoSeries(signatureMD->ArgumentCount, signatureMD->ArgumentTokens));
	return true;
}

bool RTM::MDImporter::ImportField(IImportSession* session, MDToken token, FieldMD* fieldMD)
{
	Int32 offset = mIndexTable[(Int32)MDRecordKinds::FieldDef][token];
	session->Relocate(offset, LocateOption::Start);
	IF_SESSION_FAIL_RET(ReadInto(fieldMD->TypeRefToken));
	IF_SESSION_FAIL_RET(ReadInto(fieldMD->NameToken));
	IF_SESSION_FAIL_RET(ReadIntoSeries(fieldMD->AttributeCount, fieldMD->AttributeTokens));
	return true;
}

bool RTM::MDImporter::ImportProperty(IImportSession* session, MDToken token, PropertyMD* propertyMD)
{
	IF_SESSION_FAIL_RET(ReadInto(propertyMD->ParentTypeRefToken));
	IF_SESSION_FAIL_RET(ReadInto(propertyMD->TypeRefToken));
	IF_SESSION_FAIL_RET(ReadInto(propertyMD->SetterToken));
	IF_SESSION_FAIL_RET(ReadInto(propertyMD->GetterToken));
	IF_SESSION_FAIL_RET(ReadInto(propertyMD->BackingFieldToken));
	IF_SESSION_FAIL_RET(ReadInto(propertyMD->NameToken));
	IF_SESSION_FAIL_RET(ReadInto(propertyMD->Flags));
	IF_SESSION_FAIL_RET(ReadIntoSeries(propertyMD->AttributeCount, propertyMD->AttributeTokens));
	return true;
}

bool RTM::MDImporter::ImportEvent(IImportSession* session, MDToken token, EventMD* eventMD)
{
	IF_SESSION_FAIL_RET(ReadInto(eventMD->ParentTypeRefToken));
	IF_SESSION_FAIL_RET(ReadInto(eventMD->TypeRefToken));
	IF_SESSION_FAIL_RET(ReadInto(eventMD->AdderToken));
	IF_SESSION_FAIL_RET(ReadInto(eventMD->RemoverToken));
	IF_SESSION_FAIL_RET(ReadInto(eventMD->BackingFieldToken));
	IF_SESSION_FAIL_RET(ReadInto(eventMD->NameToken));
	IF_SESSION_FAIL_RET(ReadInto(eventMD->Flags));
	IF_SESSION_FAIL_RET(ReadIntoSeries(eventMD->AttributeCount, eventMD->AttributeTokens));
	return true;
}

bool RTM::MDImporter::ImportType(IImportSession* session, MDToken token, TypeMD* typeMD)
{
	Int32 offset = mIndexTable[(Int32)MDRecordKinds::TypeDef][token];
	session->Relocate(offset, LocateOption::Start);
	IF_SESSION_FAIL_RET(ReadInto(typeMD->ParentAssemblyToken));
	IF_SESSION_FAIL_RET(ReadInto(typeMD->ParentTypeRefToken));
	IF_SESSION_FAIL_RET(ReadInto(typeMD->NameToken));
	IF_SESSION_FAIL_RET(ReadInto(typeMD->EnclosingTypeRefToken));
	IF_SESSION_FAIL_RET(ReadInto(typeMD->NamespaceToken));
	IF_SESSION_FAIL_RET(ReadInto(typeMD->CoreType));
	IF_SESSION_FAIL_RET(ReadIntoSeries(typeMD->FieldCount, typeMD->FieldTokens));
	IF_SESSION_FAIL_RET(ReadIntoSeries(typeMD->MethodCount, typeMD->MethodTokens));
	IF_SESSION_FAIL_RET(ReadIntoSeries(typeMD->PropertyCount, typeMD->PropertyTokens));
	IF_SESSION_FAIL_RET(ReadIntoSeries(typeMD->EventCount, typeMD->EventTokens));
	IF_SESSION_FAIL_RET(ReadIntoSeries(typeMD->InterfaceCount, typeMD->InterfaceTokens));
	IF_SESSION_FAIL_RET(ReadIntoSeries(typeMD->GenericParameterCount, typeMD->GenericParameterTokens));
	IF_SESSION_FAIL_RET(ReadIntoSeries(typeMD->AttributeCount, typeMD->AttributeTokens));
	return true;
}

bool RTM::MDImporter::PreImportString(IImportSession* session, MDToken token, StringMD* stringMD)
{
	Int32 offset = mIndexTable[(Int32)MDRecordKinds::String][token];
	session->Relocate(offset, LocateOption::Start);
	IF_SESSION_FAIL_RET(ReadInto(stringMD->Count));
	return true;
}

bool RTM::MDImporter::ImportString(IImportSession* session, StringMD* stringMD)
{
	Int32 read = session->ReadInto((UInt8*)stringMD->CharacterSequence, sizeof(UInt16) * stringMD->Count);
	return sizeof(UInt16) * stringMD->Count == read;
}

bool RTM::MDImporter::ImportTypeRef(IImportSession* session, TypeRefMD* typeRefMD)
{
	IF_SESSION_FAIL_RET(ReadInto(typeRefMD->AssemblyToken));
	IF_SESSION_FAIL_RET(ReadInto(typeRefMD->TypeDefToken));
	return true;
}

bool RTM::MDImporter::ImportMemberRef(IImportSession* session, MemberRefMD* memberRefMD)
{
	IF_SESSION_FAIL_RET(ReadInto(memberRefMD->TypeRefToken));
	IF_SESSION_FAIL_RET(ReadInto(memberRefMD->MemberDefKind));
	IF_SESSION_FAIL_RET(ReadInto(memberRefMD->MemberDefToken));
	return true;
}

RTM::IImportSession* RTM::MDImporter::NewSession(Int32 offset)
{
	IImportSession* ret = nullptr;
	if (mImportOption == ImportOption::Fast)
		ret = new MappedImportSession(mHeap, mAssemblyMappedAddress, mAssemblyLength);
	else
		ret = new ImportSession(mHeap, mAssemblyFile);
	ret->Relocate(offset, RTI::LocateOption::Start);
	return ret;
}

void RTM::MDImporter::ReturnSession(IImportSession* session)
{
	delete session;
}

bool RTM::MDImporter::ImportTypeRefTable(IImportSession* session, TypeRefMD*& typeRefTable)
{
	Int32 offset = mRefTableHeader.TypeRefTableOffset;
	Int32 count = mRefTableHeader.TypeRefCount;
	typeRefTable = new (mHeap) TypeRefMD[count];

	for (Int32 i = 0; i < count; ++i)
		IF_FAIL_RET(ImportTypeRef(session, &typeRefTable[i]));

	return true;
}

bool RTM::MDImporter::ImportMemberRefTable(IImportSession* session, MemberRefMD*& memberRefTable)
{
	Int32 offset = mRefTableHeader.MemberRefTableOffset;
	Int32 count = mRefTableHeader.MemberRefCount;
	memberRefTable = new (mHeap) MemberRefMD[count];

	for (Int32 i = 0; i < count; ++i)
		IF_FAIL_RET(ImportMemberRef(session, &memberRefTable[i]));

	return true;
}

#undef IF_FAIL_RET
#undef IF_SESSION_FAIL_RET
