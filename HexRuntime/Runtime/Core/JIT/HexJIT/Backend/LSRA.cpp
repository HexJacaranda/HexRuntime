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

void RTJ::Hex::LSRA::WatchOnLoad(UInt16 variableIndex, UInt8 newVirtualRegister, ConcreteInstruction instruction)
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
	mLoadInsOnWatch[variableIndex] = instruction;
}

std::optional<RTJ::Hex::ConcreteInstruction> RTJ::Hex::LSRA::RequestLoad(
	UInt8 virtualRegister,
	UInt64 registerMask)
{
	if (!mVReg2PReg.contains(virtualRegister))
	{
		//Allocate register for virtual register
		auto registerRequest = TryPollRegister(registerMask);
		if (registerRequest.has_value())
		{
			auto newRegister = registerRequest.value();
			//Maybe it's a completely independent instruction
			auto variableResult = mVReg2Local.find(virtualRegister);
			if (variableResult != mVReg2Local.end())
			{
				auto variableIndex = variableResult->second;
				//Get freed register and update state
				auto instruction = CopyInstruction(mLoadInsOnWatch[variableIndex], ConcreteInstruction::LoadOperandCount);
				//Write register to instruction
				auto&& registerOperand = instruction.GetOperands()[0];
				registerOperand.Kind = OperandKind::Register;
				registerOperand.Register = newRegister;
				//Update mapping
				mVReg2PReg[virtualRegister] = newRegister;
				return instruction;
			}
			else
			{
				//Update mapping
				mVReg2PReg[virtualRegister] = newRegister;
				return {};
			}
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

void RTJ::Hex::LSRA::UsePhysicalResigster(InstructionOperand& operand)
{
	operand.Kind = OperandKind::Register;
	operand.Register = mVReg2PReg[operand.VirtualRegister];
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
		mLoadInsOnWatch.erase(localIndex);
	}
}

void RTJ::Hex::LSRA::InvalidateLocalVariable(UInt8 variable)
{
	auto iterator = mLocal2VReg.find(variable);
	if (iterator != mLocal2VReg.end())
	{
		auto virtualRegister = iterator->second;
		mVReg2Local.erase(virtualRegister);
		mLocal2VReg.erase(iterator);
		//Maybe we can just leave the mapping here
		mLoadInsOnWatch.erase(variable);
	}
}

void RTJ::Hex::LSRA::InvalidateContextWithLiveness()
{
	for (auto iterator = mLiveness.begin(); iterator != mLiveness.end(); ++iterator)
	{
		auto&& [local, durationQueue] = *iterator;
		if (!durationQueue.empty())
		{
			auto&& liveness = durationQueue.front();
			if (liveness.To <= mLivenessIndex)
				durationQueue.pop();
			else if (mLivenessIndex < liveness.From)
				InvalidateLocalVariable(local);
		}
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
	for (BasicBlock* basicBlock = bbHead;
		basicBlock != nullptr;
		basicBlock = basicBlock->Next)
	{
		for (Statement* stmt = basicBlock->Now;
			stmt != nullptr;
			stmt = stmt->Next)
		{
			/* Here we can assume that all the nodes are flattened by Linearizer.
			* All the depth should not be greater than 3 (maybe 4?)
			*/
			UpdateLivenessFor(stmt->Now);
			mLivenessIndex++;
		}

		if (basicBlock->BranchConditionValue != nullptr)
		{
			UpdateLivenessFor(basicBlock->BranchConditionValue);
			mLivenessIndex++;
		}
		if (basicBlock->BranchKind != PPKind::Sequential)
			//Gap to stop allocation context propagating
			mLivenessIndex++;
	}

	//Reset index for allocation
	mLivenessIndex = 0;
}

void RTJ::Hex::LSRA::AllocateRegisters()
{
	auto bbHead = mContext->BBs.front();
	for (BasicBlock* basicBlock = bbHead;
		basicBlock != nullptr;
		basicBlock = basicBlock->Next)
	{
		for (Statement* stmt = basicBlock->Now;
			stmt != nullptr;
			stmt = stmt->Next)
		{
			mInterpreter.Interpret(stmt->Now,
				[this](ConcreteInstruction ins) { this->AllocateRegisterFor(ins); });
			mLivenessIndex++;
			//Clean up context according to liveness
			InvalidateContextWithLiveness();
		}

		//If we need branch instruction
		if (basicBlock->BranchKind != PPKind::Sequential)
		{
			mInterpreter.InterpretBranch(basicBlock->BranchConditionValue,
				[this](ConcreteInstruction ins) { this->AllocateRegisterFor(ins); });
			mLivenessIndex++;
			if (basicBlock->BranchConditionValue != nullptr)
				mLivenessIndex++;
			//Clean up context according to liveness
			InvalidateContextWithLiveness();
		}
	}
}

void RTJ::Hex::LSRA::AllocateRegisterFor(ConcreteInstruction instruction)
{
	auto operands = instruction.GetOperands();
	if (instruction.IsLocalLoad())
	{
		auto&& destinationRegister = operands[0];
		auto&& sourceVariable = operands[1];
		WatchOnLoad(sourceVariable.VariableIndex, destinationRegister.VirtualRegister, instruction);
	}
	else if (instruction.IsLocalStore())
	{
		auto&& destinationVariable = operands[0];
		auto&& sourceRegister = operands[1];
		WatchOnStore(destinationVariable.VariableIndex, sourceRegister.VirtualRegister);
	}
	else
	{
		auto&& constraints = instruction.Instruction->AddressConstraints;
		for (Int32 i = 0; i < (Int32)instruction.Instruction->ConstraintLength; ++i)
		{
			auto&& operand = operands[i];
			if (operand.Flags & OperandKind::VirtualRegister)
			{
				auto emitInstruction = RequestLoad(operand.VirtualRegister, constraints[i].RegisterAvaliableMask);
				if (emitInstruction.has_value())
					mInstructions.push_back(emitInstruction.value());

				//Update to physical register
				UsePhysicalResigster(operand);

				//Check if we should invalidate the relationship (v-reg, local variable)
				if (operand.IsModifyingRegister())
					InvalidateVirtualRegister(operand.VirtualRegister);
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
			livenessList.push({ mLivenessIndex, mLivenessIndex + 1 });
		else
		{
			auto&& previous = livenessList.back();
			if (previous.To == mLivenessIndex)
				previous.To++;
			else
				livenessList.push({ mLivenessIndex, mLivenessIndex + 1 });
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
