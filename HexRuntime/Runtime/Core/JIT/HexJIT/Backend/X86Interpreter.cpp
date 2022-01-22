#include "X86CodeInterpreter.h"

#define USE_INS(NAME) std::copy(NativePlatformInstruction::NAME.begin(), NativePlatformInstruction::NAME.end(), instruction.Opcode.begin())
#define NREG RTP::Register::X86::RegisterSet<RTP::CurrentWidth>

//TODO: Report
#define USE_DISP(OPERAND, VAR) (OPERAND).Kind = OperandPreference::Displacement; \
									(OPERAND).Displacement.Base = NREG::BP

#define MR_USE_INS(MR_INST, RM_INST) \
		if (MR) \
		{ \
			USE_INS(MR_INST); \
		} \
		else \
		{ \
			USE_INS(RM_INST); \
		} 

#define POOL mContext->Memory

namespace RTJ::Hex::X86
{
	void X86NativeCodeGenerator::CodeGenFor(StoreNode* store)
	{
		bool shouldNotEmit = false;

		auto source = store->Source;
		auto destination = store->Destination;
		//Special handle for morphed call
		if (source->Is(NodeKinds::MorphedCall))
			return CodeGenFor(source->As<MorphedCallNode>(), destination);

		/*---------------Handle the destination operand---------------*/
		Operand destinationOperand{};
		switch (destination->Kind)
		{
		case NodeKinds::OffsetOf:
		{
			auto offset = destination->As<OffsetOfNode>();
			RTAssert(offset->Base->Is(NodeKinds::LocalVariable));
			USE_DISP(destinationOperand, offset->Base->As<LocalVariableNode>());
			break;
		}
		case NodeKinds::ArrayOffsetOf:
		{
			auto offset = destination->As<ArrayOffsetOfNode>();
			destinationOperand.Kind = OperandPreference::SIB;

			//Set base
			RTAssert(offset->Array->Is(NodeKinds::LocalVariable));
			auto arrayVar = offset->Array->As<LocalVariableNode>();
			auto arrayReg = AllocateRegisterAndGenerateCodeFor(
				arrayVar->LocalIndex,
				NREG::Common,
				arrayVar->TypeInfo->GetCoreType());
			destinationOperand.SIB.Base = arrayReg;

			//See if we can directly use the scale
			auto scale = offset->Scale;
			switch (offset->Scale)
			{
			case 1:
			case 2:
			case 4:
			case 8:
			{
				scale = offset->Scale;
				break;
			}
			default:
			{
				//Need a separate instruction to calculate index * scale
				scale = 1;
				break;
			}
			}
			destinationOperand.SIB.Scale = scale;

			//TODO: Calculate index * scale

			RTAssert(offset->Index->Is(NodeKinds::LocalVariable));
			auto indexVar = offset->Index->As<LocalVariableNode>();
			auto indexReg = AllocateRegisterAndGenerateCodeFor(
				indexVar->LocalIndex,
				NREG::Common,
				indexVar->TypeInfo->GetCoreType());

			break;
		}
		case NodeKinds::LocalVariable:
		{
			auto local = destination->As<LocalVariableNode>();
			//TODO: Volatile requires immediate memory write

			/*If it's a local store, then we should consider the store action of JIT generated variable
			* 1. Source is immediate, no 'result' register from operation is available and thus
			* we need to require a register if necessary
			* 2. Source is local variable
			* 3. Source is operation, then we will expect a register as result and cancel emitting
			* the store instruction
			*/

			switch (source->Kind)
			{
			case NodeKinds::Load:
			{
				//TODO: Need redesign: should we allow multiple variable holds the same logical register?
				auto sourceLoad = source->As<LoadNode>();
				if (sourceLoad->Is(NodeKinds::LocalVariable))
				{
					auto onFlyReg = mRegContext->TryGetRegister(local->LocalIndex);
					if (onFlyReg.has_value())
						destinationOperand = Operand::FromRegister(onFlyReg.value());
					else
					{
						//See if it's worth allocating register
						if (IsValueUnusedAfter(mLiveness, local->LocalIndex))
						{
							USE_DISP(destinationOperand, local->LocalIndex);
							//Mark variable landed
							MarkVariableLanded(local->LocalIndex);
							break;
						}
					}
				}
			}
			case NodeKinds::Constant:
			{
				/*Why only register path for constant?
				* Because mov memory <- imm is impossible for float / double and there is no
				* mov memory <- imm64, which obviously will make instruction selection more complex
				*/
				UInt8 newRegister = AllocateRegisterAndGenerateCodeFor(
					local->LocalIndex,
					NREG::Common,
					local->TypeInfo->GetCoreType(),
					false);
				destinationOperand = Operand::FromRegister(newRegister);
				//Mark variable on the fly since write to memory is not required
				MarkVariableOnFly(local->LocalIndex);

				break;
			}
			default:
				//The same as above, see case 3.
				MarkVariableOnFly(local->LocalIndex);
				shouldNotEmit = true;
				break;
			}
			break;
		}
		default:
			THROW("Destination node kind not allowed");
		}

		/*---------------Handle the source operand---------------*/
		Operand sourceOperand{};
		
		switch (source->Kind)
		{
		case NodeKinds::Load:
		{
			auto sourceLoad = source->As<LoadNode>();
			switch (sourceLoad->Source->Kind)
			{
			case NodeKinds::LocalVariable:
			{
				auto local = sourceLoad->Source->As<LocalVariableNode>();
				//Get physical register from state
				auto nativeReg = mRegContext->TryGetRegister(local->LocalIndex);
				if (nativeReg.has_value())
				{
					sourceOperand = Operand::FromRegister(nativeReg.value());
				}
				else
				{
					/* If destination has already used the advanced addressing,
					* then we should use register and emit instruction to load it from memory.
					* If the destination is simple register, in this case, we'll
					* consider if this lives long enough or just is short-lived
					*/
					if (sourceOperand.IsAdvancedAddressing() ||
						!IsValueUnusedAfter(mLiveness, local->LocalIndex))
					{
						//Cannot use memory operation
						UInt8 newRegister = AllocateRegisterAndGenerateCodeFor(
							local->LocalIndex,
							NREG::Common,
							local->TypeInfo->GetCoreType());

						sourceOperand.Kind = OperandPreference::Register;
						sourceOperand.Register = newRegister;
					}
					else
					{
						//Pefer or forced to use memory operation
						USE_DISP(sourceOperand, local);
					}
				}
				break;
			}
			case NodeKinds::OffsetOf:
			{
				THROW("Not implemented");
				break;
			}
			case NodeKinds::ArrayOffsetOf:
			{
				THROW("Not implemented");
				break;
			}
			}	
			break;
		}
		case NodeKinds::Constant:
		{
			auto constant = source->As<ConstantNode>();
			sourceOperand.Kind = OperandPreference::Immediate;
			sourceOperand.Immediate = constant->I8;
			break;
		}
		default:
		{
			//Other operation, requires register as intermediate status
			sourceOperand = CodeGenFor(source);
			if (shouldNotEmit)
				return;
			break;
		}
		}

		//Determine the instruction
		Instruction instruction = RetriveBinaryInstruction(
			SemanticGroup::MOV,
			source->TypeInfo->GetCoreType(),
			destinationOperand.Kind, sourceOperand.Kind);

		//Emit
		EmitBinary(instruction, destinationOperand, sourceOperand);
	}

