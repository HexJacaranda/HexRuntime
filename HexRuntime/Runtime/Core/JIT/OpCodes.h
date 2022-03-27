#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Meta\CoreTypes.h"

namespace RTJ
{
//Define one-byte il instruction
#define IL(Name, Value, BaeValue) static constexpr UInt8 Name = Value; \
							 static constexpr UInt8 Name##Bae = BaeValue

//Define extended il instruction with 0x00 leading byte (ext)
#define IL_EX(Name, Value, BaeValue) static constexpr UInt8 Name = Value; \
							 static constexpr UInt8 Name##Bae = BaeValue

	/// <summary>
	/// Number of opcodes may increases with time. Currently we support up to 255 different opcodes(8 bit).
	/// </summary>
	class OpCodes
	{
	public:
		/// <summary>
		/// Special for extended instruction.
		/// </summary>
		IL(Ext, 0x00, 0x00);

		//-----------------------------------------------------
		//Load related instructions, 0x01 ~ 0x1F space reserved
		//-----------------------------------------------------
	public:
		/// <summary>
		/// Followed by uint32 field reference token
		/// </summary>
		IL(LdFld, 0x01, 0xF1);

		/// <summary>
		/// Followed by uint32 field reference token
		/// </summary>
		IL(LdFldA, 0x02, 0xF1);

		/// <summary>
		/// Followed by int16 local index
		/// </summary>
		IL(LdLoc, 0x03, 0x01);

		/// <summary>
		/// Followed by uint16 local index
		/// </summary>
		IL(LdLocA, 0x04, 0x01);

		/// <summary>
		/// Followed by int16 argument index
		/// </summary>
		IL(LdArg, 0x05, 0x01);

		/// <summary>
		/// Followed by int16 argument index
		/// </summary>
		IL(LdArgA, 0x06, 0x01);

		/// <summary>
		/// Load element of szarray
		/// </summary>
		IL(LdElem, 0x07, 0x21);

		/// <summary>
		/// Load address of element of szarray
		/// </summary>
		IL(LdElemA, 0x08, 0x21);

		/// <summary>
		/// Load string, followed by uint32 string token
		/// </summary>
		IL(LdStr, 0x09, 0x01);

		/// <summary>
		/// Load constant literal, followed by uint8 valid CoreType and its corresponding value.
		/// </summary>
		IL(LdC, 0x0A, 0x01);

		/// <summary>
		/// Followed by uint32 method reference token
		/// </summary>
		IL(LdFn, 0x0B, 0x01);

		/// <summary>
		/// Load null
		/// </summary>
		IL(LdNull, 0x0C, 0x01);

		/// <summary>
		/// Load content from pointer or ref<>
		/// </summary>
		IL(LdInd, 0x0D, 0x11);

		//------------------------------------------------------
		//Store related instructions, 0x20 ~ 0x2F space reserved
		//------------------------------------------------------
	public:
		/// <summary>
		/// Followed by uint32 field reference token
		/// </summary>
		IL(StFld, 0x20, 0x1F);

		/// <summary>
		/// Followed by int16 local index
		/// </summary>
		IL(StLoc, 0x21, 0x10);

		/// <summary>
		/// Followed by int16 argument index
		/// </summary>
		IL(StArg, 0x22, 0x10);

		/// <summary>
		/// Store element to szarray
		/// </summary>
		IL(StElem, 0x23, 0x30);
	
		/// <summary>
		/// Followed by uint32 type reference token
		/// </summary>
		IL(StTA, 0x24, 0x11);

		//---------------------------------------------------
		//Arithmetic instructions, 0x30 ~ 0x3F space reserved
		//---------------------------------------------------
	public:
		static constexpr UInt8 BinaryOpBae = 0x21;
		static constexpr UInt8 UnaryOpBae = 0x11;
		/// <summary>
		/// Followed by uint8 operand type
		/// </summary>
		IL(Add, 0x30, BinaryOpBae);
		
		/// <summary>
		/// Followed by uint8 operand type
		/// </summary>
		IL(Sub, 0x31, BinaryOpBae);
		
		/// <summary>
		/// Followed by uint8 operand type
		/// </summary>
		IL(Mul, 0x32, BinaryOpBae);
		
		/// <summary>
		/// Followed by uint8 operand type
		/// </summary>
		IL(Div, 0x33, BinaryOpBae);

		/// <summary>
		/// Followed by uint8 operand type
		/// </summary>
		IL(Mod, 0x34, BinaryOpBae);

		/// <summary>
		/// And
		/// </summary>
		IL(And, 0x35, BinaryOpBae);

		/// <summary>
		/// Or
		/// </summary>
		IL(Or, 0x36, BinaryOpBae);
		
		/// <summary>
		/// Xor
		/// </summary>
		IL(Xor, 0x37, BinaryOpBae);

