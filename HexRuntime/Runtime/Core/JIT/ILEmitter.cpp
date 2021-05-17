#include "ILEmitter.h"

void RTJ::ILEmitter::Requires(Int32 count)
{
	Int32 size = mCurrent - mIL;
	if (size + count > mCapacity) {
		Int8* newIL = (Int8*)std::malloc(mCapacity * 2);
		if (newIL == nullptr)
			RTE::Throw(Text("OOM"));
		std::memcpy(newIL, mIL, size);
		std::free(mIL);
		mIL = newIL;
		mCurrent = newIL + size;
	}
}

void RTJ::ILEmitter::Emit(UInt8 opcode, UInt8 bae)
{
	Requires(OpcodeSize);
	Write(opcode);
	Write(bae);
}

void RTJ::ILEmitter::Emit(UInt8 opcode, UInt8 bae, UInt32 token)
{
	Requires(OpcodeSize + sizeof(UInt32));
	Write(opcode);
	Write(bae);
	Write(token);
}

void RTJ::ILEmitter::Emit(UInt8 opcode, Int32 offset)
{
	Requires(OpcodeSize + sizeof(UInt32));
	Write(opcode);
	Write(0x00);
	Write(offset);
}
