//#include "RetargetableGenerator.h"
//#include "..\..\..\Exception\RuntimeException.h"
//#include "..\..\..\Meta\TypeDescriptor.h"
//#include "..\..\..\Platform\PlatformSpecialization.h"
//#include <set>
//#include <ranges>
//
//#define POOL mContext->Memory
//
//void RTJ::Hex::RetargetableGenerator::InvalidateWithinBasicBlock(BasicBlock* bb, Int32 livenessIndex)
//{
//	Int32 next = livenessIndex + 1;
//	if (next >= bb->Liveness.size())
//		bb->RegisterContext->InvalidateLocalVariables(bb->Liveness[livenessIndex]);
//	else
//		bb->RegisterContext->InvalidateLocalVariables(bb->Liveness[livenessIndex] - bb->Liveness[next]);
//}
//
//void RTJ::Hex::RetargetableGenerator::InvalidateAcrossBaiscBlock()
//{
//
//}
//
//void RTJ::Hex::RetargetableGenerator::MergeContext(BasicBlock* bb)
//{
//	AllocationContext* newContext = new (POOL) AllocationContext(
//		mContext->Memory,
//		&mInterpreter);
//
//	//Speical cases: fast path
//	if (bb->BBIn.size() == 0)
//	{
//		bb->RegisterContext = newContext;
//		return;
//	}
//
//	if (bb->BBIn.size() == 1)
//	{
//		//Assign and reserve only varialbeLiveIn
//		*newContext = *bb->BBIn.front()->RegisterContext;
//		newContext->InvalidateLocalVariablesExcept(bb->VariablesLiveIn);
//		bb->RegisterContext = newContext;
//		return;
//	}
//
//	//First traverse: Find intersection of (Variable, PReg) allocation
//	std::unordered_set<RegisterAllocationChain> chainSet{};
//	VariableSet remainSet = bb->VariablesLiveIn;
//	for (auto&& in : bb->BBIn)
//	{
//		for (auto&& variableIn : remainSet)
//		{
//			auto chain = in->RegisterContext->GetAlloactionChainOf(variableIn);
//			if (!chain.has_value())
//				remainSet.Remove(variableIn);
//
//			if (!chainSet.contains(chain.value()))
//				remainSet.Remove(variableIn);	
//
//			if (mVariableLanded[in->Index].Contains(variableIn))
//				remainSet.Remove(variableIn);
//		}
//
//		if (remainSet.Count() == 0)
//			break;
//	}
//
//	//Second traverse: Land variables that has not been landed
//	for (auto&& in : bb->BBIn)
//	{
//		auto diffSet = in->VariablesLiveOut - remainSet;
//		for (auto&& variableToLand : diffSet)
//		{
//			if (!mVariableLanded[in->Index].Contains(variableToLand))
//			{
//				mVariableLanded[in->Index].Add(variableToLand);
//				//Land variable
//				auto pReg = in->RegisterContext->GetPhysicalRegister(variableToLand);
//				auto ins = mInterpreter.ProvideStore(variableToLand, pReg.value());
//				in->Instructions.push_back(ins);
//			}
//		}
//	}
//
//	for (auto&& chain : chainSet |
//		std::views::filter([&](auto&& x) { return remainSet.Contains(x.Variable); }))
//	{
//		newContext->EstablishDirectAssignment(chain);
//	}
//
//	bb->RegisterContext = newContext;
//}
//
//std::optional<std::tuple<RT::UInt8, RT::UInt16>> RTJ::Hex::RetargetableGenerator::RetriveSpillCandidate(
//	BasicBlock* bb, UInt64 mask, Int32 livenessIndex)
//{
//	auto&& liveMap = bb->Liveness;
//	VariableSet set{};
//	for (auto&& variable : liveMap[livenessIndex])
//	{
//		auto physicalReg = mRegContext->GetPhysicalRegister(variable);
//		if (physicalReg.has_value() && Bit::TestAt(mask, physicalReg.value()))
//			set.Add(variable);
//	}
//
//	//Impossible
//	if (set.Count() == 0)
//		return {};
//
//	for (Int32 i = livenessIndex + 1; i < liveMap.size(); ++i)
//	{
//		auto newOne = set & liveMap[i];
//		if (newOne.Count() == 0)
//			break;
//		set = std::move(newOne);
//	}
//
//	return std::make_tuple(
//		mRegContext->GetVirtualRegister(set.Front()).value(),
//		set.Front());
//}
//
//void RTJ::Hex::RetargetableGenerator::ChooseCandidate()
//{
//	/* Trackable flag will be reused here. And our criteria of candidates are:
//	* 1. Local size <= sizeof(void*)
//	* 2. Use count > 0
//	*/
//
//	for (auto&& local : mContext->LocalAttaches)
//	{
//		if (local.UseCount > 0 && local.JITVariableType->GetSize() <= sizeof(void*))
//			local.Flags |= LocalAttachedFlags::Trackable;
//		else
//			local.Flags &= ~LocalAttachedFlags::Trackable;
//	}
//
//	for (auto&& argument : mContext->ArgumentAttaches)
//	{
//		if (argument.UseCount > 0 && argument.JITVariableType->GetSize() <= sizeof(void*))
//			argument.Flags |= LocalAttachedFlags::Trackable;
//		else
//			argument.Flags &= ~LocalAttachedFlags::Trackable;
//	}
//}
//
//void RTJ::Hex::RetargetableGenerator::BuildLivenessDurationPhaseOne()
//{
//	for (auto&& basicBlock : mContext->BBs)
//	{
//		/* Here we can assume that all the nodes are flattened by Linearizer.
//		 * All the depth should not be greater than 3 (maybe 4?)
//		 */
//		Statement* tail = basicBlock->Now;
//		Int32 livenessMapIndex = 0;
//
//		//Iterate over normal body
//		while (tail != nullptr && tail->Next != nullptr)
//		{
//			tail = tail->Next;
//			livenessMapIndex++;
//		}
//		//Condition statement
//		if (basicBlock->BranchConditionValue != nullptr)
//			livenessMapIndex++;
//
//		auto&& liveness = basicBlock->Liveness = std::move(LivenessMapT(livenessMapIndex + 1));
//		VariableSet liveSet{};
//		//In reverse order
//		if (basicBlock->BranchConditionValue != nullptr)
//		{
//			UpdateLiveSet(basicBlock->BranchConditionValue, basicBlock, liveSet);
//			liveness[livenessMapIndex--] = liveSet;
//		}
//
//		for (Statement* stmt = tail; stmt != nullptr; stmt = stmt->Prev)
//		{
//			UpdateLiveSet(stmt->Now, basicBlock, liveSet);
//			liveness[livenessMapIndex--] = liveSet;
//		}
//	}
//}
//
//void RTJ::Hex::RetargetableGenerator::BuildLivenessDurationPhaseTwo()
//{
//	BitSet stableMap(mContext->BBs.size());
//
//	while (!stableMap.IsOne())
//	{
//		for (Int32 i = 0; i < stableMap.Count(); i++)
//		{
//			auto&& basicBlock = mContext->BBs[i];
//			//Reduce computation for stable BB
//			if (!stableMap.Test(i) && !basicBlock->IsUnreachable())
//			{
//				auto&& oldIn = basicBlock->VariablesLiveIn;
//				auto&& oldOut = basicBlock->VariablesLiveOut;
//
//				auto newIn = basicBlock->VariablesUse | (oldOut - basicBlock->VariablesDef);
//				auto newOut = VariableSet{};
//
//				basicBlock->ForeachSuccessor(
//					[&](auto successor)
//					{
//						newOut = std::move(newOut | successor->VariablesLiveIn);
//					});
//
//				/* We only compare newIn with oldIn because:
//				* 1. Changes from LiveOut will be propagated to LiveIn
//				* 2. LiveIn changes will be propagated to predecessors' LiveOut
//				*/
//
//				if (newIn != oldIn)
//				{
//					//Propagate to predecessors
//					for (auto&& bbIn : basicBlock->BBIn)
//						stableMap.SetZero(bbIn->Index);
//
//					//Update to new set
//					oldIn = std::move(newIn);
//					oldOut = std::move(newOut);
//				}
//				else
//					stableMap.SetOne(i);
//			}
//		}
//	}
//}
//
//void RTJ::Hex::RetargetableGenerator::BuildLivenessDuration()
//{
//	BuildLivenessDurationPhaseOne();
//	BuildLivenessDurationPhaseTwo();
//}
//
//void RTJ::Hex::RetargetableGenerator::BuildTopologciallySortedBB(BasicBlock* bb, BitSet& visited)
//{
//	visited.SetOne(bb->Index);
//
//	bb->ForeachSuccessor([&](BasicBlock* successor) {
//		if (!visited.Test(successor->Index))
//			BuildTopologciallySortedBB(successor, visited);
//		});
//
//	//In reverse order
//	mSortedBB.push_back(bb);
//}
//
//void RTJ::Hex::RetargetableGenerator::BuildTopologciallySortedBB()
//{
//	BitSet visit(mContext->BBs.size());
//	while (!visit.IsOne())
//	{
//		Int32 index = visit.ScanBiggestUnsetIndex();
//		BuildTopologciallySortedBB(mContext->BBs[index], visit);
//	}
//	std::reverse(mContext->BBs.begin(), mContext->BBs.end());
//}
//
//void RTJ::Hex::RetargetableGenerator::AllocateRegisters()
//{
//	for (auto&& basicBlock : mSortedBB)
//	{
//		//Call merge method at first
//		MergeContext(basicBlock);
//
//		Int32 index = 0;
//		for (Statement* stmt = basicBlock->Now; stmt != nullptr; stmt = stmt->Next, index++)
//		{
//			if (stmt->Now->Is(NodeKinds::MorphedCall))
//			{
//				//Do special generation
//				AllocateRegisterFor(basicBlock, index, stmt->Now->As<MorphedCallNode>(), nullptr);
//				continue;
//			}
//			else 
//			{
//				if (stmt->Now->Is(NodeKinds::Store))
//				{
//					auto store = stmt->Now->As<StoreNode>();
//					if (store->Source->Is(NodeKinds::MorphedCall))
//					{
//						AllocateRegisterFor(basicBlock, index, store->Source->As<MorphedCallNode>(), store);
//						continue;
//					}
//				}
//				mInterpreter.Interpret(stmt->Now,
//					[&](ConcreteInstruction ins) { this->AllocateRegisterFor(basicBlock, index, ins); });
//			}
//
//			//Clean up context according to liveness
//			InvalidateWithinBasicBlock(basicBlock, index);
//		}
//
//		//If we need branch instruction
//		if (basicBlock->BranchKind != PPKind::Sequential)
//		{
//			mInterpreter.InterpretBranch(basicBlock->BranchConditionValue,
//				basicBlock->BranchedBB->Index,
//				[&](ConcreteInstruction ins) { this->AllocateRegisterFor(basicBlock, index, ins); });
//
//			//Clean up context according to liveness
//			InvalidateAcrossBaiscBlock();
//		}
//	}
//}
//
//void RTJ::Hex::RetargetableGenerator::AllocateRegisterFor(BasicBlock* bb, Int32 livenessIndex, ConcreteInstruction instruction)
//{
//	auto operands = instruction.GetOperands();
//	if (instruction.IsLocalLoad())
//	{
//		auto&& destinationRegister = operands[0];
//		auto&& sourceVariable = operands[1];
//		mRegContext->WatchOnLoad(sourceVariable.VariableIndex, destinationRegister.VirtualRegister);
//		
//	}
//	else if (instruction.IsLocalStore())
//	{
//		auto&& destinationVariable = operands[0];
//		auto&& sourceRegister = operands[1];
//		mRegContext->WatchOnStore(destinationVariable.VariableIndex, sourceRegister.VirtualRegister);
//		/* It is an attempt of storing value into stack. But we cannot tell whether it
//		*  will be carried out somewhere else. So we will remove it from landed set.
//		*/
//		mVariableLanded[bb->Index].Remove(destinationVariable.VariableIndex);
//	}
//	else
//	{
//		auto&& constraints = instruction.Instruction->AddressConstraints;
//		//Prevent spilling the same register
//		UInt64 currentUseMask = 0ull;
//		//Should go backward since modifying operation is arranged at first
//		for (Int32 i = (Int32)instruction.Instruction->ConstraintLength - 1; i >= 0; --i)
//		{
//			//Traverse complex operands
//			ForeachOperand(operands[i],
//				[&](InstructionOperand& operand, Int32 index)
//				{
//					if (operand.Flags & OperandKind::VirtualRegister)
//					{
//						auto availiableMask = constraints[i].RegisterAvaliableMask & ~currentUseMask;
//						auto [emitInstruction, needSpill] = mRegContext->RequestLoad(operand.VirtualRegister, availiableMask);
//						auto variable = mRegContext->GetLocal(operand.VirtualRegister).value();
//						if (needSpill)
//						{
//							//Firstly query if we can use memory operation instead
//							if (mInterpreter.PurposeMemoryOperation(instruction, i, variable))
//								return;
//
//							//Get candidate and spill it
//							auto candidate = RetriveSpillCandidate(bb, ~currentUseMask, livenessIndex);
//							if (!candidate.has_value())
//								THROW("Unable to spill variable.");
//
//							auto&& [oldVirtualRegister, oldVariableIndex] = candidate.value();
//							auto [storeInstruction, loadInstruction] = mRegContext->RequestSpill(
//								oldVirtualRegister, oldVariableIndex, operand.VirtualRegister, variable);
//
//							/* Store the variable on register whether it's forced by spill or requested by
//							* other BB when merging the context. But be careful that when we meet the next
//							* store (not by spill) for this variable, if it's alreadly been stored, we need
//							* to remove the variable for correction. Thus we can avoid appending redundant
//							* store instructions.
//							*/
//							if (!mVariableLanded[bb->Index].Contains(oldVariableIndex))
//							{
//								mVariableLanded[bb->Index].Add(oldVariableIndex);
//								bb->Instructions.push_back(storeInstruction);
//							}
//							
//							bb->Instructions.push_back(loadInstruction);					
//						}
//						if (emitInstruction.has_value())
//							bb->Instructions.push_back(emitInstruction.value());
//
//						//Update to physical register
//						mRegContext->UsePhysicalResigster(operand, currentUseMask);
//
//						//Check if we should invalidate the relationship (v-reg, local variable)
//						if (operand.IsModifyingRegister())
//							mRegContext->InvalidateVirtualRegister(operand.VirtualRegister);
//					}
//				});
//		}
//
//		//Push the final instruction
//		bb->Instructions.push_back(instruction);
//	}
//}
//
//void RTJ::Hex::RetargetableGenerator::AllocateRegisterFor(BasicBlock* bb, Int32 livenessIndex, MorphedCallNode* call, StoreNode* store)
//{
//	//Handle the argument
//	ForeachInlined(call->Arguments, call->ArgumentCount, 
//		[&](TreeNode* arg, Int32 index) 
//		{
//			auto&& passway = call->CallingConvention->ArgumentPassway[index];
//			UseSpaceForCallingConvention(bb, livenessIndex, arg, passway);
//		});
//
//	CallNode* managedCall = nullptr;
//	if (call->Origin != nullptr && 
//		call->Origin->Is(NodeKinds::Call))
//	{
//		managedCall = call->Origin->As<CallNode>();
//		auto owningHeap = managedCall->Method->GetOwningTable()->GetOwningType()->GetAssembly()->Heap;
//		auto callingConv = GenerateCallingConvFor(owningHeap, managedCall->Method->GetSignature());
//
//		managedCall->Method->SetCallingConvention(callingConv);
//		ForeachInlined(managedCall->Arguments, managedCall->ArgumentCount, 
//			[&](TreeNode* arg, Int32 index) 
//			{
//				auto&& passway = callingConv->ArgumentPassway[index];
//				UseSpaceForCallingConvention(bb, livenessIndex, arg, passway);
//			});
//	}
//
//	//Some variables have already been landed, we'll scan the rest
//	mRegContext->WalkToLandVariable(
//		[&](UInt16 variable, UInt8 physicalRegister)
//		{
//			if (!mVariableLanded[bb->Index].Contains(variable))
//			{
//				bb->Instructions.push_back(mInterpreter.ProvideStore(variable, physicalRegister));
//				mVariableLanded[bb->Index].Add(variable);
//			}
//		});
//
//	//Handle return value
//	RTP::AddressConstraint returnConstraint{};	
//	if (managedCall != nullptr)
//		returnConstraint = managedCall->Method->GetCallingConvention()->ReturnPassway;
//	else
//		returnConstraint = call->CallingConvention->ReturnPassway;
//	//Check if there is need for load address into special place
//	if ((returnConstraint.Flags & MEM_C) && 
//		(returnConstraint.Flags & REG_C))
//	{
//		//Load the address of destination into specific register
//		UseSpaceForCallingConvention(bb, livenessIndex, store->Destination, returnConstraint);
//	}
//
//	//TODO: Generate call
//	mInterpreter.InterpretCall(call->NativeEntry,
//		[&](ConcreteInstruction const& ins) { bb->Instructions.push_back(ins); });
//	
//	//Clean up all the register status
//	mRegContext->CleanUp();
//}
//
//void RTJ::Hex::RetargetableGenerator::UseSpaceForCallingConvention(
//	BasicBlock* bb,
//	Int32 livenessIndex,
//	TreeNode* value, 
//	RTP::AddressConstraint const& target)
//{
//	if (!value->Is(NodeKinds::Load))
//		THROW("Linearized arguments of call should always be of type Load");
//
//	auto tryInvalidateOldVariableFromRegister = [&](std::optional<UInt16> variable, UInt8 physicalRegister)
//	{
//		if (auto local = mRegContext->GetLocalByPhysicalRegister(physicalRegister);
//			local.has_value())
//		{
//			if (variable == local.value())
//				return;
//
//			/* Firstly check if this variable crosses the call.
//			*  If it's totally dead after call, then we should not store it.
//			*/
//
//			if (IsValueUnusedAfter(bb, livenessIndex, variable.value()) &&
//				!mVariableLanded[bb->Index].Contains(local.value()))
//			{
//				bb->Instructions.push_back(mInterpreter.ProvideStore(local.value(), physicalRegister));
//				mVariableLanded[bb->Index].Add(local.value());
//			}
//		}
//	};
//
//	auto occupyPhysicalRegister = [&](UInt16 variable, UInt8 physicalRegister)
//	{
//		tryInvalidateOldVariableFromRegister(variable, physicalRegister);
//		mRegContext->EstablishDirectAssignment({ variable, physicalRegister });
//	};
//
//	bool isValueRegisterTarget =
//		(target.Flags & RTP::AddressConstraintFlags::SpecificRegister) &&
//		!(target.Flags & RTP::AddressConstraintFlags::Memory);
//
//	InstructionOperand sourceOperand{};
//
//	auto load = value->As<LoadNode>();
//	auto source = load->Source;
//	if (source->Is(NodeKinds::LocalVariable))
//	{
//		UInt16 variable = source->As<LocalVariableNode>()->LocalIndex;
//		//Set to direct load at first
//		sourceOperand.Kind = OperandKind::Local;
//		if (load->LoadType == SLMode::Direct)
//		{
//			//For direct value load, check assigned physical register
//			if (isValueRegisterTarget)
//				occupyPhysicalRegister(variable, target.SingleRegister);
//		}
//	}
//	else if (source->Is(NodeKinds::Constant))
//	{
//		auto constant = source->As<ConstantNode>();
//		//Map to reserved core type range
//		sourceOperand.Kind = constant->CoreType + OperandKind::CoreTypesRangeBase;
//		//Copy all
//		sourceOperand.Immediate64 = constant->U8;
//
//		if (isValueRegisterTarget)
//			tryInvalidateOldVariableFromRegister({}, target.SingleRegister);
//	}
//
//	mInterpreter.InterpretLoad(
//		target,
//		sourceOperand,
//		[&](ConcreteInstruction const& ins)
//		{
//			bb->Instructions.push_back(ins);
//		});
//}
//
//bool RTJ::Hex::RetargetableGenerator::IsValueUnusedAfter(BasicBlock* bb, Int32 livenessIndex, UInt16 variableIndex) const
//{
//	Int32 nextIndex = livenessIndex + 1;
//	if (nextIndex >= bb->Liveness.size())
//		return false;
//	return bb->Liveness[nextIndex].Contains(variableIndex);
//}
//
//void RTJ::Hex::RetargetableGenerator::UpdateLiveSet(TreeNode* node, BasicBlock* currentBB, VariableSet& liveSet)
//{
//	auto isQualified = [this](UInt16 index) -> std::optional<UInt16> {
//		UInt16 indexValue = LocalVariableNode::GetIndex(index);
//		if (LocalVariableNode::IsArgument(index))
//			return mContext->ArgumentAttaches[indexValue].IsTrackable();
//		else
//			return mContext->LocalAttaches[indexValue].IsTrackable();
//		return {};
//	};
//
//	auto use = [&](UInt16 originIndexValue)
//	{
//		if (auto index = isQualified(originIndexValue); index.has_value())
//		{
//			auto indexValue = index.value();
//			currentBB->VariablesUse.Add(indexValue);
//			liveSet.Add(indexValue);
//		}
//	};
//
//	auto kill = [&](UInt16 originIndexValue)
//	{
//		if (auto index = isQualified(originIndexValue); index.has_value())
//		{
//			auto indexValue = index.value();
//			currentBB->VariablesDef.Add(indexValue);
//			liveSet.Remove(indexValue);
//		}
//	};
//
//	switch (node->Kind)
//	{
//	case NodeKinds::Store:
//	{
//		auto storeNode = node->As<StoreNode>();
//		if (auto variable = GuardedDestinationExtract(storeNode))
//			kill(variable->LocalIndex);
//
//		//The source node may be direct morphed call
//		auto sourceNode = storeNode->Source;
//		if (sourceNode->Is(NodeKinds::MorphedCall))
//			UpdateLiveSet(sourceNode, currentBB, liveSet);
//		break;
//	}
//	case NodeKinds::Load:
//	{
//		if (auto variable = GuardedSourceExtract(node->As<LoadNode>()))
//			use(variable->LocalIndex);
//		break;
//	}
//	case NodeKinds::MorphedCall:
//	{
//		auto call = node->As<MorphedCallNode>();
//		ForeachInlined(call->Arguments, call->ArgumentCount,
//			[&](auto argument) { UpdateLiveSet(argument, currentBB, liveSet); });
//
//		auto origin = call->Origin;
//		if (origin->Is(NodeKinds::Call))
//		{
//			auto managedCall = origin->As<CallNode>();
//			ForeachInlined(managedCall->Arguments, managedCall->ArgumentCount,
//				[&](auto argument) { UpdateLiveSet(argument, currentBB, liveSet); });
//		}
//		break;
//	}
//	default:
//		THROW("Should not reach here");
//	}
//}
//
//RTJ::Hex::LocalVariableNode* RTJ::Hex::RetargetableGenerator::GuardedDestinationExtract(StoreNode* store)
//{
//	auto dest = store->Destination;
//
//	if (dest->Is(NodeKinds::LocalVariable))
//		return dest->As<LocalVariableNode>();
//	return nullptr;
//}
//
//RTJ::Hex::LocalVariableNode* RTJ::Hex::RetargetableGenerator::GuardedSourceExtract(LoadNode* store)
//{
//	auto source = store->Source;
//
//	if (source->Is(NodeKinds::LocalVariable))
//		return source->As<LocalVariableNode>();
//	return nullptr;
//}
//
//RTJ::Hex::RetargetableGenerator::RetargetableGenerator(HexJITContext* context) : mContext(context), mInterpreter(context->Memory)
//{
//}
//
//RTJ::Hex::BasicBlock* RTJ::Hex::RetargetableGenerator::PassThrough()
//{
//	ChooseCandidate();
//	BuildLivenessDuration();
//	this->BuildTopologciallySortedBB();
//	AllocateRegisters();
//	return mContext->BBs.front();
//}
//
//RTJ::Hex::AllocationContext::AllocationContext(RTMM::SegmentHeap* heap, InterpreterT* interpreter)
//	: mHeap(heap), mInterpreter(interpreter)
//{
//}
//
//void RTJ::Hex::AllocationContext::WatchOnLoad(UInt16 variableIndex, UInt8 newVirtualRegister)
//{
//	//Check if there's any alreadly in state mapping
//	if (auto iterator = mLocal2VReg.find(variableIndex); iterator != mLocal2VReg.end())
//	{
//		/* We don't care whether the physical register is allocated
//		* or just on-hold. We just simply inherit the value from the
//		* history and it will be used when we encounter the usage of
//		* v-register
//		*/
//		auto&& virtualRegister = iterator->second;
//
//		if (virtualRegister == ReservedVirtualRegister)
//		{
//			if (auto pRegLocator = mLocal2PReg.find(variableIndex);
//				pRegLocator != mLocal2PReg.end())
//			{
//				auto physicalRegister = pRegLocator->second;
//				//Remove temporary mapping
//				mLocal2PReg.erase(pRegLocator);
//				//Update to new mapping
//				mVReg2PReg[newVirtualRegister] = physicalRegister;
//			}
//		}
//		else if (virtualRegister != newVirtualRegister)
//		{
//			//Check if origin allocated virtual register is the same with new one
//			// 
//			//Remove origin mapping
//			mVReg2Local.erase(virtualRegister);
//			//Update to new mapping
//			mVReg2Local[newVirtualRegister] = variableIndex;
//			//Update virtual register
//			virtualRegister = newVirtualRegister;
//
//			//Check if there is already a physical register (renaming)
//			if (auto pRegLocator = mVReg2PReg.find(virtualRegister);
//				pRegLocator != mVReg2PReg.end())
//			{
//				auto physicalRegister = pRegLocator->second;
//				//Remove origin mapping
//				mVReg2PReg.erase(virtualRegister);
//				//Update to new mapping
//				mVReg2PReg[newVirtualRegister] = physicalRegister;
//			}
//		}
//	}
//	else
//	{
//		//If there is non, create on-hold state
//		mVReg2Local[newVirtualRegister] = variableIndex;
//		mLocal2VReg[variableIndex] = newVirtualRegister;
//	}
//}
//
//void RTJ::Hex::AllocationContext::WatchOnStore(UInt16 variableIndex, UInt8 virtualRegister)
//{	
//	if (auto iterator = mVReg2Local.find(virtualRegister); iterator != mVReg2Local.end())
//	{
//		if (auto oldVariable = iterator->second; oldVariable != variableIndex)
//		{
//			mVReg2Local.erase(iterator);
//			mLocal2VReg.erase(oldVariable);
//		}
//	}
//
//	mVReg2Local[virtualRegister] = variableIndex;
//	mLocal2VReg[variableIndex] = virtualRegister;
//}
//
//std::optional<RT::UInt8> RTJ::Hex::AllocationContext::TryPollRegister(UInt64 mask)
//{
//	UInt64 maskedValue = mRegisterPool & mask;
//	if (maskedValue)
//	{
//		UInt8 freeRegister = Bit::LeftMostSetBit(maskedValue);
//		Bit::SetZero(mRegisterPool, freeRegister);
//		return freeRegister;
//	}
//	else
//		return {};
//}
//
//void RTJ::Hex::AllocationContext::ReturnRegister(UInt8 physicalRegister)
//{
//	Bit::SetOne(mRegisterPool, physicalRegister);
//}
//
//void RTJ::Hex::AllocationContext::UsePhysicalResigster(InstructionOperand& operand, UInt64& usedMask)
//{
//	ForeachOperand(operand,
//		[&](InstructionOperand& each)
//		{
//			each.Kind = OperandKind::Register;
//			each.Register = mVReg2PReg[each.VirtualRegister];
//		});
//}
//
//void RTJ::Hex::AllocationContext::InvalidateVirtualRegister(UInt8 virtualRegister)
//{
//	if (auto iterator = mVReg2Local.find(virtualRegister); iterator != mVReg2Local.end())
//	{
//		auto localIndex = iterator->second;
//		mVReg2Local.erase(iterator);
//		mLocal2VReg.erase(localIndex);
//	}
//}
//
//void RTJ::Hex::AllocationContext::InvalidateLocalVariable(UInt16 variable)
//{
//	if (auto directLocator = mLocal2PReg.find(variable); directLocator != mLocal2PReg.end())
//	{
//		mLocal2PReg.erase(directLocator);
//		return;
//	}
//	if (auto iterator = mLocal2VReg.find(variable); iterator != mLocal2VReg.end())
//	{
//		auto virtualRegister = iterator->second;
//		mVReg2Local.erase(virtualRegister);
//		mLocal2VReg.erase(iterator);
//	}
//}
//
//void RTJ::Hex::AllocationContext::InvalidatePVAllocation(UInt16 variable)
//{
//	if (auto vReg = mLocal2VReg.find(variable); vReg != mLocal2VReg.end())
//	{
//		auto iterator = mVReg2PReg.find(vReg->second);
//		if (iterator != mVReg2PReg.end())
//		{
//			ReturnRegister(iterator->second);
//			mVReg2PReg.erase(iterator);
//		}
//	}
//}
//
//std::tuple<std::optional<RTJ::Hex::ConcreteInstruction>, bool> 
//RTJ::Hex::AllocationContext::RequestLoad(
//	UInt8 virtualRegister,
//	UInt64 registerMask)
//{
//	if (!mVReg2PReg.contains(virtualRegister))
//	{
//		//Allocate register for virtual register
//		auto registerRequest = TryPollRegister(registerMask);
//		if (registerRequest.has_value())
//		{
//			auto newRegister = registerRequest.value();
//			//Maybe it's a completely independent instruction			
//			if (auto variableResult = mVReg2Local.find(virtualRegister); 
//				variableResult != mVReg2Local.end())
//			{
//				auto&& [_, variableIndex] = *variableResult;
//				//Get freed register and update state
//				auto instruction = mInterpreter->ProvideLoad(variableIndex, newRegister);
//				//Update mapping
//				mVReg2PReg[virtualRegister] = newRegister;
//				return { instruction, false };
//			}
//			else
//			{
//				//Update mapping
//				mVReg2PReg[virtualRegister] = newRegister;
//				return { {}, false };
//			}
//		}
//		else
//		{
//			//Need to spill
//			return { {}, true };
//		}
//	}
//	return { {}, false };
//}
//
//void RTJ::Hex::AllocationContext::EstablishDirectAssignment(RegisterAllocationChain const& chain)
//{
//	mLocal2VReg[chain.Variable] = ReservedVirtualRegister;
//	mLocal2PReg[chain.Variable] = chain.PhysicalRegister;
//	Bit::SetZero(mRegisterPool, chain.PhysicalRegister);
//}
//
//void RTJ::Hex::AllocationContext::InvalidateLocalVariables(VariableSet const& diffSet)
//{
//	for (auto&& variable : diffSet)
//	{
//		mLocal2PReg.erase(variable);
//
//		if (auto locator = mLocal2VReg.find(variable);
//			locator != mLocal2VReg.end())
//		{
//			if (auto pRegLocator = mVReg2PReg.find(locator->second); pRegLocator != mVReg2PReg.end())
//			{
//				mVReg2PReg.erase(pRegLocator);
//				ReturnRegister(pRegLocator->second);
//			}
//		}
//	}
//}
//
//void RTJ::Hex::AllocationContext::InvalidateLocalVariablesExcept(VariableSet const& set)
//{
//	for (auto iterator = mLocal2PReg.begin(); iterator != mLocal2PReg.end(); ++iterator)
//	{
//		auto&& [variable, physicalReg] = *iterator;
//		if (!set.Contains(variable))
//		{
//			ReturnRegister(physicalReg);
//			iterator = mLocal2PReg.erase(iterator);		
//			continue;
//		}
//	}
//
//	for (auto iterator = mLocal2VReg.begin(); iterator != mLocal2VReg.end(); ++iterator)
//	{
//		auto&& [variable, virtualReg] = *iterator;
//		if (!set.Contains(variable))
//		{
//			if (auto pRegLocator = mVReg2PReg.find(virtualReg);
//				pRegLocator != mVReg2PReg.end())
//			{
//				ReturnRegister(pRegLocator->second);
//				mVReg2PReg.erase(pRegLocator);
//			}
//			iterator = mLocal2PReg.erase(iterator);
//			continue;
//		}
//	}
//}
//
//std::optional<RT::UInt16>  RTJ::Hex::AllocationContext::GetLocal(UInt8 virtualRegister) const
//{
//	if(auto location = mVReg2Local.find(virtualRegister); location != mVReg2Local.end())
//		return location->second;
//	return {};
//}
//
//std::optional<RT::UInt16> RTJ::Hex::AllocationContext::GetLocalByPhysicalRegister(UInt8 physicalRegister) const
//{
//	for (auto&& [local, pReg] : mLocal2PReg)
//		if (pReg == physicalRegister)
//			return local;
//
//	std::optional<UInt8> vReg{};
//	for (auto&& [key, pReg] : mVReg2PReg)
//	{
//		if (pReg == physicalRegister)
//		{
//			vReg = key;
//			break;
//		}
//	}
//	if (!vReg.has_value())
//		return {};
//
//	if (auto localLocator = mVReg2Local.find(vReg.value());
//		localLocator != mVReg2Local.end())
//	{
//		return localLocator->second;
//	}
//
//	return {};
//}
//
//std::tuple<RTJ::Hex::ConcreteInstruction, RTJ::Hex::ConcreteInstruction>
//RTJ::Hex::AllocationContext::RequestSpill(
//	UInt8 oldVirtualRegister, UInt16 oldVariable, UInt8 newVirtualRegister, UInt16 newVariable)
//{
//	auto pRegRequest = mVReg2PReg.find(oldVirtualRegister);
//	auto pReg = pRegRequest->second;
//	//Move physical register to new virtual register
//	mVReg2PReg[newVirtualRegister] = pReg;
//	//Remove from origin
//	mVReg2PReg.erase(pRegRequest);
//	return {
//		mInterpreter->ProvideStore(oldVariable, pReg),
//		mInterpreter->ProvideLoad(newVariable, pReg)
//	};
//}
//
//std::optional<RT::UInt8> RTJ::Hex::AllocationContext::GetPhysicalRegister(UInt16 variable) const
//{
//	//Check temporary mapping
//	if (auto directPRegLocator = mLocal2PReg.find(variable);
//		directPRegLocator != mLocal2PReg.end())
//		return directPRegLocator->second;
//
//	if (auto vReg = mLocal2VReg.find(variable); vReg != mLocal2VReg.end())
//		if (auto pReg = mVReg2PReg.find(vReg->second); pReg != mVReg2PReg.end())
//			return pReg->second;
//	return {};
//}
//
//std::optional<RT::UInt8> RTJ::Hex::AllocationContext::GetVirtualRegister(UInt16 variable) const
//{
//	if (auto vReg = mLocal2VReg.find(variable); vReg != mLocal2VReg.end())
//		return vReg->second;
//	return {};
//}
//
//std::optional<RTJ::Hex::RegisterAllocationChain> RTJ::Hex::AllocationContext::GetAlloactionChainOf(UInt16 variable) const
//{
//	if (auto directPRegLocator = mLocal2PReg.find(variable);
//		directPRegLocator != mLocal2PReg.end())
//		return RegisterAllocationChain{ variable, directPRegLocator->second };
//
//	auto vRegLocator = mLocal2VReg.find(variable);
//	if (vRegLocator == mLocal2VReg.end())
//		return {};
//
//	auto pRegLocator = mVReg2PReg.find(vRegLocator->second);
//	if (pRegLocator == mVReg2PReg.end())
//		return {};
//
//	return RegisterAllocationChain{ variable, pRegLocator->second };
//}
//
//RTP::PlatformCallingConvention* RTJ::Hex::GenerateCallingConvFor(RTMM::PrivateHeap* heap, RTM::MethodSignatureDescriptor* signature)
//{
//	auto convert = [](RTM::Type* type) -> RTP::PlatformCallingArgument {
//		auto coreType = type->GetCoreType();
//		if (CoreTypes::IsIntegerLike(coreType) || CoreTypes::IsRef(coreType))
//			return { CoreTypes::GetCoreTypeSize(coreType), RTP::CallingArgumentType::Integer };
//		else if (CoreTypes::IsFloatLike(coreType))
//			return { CoreTypes::GetCoreTypeSize(coreType), RTP::CallingArgumentType::Float };
//		else
//			return { type->GetSize(), RTP::CallingArgumentType::Struct };
//	};
//
//	RTP::PlatformCallingArgument returnArg{};
//	if (auto returnType = signature->GetReturnType())
//		returnArg = convert(returnType);
//
//	auto args = signature->GetArguments();
//	std::vector<RTP::PlatformCallingArgument> callingArgs(args.Count + 1);
//	callingArgs.front() = returnArg;
//	for (Int32 i = 0; i < args.Count; ++i)
//		callingArgs[i + 1] = convert(args[i].GetType());
//
//	return RTP::PlatformCallingConventionProvider<
//		RTP::CallingConventions::JIT,
//		USE_CURRENT_PLATFORM>(heap).GetConvention(callingArgs);
//}
