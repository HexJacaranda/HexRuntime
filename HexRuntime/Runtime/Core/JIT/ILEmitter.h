#pragma once
#include "../../RuntimeAlias.h"
#include "../Exception/RuntimeException.h"
#include "OpCodes.h"
#include <memory>

namespace RTJ
{
	struct FlowEntry
	{
		Int32 Offset;
	};
	/// <summary>
	/// Used for debugging JIT now, but may be used to support DynamicMethod in the future.
	/// </summary>
	class ILEmitter
	{
		static constexpr Int32 OpcodeSize = sizeof(UInt8);
		static constexpr Int32 OpcodeSizeWithBae = 2 * sizeof(UInt8);
		UInt8* mIL = nullptr;
		UInt8* mCurrent = nullptr;
		Int32 mCapacity = 0;
		void Requires(Int32 count);
		template<class T>
		void Write(T value) requires(sizeof(T) <= sizeof(Int64)) {
			*(T*)(mCurrent) = value;
			mCurrent += sizeof(T);
		}
	public:
		ILEmitter();
		~ILEmitter();
		UInt8* GetIL()const;
		Int32 GetLength()const;
	public:
		/// <summary>
		/// Emit opcode without param
		/// </summary>
		/// <param name="opcode"></param>
		/// <param name="bae"></param>
		void Emit(UInt8 opcode, UInt8 bae);
		/// <summary>
		/// Emit opcode without bae
		/// </summary>
		/// <param name="opcode"></param>
		void Emit(UInt8 opcode);
		/// <summary>
		/// Get the IL offset of current progress.
		/// </summary>
		/// <returns></returns>
		Int32 GetOffset()const;
		/// <summary>
		/// Emit jmp with offset
		/// </summary>
		/// <param name=""></param>
		/// <param name="offset"></param>
		FlowEntry EmitJmp(Int32 offset);
		/// <summary>
		/// Emit jcc with offset
		/// </summary>
		/// <param name="condition"></param>
		/// <param name="offset"></param>
		FlowEntry EmitJcc(Int32 offset);
		/// <summary>
		/// Emit load constant
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="coreType"></param>
		/// <param name="constant"></param>
		template<class T>
		void EmitLdC(UInt8 coreType, T constant) requires(sizeof(T) <= sizeof(Int64))
		{
			Requires(OpcodeSize + sizeof(UInt8) + sizeof(T));
			Write(OpCodes::LdC);
			Write(coreType);
			Write(constant);
		}
		/// <summary>
		/// Emit convert opcode
		/// </summary>
		/// <param name="from"></param>
		/// <param name="to"></param>
		void EmitConv(UInt8 to);
		/// <summary>
		/// Emit arithmetic operation
		/// </summary>
		/// <param name="opcode"></param>
		/// <param name="coreType"></param>
		void EmitAriOperation(UInt8 opcode);
		/// <summary>
		/// Emit compare operation
		/// </summary>
		void EmitCompare(UInt8 condition);
		/// <summary>
		/// Update the previously defined flow entry.
		/// </summary>
		/// <param name="entry"></param>
		/// <param name="offset"></param>
		void UpdateEntry(FlowEntry entry, Int32 offset);
		/// <summary>
		/// Emit load local variable or argument
		/// </summary>
		/// <param name="opcode"></param>
		/// <param name="index"></param>
		void EmitLoad(UInt8 opcode, Int16 index);
		/// <summary>
		/// Emit store local variable or argument
		/// </summary>
		/// <param name="opcode"></param>
		/// <param name="index"></param>
		void EmitStore(UInt8 opcode, Int16 index);
		/// <summary>
		/// Emit return
		/// </summary>
		/// <param name="bae"></param>
		void EmitRet(UInt8 bae);
	};
}