		/// <summary>
		/// Not
		/// </summary>
		IL(Not, 0x38, UnaryOpBae);

		/// <summary>
		/// Negative
		/// </summary>
		IL(Neg, 0x39, UnaryOpBae);

		/// <summary>
		/// Followed by uint8(from), uint8(to) operand type
		/// </summary>
		IL(Conv, 0x3A, UnaryOpBae);

		/// <summary>
		/// Left shift
		/// </summary>
		IL(Shl , 0x3B, BinaryOpBae);

		/// <summary>
		/// Right shift
		/// </summary>
		IL(Shr, 0x3C, BinaryOpBae);
		
		//-----------------------------------------------------
		//Flow control instructions, 0x40 ~ 0x4F space reserved
		//-----------------------------------------------------
	public:
		/// <summary>
		/// Followed by uint32 method reference
		/// </summary>
		IL(Call, 0x40, 0xFF);
		
		/// <summary>
		/// Followed by uint32 method reference
		/// </summary>
		IL(CallVirt, 0x41, 0xFF);
		
		/// <summary>
		/// Return from current method
		/// </summary>
		IL(Ret, 0x42, 0xF0);

		/// <summary>
		/// Followed by uint8 condition
		/// </summary>
		IL(Cmp, 0x43, 0x21);
		
		/// <summary>
		/// Conditional, followed by int32 il offset
		/// </summary>
		IL(Jcc, 0x44, 0x10);
		
		/// <summary>
		/// Unconditional, followed by int32 il offset
		/// </summary>
		IL(Jmp, 0x45, 0x00);
		
		/// <summary>
		/// Throw an exception
		/// </summary>
		IL(Throw, 0x46, 0x00);
		
		/// <summary>
		/// Try block, followed by int16 il offset
		/// </summary>
		IL(Try, 0x47, 0x00);
		
		/// <summary>
		/// Catch block, followed by uint32 exception type token and int16 il offset
		/// </summary>
		IL(Catch, 0x48, 0x00);
		
		/// <summary>
		/// Finally block, followed by int16 il offset
		/// </summary>
		IL(Finally, 0x49, 0x00);

		//--------------------------------------------------
		//Accessory instructions, 0x50 ~ 0x5F space reserved
		//--------------------------------------------------
	public:
		/// <summary>
		/// Ensure R/W memory order
		/// </summary>
		IL(Volatile, 0x50, 0x00);

		/// <summary>
		/// Ensure arithmetic operation won't go wild (overflow or underflow)
		/// </summary>
		IL(ArC, 0x51, 0x00);

		//--------------------------------------------------
		//GC instructions, 0x60 ~ 0x6F space reserved
		//--------------------------------------------------
	public:
		/// <summary>
		/// New object, followed by uint32 type reference.
		/// </summary>
		IL(New, 0x60, 0x01);

		/// <summary>
		/// New array, followed by uint32 type reference.
		/// </summary>
		IL(NewArr, 0x61, 0xF1);

	public:
		//--------------------------------------------------
		//Cast instructions, 0x70 ~ 0x7F space reserved
		//--------------------------------------------------

		/// <summary>
		/// Cast check for object
		/// </summary>
		IL(Cast, 0x70, 0x11);

		/// <summary>
		/// Box value type to object
		/// </summary>
		IL(Box, 0x71, 0x11);

		/// <summary>
		/// Unbox object to value type
		/// </summary>
		IL(UnBox, 0x72, 0x11);

		/// <summary>
		/// Is certian type
		/// </summary>
		IL(Is, 0x73, 0x11);

		//-----------------------------------------------------
		//Other instructions, 0xF0 ~ 0xFE space reserved
		//-----------------------------------------------------	
	public:
		/// <summary>
		/// Duplicate the value on the top of eval-stack
		/// </summary>
		IL(Dup, 0xF0, 0x01);

		/// <summary>
		/// Pop out the value on the top of eval-stack
		/// </summary>
		IL(Pop, 0xF1, 0x10);
	public:
		/// <summary>
		/// Doing nothing
		/// </summary>
		IL(Nop, 0xFF, 0x00);
	};
#undef IL
#undef IL_EX

	/// <summary>
	/// Condition constant is encoded in 8 bits following the stack item slot.
	/// </summary>
	class CmpCondition
	{
	public:
		static constexpr UInt8 EQ = 0b0000;
		static constexpr UInt8 NE = 0b0001;
		static constexpr UInt8 GT = 0b0010;
		static constexpr UInt8 LT = 0b0011;
		static constexpr UInt8 GE = 0b0100;
		static constexpr UInt8 LE = 0b0101;

		/// <summary>
		/// Offset integer of up to 15 bytes is supported.
		/// </summary>
		static constexpr UInt8 OffsetSizeMask = 0b1111;
	};

	class OpCodeOperandTypeConstant : public CoreTypes
	{

	};
}
