#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Interfaces\OSFile.h"
#include "MDRecords.h"
#include "MDHeap.h"

using namespace RTI;
namespace RTM
{
	/// <summary>
	/// Meta data importer, it's per assembly and its thread safety is
	/// guaranteed by meta manager
	/// </summary>
	class MDImporter
	{
		RTString mAssemblyFileName;
		RTI::FileHandle mAssemblyFile;
		MDIndexTable* mIndexTable = nullptr;
		MDPrivateHeap* mHeap = nullptr;
	private:
		template<class T>
		bool ReadInto(T& target) {
			Int32 readBytes = OSFile::ReadInto(mAssemblyFile, (UInt8*)&target, sizeof(T));
			return readBytes == sizeof(T);
		}

		template<class T>
		bool ReadIntoSeries(Int32& countTarget, T*& target) {
			Int32 readBytes = OSFile::ReadInto(mAssemblyFile, (UInt8*)&countTarget, sizeof(Int32));
			if (readBytes != sizeof(Int32))
				return false;
			target = new (mHeap) T[countTarget];
			readBytes = OSFile::ReadInto(mAssemblyFile, (UInt8*)target, countTarget * sizeof(T));
			return readBytes == countTarget * sizeof(T);
		}
		bool ReadCode(Int32& countTarget, UInt8*& target);

		bool PrepareImporter();
	public:
		MDImporter(RTString assemblyName, MDToken assembly);
	public:
		static inline bool IsCanonicalToken(MDToken typeRefToken);
		bool ImportAssemblyHeader(AssemblyHeaderMD* assemblyMD);
		bool ImportAttribute(MDToken token, AtrributeMD* attributeMD);
		bool ImportArgument(MDToken token, ArgumentMD* argumentMD);
		bool ImportMethod(MDToken token, MethodMD* methodMD);
		bool ImportMethodSignature(MethodSignatureMD* signatureMD);
		bool ImportField(MDToken token, FieldMD* fieldMD);
		bool ImportProperty(MDToken token, PropertyMD* propertyMD);
		bool ImportEvent(MDToken token, EventMD* eventMD);
		bool ImportType(MDToken token, TypeMD* typeMD);
	};
}