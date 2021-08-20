#pragma once
#include "..\..\RuntimeAlias.h"

namespace RTME
{
#define BEGIN_FLAGS(UNDERLYING_TYPE) UNDERLYING_TYPE Flags;

#define FLAG_GET(NAME, BIT) bool NAME()const { return !!(Flags & (1 << BIT)); } 

#define END_FLAGS

	/*  Specially, when the highest bit of type ref token is set to 1. Then
		it refers to generic parameter.
	*/

	enum class MDRecordKinds: Int16
	{
		String,
		Argument,
		GenericParameter,
		TypeDef,
		GenericInstantiationDef,
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

	template<class U>
	static UInt32 ComputeHashCode(U const& value) {
		const UInt8* _First = (const UInt8*)&value;

		const UInt32 _FNV_offset_basis = 2166136261U;
		const UInt32 _FNV_prime = 16777619U;
		UInt32 _Val = _FNV_offset_basis;
		for (UInt32 _Next = 0; _Next < sizeof(U); ++_Next)
		{
			_Val ^= (UInt32)_First[_Next];
			_Val *= _FNV_prime;
		}
		return (_Val);
	}

	struct GUID
	{
		Int32 X;
		Int16 Y;
		Int16 Z;
		Int16 U[4];
	public:
		UInt32 GetHashCode()const
		{
			return ComputeHashCode(*this);
		}
	};

	struct AssemblyHeaderMD
	{
		MDToken NameToken;
		Int32 MajorVersion;
		Int32 MinorVersion;
		MDToken GroupNameToken;
		GUID GUID;

		static constexpr Int32 CompactSize =
			sizeof(AssemblyHeaderMD::NameToken) +
			sizeof(AssemblyHeaderMD::MajorVersion) +
			sizeof(AssemblyHeaderMD::MinorVersion) +
			sizeof(AssemblyHeaderMD::GroupNameToken) +
			sizeof(AssemblyHeaderMD::GUID);
	};

	struct RefTableHeaderMD
	{
		Int32 TypeRefTableOffset;
		Int32 TypeRefCount;
		Int32 MemberRefTableOffset;
		Int32 MemberRefCount;
		Int32 AssemblyRefTableOffset;
		Int32 AssemblyRefCount;

		static constexpr Int32 CompactSize =
			sizeof(RefTableHeaderMD::TypeRefTableOffset) +
			sizeof(RefTableHeaderMD::TypeRefCount) +
			sizeof(RefTableHeaderMD::MemberRefTableOffset) +
			sizeof(RefTableHeaderMD::MemberRefCount) +	
			sizeof(RefTableHeaderMD::AssemblyRefTableOffset) +
			sizeof(RefTableHeaderMD::AssemblyRefCount);
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
		/// <summary>
		/// Only GenericInstantiationDef, GenericParamter and TypeDef are allowed
		/// </summary>
		MDRecordKinds DefKind;
		union 
		{
			/// <summary>
			/// Actually the TypeDefToken is at the same level with GenericInstantiationDefToken.
			/// They really points to some While GenericParameterDefToken here 
			/// </summary>
			MDToken TypeDefToken;
			MDToken GenericInstantiationDefToken;
			/// <summary>
			/// Use the generic parameter from outer context
			/// </summary>
			MDToken GenericParameterDefToken;
		};		
	};

	/// <summary>
	/// Member reference could be field, method, property, event
	/// </summary>
	struct MemberRefMD
	{
		MDToken TypeRefToken;
		/// <summary>
		/// Highest bit to indicate whether it's a generic instantiation
		/// </summary>
		MDRecordKinds MemberDefKind;
		union
		{
			MDToken MemberDefToken;
			//Only available when it's a method 
			MDToken GenericInstantiationDefToken;
		};		
		MDRecordKinds GetMemberDefKind()const { 
			return (MDRecordKinds)((Int32)MemberDefKind & ~0x80000000); 
		};
		bool IsGenericInstantiation()const { 
			return !!((Int32)MemberDefKind & 0x80000000);
		}
	};

	struct AssemblyRefMD
	{
		static constexpr MDToken SelfReference = 0u;
		GUID GUID;
		MDToken AssemblyName;
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
		UInt8 Accessibility;

		BEGIN_FLAGS(UInt16)
			FLAG_GET(IsVolatile, 0)
			FLAG_GET(IsStatic, 1)
			FLAG_GET(IsConstant, 2)
			FLAG_GET(IsReadOnly, 3)
			FLAG_GET(IsThreadLocal, 4)
		END_FLAGS

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

		TOKEN_SERIES(Attribute);
	};

	struct ArgumentMD
	{
		MDToken TypeRefToken;
		MDToken NameToken;
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
		MDToken TypeRefToken;
		MDToken NameToken;
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

	struct GenericInstantiationMD
	{
		MDToken CanonicalTypeRefToken;
		TOKEN_SERIES(TypeParameter);
	};

	struct MethodMD
	{
		MDToken ParentTypeRefToken;
		MDToken NameToken;
		UInt8 Accessibility;

		BEGIN_FLAGS(UInt16)
			FLAG_GET(IsInstance, 0)
			FLAG_GET(IsVirtual, 1)
			FLAG_GET(IsStatic, 2)
			FLAG_GET(IsOverride, 3)
			FLAG_GET(IsFinal, 4)
			FLAG_GET(IsGeneric, 5)
			FLAG_GET(IsRTSpecial, 6)
		END_FLAGS

		MethodSignatureMD Signature;
		MDToken OverridesMethodRef;
		ILMD ILCodeMD;
		Int32 NativeLinkCount;
		NativeLinkMD* NativeLinks;

		TOKEN_SERIES(GenericParameter);
		TOKEN_SERIES(Attribute);
	};

	struct TypeMD
	{
		MDToken ParentAssemblyToken;
		MDToken ParentTypeRefToken;
		MDToken NameToken;
		MDToken EnclosingTypeRefToken;
		MDToken CanonicalTypeRefToken;
		MDToken NamespaceToken;
		UInt8 CoreType;
		UInt8 Accessibility;

		BEGIN_FLAGS(UInt16)
			FLAG_GET(IsSealed, 0)
			FLAG_GET(IsAbstract, 1)
			FLAG_GET(IsStruct, 3)
			FLAG_GET(IsInterface, 4)
			FLAG_GET(IsAttribute, 5)
			FLAG_GET(IsGeneric, 6)
			FLAG_GET(IsNested, 7)
		END_FLAGS

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