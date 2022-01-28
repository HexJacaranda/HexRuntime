#include "X86CodeInterpreter.h"
#include "StackFrameGenerator.h"
#include "..\..\..\..\Bit.h"

#define USE_INS(NAME) { \
				instruction.Opcode = NativePlatformInstruction::NAME.data(); \
				instruction.Length = NativePlatformInstruction::NAME.size(); \
				instruction.Flags = NativePlatformInstruction::NAME##_FLG; }

#define NREG RTP::Register::X86::RegisterSet<RTP::CurrentWidth>
#define NREG64 RTP::Register::X86::RegisterSet<RTP::Platform::Bit64>

//TODO: Report
#define USE_DISP(OPERAND, VAR)  {(OPERAND).Kind = OperandPreference::Displacement; \
								(OPERAND).Displacement.Base = NREG::BP; \
								(OPERAND).RefVariable = VAR;}

#define USE_MASK (mask & mGenContext.RegisterMask)
#define MARK_UNSPILLABLE(REG) session.Borrow((REG))
#define INVALIDATE_VAR(VAR) mRegContext->TryInvalidateFor(VAR)
#define RET_REG(REG) session.Return((REG))

#define ST_VAR (mGenContext.StoringVariable)
#define RE_REG (mGenContext.ResultRegister)

#define MR_USE_INS(MR_INST, RM_INST) {\
		if (MR) \
		{ \
			USE_INS(MR_INST); \
		} \
		else \
		{ \
			USE_INS(RM_INST); \
		} }

#define POOL mContext->Memory

#ifdef REG
#undef REG
#endif // REG

#define REG(R) Operand::FromRegister(NREG::R)
#define REGV(R) Operand::FromRegister(R)
#define MSK_REG(R) (1ull << R)

#define DISP(R, OFFSET) Operand::FromDisplacement(R, OFFSET)
#define IMM8_I(IMM_VAL) Operand::FromImmediate((Int8)IMM_VAL, CoreTypes::I1)
#define IMM16_I(IMM_VAL) Operand::FromImmediate((Int16)IMM_VAL, CoreTypes::I2)
#define IMM32_I(IMM_VAL) Operand::FromImmediate((Int32)IMM_VAL, CoreTypes::I4)

#define IMM8_U(IMM_VAL) Operand::FromImmediate((UInt8)IMM_VAL, CoreTypes::U1)
#define IMM16_U(IMM_VAL) Operand::FromImmediate((UInt16)IMM_VAL, CoreTypes::U2)
#define IMM32_U(IMM_VAL) Operand::FromImmediate((UInt32)IMM_VAL, CoreTypes::U4)

#define ASM(INST, ...) \
		{ \
			Instruction instruction{}; \
			USE_INS(INST); \
			instruction.CoreType = ASMDefaultCoreType; \
			Emit(instruction, __VA_ARGS__); \
		} 

#define ASM_C(INST, CORE_TYPE,...) \
		{ \
			Instruction instruction{}; \
			USE_INS(INST); \
			instruction.CoreType = CoreTypes::CORE_TYPE; \
			Emit(instruction, __VA_ARGS__); \
		} 

#define REP_TRK(NAME) (mGenContext.NAME##Track).Offset
#define REP_TRK_SIZE(NAME) (mGenContext.NAME##Track).Size

#define IS_TRK(NAME) (mGenContext.NAME##Track).Size == Tracking::RequestTracking
#define TRK(NAME) (mGenContext.NAME##Track).Size = Tracking::RequestTracking; \
				  REP_TRK(NAME) = {}



namespace RTJ::Hex::X86
{
	static constexpr UInt8 ASMDefaultCoreType = 
		RTP::CurrentWidth == RTP::Platform::Bit64 ? CoreTypes::I8 : CoreTypes::I4;

	void X86NativeCodeGenerator::CodeGenFor(StoreNode* store)
	{
		RegisterConflictSession session{ &mGenContext };

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
			auto index = offset->Base->As<LocalVariableNode>()->LocalIndex;
			USE_DISP(destinationOperand, index);
			break;
		}
		case NodeKinds::ArrayOffsetOf:
		{
			THROW("Not implemented");
			auto offset = destination->As<ArrayOffsetOfNode>();
			destinationOperand.Kind = OperandPreference::SIB;

			//Set base
			RTAssert(offset->Array->Is(NodeKinds::LocalVariable));
			auto arrayVar = offset->Array->As<LocalVariableNode>();
			auto arrayReg = AllocateRegisterAndGenerateCodeFor(
				arrayVar->LocalIndex,
				NREG::Common,
				arrayVar->TypeInfo->GetCoreType());

			MARK_UNSPILLABLE(arrayReg);
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
			//Set generation context
			ST_VAR = local->LocalIndex;

			//TODO: Volatile requires immediate memory write
			if (false)
			{
				USE_DISP(destinationOperand, local->LocalIndex);
				break;
			}

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
				auto sourceLoad = source->As<LoadNode>();
				if (sourceLoad->Source->Is(NodeKinds::LocalVariable))
				{
					auto onFlyReg = mRegContext->TryGetRegister(local->LocalIndex);
					if (onFlyReg.has_value())
					{
						//Keep this unspillable
						MARK_UNSPILLABLE(onFlyReg.value());
						destinationOperand = Operand::FromRegister(onFlyReg.value());
					}
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
				break;
			}
			case NodeKinds::Constant:
			default:
				//The same as above, see case 3.
				MarkVariableOnFly(local->LocalIndex);			
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
				//TODO: Volatile requires direct memory read

				auto local = sourceLoad->Source->As<LocalVariableNode>();
				//Get physical register from state
				auto nativeReg = mRegContext->TryGetRegister(local->LocalIndex);
				if (nativeReg.has_value())
				{
					MARK_UNSPILLABLE(nativeReg.value());
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

						//Keep this unspillable
						MARK_UNSPILLABLE(newRegister);
						sourceOperand = Operand::FromRegister(newRegister);
					}
					else
					{
						//Pefer or forced to use memory operation
						USE_DISP(sourceOperand, local->LocalIndex);
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
			//TODO: Can optimize more for size < int64		
			if (ST_VAR.has_value())
			{
				//Always use register for constant loading (see our comment above) 
				sourceOperand = UseImmediate(source->As<ConstantNode>(), true);
				RTAssert(RE_REG.has_value());
				MARK_UNSPILLABLE(RE_REG.value());
				mRegContext->Establish(ST_VAR.value(), RE_REG.value());

				shouldNotEmit = true;
			}
			else
			{
				sourceOperand = UseImmediate(source->As<ConstantNode>(), destinationOperand.IsAdvancedAddressing());
				//Mark if possible
				if (RE_REG.has_value())
					MARK_UNSPILLABLE(RE_REG.value());
			}
			break;
		}
		default:
		{
			//Other operation, requires register as intermediate status
			CodeGenFor(source);
			if (ST_VAR.has_value())
			{
				RTAssert(RE_REG.has_value());
				mRegContext->Establish(ST_VAR.value(), RE_REG.value());

				shouldNotEmit = true;
			}		
			break;
		}
		}

