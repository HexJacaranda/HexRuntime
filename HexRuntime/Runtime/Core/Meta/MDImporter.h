#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Interfaces\OSFile.h"
#include "MDRecords.h"
#include "MDHeap.h"
#include "IImportSession.h"
#include <type_traits>

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

	template<class Fn>
	concept SessionActionT = requires(Fn action, IImportSession * session, bool result)
	{
		result = action(session);
	};

	/// <summary>
	/// Meta data importer, it's per assembly and its thread safety is
	/// guaranteed by meta manager
	/// </summary>
	class MDImporter
	{
		//IO file part
		ImportOption mImportOption = ImportOption::Default;
		RTString mAssemblyFileName = nullptr;
		Int32 mAssemblyLength = 0;
		FileHandle mAssemblyFile = nullptr;
		FileMappingHandle mAssemblyMapping = nullptr;
		UInt8* mAssemblyMappedAddress = nullptr;

		//Meta data table part
		MDIndexTable* mIndexTable = nullptr;
		RefTableHeaderMD mRefTableHeader;
		MDPrivateHeap* mHeap = nullptr;
	private:
		bool ReadCode(IImportSession* session, Int32& countTarget, UInt8*& target);
		void LocateSession(IImportSession* session, MDRecordKinds kind, MDToken token);
		void PrepareFile(ImportOption option);
		bool PrepareImporter();
	public:
		MDImporter(RTString assemblyName, MDToken assembly, ImportOption option);
		~MDImporter();
	private:
		bool ImportMethodSignature(IImportSession* session, MethodSignatureMD* signatureMD);
		bool ImportTypeRef(IImportSession* session, TypeRefMD* typeRefMD);
		bool ImportMemberRef(IImportSession* session, MemberRefMD* memberRefMD);
		bool ImportIL(IImportSession* session, ILMD* ilMD);
		bool ImportNativeLink(IImportSession* session, NativeLinkMD* nativeLinkMD);
		bool ImportLocalVariable(IImportSession* session, LocalVariableMD* localMD);
	public:
		/// <summary>
		/// Remember to return to importer
		/// </summary>
		/// <returns></returns>
		IImportSession* NewSession(Int32 offset = 0);
		void ReturnSession(IImportSession* session);

		/// <summary>
		/// Use session
		/// </summary>
		/// <param name="action"></param>
		/// <param name="offset"></param>
		/// <returns></returns>
		template<SessionActionT Fn>
		bool UseSession(Fn&& action, Int32 offset = 0)
		{
			IImportSession* session = NewSession(offset);
			bool result = std::forward<Fn>(action)(session);
			ReturnSession(session);
			return result;
		}
	public:
		static inline bool IsCanonicalToken(MDToken typeRefToken);
		bool ImportAssemblyHeader(IImportSession* session, AssemblyHeaderMD* assemblyMD);
		bool ImportAttribute(IImportSession* session, MDToken token, AtrributeMD* attributeMD);
		bool ImportArgument(IImportSession* session, MDToken token, ArgumentMD* argumentMD);
		bool ImportMethod(IImportSession* session, MDToken token, MethodMD* methodMD);
		
		bool ImportField(IImportSession* session, MDToken token, FieldMD* fieldMD);
		bool ImportProperty(IImportSession* session, MDToken token, PropertyMD* propertyMD);
		bool ImportEvent(IImportSession* session, MDToken token, EventMD* eventMD);
		bool ImportType(IImportSession* session, MDToken token, TypeMD* typeMD);
		
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
		bool PreImportString(IImportSession* session, MDToken token, StringMD* stringMD);
		bool ImportString(IImportSession* session, StringMD* stringMD);

		bool ImportTypeRefTable(IImportSession* session, TypeRefMD*& typeRefTable);
		bool ImportMemberRefTable(IImportSession* session, MemberRefMD*& memberRefTable);
	};
}