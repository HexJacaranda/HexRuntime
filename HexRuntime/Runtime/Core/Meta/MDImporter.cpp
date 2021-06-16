#include "MDImporter.h"

void RTM::MDImporter::ReadCode(Int32& countTarget, UInt8*& target)
{
	OSFile::ReadInto(mAssemblyFile, (UInt8*)&countTarget, sizeof(Int32));
	target = mHeap->Allocate(countTarget);
	OSFile::ReadInto(mAssemblyFile, target, countTarget * sizeof(UInt8));
}

void RTM::MDImporter::PrepareImporter()
{
	//Prepare index table
	mIndexTable = (MDIndexTable*)mHeap->Allocate(sizeof(MDIndexTable) * (Int32)MDTableKinds::TableLimit);
	//Open assembly file for read only
	mAssemblyFile = OSFile::Open(mAssemblyFileName, UsageOption::Read);
	//Locate to table stream
	OSFile::Locate(mAssemblyFile, sizeof(AssemblyHeaderMD), LocateOption::Start);
	
	for (Int32 i = 0; i < (Int32)MDTableKinds::TableLimit; ++i)
	{
		ReadInto(mIndexTable[i].Kind);
		ReadIntoSeries(mIndexTable[i].Count, mIndexTable[i].Offsets);
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
	return false;
}

bool RTM::MDImporter::ImportAttribute(MDToken token, AtrributeMD* attributeMD)
{
	return false;
}

bool RTM::MDImporter::ImportArgument(MDToken token, ArgumentMD* argumentMD)
{
	return false;
}

bool RTM::MDImporter::ImportMethod(MDToken token, MethodMD* methodMD)
{
	Int32 offset = mIndexTable[(Int32)MDTableKinds::MethodDef][token];
	OSFile::Locate(mAssemblyFile, offset, LocateOption::Start);

	ReadInto(methodMD->NameToken);
	ReadInto(methodMD->Flags);

	ImportMethodSignature(&methodMD->SignatureMD);

	ReadInto(methodMD->Entry.Type);
	ReadInto(methodMD->Entry.CallingConvetion);
	
	ReadCode(methodMD->Entry.ILLength, methodMD->Entry.IL);
	ReadCode(methodMD->Entry.NativeLength, methodMD->Entry.Native);

	return true;
}

bool RTM::MDImporter::ImportMethodSignature(MethodSignatureMD* signatureMD)
{
	ReadInto(signatureMD->ReturnTypeRefToken);
	ReadIntoSeries(signatureMD->ArgumentCount, signatureMD->ArgumentTokens);
	ReadIntoSeries(signatureMD->AttributeCount, signatureMD->AttributeTokens);
	return true;
}

bool RTM::MDImporter::ImportField(MDToken token, FieldMD* fieldMD)
{
	return false;
}

bool RTM::MDImporter::ImportProperty(MDToken token, PropertyMD* propertyMD)
{
	return false;
}

bool RTM::MDImporter::ImportEvent(MDToken token, EventMD* eventMD)
{
	return false;
}

bool RTM::MDImporter::ImportType(MDToken token, TypeMD* typeMD)
{
	Int32 offset = mIndexTable[(Int32)MDTableKinds::TypeDef][token];
	return false;
}
