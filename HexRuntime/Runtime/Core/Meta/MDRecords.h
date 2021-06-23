#pragma once
#include "..\..\RuntimeAlias.h"

namespace RTM
{
	/*  Specially, when the highest bit of type ref token is set to 1. Then
		it refers to generic parameter.
	*/

	enum class MDRecordKinds
	{
		String,
		Argument,
		GenericParameter,
		TypeDef,
		AttributeDef,
		MethodDef,
		FieldDef,
		PropertyDef,
		EventDef,
		//Used for counting
		KindLimit,
		TypeRef,
		FieldRef,
		MethodRef,
		PropertyRef,
		EventRef
	};

	struct AssemblyHeaderMD
	{
		MDToken NameToken;
		Int32 MajorVersion;
		Int32 MinorVersion;
		MDToken GroupNameToken;
		struct
		{
			Int32 X;
			Int16 Y;
			Int16 Z;
			Int16 U[4];
		} GUID;

		static constexpr Int32 CompactSize =
			sizeof(AssemblyHeaderMD::NameToken) +
			sizeof(AssemblyHeaderMD::MajorVersion) +
			sizeof(AssemblyHeaderMD::MinorVersion) +
			sizeof(AssemblyHeaderMD::GUID);
	};

	struct RefTableHeaderMD
	{
		Int32 TypeRefTableOffset;
		Int32 TypeRefCount;
		Int32 MemberRefTableOffset;
		Int32 MemberRefCount;
		static constexpr Int32 CompactSize =
			sizeof(RefTableHeaderMD::TypeRefTableOffset) +
			sizeof(RefTableHeaderMD::TypeRefCount) +
			sizeof(RefTableHeaderMD::MemberRefTableOffset) +
			sizeof(RefTableHeaderMD::MemberRefCount);
	};
	/// <summary>
	/// MD index table stores offsets of record in assembly
	/// </summary>
	struct MDIndexTable
	{
		MDRecordKinds Kind;
		Int32 Count;
		Int32* Offsets;
	public:
		Int32 operator[](Int32 index) {
			return Offsets[index];
		}
	};

#define TOKEN_SERIES(NAME) \
	Int32 NAME##Count; \
	MDToken* NAME##Tokens

	struct TypeRefMD
	{
		MDToken AssemblyToken;
		MDToken TypeDefToken;
	};

	/// <summary>
	/// Member reference could be field, method, property, event
	/// </summary>
	struct MemberRefMD
	{
		MDToken TypeRefToken;
		MDRecordKinds MemberDefKind;
		MDToken MemberDefToken;
	};

	struct StringMD
	{
		Int32 Count;
		RTString CharacterSequence;
	};

	/* Attribute is a kind of special metadata. Its layout is described by referenced type.
	   All supported field types: 
	   1. primitive types (int, long, double...)
	   2. string literal (stored as string token)
	   3. type literal (stored as type reference token)
	*/
	struct AtrributeMD
	{
		MDRecordKinds ParentKind;
		MDToken ParentToken;
		MDToken TypeRefToken;
		Int32 AttributeSize;
		UInt8* AttributeBody;
	};

	struct GenericParamterMD
	{
		MDToken NameToken;
	};

	struct FieldMD
	{
		MDToken ParentTypeRefToken;
		MDToken TypeRefToken;
		MDToken NameToken;
		struct
		{
			UInt8 Accessibility;
			bool IsVolatile : 1;
			bool IsInstance : 1;
			bool IsConstant : 1;
		} Flags;

		TOKEN_SERIES(Attribute);
	};

	struct PropertyMD
	{
		MDToken ParentTypeRefToken;
		MDToken TypeRefToken;
		MDToken SetterToken;
		MDToken GetterToken;
		MDToken BackingFieldToken;
		MDToken NameToken;
		struct
		{
			UInt8 Accessibility;
			bool IsInstance : 1;
			bool IsVirtual : 1;
			bool IsOverride : 1;
			bool IsFinal : 1;
			bool IsRTSpecial : 1;
		} Flags;
		TOKEN_SERIES(Attribute);
	};

	struct EventMD
	{
		MDToken ParentTypeRefToken;
		MDToken TypeRefToken;
		MDToken AdderToken;
		MDToken RemoverToken;
		MDToken BackingFieldToken;
		MDToken NameToken;
		struct
		{
			UInt8 Accessibility;
			bool IsInstance : 1;
			bool IsVirtual : 1;
			bool IsOverride : 1;
			bool IsFinal : 1;
		} Flags;
		TOKEN_SERIES(Attribute);
	};

	struct ArgumentMD
	{
		MDToken TypeRefToken;
		MDToken NameToken;
		UInt8 CoreType;	
		union {
			MDToken StringRefToken;
			Int64 Data;
		} DefaultValue;
		TOKEN_SERIES(Attribute);
	};

	struct MethodSignatureMD
	{
		MDToken ReturnTypeRefToken;
		TOKEN_SERIES(Argument);
	};
	
	struct LocalVariableMD
	{
		UInt8 CoreType;
		MDToken TypeRefToken;
	};

	struct ILMD
	{
		Int32 LocalVariableCount;
		LocalVariableMD* LocalVariables;
		Int32 CodeLength;
		UInt8* IL;
	};

	struct NativeLinkMD
	{
		struct {

		};
	};

	struct MethodMD
	{
		MDToken ParentTypeRefToken;
		MDToken NameToken;
		struct 
		{
			UInt8 Accessibility;
			bool IsInstance : 1;
			bool IsVirtual : 1;
			bool IsOverride : 1;
			bool IsFinal : 1;
			bool IsGeneric : 1;
			bool IsRTSpecial : 1;
		} Flags;
		MethodSignatureMD Signature;
		ILMD ILCodeMD;
		Int32 NativeLinkCount;
		NativeLinkMD* NativeLinks;
	};

	struct TypeMD
	{
		MDToken ParentAssemblyToken;
		MDToken ParentTypeRefToken;
		MDToken NameToken;
		MDToken EnclosingTypeRefToken;
		MDToken NamespaceToken;
		UInt8 CoreType;
		struct {
			UInt8 Accessibility;
			bool IsSealed : 1;
			bool IsAbstract : 1;
			bool IsStruct : 1;
			bool IsInterface : 1;
			bool IsAttribute : 1;
			bool IsGeneric : 1;
		} Flags;
		TOKEN_SERIES(Field);
		TOKEN_SERIES(Method);
		TOKEN_SERIES(Property);
		TOKEN_SERIES(Event);
		TOKEN_SERIES(Interface);
		TOKEN_SERIES(GenericParameter);
		TOKEN_SERIES(Attribute);
	};

#undef TOKEN_SERIES
}