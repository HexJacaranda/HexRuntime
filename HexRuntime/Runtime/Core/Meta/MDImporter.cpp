#include "MDImporter.h"

#define IF_FAIL_RET(EXPR) if (!(EXPR)) return false;

bool RTM::MDImporter::ReadCode(Int32& countTarget, UInt8*& target)
{
	Int32 readBytes = OSFile::ReadInto(mAssemblyFile, (UInt8*)&countTarget, sizeof(Int32));
	if (readBytes != sizeof(Int32))
		return false;
	target = mHeap->AllocateForCode(countTarget);
	readBytes = OSFile::ReadInto(mAssemblyFile, target, countTarget * sizeof(UInt8));
	return readBytes == countTarget * sizeof(UInt8);
}

bool RTM::MDImporter::PrepareImporter()
{
	//Prepare index table
	mIndexTable = new (mHeap) MDIndexTable[(Int32)MDRecordKinds::KindLimit];
	//Open assembly file for read only
	mAssemblyFile = OSFile::Open(mAssemblyFileName, UsageOption::Read, SharingOption::SharedRead);
	//Locate to table stream
	OSFile::Locate(mAssemblyFile, AssemblyHeaderMD::CompactSize, LocateOption::Start);
	
	//Read ref table header
	IF_FAIL_RET(ReadInto(mRefTableHeader.TypeRefTableOffset));
	IF_FAIL_RET(ReadInto(mRefTableHeader.TypeRefCount));
	IF_FAIL_RET(ReadInto(mRefTableHeader.MemberRefTableOffset));
	IF_FAIL_RET(ReadInto(mRefTableHeader.MemberRefCount));

	//Read regular table stream
	for (Int32 i = 0; i < (Int32)MDRecordKinds::KindLimit; ++i)
	{
		IF_FAIL_RET(ReadInto(mIndexTable[i].Kind));
		IF_FAIL_RET(ReadIntoSeries(mIndexTable[i].Count, mIndexTable[i].Offsets));
	}
}

RTM::MDImporter::MDImporter(RTString assemblyName, MDToken assembly):
	mAssemblyFileName(assemblyName)
{
	mHeap = MDHeap::GetPrivateHeap(assembly);
	PrepareImporter();
}

inline bool RTM::MDImporter::IsCanonicalToken(MDToken typeRefToken)
{
	return typeRefToken & 0x80000000;
}

bool RTM::MDImporter::ImportAssemblyHeader(AssemblyHeaderMD* assemblyMD)
{
	OSFile::Locate(mAssemblyFile, 0, LocateOption::Start);
	IF_FAIL_RET(ReadInto(assemblyMD->NameToken));
	IF_FAIL_RET(ReadInto(assemblyMD->MajorVersion));
	IF_FAIL_RET(ReadInto(assemblyMD->MinorVersion));
	IF_FAIL_RET(ReadInto(assemblyMD->GroupNameToken));
	IF_FAIL_RET(ReadInto(assemblyMD->GUID));
	return true;
}

bool RTM::MDImporter::ImportAttribute(MDToken token, AtrributeMD* attributeMD)
{
	//Rather complicated to resolve for meta manager
	Int32 offset = mIndexTable[(Int32)MDRecordKinds::AttributeDef][token];
	OSFile::Locate(mAssemblyFile, offset, LocateOption::Start);

	IF_FAIL_RET(ReadInto(attributeMD->ParentKind));
	IF_FAIL_RET(ReadInto(attributeMD->ParentToken));
	IF_FAIL_RET(ReadInto(attributeMD->TypeRefToken));
	IF_FAIL_RET(ReadIntoSeries(attributeMD->AttributeSize, attributeMD->AttributeBody));
	return true;
}

bool RTM::MDImporter::ImportArgument(MDToken token, ArgumentMD* argumentMD)
{
	Int32 offset = mIndexTable[(Int32)MDRecordKinds::Argument][token];
	OSFile::Locate(mAssemblyFile, offset, LocateOption::Start);

	IF_FAIL_RET(ReadInto(argumentMD->TypeRefToken));
	IF_FAIL_RET(ReadInto(argumentMD->CoreType));
	IF_FAIL_RET(ReadInto(argumentMD->DefaultValue));
	IF_FAIL_RET(ReadIntoSeries(argumentMD->AttributeCount, argumentMD->AttributeTokens));

	return true;
}

bool RTM::MDImporter::ImportMethod(MDToken token, MethodMD* methodMD)
{
	Int32 offset = mIndexTable[(Int32)MDRecordKinds::MethodDef][token];
	OSFile::Locate(mAssemblyFile, offset, LocateOption::Start);

	IF_FAIL_RET(ReadInto(methodMD->NameToken));
	IF_FAIL_RET(ReadInto(methodMD->Flags));

	ImportMethodSignature(&methodMD->SignatureMD);

	IF_FAIL_RET(ReadInto(methodMD->Entry.Type));
	IF_FAIL_RET(ReadInto(methodMD->Entry.CallingConvetion));
	
	ReadCode(methodMD->Entry.ILLength, methodMD->Entry.IL);
	ReadCode(methodMD->Entry.NativeLength, methodMD->Entry.Native);

	return true;
}

bool RTM::MDImporter::ImportMethodSignature(MethodSignatureMD* signatureMD)
{
	IF_FAIL_RET(ReadInto(signatureMD->ReturnTypeRefToken));
	IF_FAIL_RET(ReadIntoSeries(signatureMD->ArgumentCount, signatureMD->ArgumentTokens));
	return true;
}

