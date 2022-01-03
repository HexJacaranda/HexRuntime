#include "LSRA.h"
#include "..\..\..\Exception\RuntimeException.h"
#include <set>
#include <ranges>

#define POOL mContext->Memory

void RTJ::Hex::LSRA::InvalidateWithinBasicBlock(BasicBlock* bb, Int32 livenessIndex)
{
	auto&& nextSet = bb->Liveness[(std::size_t)livenessIndex + 1];
	bb->RegisterContext->InvalidateLocalVariableExcept(nextSet);
}

void RTJ::Hex::LSRA::InvalidateAcrossBaiscBlock()
{

}

void RTJ::Hex::LSRA::MergeContext(BasicBlock* bb)
{
	AllocationContext* newContext = new (POOL) AllocationContext(
		mContext->Memory,
		&mInterpreter);

	//Speical cases: fast path
	if (bb->BBIn.size() == 0)
	{
		bb->RegisterContext = newContext;
		return;
	}

	if (bb->BBIn.size() == 1)
	{
		//Assign and reserve only varialbeLiveIn
		*newContext = *bb->BBIn.front()->RegisterContext;
		newContext->InvalidateLocalVariableExcept(bb->VariablesLiveIn);
		bb->RegisterContext = newContext;
		return;
	}

	//First traverse: Find intersection of (Variable, PReg) allocation
	std::unordered_set<RegisterAllocationChain> chainSet{};
	VariableSet remainSet = bb->VariablesLiveIn;
	for (auto&& in : bb->BBIn)
	{
		for (auto&& variableIn : remainSet)
		{
			auto chain = in->RegisterContext->GetAlloactionChainOf(variableIn);
			if (!chain.has_value())
				remainSet.Remove(variableIn);

			if (!chainSet.contains(chain.value()))
				remainSet.Remove(variableIn);	

			if (mVariableLanded[in->Index].Contains(variableIn))
				remainSet.Remove(variableIn);
		}

		if (remainSet.Count() == 0)
			break;
	}

	//Second traverse: Land variables that has not been landed
	for (auto&& in : bb->BBIn)
	{
		auto diffSet = in->VariablesLiveOut - remainSet;
		for (auto&& variableToLand : diffSet)
		{
			if (!mVariableLanded[in->Index].Contains(variableToLand))
			{
				mVariableLanded[in->Index].Add(variableToLand);
				//Land variable
				auto pReg = in->RegisterContext->GetPhysicalRegister(variableToLand);
				auto ins = mInterpreter.ProvideStore(variableToLand, pReg.value());
				in->Instructions.push_back(ins);
			}
		}
	}

	for (auto&& chain : chainSet |
		std::views::filter([&](auto&& x) { return remainSet.Contains(x.Variable); }))
	{
		newContext->LoadFromMergeContext(chain);
	}

	bb->RegisterContext = newContext;
}

