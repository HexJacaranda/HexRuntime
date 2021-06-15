#pragma once
#include "..\..\RuntimeAlias.h"

namespace RTM
{
	/*  Specially, when the highest bit of type ref token is set to 1. Then
		it refers to generic parameter.
	*/

	enum class MDTableKinds
	{
		TypeRef,
		FieldRef,
		MethodRef,
		PropertyRef,
		EventRef,
		Argument,
		GenericParameter,
		TypeDef,
		AttributeDef,
		MethodDef,
		//Used for counting
		TableLimit
	};

	struct AssemblyHeaderMD
	{

	};


	/// <summary>
	/// MD index table stores offsets of record in assembly
	/// </summary>
	struct MDIndexTable
	{
		MDTableKinds Kind;
		Int32 Count;
		Int32* Offsets;
	public:
		Int32 operator[](Int32 index) {
			return Offsets[index];
		}
	};

	struct TypeRefMD
	{
		MDToken AssemblyRefToken;
		MDToken TypeDefToken;
	};

	/// <summary>
	/// Member reference could be field, method
	/// </summary>
	struct MemberRefMD
	{
		MDToken TypeRefToken;
		/// <summary>
		/// The type of member can be determined by JIT via opcodes.
		/// </summary>
		MDToken MemberDefToken;
	};

	struct AtrributeMD
	{
		MDToken TypeRefToken;
	};

	struct GenericParamterMD
	{
		MDToken NameToken;
	};

	struct FieldMD
	{
		MDToken TypeRefToken;
		struct
		{
			UInt8 Accessibility;
			bool IsVolatile : 1;
			bool IsInstance : 1;
			bool IsConstant : 1;
		} Flags;
		MDToken AttributesTableToken;
	};

	struct PropertyMD
	{
		MDToken TypeRefToken;
		MDToken SetterToken;
		MDToken GetterToken;
		struct
		{
			UInt8 Accessibility;
			bool IsInstance : 1;
			bool IsVirtual : 1;
			bool IsOverride : 1;
			bool IsFinal : 1;
		};
	};

	struct EventMD
	{

	};

	struct ArgumentMD
	{
		MDToken TypeRefToken;
		UInt8 CoreType;
		union {
			MDToken StringRefToken;
			Int64 Data;
		} DefaultValue;
		MDToken AttributesTableToken;
	};

	struct MethodSignatureMD
	{
		MDToken ReturnTypeRefToken;
		Int32 ArgumentsCount;
		MDToken* ArgumentTokens;
	};

	class SlotType
	{
	public:
		constexpr static UInt8 IL = 0x01;
		constexpr static UInt8 PreJIT = 0x03;
		constexpr static UInt8 JIT = 0x04;
		constexpr static UInt8 Native = 0x02;
	};

	class CallingConvention
	{
	public:
		constexpr static UInt8 JIT = 0x01;
		constexpr static UInt8 Cdecl = 0x02;
		constexpr static UInt8 StdCall = 0x03;
		constexpr static UInt8 FastCall = 0x04;
		constexpr static UInt8 VectorCall = 0x05;
	};

	struct MethodMD
	{
		MDToken NameToken;
		struct 
		{
			UInt8 Accessibility;
			bool IsInstance : 1;
			bool IsVirtual : 1;
			bool IsOverride : 1;
			bool IsFinal : 1;
			bool IsGeneric : 1;
		} Flags;
		MethodSignatureMD SignatureMD;
		struct
		{
			UInt8 Type;
			UInt8 CallingConvetion;
			Int32 ILLength;
			UInt8* IL;
			Int32 NativeLength;
			UInt8* Native;
		} Entry;
	};

	struct TypeMD
	{
		MDToken ParentTypeRefToken;

		struct {
			UInt8 Accessibility;
			bool IsSealed : 1;
			bool IsAbstract : 1;
			bool IsStruct : 1;
			bool IsInterface : 1;
			bool IsAttribute : 1;
			bool IsGeneric : 1;
		} Flags;
		Int32 FieldCount;
		MDToken* FieldTokens;
		Int32 MethodCount;
		MDToken* MethodTokens;
		Int32 PropertyCount;
		MDToken* PropertyTokens;
		Int32 EventCount;
		MDToken* EventTokens;
		Int32 InterfaceCount;
		MDToken* InterfaceTokens;
		Int32 GenericParameterCount;
		MDToken* GenericParameterTokens;
	};
}