bool RTM::MDImporter::ImportField(MDToken token, FieldMD* fieldMD)
{
	Int32 offset = mIndexTable[(Int32)MDRecordKinds::FieldDef][token];
	OSFile::Locate(mAssemblyFile, offset, LocateOption::Start);
	IF_FAIL_RET(ReadInto(fieldMD->TypeRefToken));
	IF_FAIL_RET(ReadInto(fieldMD->NameToken));
	IF_FAIL_RET(ReadIntoSeries(fieldMD->AttributeCount, fieldMD->AttributeTokens));
	return true;
}

bool RTM::MDImporter::ImportProperty(MDToken token, PropertyMD* propertyMD)
{
	IF_FAIL_RET(ReadInto(propertyMD->ParentTypeRefToken));
	IF_FAIL_RET(ReadInto(propertyMD->TypeRefToken));
	IF_FAIL_RET(ReadInto(propertyMD->SetterToken));
	IF_FAIL_RET(ReadInto(propertyMD->GetterToken));
	IF_FAIL_RET(ReadInto(propertyMD->BackingFieldToken));
	IF_FAIL_RET(ReadInto(propertyMD->NameToken));
	IF_FAIL_RET(ReadInto(propertyMD->Flags));
	IF_FAIL_RET(ReadIntoSeries(propertyMD->AttributeCount, propertyMD->AttributeTokens));
	return true;
}

bool RTM::MDImporter::ImportEvent(MDToken token, EventMD* eventMD)
{
	IF_FAIL_RET(ReadInto(eventMD->ParentTypeRefToken));
	IF_FAIL_RET(ReadInto(eventMD->TypeRefToken));
	IF_FAIL_RET(ReadInto(eventMD->AdderToken));
	IF_FAIL_RET(ReadInto(eventMD->RemoverToken));
	IF_FAIL_RET(ReadInto(eventMD->BackingFieldToken));
	IF_FAIL_RET(ReadInto(eventMD->NameToken));
	IF_FAIL_RET(ReadInto(eventMD->Flags));
	IF_FAIL_RET(ReadIntoSeries(eventMD->AttributeCount, eventMD->AttributeTokens));
	return true;
}

bool RTM::MDImporter::ImportType(MDToken token, TypeMD* typeMD)
{
	Int32 offset = mIndexTable[(Int32)MDRecordKinds::TypeDef][token];
	OSFile::Locate(mAssemblyFile, offset, LocateOption::Start);
	IF_FAIL_RET(ReadInto(typeMD->ParentAssemblyToken));
	IF_FAIL_RET(ReadInto(typeMD->ParentTypeRefToken));
	IF_FAIL_RET(ReadInto(typeMD->NameToken));
	IF_FAIL_RET(ReadInto(typeMD->EnclosingTypeRefToken));
	IF_FAIL_RET(ReadInto(typeMD->CoreType));
	IF_FAIL_RET(ReadIntoSeries(typeMD->FieldCount, typeMD->FieldTokens));
	IF_FAIL_RET(ReadIntoSeries(typeMD->MethodCount, typeMD->MethodTokens));
	IF_FAIL_RET(ReadIntoSeries(typeMD->PropertyCount, typeMD->PropertyTokens));
	IF_FAIL_RET(ReadIntoSeries(typeMD->EventCount, typeMD->EventTokens));
	IF_FAIL_RET(ReadIntoSeries(typeMD->InterfaceCount, typeMD->InterfaceTokens));
	IF_FAIL_RET(ReadIntoSeries(typeMD->GenericParameterCount, typeMD->GenericParameterTokens));
	IF_FAIL_RET(ReadIntoSeries(typeMD->AttributeCount, typeMD->AttributeTokens));
	return true;
}

bool RTM::MDImporter::PreImportString(MDToken token, StringMD* stringMD)
{
	Int32 offset = mIndexTable[(Int32)MDRecordKinds::String][token];
	OSFile::Locate(mAssemblyFile, offset, LocateOption::Start);
	IF_FAIL_RET(ReadInto(stringMD->CharacterSequence));
	return true;
}

bool RTM::MDImporter::ImportString(StringMD* stringMD)
{
	Int32 read = OSFile::ReadInto(mAssemblyFile, (UInt8*)stringMD->CharacterSequence, sizeof(UInt16) * stringMD->Count);
	return sizeof(UInt16) * stringMD->Count == read;;
}

bool RTM::MDImporter::ImportTypeRef(TypeRefMD* typeRefMD)
{
	IF_FAIL_RET(ReadInto(typeRefMD->AssemblyToken));
	IF_FAIL_RET(ReadInto(typeRefMD->TypeDefToken));
	return true;
}

bool RTM::MDImporter::ImportMemberRef(MemberRefMD* memberRefMD)
{
	IF_FAIL_RET(ReadInto(memberRefMD->TypeRefToken));
	IF_FAIL_RET(ReadInto(memberRefMD->MemberDefKind));
	IF_FAIL_RET(ReadInto(memberRefMD->MemberDefToken));
	return true;
}

bool RTM::MDImporter::ImportTypeRefTable(TypeRefMD*& typeRefTable)
{
	Int32 offset = mRefTableHeader.TypeRefTableOffset;
	Int32 count = mRefTableHeader.TypeRefCount;
	typeRefTable = new (mHeap) TypeRefMD[count];

	for (Int32 i = 0; i < count; ++i)
		IF_FAIL_RET(ImportTypeRef(&typeRefTable[i]));

	return true;
}

bool RTM::MDImporter::ImportMemberRefTable(MemberRefMD*& memberRefTable)
{
	Int32 offset = mRefTableHeader.MemberRefTableOffset;
	Int32 count = mRefTableHeader.MemberRefCount;
	memberRefTable = new (mHeap) MemberRefMD[count];

	for (Int32 i = 0; i < count; ++i)
		IF_FAIL_RET(ImportMemberRef(&memberRefTable[i]));

	return true;
}

#undef IF_FAIL_RET
