#include "ILEmitter.h"

void RTJ::ILEmitter::Requires(Int32 count)
{
	Int32 size = mCurrent - mIL;
	if (size + count > mCapacity) {
		Int32 newCapacity = 0;
		if (mCapacity * 2 < size + count)
			newCapacity = size + count;
		else
			newCapacity = mCapacity * 2;

		UInt8* newIL = (UInt8*)std::malloc(newCapacity);
		if (newIL == nullptr)
			RTE::Throw(Text("OOM"));
		else
		{
			std::memcpy(newIL, mIL, size);
			std::free(mIL);
			mIL = newIL;
			mCurrent = newIL + size;
			mCapacity = newCapacity;
		}
	}
}

RTJ::ILEmitter::ILEmitter()
{
	Requires(16);
}

RTJ::ILEmitter::~ILEmitter()
{
	if (mIL != nullptr)
	{
		std::free(mIL);
		mIL = nullptr;
	}
}

RT::UInt8* RTJ::ILEmitter::GetIL() const
{
	return mIL;
}

RT::Int32 RTJ::ILEmitter::GetLength() const
{
	return mCurrent - mIL;
}

void RTJ::ILEmitter::Emit(UInt8 opcode, UInt8 bae)
{
	Requires(OpcodeSizeWithBae);
	Write(opcode);
	Write(bae);
}

void RTJ::ILEmitter::Emit(UInt8 opcode)
{
	Requires(OpcodeSize);
	Write(opcode);
}

RT::Int32 RTJ::ILEmitter::GetOffset() const
{
	return mCurrent - mIL;
}

RTJ::FlowEntry RTJ::ILEmitter::EmitJcc(Int32 offset)
{
	FlowEntry entry{};
	Requires(OpcodeSize + sizeof(UInt32));
	Write(OpCodes::Jcc);
	entry.Offset = GetOffset();
	Write(offset);
	return entry;
}

RTJ::FlowEntry RTJ::ILEmitter::EmitJmp(Int32 offset)
{
	FlowEntry entry{};
	Requires(OpcodeSize + sizeof(UInt32));
	Write(OpCodes::Jmp);
	entry.Offset = GetOffset();
	Write(offset);
	return entry;
}

void RTJ::ILEmitter::EmitConv(UInt8 from, UInt8 to)
{
	Requires(OpcodeSize + 2 * sizeof(UInt8));
	Write(OpCodes::Conv);
	Write(from);
	Write(to);
}

void RTJ::ILEmitter::EmitAriOperation(UInt8 opcode, UInt8 coreType)
{
	Requires(OpcodeSize + sizeof(UInt8));
	Write(opcode);
	Write(coreType);
}

void RTJ::ILEmitter::EmitCompare(UInt8 condition)
{
	Requires(OpcodeSize + sizeof(UInt8));
	Write(OpCodes::Cmp);
	Write(condition);
}

void RTJ::ILEmitter::UpdateEntry(FlowEntry entry, Int32 offset)
{
	*(Int32*)(mIL + entry.Offset) = offset;
}

void RTJ::ILEmitter::EmitLoad(UInt8 opcode, Int16 index)
{
	Requires(OpcodeSize + sizeof(Int16));
	Write(opcode);
	Write(index);
}

void RTJ::ILEmitter::EmitStore(UInt8 opcode, Int16 index)
{
	Requires(OpcodeSize + sizeof(Int16));
	Write(opcode);
	Write(index);
}

void RTJ::ILEmitter::EmitRet(UInt8 bae)
{
	Requires(OpcodeSizeWithBae);
	Write(OpCodes::Ret);
	Write(bae);
}
