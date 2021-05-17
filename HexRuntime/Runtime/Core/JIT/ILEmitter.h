#pragma once
#include "../../RuntimeAlias.h"
#include "../Exception/RuntimeException.h"
#include <memory>

namespace RTJ
{
	/// <summary>
	/// Used for debugging JIT now, but may be used to support DynamicMethod in the future.
	/// </summary>
	class ILEmitter
	{
		static constexpr Int32 OpcodeSize = 2 * sizeof(UInt8);
		Int8* mIL = nullptr;
		Int8* mCurrent = nullptr;
		Int32 mCapacity = 16;
		void Requires(Int32 count);
		template<class T>
		void Write(T value) requires(sizeof(T) <= sizeof(Int64)) {
			*(T*)(mCurrent) = value;
			mCurrent += sizeof(T);
		}
	public:
		/// <summary>
		/// Emit opcode without param
		/// </summary>
		/// <param name="opcode"></param>
		/// <param name="bae"></param>
		void Emit(UInt8 opcode, UInt8 bae);
		/// <summary>
		/// Emit opcode with single token
		/// </summary>
		/// <param name="opcode"></param>
		/// <param name="bae"></param>
		/// <param name="token"></param>
		void Emit(UInt8 opcode, UInt8 bae, UInt32 token);
		/// <summary>
		/// Emit opcode with offset(flow control)
		/// </summary>
		/// <param name=""></param>
		/// <param name="offset"></param>
		void Emit(UInt8 opcode, Int32 offset);
	};
}