std::optional<std::tuple<RT::UInt8, RT::UInt16>> RTJ::Hex::LSRA::RetriveSpillCandidate(
	BasicBlock* bb, UInt64 mask, Int32 livenessIndex)
{
	auto&& liveMap = bb->Liveness;
	VariableSet set{};
	for (auto&& variable : liveMap[livenessIndex])
	{
		auto physicalReg = mRegContext->GetPhysicalRegister(variable);
		if (physicalReg.has_value() && Bit::TestAt(mask, physicalReg.value()))
			set.Add(variable);
	}

	//Impossible
	if (set.Count() == 0)
		return {};

	for (Int32 i = livenessIndex + 1; i < liveMap.size(); ++i)
	{
		auto newOne = set & liveMap[i];
		if (newOne.Count() == 0)
			break;
		set = std::move(newOne);
	}

	return std::make_tuple(
		mRegContext->GetVirtualRegister(set.Front()).value(),
		set.Front());
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

void RTJ::Hex::LSRA::BuildLivenessDurationPhaseOne()
{
	for (auto&& basicBlock : mContext->BBs)
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
		VariableSet liveSet{};
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

void RTJ::Hex::LSRA::BuildLivenessDurationPhaseTwo()
{
	BitSet stableMap(mContext->BBs.size());

	while (!stableMap.IsOne())
	{
		for (Int32 i = 0; i < stableMap.Count(); i++)
		{
			auto&& basicBlock = mContext->BBs[i];
			//Reduce computation for stable BB
			if (!stableMap.Test(i) && !basicBlock->IsUnreachable())
			{
				auto&& oldIn = basicBlock->VariablesLiveIn;
				auto&& oldOut = basicBlock->VariablesLiveOut;

				auto newIn = basicBlock->VariablesUse | (oldOut - basicBlock->VariablesDef);
				auto newOut = VariableSet{};

				basicBlock->ForeachSuccessor(
					[&](auto successor)
					{
						newOut = std::move(newOut | successor->VariablesLiveIn);
					});

				/* We only compare newIn with oldIn because:
				* 1. Changes from LiveOut will be propagated to LiveIn
				* 2. LiveIn changes will be propagated to predecessors' LiveOut
				*/

				if (newIn != oldIn)
				{
					//Propagate to predecessors
					for (auto&& bbIn : basicBlock->BBIn)
						stableMap.SetZero(bbIn->Index);

					//Update to new set
					oldIn = std::move(newIn);
					oldOut = std::move(newOut);
				}
				else
					stableMap.SetOne(i);
			}
		}
	}
}

void RTJ::Hex::LSRA::BuildLivenessDuration()
{
	BuildLivenessDurationPhaseOne();
	BuildLivenessDurationPhaseTwo();
}

void RTJ::Hex::LSRA::BuildTopologciallySortedBB(BasicBlock* bb, BitSet& visited)
{
	visited.SetOne(bb->Index);

	bb->ForeachSuccessor([&](BasicBlock* successor) {
		if (!visited.Test(successor->Index))
			BuildTopologciallySortedBB(successor, visited);
		});

	//In reverse order
	mSortedBB.push_back(bb);
}

void RTJ::Hex::LSRA::BuildTopologciallySortedBB()
{
	BitSet visit(mContext->BBs.size());
	while (!visit.IsOne())
	{
		Int32 index = visit.PickRight();
		BuildTopologciallySortedBB(mContext->BBs[index], visit);
	}
	std::reverse(mContext->BBs.begin(), mContext->BBs.end());
}

void RTJ::Hex::LSRA::AllocateRegisters()
{
	for (auto&& basicBlock : mSortedBB)
	{
		//Call merge method at first
		MergeContext(basicBlock);

		Int32 index = 0;
		for (Statement* stmt = basicBlock->Now; stmt != nullptr; stmt = stmt->Next, index++)
		{
			mInterpreter.Interpret(stmt->Now,
				[&](ConcreteInstruction ins) { this->AllocateRegisterFor(basicBlock, index, ins); });

			//Clean up context according to liveness
			InvalidateWithinBasicBlock(basicBlock, index);
		}

		//If we need branch instruction
		if (basicBlock->BranchKind != PPKind::Sequential)
		{
			mInterpreter.InterpretBranch(basicBlock->BranchConditionValue,
				[&](ConcreteInstruction ins) { this->AllocateRegisterFor(basicBlock, index, ins); });

			//Clean up context according to liveness
			InvalidateAcrossBaiscBlock();
		}
	}
}

void RTJ::Hex::LSRA::AllocateRegisterFor(BasicBlock* bb, Int32 livenessIndex, ConcreteInstruction instruction)
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
		//Prevent spilling the same register
		UInt64 currentUseMask = 0ull;
		for (Int32 i = 0; i < (Int32)instruction.Instruction->ConstraintLength; ++i)
		{
			auto&& operand = operands[i];
			//TODO: Adapt to SIB which may contains multiple register references in one operand
			if (operand.Flags & OperandKind::VirtualRegister)
			{
				auto availiableMask = constraints[i].RegisterAvaliableMask & ~currentUseMask;
				auto [emitInstruction, needSpill] = mRegContext->RequestLoad(operand.VirtualRegister, availiableMask);
				auto variable = mRegContext->GetLocal(operand.VirtualRegister);
				if (needSpill)
				{
					//Get candidate and spill it
					auto candidate = RetriveSpillCandidate(bb, ~currentUseMask, livenessIndex);
					if (!candidate.has_value())
						THROW("Unable to spill variable.");

					auto&& [oldVirtualRegister, oldVariableIndex] = candidate.value();
					auto [storeInstruction, loadInstruction] = mRegContext->RequestSpill(
						oldVirtualRegister, oldVariableIndex, operand.VirtualRegister, variable.value());

					//Append instructions
					bb->Instructions.push_back(storeInstruction);
					bb->Instructions.push_back(loadInstruction);
					/* Store the variable on register whether it's forced by spill or requested by
					* other BB when merging the context. But be careful that when we meet the next
					* store (not by spill) for this variable, if it's alreadly been stored, we need
					* to remove the variable for correction. Otherwise we can avoid appending redundant
					* store instruction to the end of BB
					*/
					if (bb->VariablesLiveOut.Contains(oldVariableIndex))
						mVariableLanded[bb->Index].Add(oldVariableIndex);
				}
				if (emitInstruction.has_value())
					bb->Instructions.push_back(emitInstruction.value());

				//Update to physical register
				mRegContext->UsePhysicalResigster(operand, currentUseMask);

				//Check if we should invalidate the relationship (v-reg, local variable)
				if (operand.IsModifyingRegister())
					mRegContext->InvalidateVirtualRegister(operand.VirtualRegister);
			}
		}
	}
}

