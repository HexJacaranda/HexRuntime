#include "StackFrameGenerator.h"
#include "..\HexJITContext.h"

namespace RTJ::Hex
{
	StackFrameGenerator::StackFrameGenerator(HexJITContext* context):
		mContext(context)
	{
		mAssemblyHeap = context->Context->Assembly->Heap;
	}

	RTEE::StackFrameInfo* StackFrameGenerator::Generate()
	{
		Int32 current = 0;
		auto getAndUpdate = [&](Int32 size) -> Int32 {
			Int32 ret = current;
			Int32 remain = size % Alignment;
			if (remain > 0)
				size = size - remain + Alignment;
			current += size;
			return ret;
		};

		std::vector<RTEE::StackFrameSlot> slots;

		//Calculate argument first
		auto&& arguments = mContext->ArgumentAttaches;
		for (Int32 i = 0; i < arguments.size(); ++i)
		{
			auto&& argument = arguments[i];
			if (argument.Flags & LocalAttachedFlags::ShouldGenerateLayout)
			{
				Int32 offset = getAndUpdate(argument.GetType()->GetLayoutSize());
				slots.push_back(
					RTEE::StackFrameSlot
					{
						offset,
						argument.GetType(),
						(UInt16)(i | LocalVariableNode::ArgumentFlag)
					});

				argument.Offset = offset;
			}
		}

		auto&& variables = mContext->LocalAttaches;
		for (Int32 i = 0; i < variables.size(); ++i)
		{
			auto&& variable = variables[i];
			if (variable.Flags & LocalAttachedFlags::ShouldGenerateLayout)
			{
				Int32 offset = getAndUpdate(variable.GetType()->GetLayoutSize());
				slots.push_back(
					RTEE::StackFrameSlot
					{
						offset,
						variable.GetType(),
						(UInt16)(i)
					});

				variable.Offset = offset;
			}
		}

		RTEE::StackFrameInfo* stackFrame = new (mAssemblyHeap) RTEE::StackFrameInfo();
		stackFrame->SlotCount = slots.size();
		stackFrame->Slots = new (mAssemblyHeap) RTEE::StackFrameSlot[slots.size()];
		std::memcpy(stackFrame->Slots, slots.data(), sizeof(RTEE::StackFrameSlot) * slots.size());
		stackFrame->StackSize = current;

		return stackFrame;
	}
}