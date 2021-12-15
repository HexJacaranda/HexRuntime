#include "LSRA.h"
#include "..\..\..\Exception\RuntimeException.h"
#include "..\..\..\..\Bit.h"

#define POOL mContext->Memory

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

void RTJ::Hex::LSRA::WatchOnLoad(UInt16 variableIndex, UInt8 newVirtualRegister)
{
	auto iterator = mLocal2VReg.find(variableIndex);
	//Check if there's any alreadly in state mapping
	if (iterator != mLocal2VReg.end())
	{
		/* We don't care whether the physical register is allocated
		* or just on-hold. We just simply inherit the value from the
		* history and it will be used when we encounter the usage of
		* v-register
		*/
		auto&& virtualRegister = iterator->second;

		//Check if origin allocated virtual register is the same with new one
		if (virtualRegister != newVirtualRegister)
		{
			mVReg2Local.erase(virtualRegister);
			mVReg2Local[virtualRegister] = variableIndex;
			//Update virtual register
			virtualRegister = newVirtualRegister;
		}
	}
	else
	{
		//If there is non, create on-hold state
		mVReg2Local[newVirtualRegister] = variableIndex;
		mLocal2VReg[variableIndex] = newVirtualRegister;
	}

	//Update or create <local, on-watch instruction>
	mInstructionOnWatch[variableIndex] = mInstructions[mInsIndex];
}

std::optional<RTJ::Hex::ConcreteInstruction> RTJ::Hex::LSRA::RequestLoad(
	UInt8 virtualRegister, 
	UInt64 registerMask)
{
	auto variableIndex = mVReg2Local[virtualRegister];
	if (!mVReg2PReg.contains(virtualRegister))
	{
		//Allocate register for virtual register
		auto registerRequest = TryPollRegister(registerMask);
		if (registerRequest.has_value())
		{
			//Get freed register and update state
			auto instruction = CopyInstruction(mInstructionOnWatch[variableIndex], ConcreteInstruction::LoadOperandCount);
			//Write register to instruction
			auto&& registerOperand = instruction.GetOperands()[0];
			registerOperand.Kind = OperandKind::Register;

			auto newRegister = registerRequest.value();
			registerOperand.Register = newRegister;
			//Update mapping
			mVReg2PReg[virtualRegister] = newRegister;

			return instruction;
		}
		else
		{
			//Spill one variable

		}
	}
	return {};
}

void RTJ::Hex::LSRA::WatchOnStore(UInt16 variableIndex, UInt8 virtualRegister)
{
	auto iterator = mVReg2Local.find(virtualRegister);
	if (iterator != mVReg2Local.end())
	{
		auto oldVariable = iterator->second;
		if (oldVariable != variableIndex)
		{
			mVReg2Local.erase(iterator);
			mLocal2VReg.erase(oldVariable);
		}
	}

	mVReg2Local[virtualRegister] = variableIndex;
	mLocal2VReg[variableIndex] = virtualRegister;
}

void RTJ::Hex::LSRA::RequestStore(UInt16 variableIndex, UInt8 virtualRegister)
{

}

void RTJ::Hex::LSRA::InvalidateVirtualRegister(UInt8 virtualRegister)
{
	auto iterator = mVReg2Local.find(virtualRegister);
	if (iterator != mVReg2Local.end())
	{
		auto localIndex = iterator->second;
		mVReg2Local.erase(iterator);
		mLocal2VReg.erase(localIndex);
		//Maybe we can just leave the mapping here
		mInstructionOnWatch.erase(localIndex);
	}
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
		mInsIndex = mInstructions.size();
		mInterpreter.Interpret(mInstructions, now);
		AllocateRegisterForNewSequence();
	});
}

void RTJ::Hex::LSRA::AllocateRegisterForNewSequence()
{
	//Loop new instructions
	while(mInsIndex < mInstructions.size())
	{
		auto&& instruction = mInstructions[mInsIndex];
		Int32 operandCountLimit = instruction.Instruction->AddressConstraints.size();
		auto operands = instruction.GetOperands();

		if (instruction.IsLocalLoad())
		{
			auto&& destinationRegister = operands[0];
			auto&& sourceVariable = operands[1];		
			WatchOnLoad(sourceVariable.VariableIndex, destinationRegister.VirtualRegister);
			//Should never emit
			instruction.SetFlag(ConcreteInstruction::ShouldNotEmit);
		}
		else if (instruction.IsLocalStore())
		{
			auto&& destinationVariable = operands[0];
			auto&& sourceRegister = operands[1];
			WatchOnStore(destinationVariable.VariableIndex, sourceRegister.VirtualRegister);
		}
		else
		{

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

RTJ::Hex::ConcreteInstruction RTJ::Hex::LSRA::CopyInstruction(ConcreteInstruction const& origin, Int32 operandsCount)
{
	ConcreteInstruction newOne = origin;
	if (operandsCount > 0)
	{
		InstructionOperand* operands = new (POOL) InstructionOperand[operandsCount];
		std::memcpy(operands, origin.GetOperands(), sizeof(InstructionOperand) * operandsCount);
		newOne.Operands = operands;
	}
	else
		newOne.Operands = nullptr;
	newOne.SetFlag(origin.GetFlag());
	return newOne;
}

RTJ::Hex::LSRA::LSRA(HexJITContext* context) : mContext(context), mInterpreter(context->Memory)
{
}

RTJ::Hex::BasicBlock* RTJ::Hex::LSRA::PassThrough()
{
	ChooseCandidate();
	ComputeLivenessDuration();
	AllocateRegisters();
	return mContext->BBs.front();
}
