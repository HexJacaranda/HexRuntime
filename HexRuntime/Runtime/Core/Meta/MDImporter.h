#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Interfaces\OSFile.h"
#include "MDRecords.h"
#include "MDHeap.h"

using namespace RTI;
namespace RTM
{
	enum class ImportOption
	{
		Default,
		/// <summary>
		/// Fast mode will use IO mapping and memcpy things to speed up importing (TO DO)
		/// </summary>
		Fast
	};

	/// <summary>
	/// Meta data importer, it's per assembly and its thread safety is
	/// guaranteed by meta manager
	/// </summary>
	class MDImporter
	{
		RTString mAssemblyFileName;
		RTI::FileHandle mAssemblyFile;
		MDIndexTable* mIndexTable = nullptr;
		RefTableHeaderMD mRefTableHeader;
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
	private:
		bool ImportMethodSignature(MethodSignatureMD* signatureMD);
		bool ImportTypeRef(TypeRefMD* typeRefMD);
		bool ImportMemberRef(MemberRefMD* memberRefMD);
	public:
		static inline bool IsCanonicalToken(MDToken typeRefToken);
		bool ImportAssemblyHeader(AssemblyHeaderMD* assemblyMD);
		bool ImportAttribute(MDToken token, AtrributeMD* attributeMD);
		bool ImportArgument(MDToken token, ArgumentMD* argumentMD);
		bool ImportMethod(MDToken token, MethodMD* methodMD);
		
		bool ImportField(MDToken token, FieldMD* fieldMD);
		bool ImportProperty(MDToken token, PropertyMD* propertyMD);
		bool ImportEvent(MDToken token, EventMD* eventMD);
		bool ImportType(MDToken token, TypeMD* typeMD);
		
		/// <summary>
		/// Special for string importation, it's controlled by two phases.
		/// In the first phase we will provide the length.
		/// Int the second phase you need to allocate managed heap space
		/// for pointer CharacterSequence inside stringMD so that we
		/// can copy directly into it
		/// </summary>
		/// <param name="token"></param>
		/// <param name="stringMD"></param>
		/// <returns></returns>
		bool PreImportString(MDToken token, StringMD* stringMD);
		bool ImportString(StringMD* stringMD);

		bool ImportTypeRefTable(TypeRefMD*& typeRefTable);
		bool ImportMemberRefTable(MemberRefMD*& memberRefTable);
	};
}