	void X86NativeCodeGenerator::MarkVariableOnFly(UInt16 variable, BasicBlock* bb)
	{
		if (bb == nullptr) bb = mCurrentBB;
		mVariableLanded[bb->Index].Remove(variable);
	}

	void X86NativeCodeGenerator::MarkVariableLanded(UInt16 variable, BasicBlock* bb)
	{
		if (bb == nullptr) bb = mCurrentBB;
		mVariableLanded[bb->Index].Add(variable);
	}

	void X86NativeCodeGenerator::CodeGenFor(MorphedCallNode* call, TreeNode* destination)
	{
		void* entry = call->NativeEntry;
		RTP::PlatformCallingConvention* callingConv = nullptr;

		if (call->Origin->Is(NodeKinds::Call))
		{
			auto managedCall = call->Origin->As<CallNode>();
			
		}
		else
			callingConv = call->CallingConvention;
	}

	void X86NativeCodeGenerator::LandVariableFor(UInt16 variable, BasicBlock* bb)
	{
		if (bb == nullptr) bb = mCurrentBB;
		auto nativeReg = bb->RegisterContext->TryGetRegister(variable);
		if (nativeReg.has_value())
		{
			MarkVariableLanded(variable, bb);
			EmitStoreR2M(
				nativeReg.value(),
				variable,
				mContext->GetLocalType(variable)->GetCoreType(),
				bb->EmitPage);			
		}
	}

	X86NativeCodeGenerator::X86NativeCodeGenerator(HexJITContext* context) :
		mContext(context)
	{
		
	}

