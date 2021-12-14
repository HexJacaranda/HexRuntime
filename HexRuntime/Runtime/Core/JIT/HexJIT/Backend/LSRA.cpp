#include "LSRA.h"
#include "..\..\..\Exception\RuntimeException.h"
#include "..\..\..\..\Bit.h"

RTJ::Hex::RegisterAllocationState* RTJ::Hex::LSRA::TryGetRegisterState(UInt16 localIndex) const
{

}

RTJ::Hex::RegisterAllocationState* RTJ::Hex::LSRA::TryGetRegisterStateFromVirtualRegister(UInt16 virtualRegister) const
{

}

std::optional<RT::UInt8> RTJ::Hex::LSRA::TryPollRegister(UInt64 mask)
{
	UInt64 maskedValue = mRegisterPool & mask;
	if (maskedValue)
	{
		UInt8 freeRegister = Bit::LeftMostSetBit(maskedValue);
		Bit::SetZero(mRegisterPool, freeRegister);
		return freeRegister;
	}
	else
		return {};
}

bool RTJ::Hex::LSRA::TryInheritFromContext(UInt16 variableIndex, UInt8 newVirtualRegister)
{
	auto iterator = mStateMapping.find(variableIndex);
	//Check if there's any alreadly on register
	if (iterator != mStateMapping.end())
	{
		auto&& state = iterator->second;

		//Remove the origin v-register mapping
		mVirtualRegisterToLocal.erase(state.VirtualRegister);
		//Insert new v-register mapping
		mVirtualRegisterToLocal[newVirtualRegister] = variableIndex;
		//Update state mapping
		state.VirtualRegister = newVirtualRegister;
		return true;
	}
	else
		return false;
}

bool RTJ::Hex::LSRA::TryAlloacteRegister(UInt16 variableIndex, UInt8 virtualRegister, UInt64 registerRequirement)
{
	auto newRegister = TryPollRegister(registerRequirement);
	if (!newRegister.has_value())
		return false;

	auto registerValue = newRegister.value();
	
	//Set mapping
	mVirtualRegisterToLocal[virtualRegister] = variableIndex;
	mStateMapping[variableIndex] = RegisterAllocationState(variableIndex, registerValue, virtualRegister);
}

void RTJ::Hex::LSRA::SpillVariableFor(UInt16 variableIndex, UInt8 virtualRegister)
{
	mVirtualRegisterToLocal[virtualRegister] = variableIndex;
	mStateMapping[variableIndex] = RegisterAllocationState(variableIndex, RegisterAllocationState::SpilledRegister, virtualRegister);
}

void RTJ::Hex::LSRA::ChooseCandidate()
{
	/* Trackable flag will be reused here. And our criteria of candidates are:
	* 1. Local size <= sizeof(void*)
	* 2. Use count > 0
	*/

	for (auto&& local : mContext->LocalAttaches)
	{
		if (local.UseCount > 0 && local.JITVariableType->GetSize() <= sizeof(void*))
			local.Flags |= LocalAttachedFlags::Trackable;
		else
			local.Flags &= ~LocalAttachedFlags::Trackable;
	}

	for (auto&& argument : mContext->ArgumentAttaches)
	{
		if (argument.UseCount > 0 && argument.JITVariableType->GetSize() <= sizeof(void*))
			argument.Flags |= LocalAttachedFlags::Trackable;
		else
			argument.Flags &= ~LocalAttachedFlags::Trackable;
	}
}

void RTJ::Hex::LSRA::ComputeLivenessDuration()
{
	auto bbHead = mContext->BBs.front();
	for (BasicBlock* bbIterator = bbHead; 
		bbIterator != nullptr; 
		bbIterator = bbIterator->Next)
	{
		for (Statement* stmtIterator = bbIterator->Now;
			stmtIterator != nullptr;
			stmtIterator = stmtIterator->Next)
		{
			/* Here we can assume that all the nodes are flattened by Linearizer.
			* All the depth should not be greater than 3 (maybe 4?)
			*/
			UpdateLivenessFor(stmtIterator->Now);
			mLivenessIndex++;
		}

		if (bbIterator->BranchConditionValue != nullptr)
		{
			UpdateLivenessFor(bbIterator->BranchConditionValue);
			mLivenessIndex++;
		}
	}

	//Reset index for allocation
	mLivenessIndex = 0;
}

void RTJ::Hex::LSRA::AllocateRegisters()
{
	ForeachStatement(mContext->BBs.front(), [&](auto now, bool _) {
		Int32 before = mInstructions.size();
		mInterpreter.Interpret(mInstructions, now);
		Int32 after = mInstructions.size();
		AllocateRegisterFor(before, after);
	});
}