		if (shouldNotEmit)
			return;

		//Determine the instruction
		Instruction instruction = RetriveBinaryInstruction(
			SemanticGroup::MOV,
			source->TypeInfo->GetCoreType(),
			destinationOperand.Kind, sourceOperand.Kind);

		//Emit
		Emit(instruction, destinationOperand, sourceOperand);
	}

	void X86NativeCodeGenerator::MarkVariableOnFly(UInt16 variable, BasicBlock* bb)
	{
		if (bb == nullptr) bb = mCurrentBB;
		mVariableLanded[bb->Index].Remove(variable);
	}

	void X86NativeCodeGenerator::MarkVariableLanded(UInt16 variable, BasicBlock* bb)
	{
		if (bb == nullptr) bb = mCurrentBB;
		//Mark
		mContext->GetLocalBase(variable)->Flags |= LocalAttachedFlags::ShouldGenerateLayout;
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
				mContext->GetLocalType(variable)->GetCoreType());			
		}
	}

	X86NativeCodeGenerator::X86NativeCodeGenerator(HexJITContext* context) :
		mContext(context) {}

	void X86NativeCodeGenerator::CodeGenForBranch(BasicBlock* basicBlock, Int32 estimatedOffset)
	{
		switch (basicBlock->BranchKind)
		{
		case PPKind::Ret:
			CodeGenForReturn(basicBlock, estimatedOffset);
			break;
		case PPKind::Conditional:
			CodeGenForJcc(basicBlock, estimatedOffset);
			break;
		case PPKind::Unconditional:
			CodeGenForJmp(basicBlock, estimatedOffset);
			break;
		}
	}

	void X86NativeCodeGenerator::CodeGenForReturn(BasicBlock* basicBlock, Int32 estimatedOffset)
	{	
		CodeGenForJmp(basicBlock, estimatedOffset);
	}

	void X86NativeCodeGenerator::CodeGenForJmp(BasicBlock* basicBlock, Int32 estimatedOffset)
	{
		//Set emit page
		mEmitPage = basicBlock->NativeCode;
		//Track immediate
		TRK(Immediate);
		/*Actually we need to jump to the epilogue code for clean up procedure*/
		if (std::numeric_limits<Int8>::min() <= estimatedOffset &&
			std::numeric_limits<Int8>::max() >= estimatedOffset)
		{
			ASM(JMP_I1, IMM8_I(DebugEstimateOffset8));
		}
		else
		{
			ASM(JMP_I4, IMM32_I(DebugEstimateOffset32));
		}

		Int32 targetIndex = basicBlock->BranchedBB == nullptr ? EpilogueBBIndex : basicBlock->BranchedBB->Index;
		mJmpOffsetFix[basicBlock->Index] = { REP_TRK(Immediate).value(),  (UInt8)REP_TRK_SIZE(Immediate), targetIndex };
	}

	void X86NativeCodeGenerator::CodeGenForJcc(BasicBlock* basicBlock, Int32 estimatedOffset)
	{
		THROW("Not implemented");
		//Set emit page
		mEmitPage = basicBlock->NativeCode;
		TRK(Immediate);

		Operand imm{};
		/*Actually we need to jump to the epilogue code for clean up procedure*/
		if (std::numeric_limits<Int8>::min() <= estimatedOffset &&
			std::numeric_limits<Int8>::max() >= estimatedOffset)
		{
			imm = IMM8_I(DebugEstimateOffset8);
		}
		else
		{
			imm = IMM32_I(DebugEstimateOffset32);
		}

		TreeNode* value = basicBlock->BranchConditionValue;	
	}

	void X86NativeCodeGenerator::SpillAndGenerateCodeFor(UInt16 variable, UInt8 coreType)
	{
		auto reg = mRegContext->TryGetRegister(variable);
		if (reg.has_value())
		{
			EmitStoreR2M(reg.value(), variable, coreType);
			MarkVariableLanded(variable);
			INVALIDATE_VAR(variable);
		}
	}

	UInt8 X86NativeCodeGenerator::AllocateRegisterAndGenerateCodeFor(UInt64 mask, UInt8 coreType)
	{
		auto freeReg = mRegContext->TryGetFreeRegister(mask);
		if (freeReg.has_value())
		{
			//Leak it
			mRegContext->ReturnRegister(freeReg.value());
			return freeReg.value();
		}
		else
		{
			auto [spillVar, spillReg] = RetriveSpillCandidate(mask, mLiveness);
			SpillAndGenerateCodeFor(spillVar, coreType);
			//Leak it
			mRegContext->ReturnRegister(spillReg);
			return spillReg;
		}
	}

	Operand X86NativeCodeGenerator::UseImmediate(ConstantNode* constant, bool forceRegister, UInt64 additionalMask)
	{
		//Clean up context
		RE_REG = {};

		UInt8 coreType = constant->CoreType;
		Operand result {};

		if (CoreTypes::IsFloatLike(coreType))
		{
			THROW("Not implemented");
			if (forceRegister)
				result.Kind = OperandPreference::Register;
			else
				result.Kind = OperandPreference::Displacement;
			//Generate constant address and loading
		}
		else if (CoreTypes::IsIntegerLike(coreType))
		{
			if (coreType == CoreTypes::I8 ||
				coreType == CoreTypes::U8 ||
				forceRegister)
			{
				//For int64 immediate, use intermediate register
				auto newRegister = AllocateRegisterAndGenerateCodeFor(NREG::Common & additionalMask, coreType);
				//Update result register
				RE_REG = newRegister;

				result = Operand::FromRegister(newRegister);				
				X86::Instruction instruction = RetriveBinaryInstruction(
					SemanticGroup::MOV, 
					coreType, 
					OperandPreference::Register, 
					OperandPreference::Immediate);

				Emit(instruction, result, Operand::FromImmediate(constant->I8, coreType));
			}
			else
			{
				result = Operand::FromImmediate(constant->I8, coreType);
			}
		}
		else
		{
			THROW("Unsupported constant type");
		}

		return result;
	}

	void X86NativeCodeGenerator::Prepare()
	{
		mVariableLanded = std::move(std::vector<VariableSet>(mContext->BBs.size()));
		InitializeContextFromCallingConv();
	}

	void X86NativeCodeGenerator::Finalize()
	{
		auto stackInfo = StackFrameGenerator(mContext).Generate();
		auto nonVolatiles = GetUsedNonVolatileRegisters();
		GeneratePrologue(stackInfo, nonVolatiles);
		GenerateEpilogue(stackInfo, nonVolatiles);
		FixUpDisplacement();
		GenerateBranch();
		AssemblyCode();
	}

	void X86NativeCodeGenerator::FixUpDisplacement()
	{
		for (auto&& [variable, maps] : mVariableDispFix)
		{
			auto varBase = mContext->GetLocalBase(variable);
			if (varBase->Offset == LocalVariableAttachedBase::InvalidOffset)
				THROW("Unexpected usage of local variable. Please check if ShouldGenerateLayout is set.");
			for (auto&& [bb, offsets] : maps)
				for (auto offset : offsets)
					RewritePageImmediate(mContext->BBs[bb]->NativeCode, offset, varBase->Offset);
		}
	}

	void X86NativeCodeGenerator::GenerateBranch()
	{
		/*Here we will generate branch for each BB if possible*/
		//Estimate the worst situation: Opcode + Imm32
		constexpr Int32 JumpInstructionSizeEstimation = 1 + 4;
		std::unordered_map<Int32, Int32> estimatedOffset{};
		{
			Int32 current = mProloguePage->CurrentOffset();
			//Compute estimated offset first
			for (auto&& bb : mContext->BBs | std::views::filter(BasicBlock::ReachableFn))
			{
				estimatedOffset[bb->Index] = current;
				current += bb->NativeCodeSize;
				//This also applys for last Return BB and epilogue since both are nullptr
				if (bb->BranchedBB == bb->Next)
					continue;
				switch (bb->BranchKind)
				{
				case PPKind::Ret:
				case PPKind::Unconditional:
				case PPKind::Conditional:
					current += JumpInstructionSizeEstimation;
				}
			}
			//Set epilogue
			estimatedOffset[EpilogueBBIndex] = current;
		}

		{
			Int32 current = mProloguePage->CurrentOffset();
			for (auto&& bb : mContext->BBs |
				std::views::filter(BasicBlock::ReachableFn))
			{
				if (bb->BranchedBB == bb->Next)
					continue;

				bb->PhysicalOffset = current;
				//Select BB index
				Int32 index = bb->BranchedBB == nullptr ? EpilogueBBIndex : bb->BranchedBB->Index;
				CodeGenForBranch(bb, estimatedOffset[index]);

				//Update next
				bb->NativeCodeSize = bb->NativeCode->CurrentOffset();
				current += bb->NativeCode->CurrentOffset();
			}
		}
	}

	void X86NativeCodeGenerator::AssemblyCode()
	{
		EmitPage* finalPage = nullptr;
		{
			Int32 totalCodeSize = mProloguePage->CurrentOffset() + mEpiloguePage->CurrentOffset();
			for (auto&& bb : mContext->BBs |
				std::views::filter(BasicBlock::ReachableFn))
				totalCodeSize += bb->NativeCodeSize;

			finalPage = new EmitPage(totalCodeSize);
		}

		Int32 epilogueOffset = 0;
		//Copy code to final page
		{
			Int32 current = 0;
			auto copyToFinal = [&](EmitPage* page)
			{
				UInt8* address = finalPage->Prepare(page->CurrentOffset());
				std::memcpy(address, page->GetRaw(), page->CurrentOffset());
				finalPage->Commit(page->CurrentOffset());

				Int32 retOffset = current;
				current += page->CurrentOffset();
				return retOffset;
			};

			copyToFinal(mProloguePage);
			for (auto&& bb : mContext->BBs |
				std::views::filter(BasicBlock::ReachableFn))
			{
				bb->PhysicalOffset = copyToFinal(bb->NativeCode);
			}
			epilogueOffset = copyToFinal(mEpiloguePage);
		}

		//Fix up jump offset
		{
			for (auto&& [bbIndex, fixUp] : mJmpOffsetFix)
			{
				Int32 finalOffset = mContext->BBs[bbIndex]->PhysicalOffset + fixUp.Offset;

				Int32 targetOffset = 0;
				if (fixUp.BasicBlock == EpilogueBBIndex)
					targetOffset = epilogueOffset;
				else
					targetOffset = mContext->BBs[fixUp.BasicBlock]->PhysicalOffset;

				switch (fixUp.Size)
				{
				case 1:
					RewritePageImmediate(finalPage, finalOffset, (Int8)targetOffset); break;
				case 2:
					RewritePageImmediate(finalPage, finalOffset, (Int16)targetOffset); break;
				case 4:
					RewritePageImmediate(finalPage, finalOffset, (Int32)targetOffset); break;
				case 8:
					RewritePageImmediate(finalPage, finalOffset, (Int64)targetOffset); break;
				default:
					THROW("Unknown immediate size");
				}
				
			}
		}

		//Set to executable
		finalPage->Finalize();
		mContext->NativeCode = finalPage;
	}

	std::vector<UInt8> X86NativeCodeGenerator::GetUsedNonVolatileRegisters() const
	{
		auto states = GetCallingConv()->RegisterStates;

		std::vector<UInt8> registers{};
		UInt64 totalMask = 0ull;
		for (auto&& bb : mContext->BBs | std::views::filter(BasicBlock::ReachableFn))
			totalMask |= bb->RegisterContext->GetUsedRegisterRecord();

		UInt8 nativeRegister = Bit::InvalidBit;
		while (true) {
			nativeRegister = Bit::LeftMostSetBit(totalMask);
			if (nativeRegister != Bit::InvalidBit)
			{
				if(states[nativeRegister] & RNVOL_F)
					registers.push_back(nativeRegister);
				Bit::SetZero(totalMask, nativeRegister);
			}
			else
				break;
		}

		return registers;
	}

	void X86NativeCodeGenerator::InitializeContextFromCallingConv()
	{
		auto bb = mContext->BBs.front();
		bb->RegisterContext = mRegContext = new RegisterAllocationContext();
	
		auto callingConv = GenerateCallingConvFor(mContext->Context->Assembly->Heap, 
			mContext->Context->MethDescriptor->GetSignature()); 
		mContext->Context->MethDescriptor->SetCallingConvention(callingConv);

		for (Int32 i = 0; i < callingConv->ArgumentCount; i++)
		{
			UInt16 index = LocalVariableNode::ArgumentFlag | i;
			UInt8 coreType = mContext->GetLocalType(index)->GetCoreType();
			auto&& arg = callingConv->ArgumentPassway[i];
			if (!arg.HasMemory() && arg.HasRegister())
				mRegContext->Establish(index, arg.SingleRegister);
		}
	}

	void X86NativeCodeGenerator::GeneratePrologue(RTEE::StackFrameInfo* info, std::vector<UInt8> const& nonVolatileRegisters)
	{
		mEmitPage = mProloguePage = new EmitPage(16);

		for (auto&& reg : nonVolatileRegisters)
		{
			ASM(PUSH_R_IU, REGV(reg));
		}

		//Do not generate for empty stack
		if (info->StackSize == 0)
			return;

		ASM(PUSH_R_IU, REG(BP));
		ASM(MOV_RM_IU, REG(BP), REG(SP));
		if (info->StackSize <= std::numeric_limits<UInt8>::max())
			ASM_C(SUB_MI_I1, I1, REG(SP), IMM8_U(info->StackSize))
		else if (info->StackSize <= std::numeric_limits<UInt16>::max())
			ASM_C(SUB_MI_IU, I2, REG(SP), IMM16_U(info->StackSize))
		else
			ASM(SUB_MI_IU, REG(SP), IMM32_U(info->StackSize))
	}

	void X86NativeCodeGenerator::GenerateEpilogue(RTEE::StackFrameInfo* info, std::vector<UInt8> const& nonVolatileRegisters)
	{
		mEmitPage = mEpiloguePage = new EmitPage(16);
		if (info->StackSize > 0)
		{
			if (info->StackSize <= std::numeric_limits<UInt8>::max())
				ASM_C(ADD_MI_I1, I1, REG(SP), IMM8_U(info->StackSize))
			else if (info->StackSize <= std::numeric_limits<UInt16>::max())
				ASM_C(ADD_MI_IU, I2, REG(SP), IMM16_U(info->StackSize))
			else
				ASM(ADD_MI_IU, REG(SP), IMM32_U(info->StackSize))
			ASM(POP_R_IU, REG(BP));
		}

		for (auto&& reg : nonVolatileRegisters | std::views::reverse)
		{
			ASM(POP_R_IU, REGV(reg));
		}
		ASM(RET);
	}

	void X86NativeCodeGenerator::StartCodeGeneration()
	{
		Prepare();

		for (auto&& bb : mContext->BBs)
		{
			//Set up context
			{
				mCurrentBB = bb;
				mEmitPage = mCurrentBB->NativeCode = new EmitPage(16);
				mLiveness = 0;
			}

			//Merge register context from BBIn
			MergeContext();

			//Generate code for BB
			for (auto stmt = bb->Now; stmt != nullptr; stmt = stmt->Next, mLiveness++)
				if (stmt->Now != nullptr)
					CodeGenFor(stmt->Now);
			
			switch (bb->BranchKind)
			{
				case PPKind::Conditional:
					PreCodeGenForJcc(bb->BranchConditionValue); break;
				case PPKind::Ret:
					PreCodeGenForRet(bb->ReturnValue); break;
			}

			//Compute code size
			bb->NativeCodeSize = mEmitPage->CurrentOffset();
		}

		Finalize();
	}

	BasicBlock* X86NativeCodeGenerator::PassThrough()
	{
		StartCodeGeneration();
		return mContext->BBs.front();
	}

	void X86NativeCodeGenerator::MergeContext()
	{
		//Some procedure has already set the context
		if (mCurrentBB->RegisterContext != nullptr)
			return;

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
		//				*  1. Origin: A -> RAX , New: A ��> RBX
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
				auto [spillVariable, spillReg] = RetriveSpillCandidate(mask, mLiveness);
				SpillAndGenerateCodeFor(spillVariable, coreType);
				//Assign to spill variable
				newRegister = spillReg;
			}

			if (requireLoading)
			{
				if (reg.has_value())
				{
					//Do register transfer
					EmitLoadR2R(newRegister.value(), reg.value(), coreType);
					mRegContext->ReturnRegister(reg.value());
				}
				else
				{
					//Do variable load
					EmitLoadM2R(newRegister.value(), variable, coreType);
				}
			}

			//Establish connection on this
			mRegContext->Establish(variable, newRegister.value());
			return newRegister.value();
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

	std::tuple<UInt16, UInt8> X86NativeCodeGenerator::RetriveSpillCandidate(UInt64 mask, Int32 livenessIndex)
	{
		//Consider dead variables firstly
		auto&& mapping = mRegContext->GetMapping();
		auto&& liveness = mCurrentBB->Liveness;
		for (auto&& [var, reg] : mapping)
		{
			if (!liveness[mLiveness].Contains(var) && Bit::TestAt(USE_MASK, reg))
				return { var, reg };
		}

		//Consider live variables
		VariableSet remainSet{};

		for (auto&& [var, reg] : mapping)
		{
			if (liveness[mLiveness].Contains(var) && Bit::TestAt(USE_MASK, reg))
				remainSet.Add(var);
		}

		if (remainSet.Count() == 0)
			THROW("Fatal: Unable to spill");

		for (Int32 i = mLiveness + 1; i < mCurrentBB->Liveness.size(); ++i)
		{
			VariableSet nextSet = remainSet & liveness[i];
			if (nextSet.Count() == 0)
				break;
		}

		auto candidateVar = remainSet.Front();
		return  { candidateVar, mapping.at(candidateVar) };
	}

	void X86NativeCodeGenerator::Emit(Instruction instruction)
	{
		WritePage(mEmitPage, instruction.Opcode, instruction.Length);
	}

	Instruction X86NativeCodeGenerator::RetriveBinaryInstruction(
		SemanticGroup semGroup, 
		UInt8 coreType, 
		OperandPreference destPreference, 
		OperandPreference sourcePreference)
	{
		Instruction instruction{};
		instruction.CoreType = coreType;
		bool MR = IsAdvancedAddressing(destPreference);
		bool immLoad = sourcePreference == OperandPreference::Immediate;
		switch (semGroup)
		{
		case SemanticGroup::MOV:
		{
			switch (coreType)
			{
			case CoreTypes::I1:
			case CoreTypes::U1:
				if (immLoad) USE_INS(MOV_RI_I1)
				else MR_USE_INS(MOV_MR_I1, MOV_RM_I1)
				break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
			case CoreTypes::U2:
			case CoreTypes::U4:
			case CoreTypes::U8:
				if (immLoad)
				{
					USE_INS(MOV_RI_IU)
					break;
				}
			case CoreTypes::Ref:
				MR_USE_INS(MOV_MR_IU, MOV_RM_IU);
				break;
			case CoreTypes::R4:
				if (immLoad) THROW("Unsupported MOV M <- I/R <- I for R4");
				MR_USE_INS(MOVSS_MR, MOVSS_RM);
				break;
			case CoreTypes::R8:
				if (immLoad) THROW("Unsupported MOV M <- I/R <- I for R8");
				MR_USE_INS(MOVSD_MR, MOVSD_RM);
				break;
			}
			break;
		}
		case SemanticGroup::ADD:
		{
			switch (coreType)
			{
			case CoreTypes::I1:
			case CoreTypes::U1:
				if (immLoad) USE_INS(ADD_MI_I1)
				else MR_USE_INS(ADD_MR_I1, ADD_RM_I1)
				break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
			case CoreTypes::U2:
			case CoreTypes::U4:
			case CoreTypes::U8:
				if (immLoad) USE_INS(ADD_MI_IU)
				else MR_USE_INS(ADD_MR_IU, ADD_RM_IU)
				break;
			case CoreTypes::R4:
				if (immLoad) THROW("Unsupported ADD M <- I/R <- I for R4");
				if (MR) THROW("Unsupported ADD M <- R for R4");
				USE_INS(ADDSS_RM);			
				break;
			case CoreTypes::R8:
				if (immLoad) THROW("Unsupported ADD M <- I/R <- I for R8");
				if (MR) THROW("Unsupported ADD M <- R for R8");
				USE_INS(ADDSD_RM);
				break;
			}
			break;
		}
		case SemanticGroup::CMP:
		{
			switch (coreType)
			{
			case CoreTypes::I1:
			case CoreTypes::U1:
				if (immLoad) USE_INS(CMP_MI_I1)
				else MR_USE_INS(CMP_MR_I1, CMP_RM_I1)
					break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
			case CoreTypes::U2:
			case CoreTypes::U4:
			case CoreTypes::U8:
				if (immLoad) USE_INS(CMP_MI_IU)
				else MR_USE_INS(CMP_MR_IU, CMP_RM_IU)
					break;
			case CoreTypes::R4:
				if (immLoad) THROW("Unsupported CMP M <- I/R <- I for R4");
				if (MR) THROW("Unsupported CMP M <- R for R4");
				USE_INS(COMISS_RM);
				break;
			case CoreTypes::R8:
				if (immLoad) THROW("Unsupported CMP M <- I/R <- I for R8");
				if (MR) THROW("Unsupported CMP M <- R for R8");
				USE_INS(COMISD_RM);
				break;
			}
			break;
		}
		}
		return instruction;
	}

	void X86NativeCodeGenerator::CodeGenForBinary(BinaryNodeAccessProxy* binary)
	{
		switch (binary->Kind)
		{
		case NodeKinds::BinaryArithmetic:
		case NodeKinds::Compare:
			break;
		default:
			THROW("Unsupported binary node");
		}

		//Clean up context
		RE_REG = {};
		RegisterConflictSession session{ &mGenContext };

		//From operand (In case of operation like Compare)
		UInt8 coreType = binary->Second->TypeInfo->GetCoreType();
		//Assume constant folding is done
		ConstantNode* constant = nullptr;
		LocalVariableNode* locals[2] = { nullptr, nullptr };
		Int32 localCount = 0;

		if (binary->First->Is(NodeKinds::Constant))
			constant = binary->First->As<ConstantNode>();
		else
			locals[localCount++] = ValueAs<LocalVariableNode>(binary->First);

		if (binary->Second->Is(NodeKinds::Constant))
			constant = binary->Second->As<ConstantNode>();
		else
			locals[localCount++] = ValueAs<LocalVariableNode>(binary->Second);

		//Select register mask
		auto regMask = NREG::Common;
		if (CoreTypes::IsFloatLike(coreType))
			regMask = NREG::CommonXMM;

		Operand source{};
		Operand destination{};
		if (constant != nullptr)
		{
			source = UseImmediate(constant);

			UInt16 variable = locals[0]->As<LocalVariableNode>()->LocalIndex;
			UInt8 reg = AllocateRegisterAndGenerateCodeFor(variable, regMask, coreType);
			destination = Operand::FromRegister(reg);

			INVALIDATE_VAR(variable);			
		} 
		else
		{
			/*Allocate register for two operand, but one of them will be
			* modified and finally as the result register.
			* (This will be solved for float-like type in AVX support(TODO))
			*/
			
			UInt16 leftVar = locals[0]->As<LocalVariableNode>()->LocalIndex;
			UInt16 rightVar = locals[1]->As<LocalVariableNode>()->LocalIndex;
			//Consider ? = a <op> a
			if (leftVar == rightVar)
			{
				UInt8 reg = AllocateRegisterAndGenerateCodeFor(leftVar, regMask, coreType);
				source = Operand::FromRegister(reg);
				destination = Operand::FromRegister(reg);
				//Remove the allocation
				INVALIDATE_VAR(leftVar);
			}
			else
			{
				UInt8 leftReg = AllocateRegisterAndGenerateCodeFor(leftVar, regMask, coreType);
				MARK_UNSPILLABLE(leftReg);
				UInt8 rightReg = AllocateRegisterAndGenerateCodeFor(rightVar, regMask, coreType);
				MARK_UNSPILLABLE(rightReg);

				RTAssert(ST_VAR.has_value());
				auto storingVar = ST_VAR.value();
				//Consider a = a <op> c
				if (storingVar == leftVar)
				{
					destination = Operand::FromRegister(leftReg);
					source = Operand::FromRegister(rightReg);
				}
				else if (storingVar == rightVar)
				{
					destination = Operand::FromRegister(rightReg);
					source = Operand::FromRegister(leftReg);
				}
				else
				{
					UInt16 longLived = RetriveLongLived(leftVar, rightVar);
					if (longLived == leftVar)
					{
						destination = Operand::FromRegister(rightReg);
						source = Operand::FromRegister(leftReg);
						INVALIDATE_VAR(rightVar);
					}
					else
					{
						//Right variable
						destination = Operand::FromRegister(leftReg);
						source = Operand::FromRegister(rightReg);
						INVALIDATE_VAR(leftVar);
					}
				}
			}		
		}
	
		SemanticGroup group{};
		switch (binary->Kind)
		{
		case NodeKinds::BinaryArithmetic:
		{
			auto binaryArithmetic = binary->As<BinaryArithmeticNode>();
			group = (SemanticGroup)((UInt8)SemanticGroup::ADD + (binaryArithmetic->Opcode - OpCodes::Add));
			break;
		}
		case NodeKinds::Compare: 
			group = SemanticGroup::CMP; break;
		}
		//Choose instruction
		RTAssert(destination.Kind == OperandPreference::Register);

		//Update result register
		RE_REG = destination.Register;
		Instruction instruction = RetriveBinaryInstruction(group, coreType, OperandPreference::Register, source.Kind);

		//Emit instruction
		Emit(instruction, destination, source);
	}

	void X86NativeCodeGenerator::CodeGenFor(UnaryArithmeticNode* unaryNode)
	{
		RE_REG = {};
	}

	void X86NativeCodeGenerator::PreCodeGenForJcc(TreeNode* conditionValue)
	{

	}

	void X86NativeCodeGenerator::PreCodeGenForRet(TreeNode* returnValue)
	{
		if (returnValue != nullptr)
		{
			auto returnPassReg = GetCallingConv()->ReturnPassway.SingleRegister;
			switch (returnValue->Kind)
			{
			case NodeKinds::Load:
			{
				RTAssert(ValueIs(returnValue, NodeKinds::LocalVariable));
				auto variable = ValueAs<LocalVariableNode>(returnValue);
				auto coreType = variable->TypeInfo->GetCoreType();

				if (RTP::CurrentWidth == RTP::Platform::Bit32 && (
					coreType == CoreTypes::I8 || coreType == CoreTypes::U8))
				{
					//TODO: Support long / ulong in 32bit
					THROW("Not implemented");
				}

				//TODO: Support complex struct returning
				if (coreType == CoreTypes::Struct)
					THROW("Not implemented");

				//Make use of AllocateRegisterAndGenerateCodeFor
				AllocateRegisterAndGenerateCodeFor(
					variable->LocalIndex,
					MSK_REG(returnPassReg),
					coreType);
			}
			break;
			case NodeKinds::Constant:
			{
				//Make use of UseImmediate
				UseImmediate(returnValue->As<ConstantNode>(), true, MSK_REG(returnPassReg));
			}
			break;
			default:
				THROW("Return type not supported");
			}
		}
	}

	UInt16 X86NativeCodeGenerator::RetriveLongLived(UInt16 left, UInt16 right)
	{
		auto&& liveness = mCurrentBB->Liveness;
		for (Int32 index = mLiveness; index < liveness.size(); index++)
		{
			if (!liveness[index].Contains(left))
				return right;
			if (!liveness[index].Contains(right))
				return left;
		}

		return left;
	}

	RTP::PlatformCallingConvention* Hex::X86::X86NativeCodeGenerator::GetCallingConv() const
	{
		return mContext->Context->MethDescriptor->GetCallingConvention();
	}

	void X86NativeCodeGenerator::CodeGenFor(TreeNode* node)
	{
		switch (node->Kind)
		{
		case NodeKinds::Compare:
		case NodeKinds::BinaryArithmetic:
			return CodeGenForBinary(node->As<BinaryNodeAccessProxy>());
		case NodeKinds::UnaryArithmetic:
			return CodeGenFor(node->As<UnaryArithmeticNode>());
		case NodeKinds::Store:
			return CodeGenFor(node->As<StoreNode>());
		default:
			THROW("Unsupported generation node");
		}
	}

	void X86NativeCodeGenerator::EmitLoadM2R(UInt8 nativeRegister, UInt16 variable, UInt8 coreType)
	{
		auto ins = RetriveBinaryInstruction(
			SemanticGroup::MOV, 
			coreType,
			OperandPreference::Register, 
			OperandPreference::Displacement);

		Operand source{};
		USE_DISP(source, variable);

		Emit(ins, Operand::FromRegister(nativeRegister), source);
	}

	void X86NativeCodeGenerator::EmitLoadR2R(UInt8 toRegister, UInt8 fromRegister, UInt8 coreType)
	{
		auto ins = RetriveBinaryInstruction(
			SemanticGroup::MOV,
			coreType,
			OperandPreference::Register,
			OperandPreference::Register);

		Emit(ins, Operand::FromRegister(toRegister), Operand::FromRegister(fromRegister));
	}

	void X86NativeCodeGenerator::EmitStoreR2M(UInt8 nativeRegister, UInt16 variable, UInt8 coreType)
	{
		auto ins = RetriveBinaryInstruction(
			SemanticGroup::MOV,
			coreType,
			OperandPreference::Displacement,
			OperandPreference::Register);

		Operand dest{};
		USE_DISP(dest, variable);

		Emit(ins, dest, Operand::FromRegister(nativeRegister));
	}

	void X86NativeCodeGenerator::Emit(Instruction instruction, Operand const& left, Operand const& right)
	{
		constexpr Int32 InvalidPos = -1;
		constexpr bool x64 = RTP::CurrentWidth == RTP::Platform::Bit64;

		//REX Prefix determination
		UInt8 REX = 0u;
		UInt8 Operand16Prefix = 0u;
		auto coreType = instruction.CoreType;

		auto setREXIfContains8RegSwitch = [&](Operand const& operand) {
			if (operand.Kind == OperandPreference::Register)
			{
				switch (operand.Register)
				{
				case NREG::AX:
				case NREG::BX:
				case NREG::CX:
				case NREG::DX:
					break;
				default:
					REX |= 0X40;
				}			
			}
		};

		switch (coreType)
		{
		case CoreTypes::I1:
		case CoreTypes::U1:
			setREXIfContains8RegSwitch(left);
			setREXIfContains8RegSwitch(right);
			break;
		case CoreTypes::Ref:
			if (!x64) break;
		case CoreTypes::I8:
		case CoreTypes::U8:
			REX |= 0x48; break;
		case CoreTypes::I2:
		case CoreTypes::U2:
		{
			Operand16Prefix = 0x66;
			REX |= 0x40; break;
		}			
		}

		bool use64Reg = false;
		auto contains64Reg = [](Operand const& operand) {
			if (operand.Kind == OperandPreference::Register && (
				NREG64::R8 <= operand.Register &&
				operand.Register <= NREG64::R15))
			{
				return true;
			}
			return false;
		};

		use64Reg |= contains64Reg(left);
		use64Reg |= contains64Reg(right);

		if (use64Reg)
			REX |= 0x40;
		
		//Write 66H to use 16bit operand
		if (Operand16Prefix > 0u)
			WritePage(mEmitPage, Operand16Prefix);

		Int32 rexPos = InvalidPos;
		//Write REX prefiex
		if (REX > 0u)
			rexPos = WritePage(mEmitPage, REX);
		
		//Write opcodes
		Int32 opcodeRegBytePos = WritePage(mEmitPage, instruction.Opcode, instruction.Length) + instruction.Length - 1;
		//Prepare for register reencoding
		UInt8 opcodeRegByte = instruction.Opcode[instruction.Length - 1];

		bool opcodeEnc = instruction.Flags & InstructionFlags::OpRegEnc;
		UInt16 operandType = instruction.Flags & InstructionFlags::OperandMSK;

		std::optional<UInt8> ModRM = {};
		std::optional<UInt8> SIB = {};
		std::optional<Int32> Disp32{};
		std::optional<Int8> Disp8{};

		std::optional<UInt16> trackingVariable{};

		UInt8 ImmediateCoreType = CoreTypes::Object;
		std::optional<ConstantStorage> Immediate{};

		//Return 3 bits encoded register and mark REX prefix if possible 
		auto encodeRegIdentity = [&](UInt8 rexBit, UInt8 registerIdentity) 
		{
			constexpr UInt8 LegacyRegisterMask = 0b111;
			UInt8 realRegister = registerIdentity;
			if (registerIdentity >= NREG::XMM0)
				realRegister -= NREG::XMM0;

			//Reserve 3 bit for encoding
			UInt8 registerToEncode = realRegister & LegacyRegisterMask;
			if (realRegister & ~LegacyRegisterMask)
			{
				if (REX > 0)
					REX |= 1u << rexBit;
				else
					THROW("Illegal 64bit register in context");
			}
			return registerToEncode;
		};

		auto handleDisplacement = [&](Int32 offset, std::optional<UInt16> refVariable) -> UInt8 {
			if (refVariable.has_value())
			{
				trackingVariable = refVariable;
				//TODO: offset estimation
				Disp32 = offset;
				return AddressingMode::RegisterAddressingDisp32;
			}
			if (offset == 0)
			{
				//Special expression for [reg]
				return AddressingMode::RegisterAddressing;
			}
			else
			{
				//Optimize for code size
				if (std::numeric_limits<Int8>::min() <= offset &&
					std::numeric_limits<Int8>::max() >= offset)
				{
					Disp8 = (Int8)offset;
					return AddressingMode::RegisterAddressingDisp8;
				}
				else
				{
					Disp32 = offset;
					return AddressingMode::RegisterAddressingDisp32;
				}
			}
		};

		auto handleMOperand = [&](const Operand& operand) -> UInt8 {
			UInt8 modRM = 0;
			switch (operand.Kind)
			{
			case OperandPreference::Displacement:
			{
				modRM |= handleDisplacement(operand.Displacement.Offset, operand.RefVariable);
				modRM |= encodeRegIdentity(REXBit::B, operand.Displacement.Base);
				break;
			}
			case OperandPreference::SIB:
			{
				modRM |= RegisterOrMemory::SIB;
				if (right.SIB.Offset == 0)
					modRM |= handleDisplacement(operand.SIB.Offset, {});

				//SIB registe encoding
				UInt8 sib = 0;
				sib |= encodeRegIdentity(REXBit::B, operand.SIB.Base);
				sib |= encodeRegIdentity(REXBit::X, operand.SIB.Index) << 3;

				//Scale encoding
				switch (operand.SIB.Scale)
				{
				case 1u:
					sib |= (0b00 << 6); break;
				case 2u:
					sib |= (0b01 << 6); break;
				case 4u:
					sib |= (0b10 << 6); break;
				case 8u:
					sib |= (0b11 << 6); break;
				}

				SIB = sib;
				break;
			}
			case OperandPreference::Register:
			{
				modRM |= AddressingMode::Register;
				modRM |= encodeRegIdentity(REXBit::B, operand.Register);
				break;
			}
			}

			//Requires magic register value encoding
			if (instruction.Flags & InstructionFlags::MagicRegUse)
				modRM |= ((instruction.GetMagicRegisterValue()) << 3);
			return modRM;
		};

		auto handleROperand = [&](const Operand& operand) -> std::optional<UInt8> {
			UInt8 modRM = 0;
			if (opcodeEnc)
			{
				//Won't modify the modRM
				opcodeRegByte |= encodeRegIdentity(REXBit::B, operand.Register);
				return {};
			}
			else
				return encodeRegIdentity(REXBit::R, operand.Register) << 3;
		};

		auto handleIOperand = [&](const Operand& operand)
		{
			ImmediateCoreType = operand.ImmediateCoreType;
			Immediate = operand.Immediate;
		};
		
		switch (operandType)
		{
		case RM_F:
			RTAssert(left.IsRegister());
			RTAssert(right.IsRegister() || right.IsAdvancedAddressing());
			{
				UInt8 modRM = handleMOperand(right);
				modRM |= handleROperand(left).value_or(0u);
				ModRM = modRM;
			}
			break;
		case MR_F:
			RTAssert(right.IsRegister());
			RTAssert(left.IsRegister() || left.IsAdvancedAddressing());
			{
				UInt8 modRM = handleMOperand(left);
				modRM |= handleROperand(right).value_or(0u);
				ModRM = modRM;
			}
			break;
		case RI_F:
			RTAssert(left.IsRegister());
			RTAssert(right.IsImmediate());
			{
				RTAssert(opcodeEnc);
				//Always encoding in opcode reg
				handleROperand(left);
				handleIOperand(right);
			}
			break;
		case MI_F:
			RTAssert(left.IsRegister() || left.IsAdvancedAddressing());
			RTAssert(right.IsImmediate());
			{
				ModRM = handleMOperand(left);
				handleIOperand(right);
			}
			break;
		case M_F:
			RTAssert(left.IsRegister() || left.IsAdvancedAddressing());
			{
				ModRM = handleMOperand(left);
			}
			break;
		case R_F:
			RTAssert(left.IsRegister());
			{
				ModRM = handleROperand(left);
			}
			break;
		case I_F:
			RTAssert(left.IsImmediate());
			{
				handleIOperand(left);
			}
			break;
		default:
			THROW("Invalid operand type");
		}

		//Do write
		if (ModRM.has_value())
			WritePage(mEmitPage, ModRM.value());

		if (SIB.has_value())
			WritePage(mEmitPage, SIB.value());

		//Cannot occur at the same time
		if (Disp8.has_value())
		{
			if (IS_TRK(Displacement))			
			{
				REP_TRK(Displacement) = mEmitPage->CurrentOffset();
				REP_TRK_SIZE(Displacement) = sizeof(UInt8);
			}
			WritePage(mEmitPage, Disp8.value());
		}
		else if (Disp32.has_value())
		{
			if (IS_TRK(Displacement))
			{
				REP_TRK(Displacement) = mEmitPage->CurrentOffset();
				REP_TRK_SIZE(Displacement) = sizeof(UInt32);
			}
			Int32 offset = WritePage(mEmitPage, Disp32.value());
			//Add fix up
			if (trackingVariable.has_value())
				mVariableDispFix[trackingVariable.value()][mCurrentBB->Index].push_back(offset);
		}

		if (Immediate.has_value())
		{
			auto&& imm = Immediate.value();
			Int32 size = CoreTypes::GetCoreTypeSize(ImmediateCoreType);

			if (IS_TRK(Immediate))
			{
				REP_TRK(Immediate) = mEmitPage->CurrentOffset();
				REP_TRK_SIZE(Immediate) = ImmediateCoreType;
			}
			switch (size)
			{
			case 1:
				WritePageImmediate(mEmitPage, imm.U1); break;
			case 2:
				WritePageImmediate(mEmitPage, imm.U2); break;
			case 4:
				WritePageImmediate(mEmitPage, imm.U4); break;
			case 8:
				WritePageImmediate(mEmitPage, imm.U8); break;
			default:
				THROW("Unsupported immediate size");
			}
		}

		//Rewrite
		if (rexPos != InvalidPos)
			RewritePage(mEmitPage, rexPos, REX);
		RewritePage(mEmitPage, opcodeRegBytePos, opcodeRegByte);
	}

	void X86NativeCodeGenerator::Emit(Instruction instruction, Operand const& operand)
	{
		Emit(instruction, operand, Operand::Empty());
	}

	RTP::PlatformCallingConvention* GenerateCallingConvFor(RTMM::PrivateHeap* heap, RTM::MethodSignatureDescriptor* signature)
	{
		auto convert = [](RTM::Type* type) -> RTP::PlatformCallingArgument {
			auto coreType = type->GetCoreType();
			if (CoreTypes::IsIntegerLike(coreType) || CoreTypes::IsRef(coreType))
				return { CoreTypes::GetCoreTypeSize(coreType), RTP::CallingArgumentType::Integer };
			else if (CoreTypes::IsFloatLike(coreType))
				return { CoreTypes::GetCoreTypeSize(coreType), RTP::CallingArgumentType::Float };
			else
				return { type->GetSize(), RTP::CallingArgumentType::Struct };
		};

		RTP::PlatformCallingArgument returnArg{};
		if (auto returnType = signature->GetReturnType())
			returnArg = convert(returnType);

		auto args = signature->GetArguments();
		std::vector<RTP::PlatformCallingArgument> callingArgs(args.Count + 1);
		callingArgs.front() = returnArg;
		for (Int32 i = 0; i < args.Count; ++i)
			callingArgs[i + 1] = convert(args[i].GetType());

		return RTP::PlatformCallingConventionProvider<
			RTP::CallingConventions::JIT,
			USE_CURRENT_PLATFORM>(heap).GetConvention(callingArgs);
	}
}

namespace RTJ::Hex::X86
{
	RegisterConflictSession::RegisterConflictSession(GenerationContext* context) :
		mGenContextRef(context) {}

	void RegisterConflictSession::Borrow(UInt8 nativeReg)
	{
		if (!Bit::TestAt(mGenContextRef->RegisterMask, nativeReg))
			//If already set
			return;

		Bit::SetZero(mGenContextRef->RegisterMask, nativeReg);
		Bit::SetOne(mRestoreBits, nativeReg);
	}

	void RegisterConflictSession::Return(UInt8 nativeReg)
	{
		if (Bit::TestAt(mGenContextRef->RegisterMask, nativeReg))
			//If not set
			return;

		Bit::SetOne(mGenContextRef->RegisterMask, nativeReg);
		Bit::SetZero(mRestoreBits, nativeReg);
	}

	RegisterConflictSession::~RegisterConflictSession()
	{
		//Restore when destructing
		mGenContextRef->RegisterMask |= mRestoreBits;
	}
}