	void X86NativeCodeGenerator::CodeGenForBranch(BasicBlock* basicBlock)
	{
		
	}

	void X86NativeCodeGenerator::Prepare()
	{
		mVariableLanded = std::move(std::vector<VariableSet>(mContext->BBs.size()));
	}

	void X86NativeCodeGenerator::StartCodeGeneration()
	{
		for (auto&& bb : mContext->TopologicallySortedBBs)
		{
			mCurrentBB = bb;
			mLiveness = 0;

			MergeContext();
			for (auto stmt = bb->Now; stmt != nullptr; stmt = stmt->Next, mLiveness++)
				if (stmt->Now != nullptr)
					CodeGenFor(stmt->Now);

			if (bb->BranchKind != PPKind::Sequential)
				CodeGenForBranch(bb);
		}
	}

	BasicBlock* X86NativeCodeGenerator::PassThrough()
	{
		Prepare();
		StartCodeGeneration();
		return mContext->BBs.front();
	}

	void X86NativeCodeGenerator::MergeContext()
	{
		//Special case
		if (mCurrentBB->BBIn.size() == 1)
		{
			mCurrentBB->RegisterContext = mRegContext =
				new (POOL) RegisterAllocationContext(*mCurrentBB->BBIn.front()->RegisterContext);
			return;
		}

		mCurrentBB->RegisterContext = mRegContext = new (POOL) RegisterAllocationContext();
		if (mCurrentBB->BBIn.size() == 0)
			return;

		//Need a clearer and correct design
		//For now we'll just land all the variable use
	
		for (auto&& in : mCurrentBB->BBIn)
		{
			for (auto&& var : mCurrentBB->VariablesLiveIn)
			{
				if (in->VariablesDef.Contains(var))
					LandVariableFor(var, in);
			}
		}

		//VariableSet remainSet = mCurrentBB->VariablesUse;
		//std::unordered_map<UInt16, UInt8> allocationMap{};

		//for (auto&& in : mCurrentBB->BBIn)
		//{
		//	for (auto iterator = remainSet.begin(); iterator != remainSet.end(); ++iterator)
		//	{
		//		auto variable = *iterator;
		//		auto physicalReg = in->RegisterContext->TryGetRegister(variable);

		//		if (auto locator = allocationMap.find(variable);
		//			locator != allocationMap.end())
		//		{
		//			//If there is
		//			if ((physicalReg.has_value() && physicalReg.value() != locator->second) ||
		//				!physicalReg.has_value())

		//			{
		//				/* Invalid allocation, need to remove record in allocation map and remainSet.
		//				*  1. Origin: A -> RAX , New: A ¡ª> RBX
		//				*  2. Origin: A -> RAX , New: A -> {}
		//				*/
		//				iterator = remainSet.erase(iterator);
		//				allocationMap.erase(locator);
		//				continue;
		//			}
		//		}
		//		else
		//		{
		//			//For empty allocation, remove it from remainSet
		//			if (physicalReg.has_value())
		//				allocationMap[variable] = physicalReg.value();
		//			else
		//				iterator = remainSet.erase(iterator);
		//		}
		//	}
		//	if (remainSet.Count() == 0)
		//		break;
		//}

		////Handle variable land
		//for (auto&& in : mCurrentBB->BBIn)
		//{
		//	auto landGlobalSet = in->VariablesDef & in->VariablesLiveOut;
		//	for (auto&& var : landGlobalSet)
		//	{
		//		if (!remainSet.Contains(var))
		//			LandVariableFor(var, in);
		//	}
		//}
	}

	UInt8 X86NativeCodeGenerator::AllocateRegisterAndGenerateCodeFor(UInt16 variable, UInt64 mask, UInt8 coreType, bool requireLoading)
	{
		auto [reg, furtherOperation] = mRegContext->AllocateRegisterFor(variable, mask);
		if (furtherOperation)
		{
			auto newRegister = mRegContext->TryGetFreeRegister(mask);
			if (!newRegister.has_value())
			{
				std::optional<UInt16> spillCandidate{};
				
				if (!spillCandidate.has_value())
					THROW("Unable to spill variable");
			}

			if (requireLoading)
			{
				if (reg.has_value())
				{
					//Do register transfer
					EmitLoadR2R(newRegister.value(), reg.value(), coreType);
					mRegContext->TransferRegisterFor(variable, newRegister.value());
				}
				else
				{
					//Do variable load
					EmitLoadM2R(newRegister.value(), variable, coreType);
				}
			}
		}
		else
			return reg.value();
	}

