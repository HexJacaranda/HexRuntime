#include "LSRA.h"
#include "..\..\..\Exception\RuntimeException.h"
#include "..\..\..\..\Bit.h"
#include <set>
#include <ranges>

#define POOL mContext->Memory

void RTJ::Hex::LSRA::InvalidateWithinBasicBlock()
{

}

void RTJ::Hex::LSRA::InvalidateAcrossBaiscBlock()
{

}

void RTJ::Hex::LSRA::MergeContext()
{

}

std::optional<RT::UInt8> RTJ::Hex::LSRA::RetriveSpillCandidate()
{
	return {};
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

void RTJ::Hex::LSRA::LivenessDurationBuildPass()
{
	for (auto&& basicBlock : mContext->BBs | std::views::reverse)
	{
		/* Here we can assume that all the nodes are flattened by Linearizer.
		 * All the depth should not be greater than 3 (maybe 4?)
		 */
		Statement* tail = basicBlock->Now;
		Int32 livenessMapIndex = 0;

		//Iterate over normal body
		while (tail != nullptr && tail->Next != nullptr)
		{
			tail = tail->Next;
			livenessMapIndex++;
		}
		//Condition statement
		if (basicBlock->BranchConditionValue != nullptr)
			livenessMapIndex++;

		auto&& liveness = basicBlock->Liveness = std::move(LivenessMapT(livenessMapIndex + 1));
		std::set<UInt16> liveSet{};
		//In reverse order
		if (basicBlock->BranchConditionValue != nullptr)
		{
			UpdateLiveSet(basicBlock->BranchConditionValue, basicBlock, liveSet);
			liveness[livenessMapIndex--] = liveSet;
		}

		for (Statement* stmt = tail; stmt != nullptr; stmt = stmt->Prev)
		{
			UpdateLiveSet(stmt->Now, basicBlock, liveSet);
			liveness[livenessMapIndex--] = liveSet;
		}
	}
}

void RTJ::Hex::LSRA::LivenessDurationCompletePass()
{
	for (auto&& basicBlock : mContext->BBs | std::views::reverse)
	{
		auto&& oldIn = basicBlock->VariablesLiveIn;
		auto&& oldOut = basicBlock->VariablesLiveOut;
		
		auto newIn = basicBlock->VariablesUse & (oldOut - basicBlock->VariablesDef);
	}
}

void RTJ::Hex::LSRA::ComputeLivenessDuration()
{
	LivenessDurationBuildPass();
	LivenessDurationCompletePass();
}

void RTJ::Hex::LSRA::AllocateRegisters()
{
	for (auto&& basicBlock : mContext->BBs)
	{
		//Call merge method at first
		MergeContext();

		for (Statement* stmt = basicBlock->Now;
			stmt != nullptr;
			stmt = stmt->Next)
		{
			mInterpreter.Interpret(stmt->Now,
				[this](ConcreteInstruction ins) { this->AllocateRegisterFor(ins); });

			//Clean up context according to liveness
			InvalidateWithinBasicBlock();
		}

		//If we need branch instruction
		if (basicBlock->BranchKind != PPKind::Sequential)
		{
			mInterpreter.InterpretBranch(basicBlock->BranchConditionValue,
				[this](ConcreteInstruction ins) { this->AllocateRegisterFor(ins); });

			//Clean up context according to liveness
			InvalidateAcrossBaiscBlock();
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
		mRegContext->WatchOnLoad(sourceVariable.VariableIndex, destinationRegister.VirtualRegister, instruction);
		
	}
	else if (instruction.IsLocalStore())
	{
		auto&& destinationVariable = operands[0];
		auto&& sourceRegister = operands[1];
		mRegContext->WatchOnStore(destinationVariable.VariableIndex, sourceRegister.VirtualRegister, instruction);
	}
	else
	{
		auto&& constraints = instruction.Instruction->AddressConstraints;
		for (Int32 i = 0; i < (Int32)instruction.Instruction->ConstraintLength; ++i)
		{
			auto&& operand = operands[i];
			if (operand.Flags & OperandKind::VirtualRegister)
			{
				auto [emitInstruction, needSpill] = mRegContext->RequestLoad(operand.VirtualRegister, constraints[i].RegisterAvaliableMask);

				if (needSpill)
				{

				}
				if (emitInstruction.has_value())
					mInstructions.push_back(emitInstruction.value());

				//Update to physical register
				mRegContext->UsePhysicalResigster(operand);

				//Check if we should invalidate the relationship (v-reg, local variable)
				if (operand.IsModifyingRegister())
					mRegContext->InvalidateVirtualRegister(operand.VirtualRegister);
			}
		}
	}
}

void RTJ::Hex::LSRA::UpdateLiveSet(TreeNode* node, BasicBlock* currentBB, std::set<UInt16>& liveSet)
{
	auto isQualified = [this](Int16 indexValue, NodeKinds kind) -> std::optional<UInt16> {
		if (kind == NodeKinds::LocalVariable &&
			!mContext->LocalAttaches[indexValue].IsTrackable())
			return {};

		if (kind == NodeKinds::Argument &&
			!mContext->ArgumentAttaches[indexValue].IsTrackable())
			return {};

		UInt16 index = indexValue;
		if (kind == NodeKinds::Argument)
			index |= 0x8000u;

		return index;
	}; 

	auto use = [&](Int16 originIndexValue, NodeKinds kind)
	{
		if (auto index = isQualified(originIndexValue, kind); index.has_value())
		{
			auto indexValue = index.value();
			SortedInsert(currentBB->VariablesUse, indexValue);
			liveSet.insert(indexValue);
		}
	};

	auto kill = [&](Int16 originIndexValue, NodeKinds kind)
	{
		if (auto index = isQualified(originIndexValue, kind); index.has_value())
		{
			auto indexValue = index.value();
			SortedInsert(currentBB->VariablesDef, indexValue);
			liveSet.erase(indexValue);
		}
	};

	switch (node->Kind)
	{
	case NodeKinds::Store:
	{
		if (auto variable = GuardedDestinationExtract(node->As<StoreNode>()))
			kill(variable->LocalIndex, variable->Kind);
		break;
	}
	case NodeKinds::Load:
	{
		if (auto variable = GuardedSourceExtract(node->As<LoadNode>()))
			use(variable->LocalIndex, variable->Kind);
		break;
	}
	case NodeKinds::MorphedCall:
	{
		auto call = node->As<MorphedCallNode>();
		ForeachInlined(call->Arguments, call->ArgumentCount,
			[&](auto argument) { UpdateLiveSet(argument, liveSet); });

		auto origin = call->Origin;
		if (origin->Is(NodeKinds::Call))
		{
			auto managedCall = origin->As<CallNode>();
			ForeachInlined(managedCall->Arguments, managedCall->ArgumentCount,
				[&](auto argument) { UpdateLiveSet(argument, liveSet); });
		}
		break;
	}
	default:
		THROW("Should not reach here");
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

RTJ::Hex::ConcreteInstruction RTJ::Hex::AllocationContext::CopyInstruction(ConcreteInstruction const& origin)
{
	ConcreteInstruction newOne = origin;
	auto operandsCount = origin.Instruction->ConstraintLength;
	if (operandsCount > 0)
	{
		InstructionOperand* operands = new (mHeap) InstructionOperand[operandsCount];
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

void RTJ::Hex::AllocationContext::WatchOnLoad(UInt16 variableIndex, UInt8 newVirtualRegister, ConcreteInstruction ins)
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

	//Update proto instruction
	mLoadInsOnWatch[variableIndex] = ins;
}

void RTJ::Hex::AllocationContext::WatchOnStore(UInt16 variableIndex, UInt8 virtualRegister, ConcreteInstruction ins)
{
	
	if (auto iterator = mVReg2Local.find(virtualRegister); iterator != mVReg2Local.end())
	{
		if (auto oldVariable = iterator->second; oldVariable != variableIndex)
		{
			mVReg2Local.erase(iterator);
			mLocal2VReg.erase(oldVariable);
		}
	}

	mVReg2Local[virtualRegister] = variableIndex;
	mLocal2VReg[variableIndex] = virtualRegister;
	mLoadInsOnWatch[variableIndex] = ins;
}

std::optional<RT::UInt8> RTJ::Hex::AllocationContext::TryPollRegister(UInt64 mask)
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

void RTJ::Hex::AllocationContext::ReturnRegister(UInt8 physicalRegister)
{
	Bit::SetOne(mRegisterPool, physicalRegister);
}

void RTJ::Hex::AllocationContext::UsePhysicalResigster(InstructionOperand& operand)
{
	operand.Kind = OperandKind::Register;
	operand.Register = mVReg2PReg[operand.VirtualRegister];
}

void RTJ::Hex::AllocationContext::InvalidateVirtualRegister(UInt8 virtualRegister)
{
	if (auto iterator = mVReg2Local.find(virtualRegister); iterator != mVReg2Local.end())
	{
		auto localIndex = iterator->second;
		mVReg2Local.erase(iterator);
		mLocal2VReg.erase(localIndex);
		//Maybe we can just leave the mapping here
		mLoadInsOnWatch.erase(localIndex);
	}
}

void RTJ::Hex::AllocationContext::InvalidateLocalVariable(UInt16 variable)
{
	if (auto iterator = mLocal2VReg.find(variable); iterator != mLocal2VReg.end())
	{
		auto virtualRegister = iterator->second;
		mVReg2Local.erase(virtualRegister);
		mLocal2VReg.erase(iterator);
		//Maybe we can just leave the mapping here
		mLoadInsOnWatch.erase(variable);
	}
}

void RTJ::Hex::AllocationContext::InvalidatePVAllocation(UInt16 variable)
{
	if (auto vReg = mLocal2VReg.find(variable); vReg != mLocal2VReg.end())
	{
		auto iterator = mVReg2PReg.find(vReg->second);
		if (iterator != mVReg2PReg.end())
		{
			ReturnRegister(iterator->second);
			mVReg2PReg.erase(iterator);
		}
	}
}

void RTJ::Hex::AllocationContext::TryInvalidatePVAllocation(UInt16 variable)
{
	if (auto vReg = mLocal2VReg.find(variable); vReg != mLocal2VReg.end())
	{
		auto iterator = mVReg2PReg.find(vReg->second);
		if (iterator != mVReg2PReg.end())
		{
			ReturnRegister(iterator->second);
			mVReg2PReg.erase(iterator);
		}
	}
}

std::tuple<std::optional<RTJ::Hex::ConcreteInstruction>, bool> 
RTJ::Hex::AllocationContext::RequestLoad(
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
			if (auto variableResult = mVReg2Local.find(virtualRegister); 
				variableResult != mVReg2Local.end())
			{
				auto variableIndex = variableResult->second;
				//Get freed register and update state
				auto instruction = CopyInstruction(mLoadInsOnWatch[variableIndex]);
				//Write register to instruction
				auto&& registerOperand = instruction.GetOperands()[0];
				registerOperand.Kind = OperandKind::Register;
				registerOperand.Register = newRegister;
				//Update mapping
				mVReg2PReg[virtualRegister] = newRegister;
				return { instruction, false };
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
			//Need to spill
			return { {}, true };
		}
	}
	return { {}, false };
}

std::optional<RTJ::Hex::ConcreteInstruction> 
RTJ::Hex::AllocationContext::RequestStore(UInt16 variableIndex, UInt8 virtualRegister)
{

}