void RTJ::Hex::LSRA::UpdateLiveSet(TreeNode* node, BasicBlock* currentBB, VariableSet& liveSet)
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
			currentBB->VariablesUse.Add(indexValue);
			liveSet.Add(indexValue);
		}
	};

	auto kill = [&](Int16 originIndexValue, NodeKinds kind)
	{
		if (auto index = isQualified(originIndexValue, kind); index.has_value())
		{
			auto indexValue = index.value();
			currentBB->VariablesDef.Add(indexValue);
			liveSet.Remove(indexValue);
		}
	};

	switch (node->Kind)
	{
	case NodeKinds::Store:
	{
		auto storeNode = node->As<StoreNode>();
		if (auto variable = GuardedDestinationExtract(storeNode))
			kill(variable->LocalIndex, variable->Kind);

		//The source node may be direct morphed call
		auto sourceNode = storeNode->Source;
		if (sourceNode->Is(NodeKinds::MorphedCall))
			UpdateLiveSet(sourceNode, currentBB, liveSet);
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
			[&](auto argument) { UpdateLiveSet(argument, currentBB, liveSet); });

		auto origin = call->Origin;
		if (origin->Is(NodeKinds::Call))
		{
			auto managedCall = origin->As<CallNode>();
			ForeachInlined(managedCall->Arguments, managedCall->ArgumentCount,
				[&](auto argument) { UpdateLiveSet(argument, currentBB, liveSet); });
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

RTJ::Hex::LSRA::LSRA(HexJITContext* context) : mContext(context), mInterpreter(context->Memory)
{
}

RTJ::Hex::BasicBlock* RTJ::Hex::LSRA::PassThrough()
{
	ChooseCandidate();
	BuildLivenessDuration();
	this->BuildTopologciallySortedBB();
	AllocateRegisters();
	return mContext->BBs.front();
}

RTJ::Hex::AllocationContext::AllocationContext(RTMM::SegmentHeap* heap, InterpreterT* interpreter)
	: mHeap(heap), mInterpreter(interpreter)
{
}

void RTJ::Hex::AllocationContext::WatchOnLoad(UInt16 variableIndex, UInt8 newVirtualRegister, ConcreteInstruction ins)
{
	//Check if there's any alreadly in state mapping
	if (auto iterator = mLocal2VReg.find(variableIndex); iterator != mLocal2VReg.end())
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

	//If there is direct allocation from merging, update and remove it from map
	if (auto pRegRequest = mLocal2PReg.find(variableIndex);
		pRegRequest != mLocal2PReg.end())
	{
		mVReg2PReg[newVirtualRegister] = pRegRequest->second;
		mLocal2PReg.erase(pRegRequest);
	}
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

void RTJ::Hex::AllocationContext::UsePhysicalResigster(InstructionOperand& operand, UInt64& usedMask)
{
	//TODO: Adapt SIB
	operand.Kind = OperandKind::Register;
	operand.Register = mVReg2PReg[operand.VirtualRegister];
	Bit::SetOne(usedMask, operand.Register);
}

void RTJ::Hex::AllocationContext::InvalidateVirtualRegister(UInt8 virtualRegister)
{
	if (auto iterator = mVReg2Local.find(virtualRegister); iterator != mVReg2Local.end())
	{
		auto localIndex = iterator->second;
		mVReg2Local.erase(iterator);
		mLocal2VReg.erase(localIndex);
	}
}

void RTJ::Hex::AllocationContext::InvalidateLocalVariable(UInt16 variable)
{
	if (auto iterator = mLocal2VReg.find(variable); iterator != mLocal2VReg.end())
	{
		auto virtualRegister = iterator->second;
		mVReg2Local.erase(virtualRegister);
		mLocal2VReg.erase(iterator);
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
				auto&& [_, variableIndex] = *variableResult;
				//Get freed register and update state
				auto instruction = mInterpreter->ProvideLoad(variableIndex, newRegister);
				//Update mapping
				mVReg2PReg[virtualRegister] = newRegister;
				return { instruction, false };
			}
			else
			{
				//Update mapping
				mVReg2PReg[virtualRegister] = newRegister;
				return { {}, false };
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

void RTJ::Hex::AllocationContext::LoadFromMergeContext(RegisterAllocationChain const& chain)
{
	mLocal2PReg[chain.Variable] = chain.PhysicalRegister;
}

void RTJ::Hex::AllocationContext::InvalidateLocalVariableExcept(VariableSet const& set)
{
	//Check direct allocation then?
	auto directIterator = mLocal2PReg.begin();
	while (directIterator != mLocal2PReg.end())
	{
		auto [local, pReg] = *directIterator;
		if (!set.Contains(local))
		{
			directIterator = mLocal2PReg.erase(directIterator);
			ReturnRegister(pReg);
		}
	}

	auto iterator = mLocal2VReg.begin();
	while (iterator != mLocal2VReg.end())
	{
		auto [local, vReg] = *iterator;
		if (!set.Contains(local))
		{
			iterator = mLocal2VReg.erase(iterator);
			//Return the occupied physical register if there is
			if (auto pRegLocator = mVReg2PReg.find(vReg); pRegLocator != mVReg2PReg.end())
			{
				mVReg2PReg.erase(pRegLocator);
				ReturnRegister(pRegLocator->second);
			}
		}
	}
}

std::optional<RT::UInt16>  RTJ::Hex::AllocationContext::GetLocal(UInt8 virtualRegister)
{
	if(auto location = mVReg2Local.find(virtualRegister); location != mVReg2Local.end())
		return location->second;
	return {};
}

std::tuple<RTJ::Hex::ConcreteInstruction, RTJ::Hex::ConcreteInstruction>
RTJ::Hex::AllocationContext::RequestSpill(
	UInt8 oldVirtualRegister, UInt16 oldVariable, UInt8 newVirtualRegister, UInt16 newVariable)
{
	auto pRegRequest = mVReg2PReg.find(oldVirtualRegister);
	auto pReg = pRegRequest->second;
	//Move physical register to new virtual register
	mVReg2PReg[newVirtualRegister] = pReg;
	//Remove from origin
	mVReg2PReg.erase(pRegRequest);
	return {
		mInterpreter->ProvideStore(oldVariable, pReg),
		mInterpreter->ProvideLoad(newVariable, pReg)
	};
}

std::optional<RT::UInt8> RTJ::Hex::AllocationContext::GetPhysicalRegister(UInt16 variable)
{
	if (auto vReg = mLocal2VReg.find(variable); vReg != mLocal2VReg.end())
		if (auto pReg = mVReg2PReg.find(vReg->second); pReg != mVReg2PReg.end())
			return pReg->second;
	return {};
}

std::optional<RT::UInt8> RTJ::Hex::AllocationContext::GetVirtualRegister(UInt16 variable)
{
	if (auto vReg = mLocal2VReg.find(variable); vReg != mLocal2VReg.end())
		return vReg->second;
	return {};
}

std::optional<RTJ::Hex::RegisterAllocationChain> RTJ::Hex::AllocationContext::GetAlloactionChainOf(UInt16 variable)
{
	auto directPRegLocator = mLocal2PReg.find(variable);
	if (directPRegLocator != mLocal2PReg.end())
		return RegisterAllocationChain{ variable, directPRegLocator->second };

	auto vRegLocator = mLocal2VReg.find(variable);
	if (vRegLocator == mLocal2VReg.end())
		return {};

	auto pRegLocator = mVReg2PReg.find(vRegLocator->second);
	if (pRegLocator == mVReg2PReg.end())
		return {};

	return RegisterAllocationChain{ variable, pRegLocator->second };
}