void RTJ::Hex::LSRA::AllocateRegisterFor(Int32 seqBeginIndex, Int32 seqEndIndex)
{
	//Loop new instructions
	for (Int32 i = seqBeginIndex; i < seqEndIndex; ++i)
	{
		auto&& instruction = mInstructions[i];
		Int32 operandCountLimit = instruction.Instruction->AddressConstraints.size();
		auto operands = instruction.GetOperands();

		if (instruction.IsLocalLoad())
		{
			auto&& destinationReg = instruction.Operands[0];
			auto&& sourceLocalVariable = instruction.Operands[1];
			//Context register may exist for local variable loading

			if (TryInheritFromContext(sourceLocalVariable.VariableIndex, destinationReg.VirtualRegister))
				//Should not emit this
				instruction.SetFlag(ConcreteInstruction::ShouldNotEmit); 
			else
			{
				//Allocate register for this variable
				auto registerRequirement = instruction.Instruction->AddressConstraints[0].RegisterAvaliableMask;
				if (!TryAlloacteRegister(sourceLocalVariable.VariableIndex, destinationReg.VirtualRegister, registerRequirement))
				{
					//If the allocation failed, we need to spill a register for this operation
					SpillVariableFor(sourceLocalVariable.VariableIndex, destinationReg.VirtualRegister);
				}
			}
		}
		else if (instruction.IsLocalStore())
		{
			auto&& destinationLocal = instruction.Operands[0];
			auto&& sourceReg = instruction.Operands[1];

			auto&& state = mStateMapping[destinationLocal.VariableIndex];

			auto allocatedState = TryGetRegisterStateFromVirtualRegister(sourceReg.VirtualRegister);
			if (allocatedState == nullptr)
			{
				//Should throw
			}

			state = RegisterAllocationState(destinationLocal.VariableIndex, allocatedState->Register, sourceReg.VirtualRegister);
		}
		else
		{
			for (Int32 k = 0; k < operandCountLimit && operands[k].Kind != OperandKind::Unused; ++k)
			{
				auto&& operand = operands[k];
				switch (operand.Kind)
				{

				}
			}
		}
	}
}

void RTJ::Hex::LSRA::UpdateLivenessFor(TreeNode* node)
{
	auto update = [&](Int16 index, NodeKinds kind) {
		if (kind == NodeKinds::LocalVariable && !mContext->LocalAttaches[index].IsTrackable())
			return;

		if (kind == NodeKinds::Argument && !mContext->ArgumentAttaches[index].IsTrackable())
			return;

		if (kind == NodeKinds::Argument)
			index |= 0x8000;

		auto&& livenessList = mLiveness[index];

		if (livenessList.size() == 0)
			livenessList.push_back({ mLivenessIndex, mLivenessIndex + 1 });
		else
		{
			auto&& previous = livenessList.back();
			if (previous.To == mLivenessIndex)
				previous.To++;
			else
				livenessList.push_back({ mLivenessIndex, mLivenessIndex + 1 });
		}
	};

	switch (node->Kind)
	{
	case NodeKinds::Store:
	{
		if (auto variable = GuardedDestinationExtract(node->As<StoreNode>()))
			update(variable->LocalIndex, variable->Kind);
		break;
	}
	case NodeKinds::Load:
	{
		if (auto variable = GuardedSourceExtract(node->As<LoadNode>()))
			update(variable->LocalIndex, variable->Kind);
		break;
	}
	case NodeKinds::MorphedCall:
	{
		auto call = node->As<MorphedCallNode>();
		ForeachInlined(call->Arguments, call->ArgumentCount,
			[&](auto node) { UpdateLivenessFor(node); });

		auto origin = call->Origin;
		if (origin->Is(NodeKinds::Call))
		{
			auto managedCall = origin->As<CallNode>();
			ForeachInlined(managedCall->Arguments, managedCall->ArgumentCount,
				[&](auto node) { UpdateLivenessFor(node); });
		}
		break;
	}
	default:
		THROW("Should not reach here");
		break;
	}
}

RTJ::Hex::LocalVariableNode* RTJ::Hex::LSRA::GuardedDestinationExtract(StoreNode* store)
{
	auto dest = store->Destination;

	if (dest->Is(NodeKinds::LocalVariable) ||
		dest->Is(NodeKinds::Argument))
	{
		return dest->As<LocalVariableNode>();
	}
	return nullptr;
}

RTJ::Hex::LocalVariableNode* RTJ::Hex::LSRA::GuardedSourceExtract(LoadNode* store)
{
	auto source = store->Source;

	if (source->Is(NodeKinds::LocalVariable) ||
		source->Is(NodeKinds::Argument))
	{
		return source->As<LocalVariableNode>();
	}
	return nullptr;
}

RTJ::Hex::LSRA::LSRA(HexJITContext* context) : mContext(context), mInterpreter(context->Memory)
{
}

RTJ::Hex::BasicBlock* RTJ::Hex::LSRA::PassThrough()
{
	ComputeLivenessDuration();
	AllocateRegisters();
	return mContext->BBs.front();
}
