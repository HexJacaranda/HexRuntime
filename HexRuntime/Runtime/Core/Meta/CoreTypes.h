#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\Utility.h"
#include <string_view>

namespace RTC
{
	/// <summary>
	/// Core types are introduced for convenience. It can be used to
	/// fasten check for special types (primitives, array, object, etc.)
	/// and to help jit frontend check type safety without querying type
	/// system.
	/// </summary>
	class CoreTypes
	{
	private:
		static constexpr Int32 SizeOfSpecialCoreType[] =
		{
			1, //Bool
			2, //Char
		};

		static constexpr std::wstring_view NameOfSpecialCoreType[] = {
			Text("[Core][global]Boolean"),
			Text("[Core][global]Char")
		};

		static constexpr Int32 SizeOfPrimitiveCoreType[] =
		{
			1, //I1
			2, //I2
			4, //I4
			8, //I8

			1, //U1
			2, //U2
			4, //U4
			8, //U8

			2, //Half
			4, //Single
			8, //Double
		};

		static constexpr std::wstring_view NameOfPrimitiveCoreType[] = {
			Text("[Core][global]Int8"),
			Text("[Core][global]Int16"),
			Text("[Core][global]Int32"),
			Text("[Core][global]Int64"),

			Text("[Core][global]UInt8"),
			Text("[Core][global]UInt16"),
			Text("[Core][global]UInt32"),
			Text("[Core][global]UInt64"),

			Text("[Core][global]Half"),
			Text("[Core][global]Float"),
			Text("[Core][global]Double"),
		};

		static constexpr Int32 SizeOfReferenceCoreType[] =
		{
			sizeof(void*), //Pointer
			sizeof(void*) //Interior Pointer
		};

		static constexpr std::wstring_view NameOfRefCoreType[] = {
			Text("Invalid"),
			Text("[Core][global]Interior<Canon>")
		};

		static constexpr std::wstring_view NameOfDetailedCoreType[] = {
			Text("[Core][global]Object"),
			Text("[Core][global]Array<Canon>"),
			Text("[Core][global]Array<Canon>"),
			Text("[Core][global]String"),
			Text("[Core][global]Delegate")
		};
	public:
		//Primitives
		static constexpr UInt8 Bool = 0x00;
		static constexpr UInt8 Char = 0x01;

		static constexpr UInt8 I1 = 0x10;
		static constexpr UInt8 I2 = 0x11;
		static constexpr UInt8 I4 = 0x12;
		static constexpr UInt8 I8 = 0x13;

		static constexpr UInt8 U1 = 0x14;
		static constexpr UInt8 U2 = 0x15;
		static constexpr UInt8 U4 = 0x16;
		static constexpr UInt8 U8 = 0x17;

		/// <summary>
		/// Half
		/// </summary>
		static constexpr UInt8 R2 = 0x18;
		/// <summary>
		/// Single
		/// </summary>
		static constexpr UInt8 R4 = 0x19;
		/// <summary>
		/// Double
		/// </summary>
		static constexpr UInt8 R8 = 0x1A;
		/// <summary>
		/// Custom structure
		/// </summary>
		static constexpr UInt8 Struct = 0x2F;

		/// <summary>
		/// Pointer
		/// </summary>
		static constexpr UInt8 Ref = 0x3C;

		/// <summary>
		/// Interior pointer
		/// </summary>
		static constexpr UInt8 InteriorRef = 0x3D;

		//Detailed type representation
		static constexpr UInt8 Object = 0x40;
		static constexpr UInt8 SZArray = 0x41;
		static constexpr UInt8 Array = 0x42;
		static constexpr UInt8 String = 0x43;
		static constexpr UInt8 Delegate = 0x44;

		ETY = UInt8;
		//SIMD Special Encoding 0xD0 ~ 0xFF

		//Width
		VAL SIMDWidthMask = 0b11111000;
		VAL SIMD128 = 0b11000000;
		VAL SIMD256 = 0b11001000;
		VAL SIMD512 = 0b11010000;

		//Value Preference
		VAL SIMDBits = 0b00000000;
		VAL SIMDI32 = 0b00000001;
		VAL SIMDI64 = 0b00000010;
		
		VAL SIMDF32 = 0b00000011;
		VAL SIMDF64 = 0b00000100;
	public:
		inline static bool IsPrimitive(UInt8 coreType) {
			return coreType < Struct;
		}
		inline static bool IsStruct(UInt8 coreType) {
			return coreType <= Struct;
		}
		inline static bool IsRef(UInt8 coreType) {
			return coreType > Struct;
		}
		inline static bool IsCategoryRef(UInt8 coreType) {
			return coreType > Struct && coreType != InteriorRef;
		}
		inline static bool IsValidCoreType(UInt8 coreType) {
			return coreType <= Delegate || coreType >= SIMD128;
		}
		inline static bool IsIntegerLike(UInt8 coreType) {
			return coreType <= U8;
		}
		inline static bool IsSignedInteger(UInt8 coreType) {
			return coreType <= I8 && I1 <= coreType;
		}
		inline static bool IsFloatLike(UInt8 coreType) {
			return R2 <= coreType && coreType <= R8;
		}
		inline static bool IsSIMD(UInt8 coreType)
		{
			return coreType >= SIMD128;
		}
		static Int32 GetCoreTypeSize(UInt8 coreType) {
			if (coreType <= Char)
				return SizeOfSpecialCoreType[coreType];
			
			if (coreType < Struct)
				return SizeOfPrimitiveCoreType[coreType - I1];

			if (coreType <= InteriorRef)
				return SizeOfReferenceCoreType[coreType - Ref];

			if (coreType <= Delegate)
				return SizeOfReferenceCoreType[Ref - Ref];

			if (coreType >= SIMD128)
			{
				switch (coreType & SIMDWidthMask)
				{
				case SIMD128:
					return 16;
				case SIMD256:
					return 32;
				case SIMD512:
					return 64;
				}
			}

			return -1;
		}

		static std::wstring_view GetCoreTypeName(UInt8 coreType) {
			if (coreType <= Char)
				return NameOfSpecialCoreType[coreType];

			if (coreType < Struct)
				return NameOfPrimitiveCoreType[coreType - I1];

			if (coreType <= InteriorRef)
				return NameOfRefCoreType[coreType - Ref];

			if (coreType <= Delegate)
				return NameOfDetailedCoreType[coreType - Object];

			return Text("Invalid");
		}
	};
}