	bool X86NativeCodeGenerator::IsValueUnusedAfter(Int32 livenessIndex, UInt16 variableIndex) const
	{
		Int32 nextIndex = livenessIndex + 1;
		if (nextIndex >= mCurrentBB->Liveness.size())
			return true;
		return !mCurrentBB->Liveness[nextIndex].Contains(variableIndex);
	}

	std::optional<std::tuple<UInt8, UInt16>> X86NativeCodeGenerator::RetriveSpillCandidate(UInt64 mask, Int32 livenessIndex)
	{
		return {};
	}

	Instruction X86NativeCodeGenerator::RetriveBinaryInstruction(
		SemanticGroup semGroup, 
		UInt8 coreType, 
		OperandPreference destPreference, 
		OperandPreference sourcePreference)
	{
		Instruction instruction{};
		instruction.OperandSize = CoreTypes::GetCoreTypeSize(coreType);
		switch (semGroup)
		{
		case SemanticGroup::MOV:
		{
			bool MR = IsAdvancedAddressing(destPreference);
			bool immLoad = sourcePreference == OperandPreference::Immediate;
			switch (coreType)
			{
			case CoreTypes::I1:
			case CoreTypes::U1:
				if (immLoad)
				{
					USE_INS(MOV_RI_I1);
				}
				else
				{
					MR_USE_INS(MOV_MR_I1, MOV_RM_I1);
				}			
				break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
			case CoreTypes::U2:
			case CoreTypes::U4:
			case CoreTypes::U8:
				if (immLoad)
				{
					USE_INS(MOV_RI_IU);
					break;
				}
			case CoreTypes::Ref:
				MR_USE_INS(MOV_MR_IU, MOV_RM_IU);
				break;
			case CoreTypes::R4:
				if (immLoad)
				{
					USE_INS(MOVSS_RM);
				}
				else
				{
					MR_USE_INS(MOVSS_MR, MOVSS_RM);
				}
				break;
			case CoreTypes::R8:
				if (immLoad)
				{
					USE_INS(MOVSD_RM);
				}
				else
				{
					MR_USE_INS(MOVSD_MR, MOVSD_RM);
				}
				break;
			}
			break;
		}
		case SemanticGroup::ADD:
		{
			break;
		}
		}
		return instruction;
	}

	Operand X86NativeCodeGenerator::CodeGenFor(BinaryArithmeticNode* binary)
	{
		return {};
	}

	Operand X86NativeCodeGenerator::CodeGenFor(TreeNode* node)
	{
		switch (node->Kind)
		{
		case NodeKinds::BinaryArithmetic:
			return CodeGenFor(node->As<BinaryArithmeticNode>());
		case NodeKinds::Store:
			CodeGenFor(node->As<StoreNode>());
			return {};
		}
	}

	void X86NativeCodeGenerator::EmitLoadM2R(UInt8 nativeRegister, UInt16 variable, UInt8 coreType, EmitPage* page)
	{
		auto ins = RetriveBinaryInstruction(
			SemanticGroup::MOV, 
			coreType,
			OperandPreference::Register, 
			OperandPreference::Displacement);

		Operand source{};
		USE_DISP(source, variable);

		EmitBinary(ins, Operand::FromRegister(nativeRegister), source, page);
	}

	void X86NativeCodeGenerator::EmitLoadR2R(UInt8 toRegister, UInt8 fromRegister, UInt8 coreType, EmitPage* page)
	{
		auto ins = RetriveBinaryInstruction(
			SemanticGroup::MOV,
			coreType,
			OperandPreference::Register,
			OperandPreference::Register);

		EmitBinary(ins, Operand::FromRegister(toRegister), Operand::FromRegister(fromRegister), page);
	}

	void X86NativeCodeGenerator::EmitStoreR2M(UInt8 nativeRegister, UInt16 variable, UInt8 coreType, EmitPage* page)
	{
		auto ins = RetriveBinaryInstruction(
			SemanticGroup::MOV,
			coreType,
			OperandPreference::Displacement,
			OperandPreference::Register);

		Operand dest{};
		USE_DISP(dest, variable);

		EmitBinary(ins, dest, Operand::FromRegister(nativeRegister), page);
	}

	void X86NativeCodeGenerator::EmitBinary(Instruction instruction, Operand const& left, Operand const& right, EmitPage* page)
	{
		if (page == nullptr) page = mEmitPage;
	}
}