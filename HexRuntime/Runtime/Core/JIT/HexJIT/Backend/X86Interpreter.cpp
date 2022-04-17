#include "X86CodeInterpreter.h"
#include "StackFrameGenerator.h"
#include "..\..\..\..\Bit.h"

#define USE_INS(NAME) { \
				instruction.Opcode = NativePlatformInstruction::NAME.data(); \
				instruction.Length = NativePlatformInstruction::NAME.size(); \
				instruction.Flags = NativePlatformInstruction::NAME##_FLG; }

#define USE_DISP(OPERAND, VAR, C)  {(OPERAND).Kind = OperandPreference::Displacement; \
								(OPERAND).M.Base = NREG::BP; \
								(OPERAND).M.Displacement = DebugOffset32; \
								(OPERAND).RefVariable = VAR; \
								(OPERAND).UnifiedCoreType = Operand::UnifyCoreType(C);}

#define USE_MASK (mask & mGenContext.RegisterMask)
#define MARK_UNSPILLABLE(REG) session.Borrow((REG))
#define MARK_SPILLABLE(REG) session.Return((REG))
#define INVALIDATE_VAR(VAR) InvalidateVaraible(VAR)
#define RET_REG(REG) session.Return((REG))

#define ST_VAR (mGenContext.StoringVariable)
#define ST_DST (mGenContext.StoringDestination)
#define RE_REG (mGenContext.ResultRegister)
#define JCC_COND (mGenContext.JccCondition)

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

#define REG(R, C) Operand::FromRegister(NREG::R, C)
#define REGV(R, C) Operand::FromRegister(R, C)
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
			Emit(instruction, __VA_ARGS__); \
		}

#define ASM_CV(INST, C) \
		{ \
			Instruction instruction{}; \
			USE_INS(INST); \
			instruction.CoreType = C; \
			Emit(instruction); \
		}

#define REP_TRK(NAME) (mGenContext.NAME##Track).Offset
#define REP_TRK_SIZE(NAME) (mGenContext.NAME##Track).Size

#define IS_TRK(NAME) (mGenContext.NAME##Track).Size == Tracking::RequestTracking
#define TRK(NAME) (mGenContext.NAME##Track).Size = Tracking::RequestTracking; \
				  REP_TRK(NAME) = {}

#define RCS(NAME) RegisterConflictSession NAME{ &mGenContext }

namespace RTJ::Hex::X86
{
	static constexpr UInt8 ASMDefaultCoreType = 
		RTP::CurrentWidth == RTP::Platform::Bit64 ? CoreTypes::I8 : CoreTypes::I4;

	void X86NativeCodeGenerator::CodeGenFor(StoreNode* store)
	{
		/*This method only cares for simple and general stores like
		* 1. store var <- var / imm
		* 2. store var.member <- var / imm
		* 3. store var[index] <- var / imm
		* Other complex 
		*/

		ST_VAR = {};
		ST_DST = {};
		RCS(session);

		bool shouldNotEmit = false;
		auto source = store->Source;
		auto destination = store->Destination;
		//Special handle for morphed call
		if (source->Is(NodeKinds::MorphedCall))
			return CodeGenFor(source->As<MorphedCallNode>(), destination);

		/*---------------Handle the destination operand---------------*/
		Operand destinationOperand{};
		switch (store->Mode)
		{
		case AccessMode::Content:
		{
			auto origin = UseOperandFrom(session, destination, OperandOptions::ForceRegister);
			RTAssert(origin.IsRegister());
			destinationOperand = Operand::FromDisplacement(origin.Register, 0, origin.UnifiedCoreType);
			break;
		}
		case AccessMode::Value:
		{
			switch (destination->Kind)
			{
			case NodeKinds::LocalVariable:
			{
				ST_VAR = destination->As<LocalVariableNode>()->LocalIndex;
				break;
			}
			case NodeKinds::OffsetOf:
			case NodeKinds::ArrayOffsetOf:
			{
				destinationOperand = UseOperandFrom(session, destination, OperandOptions::ReserveAsMemory);
				RTAssert(destinationOperand.IsAdvancedAddressing());
				break;
			}
			}
			break;
		}
		default:
			THROW("Unexpected store mode");
		}

		/*---------------Handle the source operand---------------*/
		Operand sourceOperand{};	
		switch (source->Kind)
		{
		case NodeKinds::Load:
		{
			auto load = source->As<LoadNode>();
			UInt8 options = OperandOptions::None;
			if (destinationOperand.IsAdvancedAddressing() || destinationOperand.IsEmpty())
				options |= OperandOptions::ForceRegister;

			sourceOperand = UseOperandFrom(session, source, options);

			if (sourceOperand.IsRegister() && ST_VAR.has_value())
			{
				shouldNotEmit = true;
				mRegContext->Establish(ST_VAR.value(), sourceOperand.Register);
				MarkVariableOnFly(ST_VAR.value());
			}
			break;
		}
		case NodeKinds::Constant:
		{
			sourceOperand = UseOperandFrom(session, source, OperandOptions::ForceRegister);
			if (ST_VAR.has_value())
			{
				shouldNotEmit = true;
				mRegContext->Establish(ST_VAR.value(), sourceOperand.Register);
				MarkVariableOnFly(ST_VAR.value());
			}
			break;
		}
		default:
		{
			//Special handle for boolean
			if (source->TypeInfo->GetCoreType() == CoreTypes::Bool)
			{
				CodeGenForBooleanStore(source);
				//This is completely decided by CodeGenForBooleanStore
				shouldNotEmit = true;
			}
			else
			{
				//Other operation, requires register as intermediate status
				CodeGenFor(source);
			}

			if (ST_VAR.has_value() && RE_REG.has_value())
			{
				mRegContext->Establish(ST_VAR.value(), RE_REG.value());
				MarkVariableOnFly(ST_VAR.value());
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

	bool X86NativeCodeGenerator::IsLanded(UInt16 variable, BasicBlock* bb)
	{
		if (bb == nullptr) bb = mCurrentBB;
		return mVariableLanded[bb->Index].Contains(variable);
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
		MarkVariableGenerateLayout(variable);
		mVariableLanded[bb->Index].Add(variable);
	}

	void X86NativeCodeGenerator::MarkVariableGenerateLayout(UInt16 variable)
	{
		mContext->GetLocalBase(variable)->Flags |= LocalAttachedFlags::ShouldGenerateLayout;
	}

	void X86NativeCodeGenerator::InvalidateVaraible(UInt16 variable)
	{
		do
		{
			if (!LocalVariableNode::IsArgument(variable))
				break;
			auto currentReg = mRegContext->TryGetRegister(variable);
			if (!currentReg.has_value())
				break;

			auto callingConv = GetCallingConv();
			auto index = LocalVariableNode::GetIndex(variable);
			auto&& passway = callingConv->ArgumentPassway[index];
			//TODO: Anything further? (for object in ref)
			if (passway.InRegister())
			{
				auto reg = passway.SingleRegister;
				if (reg != currentReg.value())
					break;

				if (ST_VAR == variable)
					break;

				if (IsValueUnusedAfter(mLiveness, variable))
					break;

				EmitStoreR2M(reg, variable, mContext->GetLocalType(variable)->GetCoreType());
				MarkVariableLanded(variable);
			}

		} while (0);
		mRegContext->TryInvalidateFor(variable);
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

			if (LocalVariableNode::IsArgument(variable))
			{
				auto&& arg = GetCallingConv()->ArgumentPassway[LocalVariableNode::GetIndex(variable)];
				//Struct address in register
				if (arg.IsRefLike())
					return EmitStoreR2M(nativeReg.value(), variable, CoreTypes::InteriorRef);
			}

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
			ASM(JMP_I1, IMM8_I(DebugOffset8));
		}
		else
		{
			ASM(JMP_I4, IMM32_I(DebugOffset32));
		}

		Int32 targetIndex = basicBlock->BranchedBB == nullptr ? EpilogueBBIndex : basicBlock->BranchedBB->Index;
		mJmpOffsetFix[basicBlock->Index] = { REP_TRK(Immediate).value(),  (UInt8)REP_TRK_SIZE(Immediate), targetIndex };
	}

	void X86NativeCodeGenerator::CodeGenForJcc(BasicBlock* basicBlock, Int32 estimatedOffset)
	{
		//Set emit page
		mEmitPage = basicBlock->NativeCode;
		TRK(Immediate);

		bool useImm32 = false;
		Operand imm{};
		/*Actually we need to jump to the epilogue code for clean up procedure*/
		if (std::numeric_limits<Int8>::min() <= estimatedOffset &&
			std::numeric_limits<Int8>::max() >= estimatedOffset)
		{
			imm = IMM8_I(DebugOffset8);
		}
		else
		{
			imm = IMM32_I(DebugOffset32);
			useImm32 = true;
		}

		UInt8 condition = CmpCondition::NE;
		
		auto&& cond = mJccConditions[basicBlock->Index];
		if (cond.has_value()) condition = cond.value();

		Instruction instruction{};
		switch (condition)
		{
		case CmpCondition::EQ:
			if (useImm32) USE_INS(JCC_EQ_I4)
			else USE_INS(JCC_EQ_I1); break;
		case CmpCondition::NE:
			if (useImm32) USE_INS(JCC_NE_I4)
			else USE_INS(JCC_NE_I1); break;
		case CmpCondition::GT:
			if (useImm32) USE_INS(JCC_GT_I4)
			else USE_INS(JCC_GT_I1); break;
		case CmpCondition::LT:
			if (useImm32) USE_INS(JCC_LT_I4)
			else USE_INS(JCC_LT_I1); break;
		case CmpCondition::GE:
			if (useImm32) USE_INS(JCC_GE_I4)
			else USE_INS(JCC_GE_I1); break;
		case CmpCondition::LE:
			if (useImm32) USE_INS(JCC_GE_I4)
			else USE_INS(JCC_LE_I1); break;
		default:
			THROW("Unexpected condition");
		}

		Emit(instruction, imm);

		Int32 targetIndex = basicBlock->BranchedBB == nullptr ? EpilogueBBIndex : basicBlock->BranchedBB->Index;
		mJmpOffsetFix[basicBlock->Index] = { REP_TRK(Immediate).value(),  (UInt8)REP_TRK_SIZE(Immediate), targetIndex };
	}

	bool X86NativeCodeGenerator::ShouldStoreBoolean(UInt16 variable)
	{
		Int32 liveness = mLiveness;
		bool usedForLoad = false;
		for (auto stmt = mCurrentStmt->Next;
			stmt != nullptr && usedForLoad != true;
			stmt = stmt->Next)
		{
			if (IsValueUnusedAfter(liveness, variable))
				return false;

			TraverseTree(
				mContext->Traversal.Space,
				mContext->Traversal.Count,
				stmt->Now,
				[&](TreeNode* node)
				{
					if (node->Is(NodeKinds::Load) &&
						ValueIs(node, NodeKinds::LocalVariable))
					{
						auto local = ValueAs<LocalVariableNode>(node);
						if (local->LocalIndex == variable)
							usedForLoad = true;
					}
				});

			liveness++;
		}
		
		return usedForLoad;
	}

	bool X86NativeCodeGenerator::IsUsedByAdjacentJcc(UInt16 variable)
	{
		//Should count empty stmt in
		Int32 lastLiveness = mCurrentBB->Liveness.size() - 1 - 2;
		if (mLiveness < lastLiveness)
			return false;
		if (mCurrentBB->BranchKind == PPKind::Conditional &&
			ValueIs(mCurrentBB->BranchConditionValue, NodeKinds::LocalVariable))
		{
			auto local = ValueAs<LocalVariableNode>(mCurrentBB->BranchConditionValue);
			if (local->LocalIndex == variable)
				return true;
		}
		return false;
	}

	void X86NativeCodeGenerator::CodeGenForBooleanStore(TreeNode* expression)
	{
		bool usedByAdjacentJcc = ST_VAR.has_value() ? IsUsedByAdjacentJcc(ST_VAR.value()) : false;
		auto mask = R8UnavailableMask;

		switch (expression->Kind)
		{
		case NodeKinds::Compare:
		{
			auto compare = expression->As<CompareNode>();
			CodeGenForBinary(compare->As<BinaryNodeAccessProxy>());

			if (usedByAdjacentJcc)
			{
				//Set to generate proper instruction
				JCC_COND = compare->Condition;
			}
			else
			{
				Instruction instruction{};
				switch (compare->Condition)
				{
				case CmpCondition::EQ:
					USE_INS(SET_EQ_I1); break;
				case CmpCondition::NE:
					USE_INS(SET_NE_I1); break;
				case CmpCondition::GT:
					USE_INS(SET_GT_I1); break;
				case CmpCondition::LT:
					USE_INS(SET_LT_I1); break;
				case CmpCondition::GE:
					USE_INS(SET_GE_I1); break;
				case CmpCondition::LE:
					USE_INS(SET_LE_I1); break;
				default:
					THROW("Unexpected condition");
				}
				
				if (ST_VAR.has_value())
				{
					UInt8 nativeReg = AllocateRegisterAndGenerateCodeFor(ST_VAR.value(), mask, CoreTypes::Bool, false);
					Emit(instruction, Operand::FromRegister(nativeReg, CoreTypes::Bool));
					//Set result register
					RE_REG = nativeReg;
				}
				else
				{
					RTAssert(ST_DST.has_value());
					//Use the operand generated from Store call
					Emit(instruction, ST_DST.value());
				}
			}
			break;
		}
		case NodeKinds::UnaryArithmetic:
		{
			auto unary = expression->As<UnaryArithmeticNode>();
			switch (unary->Opcode)
			{
				//Logical operation
			case OpCodes::Not:
			{
				RTAssert(ValueIs(unary, NodeKinds::LocalVariable));
				auto variable = ValueAs<LocalVariableNode>(unary);
				RTAssert(variable->TypeInfo->GetCoreType() == CoreTypes::Bool);

				UInt8 nativeReg = AllocateRegisterAndGenerateCodeFor(variable->LocalIndex, mask, CoreTypes::Bool);
				Operand operand = Operand::FromRegister(nativeReg, CoreTypes::Bool);
				ASM(TEST_MR_I1, operand, operand);

				if (usedByAdjacentJcc)
				{
					JCC_COND = CmpCondition::EQ;
				}
				else
				{
					ASM(SET_EQ_I1, ST_DST.value());
				}
				break;
			}
			default:
				THROW("Unsupported boolean operation");
			}
			break;
		}
		case NodeKinds::BinaryArithmetic:
		{
			auto binary = expression->As<BinaryArithmeticNode>();
			switch (binary->Opcode)
			{
			case OpCodes::And:
			case OpCodes::Or:
			case OpCodes::Xor:
				CodeGenForBinary(binary->As<BinaryNodeAccessProxy>());
				break;
			default:
				THROW("Unsupported boolean operation");
			}
			RTAssert(RE_REG.has_value());
			//The instruction will set the EFlags, so directly use JE
			if (usedByAdjacentJcc)
			{
				JCC_COND = CmpCondition::EQ;
			}
			break;
		}
		}
	}

	void X86NativeCodeGenerator::CodeGenForShift(
		Int32 localCount,
		LocalVariableNode* locals[2],
		ConstantNode* constant,
		bool immLeft,
		UInt8 opcode)
	{
		bool byOne = false;
		bool useImm = false;
		UInt8 coreType = CoreTypes::I1;
		Operand left{};
		Operand right{};

		RCS(session);
		if (localCount == 1)
		{
			auto local = locals[0];
			if (immLeft)
			{
				coreType = constant->CoreType;

				left = UseOperandFrom(session, constant, OperandOptions::ForceRegister);
				right = UseOperandFrom(session, local, OperandOptions::ForceRegister, MSK_REG(NREG::CX));
				if (right.UnifiedCoreType != CoreTypes::I2)
				{
					auto converted = Operand::FromRegister(right.Register, CoreTypes::I2);
					CodeGenForConvertCore(right.UnifiedCoreType, CoreTypes::I2, right, converted);
					right = converted;
				}

				RE_REG = left.Register;
			}
			else
			{
				coreType = local->TypeInfo->GetCoreType();
				left = UseOperandFrom(session, local, OperandOptions::ForceRegister);			
				right = Operand::FromImmediate(constant->I1, CoreTypes::I1);
				if (constant->I1 == 1)
					byOne = true;
				useImm = true;

				INVALIDATE_VAR(local->LocalIndex);
				RE_REG = left.Register;			
			}
		}
		else if (localCount == 2)
		{
			left = UseOperandFrom(session, locals[0], OperandOptions::ForceRegister);
			right = UseOperandFrom(session, locals[1], OperandOptions::ForceRegister, MSK_REG(NREG::CX));
			if (right.UnifiedCoreType != CoreTypes::I2)
			{
				auto converted = Operand::FromRegister(right.Register, CoreTypes::I2);
				CodeGenForConvertCore(right.UnifiedCoreType, CoreTypes::I2, right, converted);
				right = converted;
			}

			INVALIDATE_VAR(locals[0]->LocalIndex);
			RE_REG = left.Register;
		}

		Instruction instruction{};
		if (opcode == OpCodes::Shr)
		{
			switch (coreType)
			{
			case CoreTypes::I1:
				if (byOne) USE_INS(SAR_M1_I1)
				else if (useImm) USE_INS(SAR_MI_I1)
				else USE_INS(SAR_MR_I1)
					break;
			case CoreTypes::U1:
				if (byOne) USE_INS(SHR_M1_I1)
				else if (useImm) USE_INS(SHR_MI_I1)
				else USE_INS(SHR_MR_I1)
					break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
				if (byOne) USE_INS(SAR_M1_IU)
				else if (useImm) USE_INS(SAR_MI_IU)
				else USE_INS(SAR_MR_IU)
					break;
			case CoreTypes::U2:			
			case CoreTypes::U4:			
			case CoreTypes::U8:
				if (byOne) USE_INS(SHR_M1_IU)
				else if (useImm) USE_INS(SHR_MI_IU)
				else USE_INS(SHR_MR_IU)
					break;
			}
		}
		else
		{
			switch (Operand::UnifyCoreType(coreType))
			{
			case CoreTypes::I1:
				if (byOne) USE_INS(SHL_M1_I1)
				else if (useImm) USE_INS(SHL_MI_I1)
				else USE_INS(SHL_MR_I1)
					break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
				if (byOne) USE_INS(SHL_M1_IU)
				else if (useImm) USE_INS(SHL_MI_IU)
				else USE_INS(SHL_MR_IU)
					break;
			}
		}

		Emit(instruction, left, right);
	}

	void X86NativeCodeGenerator::CodeGenForConvert(ConvertNode* conv)
	{
		auto to = conv->To;
		auto from = conv->Value->TypeInfo->GetCoreType();
		auto operand = CodeGenForConvert(from, to, conv->Value);
		RTAssert(operand.IsRegister());

		RE_REG = operand.Register;
	}

	Operand X86NativeCodeGenerator::CodeGenForConvert(UInt8 from, UInt8 to, TreeNode* expression)
	{
		RCS(session);
		auto origin = UseOperandFrom(session, expression, OperandOptions::ForceRegister);
		RTAssert(origin.IsRegister());

		from = CoreTypes::GetSignedType(from);
		to = CoreTypes::GetSignedType(to);

		bool canBeReused = false;
		switch (expression->Kind)
		{
		case NodeKinds::Constant:
			canBeReused = true; 
			break;
		case NodeKinds::Load:
		{
			RTAssert(ValueIs(expression, NodeKinds::LocalVariable));
			auto local = ValueAs<LocalVariableNode>(expression);
			if (IsVariableUnusedFromNow(local->LocalIndex))
			{
				mRegContext->TryInvalidateFor(local->LocalIndex);
				canBeReused = true;
			}
			break;
		}
		}

		bool isCompatible =
			(CoreTypes::IsIntegerLike(from) && CoreTypes::IsIntegerLike(to)) ||
			(CoreTypes::IsFloatLike(from) && CoreTypes::IsFloatLike(to));

		Operand converted{};

		if (isCompatible && canBeReused)
			converted = Operand::FromRegister(origin.Register, to);
		else
		{
			auto mask = GetMaskFor(to);
			auto nativeReg = AllocateRegisterAndGenerateCodeFor(mask, to);
			converted = Operand::FromRegister(nativeReg, to);
		}

		CodeGenForConvertCore(from, to, origin, converted);

		return converted;
	}

	void X86NativeCodeGenerator::CodeGenForConvertCore(UInt8 from, UInt8 to, Operand const& origin, Operand const& converted)
	{
		switch (from)
		{
		case CoreTypes::I8:
			switch (to)
			{
			case CoreTypes::I8:
			case CoreTypes::I4:
				return;
			case CoreTypes::I2:
				ASM(MOVZX_RM_I4_I2, converted, origin); break;
			case CoreTypes::I1:
				ASM(MOVZX_RM_I4_I1, converted, origin); break;
			case CoreTypes::R4:
				ASM(XORPS_RM, converted, converted);
				ASM(CVTSI2SS_RM, converted, origin);
				break;
			case CoreTypes::R8:
				ASM(XORPD_RM, converted, converted);
				ASM(CVTSI2SD_RM, converted, origin);
				break;
			}
			break;
		case CoreTypes::I4:
			switch (to)
			{
			case CoreTypes::I8:
				ASM(MOVSX_RM_I8_I4, converted, origin); break;
			case CoreTypes::I4:
				return;
			case CoreTypes::I2:
				ASM(MOVZX_RM_I4_I2, converted, origin); break;
			case CoreTypes::I1:
				ASM(MOVZX_RM_I4_I1, converted, origin); break;
			case CoreTypes::R4:
				ASM(MOVD_RM, converted, origin);
				ASM(CVTDQ2PS_RM, converted, converted); break;
			case CoreTypes::R8:
				ASM(MOVD_RM, converted, origin);
				ASM(CVTDQ2PD_RM, converted, converted); break;
			}
			break;
		case CoreTypes::I2:
			switch (to)
			{
			case CoreTypes::I8:
				ASM(MOVSX_RM_I8_I2, converted, origin); break;
			case CoreTypes::I4:
				ASM(MOVSX_RM_I4_I2, converted, origin); break;
			case CoreTypes::I2:
				return;
			case CoreTypes::I1:
				ASM(MOVZX_RM_I2_I1, converted, origin); break;
			case CoreTypes::R4:
			{
				auto i4 = REGV(origin.Register, CoreTypes::I4);
				CodeGenForConvertCore(CoreTypes::I2, CoreTypes::I4, origin, i4);
				CodeGenForConvertCore(CoreTypes::I4, CoreTypes::R4, i4, converted);
				break;
			}
			case CoreTypes::R8:
			{
				auto i4 = REGV(origin.Register, CoreTypes::I4);
				CodeGenForConvertCore(CoreTypes::I2, CoreTypes::I4, origin, i4);
				CodeGenForConvertCore(CoreTypes::I4, CoreTypes::R8, i4, converted);
				break;
			}
			}
			break;
		case CoreTypes::I1:
			switch (to)
			{
			case CoreTypes::I8:
				ASM(MOVSX_RM_I8_I1, converted, origin); break;
			case CoreTypes::I4:
				ASM(MOVSX_RM_I4_I1, converted, origin); break;
			case CoreTypes::I2:
				ASM(MOVSX_RM_I2_I1, converted, origin); break;
			case CoreTypes::I1: return;
			case CoreTypes::R4:
			{
				auto i4 = REGV(origin.Register, CoreTypes::I4);
				CodeGenForConvertCore(CoreTypes::I1, CoreTypes::I4, origin, i4);
				CodeGenForConvertCore(CoreTypes::I4, CoreTypes::R4, i4, converted);
				break;
			}
			case CoreTypes::R8:
			{
				auto i4 = REGV(origin.Register, CoreTypes::I4);
				CodeGenForConvertCore(CoreTypes::I1, CoreTypes::I4, origin, i4);
				CodeGenForConvertCore(CoreTypes::I4, CoreTypes::R8, i4, converted);
				break;
			}
			}
			break;
		case CoreTypes::R4:
			switch (to)
			{
			case CoreTypes::I8:
				ASM(CVTTSS2SI_RM, converted, origin); break;
			case CoreTypes::I4:
				ASM(CVTTSS2SI_RM, converted, origin); break;
			case CoreTypes::I2:
			{
				auto i4 = REGV(converted.Register, CoreTypes::I4);
				CodeGenForConvertCore(CoreTypes::R4, CoreTypes::I4, origin, i4);
				CodeGenForConvertCore(CoreTypes::I4, CoreTypes::I2, i4, converted);
				break;
			}
			case CoreTypes::I1:
			{
				auto i4 = REGV(converted.Register, CoreTypes::I4);
				CodeGenForConvertCore(CoreTypes::R4, CoreTypes::I4, origin, i4);
				CodeGenForConvertCore(CoreTypes::I4, CoreTypes::I1, i4, converted);
				break;
			}
			case CoreTypes::R4:
				return;
			case CoreTypes::R8:
				ASM(CVTSS2SD_RM, converted, origin); break;
			}
			break;
		case CoreTypes::R8:
			switch (to)
			{
			case CoreTypes::I8:
				ASM(CVTTSD2SI_RM, converted, origin); break;
			case CoreTypes::I4:
				ASM(CVTTSD2SI_RM, converted, origin); break;
			case CoreTypes::I2:
			{
				auto i4 = REGV(converted.Register, CoreTypes::I4);
				CodeGenForConvertCore(CoreTypes::R8, CoreTypes::I4, origin, i4);
				CodeGenForConvertCore(CoreTypes::I4, CoreTypes::I1, i4, converted);
				break;
			}
			case CoreTypes::I1:
			{
				auto i4 = REGV(converted.Register, CoreTypes::I4);
				CodeGenForConvertCore(CoreTypes::R8, CoreTypes::I4, origin, i4);
				CodeGenForConvertCore(CoreTypes::I4, CoreTypes::I1, i4, converted);
				break;
			}
			case CoreTypes::R4:
				ASM(CVTSD2SS_RM, converted, origin); break;
			case CoreTypes::R8:
				return;
			}
			break;
		}
	}

	Operand X86NativeCodeGenerator::UseOperandFrom(
		RegisterConflictSession& session, 
		TreeNode* target, 
		UInt8 options,
		std::optional<UInt64> maskOpt)
	{
		UInt8 actualCoreType = CoreTypes::Ref;
		switch (options & OperandOptions::MemorySemanticMask)
		{
		case OperandOptions::AddressOf:
			actualCoreType = CoreTypes::InteriorRef; 
			break;
		case OperandOptions::ContentOf:
			actualCoreType = target->TypeInfo->GetFirstTypeArgument()->GetCoreType();
			break;
		case OperandOptions::ValueOf:
			actualCoreType = target->TypeInfo->GetCoreType();
			break;
		}
	
		auto mask = GetMaskFor(actualCoreType, maskOpt);

		//Disable partial register
		if (CoreTypes::GetCoreTypeSize(actualCoreType) == 1)
			mask &= R8UnavailableMask;

		switch (target->Kind)
		{
		case NodeKinds::Load:
		{
			auto load = target->As<LoadNode>();
			UInt8 newOptions = options;
			switch (load->Mode)
			{
			case AccessMode::Address: newOptions |= OperandOptions::AddressOf; break;
			case AccessMode::Content: newOptions |= OperandOptions::ContentOf; break;
			}
		
			auto value = ValueAs<LocalVariableNode>(load);
			switch (value->Kind)
			{
			case NodeKinds::LocalVariable:
			case NodeKinds::ArrayOffsetOf:
			case NodeKinds::OffsetOf:
			case NodeKinds::Constant:
				return UseOperandFrom(session, value, newOptions, maskOpt);
			default:
				THROW("Unknown load source");
			}
		}
		case NodeKinds::LocalVariable:
		{
			auto local = target->As<LocalVariableNode>();
			return UseLocal(session, local, options, USE_MASK);
		}
		case NodeKinds::Constant:
		{
			auto constant = target->As<ConstantNode>();
			return UseImmediate(constant, options & OperandOptions::ForceRegister, USE_MASK);
		}
		case NodeKinds::OffsetOf:
		{
			auto offset = target->As<OffsetOfNode>();
			return UseOffsetOf(session, offset, options, maskOpt);
		}
		case NodeKinds::ArrayOffsetOf:
		{
			auto offset = target->As<ArrayOffsetOfNode>();
			return UseArrayOffsetOf(session, offset, options, maskOpt);
		}
		default:
			THROW("Not implemented");
			break;
		}
	}

	Operand X86NativeCodeGenerator::UseAddressOfLocal(RegisterConflictSession& session, LocalVariableNode* local, UInt8 options, std::optional<UInt64> maskOpt)
	{
		auto mask = maskOpt.value_or(NREG::Common);

		auto localCoreType = local->TypeInfo->GetCoreType();
		auto isRef = CoreTypes::IsRef(localCoreType);
		auto localIndex = local->LocalIndex;

		//Need to check if local is on register
		auto currentRegister = mRegContext->TryGetRegister(localIndex);

		if (currentRegister.has_value())
		{
			if (IsAsRefSemantic(localIndex))
			{
				MARK_UNSPILLABLE(currentRegister.value());
				Operand::FromRegister(currentRegister.value(), CoreTypes::InteriorRef);
			}
			LandVariableFor(localIndex);
		}
			
		Operand onStackAddress{};
		USE_DISP(onStackAddress, localIndex, localCoreType);

		if (IsAsRefSemantic(localIndex))
		{
			auto nativeReg = AllocateRegisterAndGenerateCodeFor(mask, CoreTypes::InteriorRef);
			auto resultOperand = Operand::FromRegister(nativeReg, CoreTypes::InteriorRef);
			MARK_UNSPILLABLE(nativeReg);

			ASM(MOV_RM_IU, resultOperand, onStackAddress);
			return resultOperand;
		}
		else
		{
			if (options & OperandOptions::ForceRegister)
			{
				auto nativeReg = AllocateRegisterAndGenerateCodeFor(mask, CoreTypes::InteriorRef);
				auto resultOperand = Operand::FromRegister(nativeReg, CoreTypes::InteriorRef);
				MARK_UNSPILLABLE(nativeReg);

				ASM(LEA_IU, resultOperand, onStackAddress);
				return resultOperand;
			}
			return onStackAddress;
		}
	}

	Operand X86NativeCodeGenerator::UseContentOfLocal(RegisterConflictSession& session, LocalVariableNode* local, UInt8 options, std::optional<UInt64> maskOpt)
	{
		auto refType = local->TypeInfo;
		auto interiorRef = Meta::MetaData->GetIntrinsicTypeFromCoreType(CoreTypes::InteriorRef);
		if (refType->GetCanonicalType() != interiorRef)
			THROW("Unexpected type of local variable");

		auto contentCoreType = refType->GetFirstTypeArgument()->GetCoreType();
		bool isRegisterCompatible =
			CoreTypes::IsIntegerLike(contentCoreType) ||
			CoreTypes::IsCategoryRef(contentCoreType);
		bool isI1Like = CoreTypes::GetCoreTypeSize(contentCoreType);
		bool canBeTransferred = IsVariableUnusedFromNow(local->LocalIndex);

		auto instruction = RetriveBinaryInstruction(
			SemanticGroup::MOV,
			contentCoreType,
			OperandPreference::Register,
			OperandPreference::Displacement);

		auto currentAddressRegister = mRegContext->TryGetRegister(local->LocalIndex);
		if (currentAddressRegister.has_value())
		{
			Operand memory = Operand::FromDisplacement(currentAddressRegister.value(), 0, contentCoreType);
			Operand result{};
			//Already has register
			if (isRegisterCompatible)
			{
				auto mask = maskOpt.value_or(NREG::Common);
				//Special for r8
				if (isI1Like)
					mask &= R8UnavailableMask;

				if (Bit::TestAt(mask, currentAddressRegister.value()) && canBeTransferred)
				{
					//If result operand can meet the requirements and be transferred
					result = Operand::FromRegister(currentAddressRegister.value(), contentCoreType);
				}
				else
				{
					//Use new register
					result = Operand::FromRegister(AllocateRegisterAndGenerateCodeFor(mask, contentCoreType), contentCoreType);
				}
			}
			else
			{
				auto resultMask = GetMaskFor(contentCoreType, maskOpt);
				//Directly use new register
				result = Operand::FromRegister(AllocateRegisterAndGenerateCodeFor(resultMask, contentCoreType), contentCoreType);
			}
			Emit(instruction, result, memory);
			return result;
		}
		else
		{
			Operand memory{};
			Operand result{};
			if (isRegisterCompatible)
			{
				auto mask = maskOpt.value_or(NREG::Common);
				if (isI1Like)
					mask &= R8UnavailableMask;

				if (canBeTransferred && mRegContext->CanAllocateFor(mask))
				{
					//Use the same register
					auto nativeReg = AllocateRegisterAndGenerateCodeFor(mask, contentCoreType);
					result = Operand::FromRegister(nativeReg, contentCoreType);
					memory = Operand::FromDisplacement(nativeReg, 0, contentCoreType);
				}
				else
				{
					//Use different register
					auto resultReg = AllocateRegisterAndGenerateCodeFor(mask, contentCoreType);
					auto address = UseLocal(session, local, OperandOptions::ForceRegister, NREG::Common);
					memory = Operand::FromDisplacement(address.Register, 0, contentCoreType);
					result = Operand::FromRegister(resultReg, contentCoreType);
				}
			}
			else
			{
				auto resultMask = GetMaskFor(contentCoreType, maskOpt);
				//Directly use new register
				result = Operand::FromRegister(AllocateRegisterAndGenerateCodeFor(resultMask, contentCoreType), contentCoreType);
				auto address = UseLocal(session, local, OperandOptions::ForceRegister, NREG::Common);
				memory = Operand::FromDisplacement(address.Register, 0, contentCoreType);
			}
			Emit(instruction, result, memory);
			return result;
		}
	}

	Operand X86NativeCodeGenerator::UseLocal(RegisterConflictSession& session, LocalVariableNode* local, UInt8 options, std::optional<UInt64> maskOpt)
	{
		switch (options & OperandOptions::MemorySemanticMask)
		{
		case OperandOptions::AddressOf:
		{
			return UseAddressOfLocal(session, local, options, maskOpt);
		}
		case OperandOptions::ContentOf:
		{
			return UseContentOfLocal(session, local, options, maskOpt);
		}
		}

		//Load value
		auto variable = local->LocalIndex;
		auto coreType = local->TypeInfo->GetCoreType();
		auto mask = GetMaskFor(coreType, maskOpt);

		//Force register
		if (options & OperandOptions::ForceRegister)
		{
			auto nativeReg = AllocateRegisterAndGenerateCodeFor(
				variable,
				USE_MASK,
				coreType,
				!(options & OperandOptions::AllocateOnly));

			if (options & OperandOptions::InvalidateVariable)
				InvalidateVaraible(variable);

			MARK_UNSPILLABLE(nativeReg);
			return Operand::FromRegister(nativeReg, coreType);
		}

		//AutoRMI
		auto [reg, furtherOperation] = mRegContext->AllocateRegisterFor(variable, USE_MASK);
		if (furtherOperation)
		{
			if (IsVariableUnusedFromNow(variable))
			{
				//Consider liveness
				Operand operand{};
				USE_DISP(operand, variable, coreType);
				return operand;
			}
			else
			{
				//Spill someone else
				auto newRegister = mRegContext->TryGetFreeRegister(USE_MASK);
				if (!newRegister.has_value())
				{
					auto [spillVariable, spillReg] = RetriveSpillCandidate(USE_MASK, mLiveness);
					SpillAndGenerateCodeFor(spillVariable, coreType);
					//Assign to spill variable
					newRegister = spillReg;
				}

				if (!(options & OperandOptions::AllocateOnly))
				{
					//Requires loading
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

				if (!(options & OperandOptions::InvalidateVariable))
					mRegContext->Establish(variable, newRegister.value());

				MARK_UNSPILLABLE(newRegister.value());
				return Operand::FromRegister(newRegister.value(), coreType);
			}
		}
		else
		{
			MARK_UNSPILLABLE(reg.value());
			return Operand::FromRegister(reg.value(), coreType);
		}
	}

	Operand X86NativeCodeGenerator::UseOffsetOf(RegisterConflictSession& session, OffsetOfNode* offset, UInt8 options, std::optional<UInt64> maskOpt)
	{
		auto mask = maskOpt.value_or(NREG::Common);
		/*Obviously, normal offset of indirect model is different from flatten model
		* 1. Ref on stack: fetch -> add
		* 2. Ref on register: add with register
		* 3. Struct on stack: add with bp
		* 4. Struct (ref) is the same with ref
		*/

		auto base = offset->Base;
		RTAssert(base->Is(NodeKinds::Load));
		RTAssert(ValueIs(base, NodeKinds::LocalVariable));
		auto local = ValueAs<LocalVariableNode>(base);
		auto validOffset = offset->Offset > 0;
		auto coreType = offset->TypeInfo->GetCoreType();

		auto originAddress = UseObjectAddressOfLocal(session, local, options, maskOpt);

		if (originAddress.IsRegister())
		{
			//Adjust to offset
			auto targetAddress = Operand::FromDisplacement(originAddress.Register, offset->Offset, CoreTypes::Ref);
			MARK_UNSPILLABLE(originAddress.Register);
			if (options & OperandOptions::ReserveAsMemory)
				return targetAddress;

			if (options & OperandOptions::AddressOf)
			{
				if (validOffset) {
					Operand resultOperand{};
					if (IsVariableUnusedFromNow(local->LocalIndex))
					{
						resultOperand = originAddress;
						mRegContext->TryInvalidateFor(local->LocalIndex);
					}
					else
					{
						resultOperand = Operand::FromRegister(AllocateRegisterAndGenerateCodeFor(mask, CoreTypes::Ref), CoreTypes::Ref);
					}

					ASM(LEA_IU, resultOperand, targetAddress);

					return resultOperand;
				}
				
				return originAddress;
			}
			
			if (options & OperandOptions::ForceRegister)
			{
				//Load value
				auto instruction = RetriveBinaryInstruction(
					SemanticGroup::MOV, coreType, OperandPreference::Register, OperandPreference::Displacement);

				Operand destination = originAddress;
				//See if address register is suitable for result value
				auto valueMask = GetMaskFor(coreType, maskOpt);
				if (!(valueMask & MSK_REG(originAddress.Register)))
				{
					//Does not fit
					auto newRegister = AllocateRegisterAndGenerateCodeFor(valueMask, coreType);
					MARK_SPILLABLE(originAddress.Register);
					destination = Operand::FromRegister(newRegister, coreType);
					MARK_UNSPILLABLE(newRegister);
				}

				Emit(instruction, destination, targetAddress);
				return destination;
			}
			else
			{
				return targetAddress;
			}
		}
		else
		{
			originAddress.M.Displacement = offset->Offset;

			if (options & OperandOptions::ReserveAsMemory)
				return originAddress;

			if (options & OperandOptions::AddressOf)
			{
				auto resultRegister = AllocateRegisterAndGenerateCodeFor(mask, CoreTypes::InteriorRef);
				auto resultOperand = Operand::FromRegister(resultRegister, CoreTypes::InteriorRef);
				ASM(LEA_IU, resultOperand, originAddress);

				return resultOperand;
			}
			else if (options & OperandOptions::ForceRegister)
			{
				auto valueMask = GetMaskFor(coreType, maskOpt);
				auto newRegister = AllocateRegisterAndGenerateCodeFor(valueMask, coreType);
				auto registerOperand = Operand::FromRegister(newRegister, coreType);
				MARK_UNSPILLABLE(newRegister);

				auto instruction = RetriveBinaryInstruction(
					SemanticGroup::MOV, coreType, OperandPreference::Register, OperandPreference::Displacement);

				Emit(instruction, registerOperand, originAddress);
				return registerOperand;
			}
			else
				return originAddress;
		}
	}

	Operand X86NativeCodeGenerator::UseArrayOffsetOf(RegisterConflictSession& session, ArrayOffsetOfNode* offset, UInt8 options, std::optional<UInt64> maskOpt)
	{
		//TODO
		RTAssert(offset->Array->Is(NodeKinds::LocalVariable));
		Operand source{ OperandPreference::SIB };

		{
			//Set base register
			RCS(newSession);
			auto arrayVar = offset->Array->As<LocalVariableNode>();
			auto arrayReg = UseOperandFrom(newSession, arrayVar, OperandOptions::ForceRegister);
			MARK_UNSPILLABLE(arrayReg.Register);

			source.M.SIB.Base = arrayReg.Register;
		}

		bool needSpecialHandle = false;

		//See if we can directly use the scale
		auto scale = offset->Scale;
		switch (offset->Scale)
		{
		case 1: case 2: case 4: case 8:
			scale = offset->Scale; break;
		default:
		{
			//Need a separate instruction to calculate index * scale
			needSpecialHandle = true;
			scale = 1;
			break;
		}
		}

		std::optional<UInt8> scaleRegister{};
		{
			//Set index
			RCS(newSession);
			auto indexVar = offset->Index->As<LocalVariableNode>();
			auto indexReg = UseOperandFrom(newSession, indexVar, OperandOptions::ForceRegister);
			MARK_UNSPILLABLE(indexReg.Register);

			if (needSpecialHandle)
			{
				auto indexCoreType = indexVar->TypeInfo->GetCoreType();

				auto scaleReg = UseImmediate(new (POOL) ConstantNode(offset->Scale), true);
				scaleRegister = scaleReg.Register;
				Instruction mul = RetriveBinaryInstruction(
					SemanticGroup::MUL, indexCoreType, OperandPreference::Register, OperandPreference::Register);

				Emit(mul, scaleReg, indexReg);

				MARK_UNSPILLABLE(scaleRegister.value());
			}
		}

		////Set scale
		//source.M.SIB.Scale = scale;

		//if (options & OperandOptions::ForceRegister)
		//{
		//	//Need to load into register
		//	Instruction instruction = RetriveBinaryInstruction(
		//		SemanticGroup::MOV,
		//		actualCoreType,
		//		OperandPreference::Register,
		//		OperandPreference::Displacement);

		//	Operand destination{};
		//	//Can we reuse the scale register?
		//	if (scaleRegister.has_value() && Bit::TestAt(mask, scaleRegister.value()))
		//		destination = Operand::FromRegister(scaleRegister.value());
		//	else
		//	{
		//		UInt8 nativeReg = AllocateRegisterAndGenerateCodeFor(USE_MASK, actualCoreType);
		//		destination = Operand::FromRegister(nativeReg);
		//	}

		//	Emit(instruction, destination, source);
		//	//Restore
		//	MARK_SPILLABLE(source.M.SIB.Base.value());
		//	MARK_SPILLABLE(source.M.SIB.Index.value());
		//	if (scaleRegister.has_value())
		//		MARK_SPILLABLE(scaleRegister.value());
		//	return destination;
		//}
		//else
			return source;
	}

	Operand X86NativeCodeGenerator::UseObjectAddressOfLocal(RegisterConflictSession& session, LocalVariableNode* local, UInt8 options, std::optional<UInt64> maskOpt)
	{
		/*If it's a ref, get the ref value, which means address of object.
		* If it's a struct, get the address of struct
		*/
		auto mask = maskOpt.value_or(NREG::Common);

		auto isRef = CoreTypes::IsRef(local->TypeInfo->GetCoreType());
		auto localIndex = local->LocalIndex;

		//Need to check if local is on register
		auto currentRegister = mRegContext->TryGetRegister(localIndex);

		if (currentRegister.has_value())
		{
			//Ref, and struct (ref) on register can be directly used
			if (isRef || IsAsRefSemantic(localIndex))
				return Operand::FromRegister(currentRegister.value(), CoreTypes::Ref);
			else
			{
				//Normal struct requires landing
				LandVariableFor(localIndex);
				Operand address{};
				USE_DISP(address, localIndex, CoreTypes::Ref);

				return address;
			}
		}
		else
		{
			//Vice versa, Ref and struct (ref) on stack requires an indirect access
			if (isRef || IsAsRefSemantic(localIndex))
			{
				Operand onStackAddress{};
				USE_DISP(onStackAddress, localIndex, CoreTypes::Ref);

				auto resultRegister = AllocateRegisterAndGenerateCodeFor(mask, CoreTypes::InteriorRef);
				auto resultOperand = Operand::FromRegister(resultRegister, CoreTypes::InteriorRef);
				ASM(MOV_RM_IU, resultOperand, onStackAddress);

				return resultOperand;
			}
			else
			{
				Operand address{};
				USE_DISP(address, localIndex, CoreTypes::Ref);

				return address;
			}
		}
	}

	bool X86NativeCodeGenerator::IsAsRefSemantic(UInt16 variable) const
	{
		if (!LocalVariableNode::IsArgument(variable))
			return false;
		auto index = LocalVariableNode::GetIndex(variable);
		auto&& arg = GetCallingConv()->ArgumentPassway[index];
		return arg.IsRefLike();
	}

	UInt64 X86NativeCodeGenerator::GetMaskFor(UInt8 coreType, std::optional<UInt64> mask)
	{
		if (mask.has_value())
			return mask.value();

		if (CoreTypes::IsFloatLike(coreType))
			return NREG::CommonXMM;

		if (CoreTypes::GetCoreTypeSize(coreType) == 1)
			return NREG::Common & R8UnavailableMask;

		return NREG::Common;
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

	UInt8 X86NativeCodeGenerator::AllocateRegisterAndGenerateCodeFor(UInt64 originMask, UInt8 coreType)
	{
		auto mask = originMask;
		//Disable partial register
		if (CoreTypes::GetCoreTypeSize(coreType) == 1)
			mask &= R8UnavailableMask;

		auto freeReg = mRegContext->TryGetFreeRegister(USE_MASK);
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
			Operand dispOperand{ OperandPreference::Displacement };
			dispOperand.M.Displacement = DebugOffset32;
			//Use RIP
			dispOperand.UseRIPAddressing = true;

			auto newRegister = AllocateRegisterAndGenerateCodeFor(NREG::CommonXMM & additionalMask, coreType);
			result = Operand::FromRegister(newRegister, coreType);

			Instruction instruction = RetriveBinaryInstruction(
				SemanticGroup::MOV,
				coreType,
				OperandPreference::Register,
				OperandPreference::Displacement);

			ConstantWithType constantKey{ coreType, constant->Storage };
			//Track and record displacement
			TRK(Displacement);
			Emit(instruction, result, dispOperand);

			//Record
			mImmediateFix[constant->CoreType].Slots[constantKey].push_back(
				RIPFixUp{
					REP_TRK(Displacement).value(),
					mEmitPage->CurrentOffset(),
					mCurrentBB->Index
				}
			);
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

				result = Operand::FromRegister(newRegister, coreType);
				Instruction instruction = RetriveBinaryInstruction(
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

		Int32 xmmIndex = 0;
		GeneratePrologue(stackInfo, nonVolatiles, xmmIndex);
		GenerateEpilogue(stackInfo, nonVolatiles, xmmIndex);
		FixUpDisplacement();
		GenerateBranch();
		AssemblyCode();
	}

	void X86NativeCodeGenerator::FixUpImmediateDisplacement(EmitPage* finalPage)
	{
		for (auto&& [coreType, segment] : mImmediateFix)
		{
			auto size = CoreTypes::GetCoreTypeSize(coreType);
			Int32 i = 0;
			for (auto&& [key, fixups] : segment.Slots)
			{
				Int32 realOffset = segment.BaseOffset + i * size;
				//Write to data segment first
				RewritePageImmediate(finalPage, realOffset, coreType, key.Storage);

				for (auto&& fixup : fixups)
				{
					//Write rip
					Int32 ripOffset = realOffset - (mContext->BBs[fixup.BasicBlock]->PhysicalOffset + fixup.RIPBase);
					Int32 ripDisplacementOffset = mContext->BBs[fixup.BasicBlock]->PhysicalOffset + fixup.Offset;
					RewritePageImmediate(finalPage, ripDisplacementOffset, ripOffset);
				}

				i++;
			}
		}
	}

	void X86NativeCodeGenerator::MarkStructUsageGenerateLayout(TreeNode* target)
	{
		TraverseTree(mContext->Traversal.Space, mContext->Traversal.Count, target, 
			[&](TreeNode* value) 
			{
				if (value->Is(NodeKinds::LocalVariable))
				{
					auto local = value->As<LocalVariableNode>();
					if (CoreTypes::IsStruct(local->TypeInfo->GetCoreType()))
						MarkVariableGenerateLayout(local->LocalIndex);
				}
			});
	}

	void X86NativeCodeGenerator::FixUpDisplacement()
	{
		auto findAdditionalOffset = [&](Int32 bb, Int32 offset) -> std::optional<Int32> {
			auto it = mVariableDispOffset.find(bb);
			if (it == mVariableDispOffset.end())
				return {};

			auto offsetIt = it->second.find(offset);
			if (offsetIt != it->second.end())
				return offsetIt->second;
			else
				return {};
		};

		for (auto&& [variable, maps] : mVariableDispFix)
		{
			auto varBase = mContext->GetLocalBase(variable);
			if (varBase->Offset == LocalVariableAttachedBase::InvalidOffset)
				THROW("Unexpected usage of local variable. Please check if ShouldGenerateLayout is set.");
			for (auto&& [bb, offsets] : maps)
				for (auto offset : offsets)
				{
					auto finalOffset = varBase->Offset;
					//Provide ability to use offset of variable(flatten model) on stack
					auto addtionalOffset = findAdditionalOffset(bb, offset);
					finalOffset += addtionalOffset.value_or(0);

					RewritePageImmediate(mContext->BBs[bb]->NativeCode, offset, finalOffset);
				}
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
			for (auto&& bb : mContext->AliveBBs)
				totalCodeSize += bb->NativeCodeSize; 

			Int32 alignedDataSize = ComputeAlignedImmediateLayout();
			finalPage = new EmitPage(alignedDataSize, totalCodeSize);
		}

		Int32 epilogueOffset = 0;
		//Copy code to final page
		{
			auto copyToFinal = [&](EmitPage* page)
			{
				UInt8* address = finalPage->Prepare(page->CurrentOffset());
				std::memcpy(address, page->GetRaw(), page->CurrentOffset());
				Int32 retOffset = finalPage->CurrentOffset();
				finalPage->Commit(page->CurrentOffset());
			
				return retOffset;
			};

			copyToFinal(mProloguePage);
			for (auto&& bb : mContext->AliveBBs)
			{
				bb->PhysicalOffset = copyToFinal(bb->NativeCode);
			}
			epilogueOffset = copyToFinal(mEpiloguePage);
		}

		//Fix up jump offset
		{
			for (auto&& [bbIndex, fixUp] : mJmpOffsetFix)
			{
				auto sourceBB = mContext->BBs[bbIndex];
				Int32 finalOffset = sourceBB->PhysicalOffset + fixUp.Offset;

				Int32 targetOffset = 0;
				Int32 relativeBase = sourceBB->PhysicalOffset + sourceBB->NativeCodeSize;
				if (fixUp.BasicBlock == EpilogueBBIndex)
					targetOffset = epilogueOffset - relativeBase;
				else
					targetOffset = mContext->BBs[fixUp.BasicBlock]->PhysicalOffset - relativeBase;

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

		FixUpImmediateDisplacement(finalPage);
		//Set to executable
		finalPage->Finalize();
		mContext->NativeCode = finalPage;
	}

	Int32 X86NativeCodeGenerator::ComputeAlignedImmediateLayout()
	{
		Int32 currentOffset = 0;
		for (auto&& [coreType, segment] : mImmediateFix)
		{
			Int32 size = CoreTypes::GetCoreTypeSize(coreType);
			Int32 remain = currentOffset % size;
			//Make sure it's aligned
			if (remain != 0)
				currentOffset = currentOffset - remain + size;

			//Store offset
			segment.BaseOffset = currentOffset;
			currentOffset += segment.Slots.size() * size;
		}

		//Requires code to be 8-byte aligned
		Int32 remain = currentOffset % CodeAlignment;
		//Make sure it's aligned
		if (remain != 0)
			currentOffset = currentOffset - remain + CodeAlignment;

		return currentOffset;
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
			if (arg.InRegister())
				mRegContext->Establish(index, arg.SingleRegister);
		}
	}

	void X86NativeCodeGenerator::GeneratePrologue(
		RTEE::StackFrameInfo* info, 
		std::vector<UInt8> const& nonVolatileRegisters, 
		Int32& xmmIndex)
	{
		mEmitPage = mProloguePage = new EmitPage(16);
		Int32 i = 0;
		for (; i < nonVolatileRegisters.size(); ++i)
		{
			auto reg = nonVolatileRegisters[i];
			if (reg == NREG::BP || reg == NREG::SP)
				continue;
			if (reg >= NREG::XMM0)
				break;
			ASM(PUSH_R_IU, REGV(reg, CoreTypes::Ref));
		}

		//Export for epilogue
		xmmIndex = i;

		//Handle xmm non-volatile registers
		constexpr Int32 XMMSize = 8;
		Int32 totalXMMReserveSize = (nonVolatileRegisters.size() - i) * XMMSize;
		if (totalXMMReserveSize > 0)
		{
			ASM(MOV_RM_IU, REG(SP, CoreTypes::Ref), IMM32_U(totalXMMReserveSize));
			for (; i < nonVolatileRegisters.size(); ++i)
			{
				auto reg = nonVolatileRegisters[i];
				//Negative offset
				Int32 offset = -(Int32)(nonVolatileRegisters.size() - i) * XMMSize;
				auto reserveOperand = Operand::FromDisplacement(NREG::SP, offset, CoreTypes::Ref);
				ASM(MOVSD_MR, reserveOperand, REGV(reg, CoreTypes::R8));
			}
		}

		//Do not generate for empty stack
		if (info->StackSize == 0)
			return;

		ASM(PUSH_R_IU, REG(BP, CoreTypes::Ref));
		ASM(MOV_RM_IU, REG(BP, CoreTypes::Ref), REG(SP, CoreTypes::Ref));
		ASM(SUB_MI_IU, REG(SP, CoreTypes::Ref), IMM32_U(info->StackSize))
	}

	void X86NativeCodeGenerator::GenerateEpilogue(
		RTEE::StackFrameInfo* info, 
		std::vector<UInt8> const& nonVolatileRegisters,
		Int32 xmmIndex)
	{
		mEmitPage = mEpiloguePage = new EmitPage(16);
		if (info->StackSize > 0)
		{
			ASM(ADD_MI_IU, REG(SP, CoreTypes::Ref), IMM32_U(info->StackSize))
			ASM(POP_R_IU, REG(BP, CoreTypes::Ref));
		}

		Int32 i = (Int32)nonVolatileRegisters.size() - 1;
		//Handle xmm non-volatile registers
		constexpr Int32 XMMSize = 8;
		for (; i >= xmmIndex; i--)
		{
			auto reg = nonVolatileRegisters[i];
			//Negative offset
			Int32 offset = -(Int32)(nonVolatileRegisters.size() - i) * XMMSize;
			auto reserveOperand = Operand::FromDisplacement(NREG::SP, offset, CoreTypes::R8);
			ASM(MOVSD_RM, REGV(reg, CoreTypes::R8), reserveOperand);
		}

		//Common register
		for (; i >= 0; i--)
		{
			auto reg = nonVolatileRegisters[i];
			if (reg == NREG::BP || reg == NREG::SP)
				continue;
			ASM(POP_R_IU, REGV(reg, CoreTypes::Ref));
		}

		ASM(RET);
	}

	void X86NativeCodeGenerator::StartCodeGeneration()
	{
		Prepare();

		for (auto&& bb : mContext->AliveBBs)
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
			for (mCurrentStmt = bb->Now; mCurrentStmt != nullptr; mCurrentStmt = mCurrentStmt->Next, mLiveness++)
			{
				if (mCurrentStmt->Now != nullptr)
				{
					MarkStructUsageGenerateLayout(mCurrentStmt->Now);
					CodeGenFor(mCurrentStmt->Now);
				}
			}
			
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

	UInt8 X86NativeCodeGenerator::AllocateRegisterAndGenerateCodeFor(UInt16 variable, UInt64 originMask, UInt8 coreType, bool requireLoading)
	{
		auto mask = originMask;
		//Disable partial register
		if (CoreTypes::GetCoreTypeSize(coreType) == 1)
			mask &= R8UnavailableMask;

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

	bool X86NativeCodeGenerator::IsVariableUnusedFromNow(UInt16 variable) const
	{
		return IsValueUnusedAfter(mLiveness, variable);
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
		Emit(instruction, Operand::Empty(), Operand::Empty());
	}

	Instruction X86NativeCodeGenerator::RetriveUnaryInstruction(
		SemanticGroup semGroup,
		UInt8 coreType,
		OperandPreference preference)
	{
		Instruction instruction{};

		switch (semGroup)
		{
		case SemanticGroup::NOT:
		{
			switch (coreType)
			{
			case CoreTypes::Char:
			case CoreTypes::I1:
			case CoreTypes::U1:
				USE_INS(NOT_I1); break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
			case CoreTypes::U2:
			case CoreTypes::U4:
			case CoreTypes::U8:
				USE_INS(NOT_IU); break;
			default:
				THROW("Unsupported core type for NOT");
			}
			break;
		}
		case SemanticGroup::NEG:
		{
			switch (coreType)
			{
			case CoreTypes::Char:
			case CoreTypes::I1:
			case CoreTypes::U1:
				USE_INS(NEG_I1); break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
			case CoreTypes::U2:
			case CoreTypes::U4:
			case CoreTypes::U8:
				USE_INS(NEG_IU); break;
			default:
				THROW("Unsupported core type for NEG");
			}
			break;
		}
		}

		return instruction;
	}

	Instruction X86NativeCodeGenerator::RetriveBinaryInstruction(
		SemanticGroup semGroup, 
		UInt8 coreType, 
		OperandPreference destPreference, 
		OperandPreference sourcePreference)
	{
		Instruction instruction{};
		bool MR = IsAdvancedAddressing(destPreference);
		bool immLoad = sourcePreference == OperandPreference::Immediate;
		switch (semGroup)
		{
		case SemanticGroup::MOV:
		{
			switch (coreType)
			{
			case CoreTypes::Bool:
			case CoreTypes::Char:
			case CoreTypes::I1:
			case CoreTypes::U1:
				if (immLoad) USE_INS(MOV_MI_I1)
				else MR_USE_INS(MOV_MR_I1, MOV_RM_I1)
				break;
			case CoreTypes::I2:
			case CoreTypes::I4:		
			case CoreTypes::U2:
			case CoreTypes::U4:
				if (immLoad) USE_INS(MOV_MI_IU)
				else MR_USE_INS(MOV_MR_IU, MOV_RM_IU)
				break;
			case CoreTypes::I8:
			case CoreTypes::U8:
				if (immLoad) USE_INS(MOV_RI_IU)
				else MR_USE_INS(MOV_MR_IU, MOV_RM_IU)
				break;
			case CoreTypes::InteriorRef:
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
			case CoreTypes::Bool:
			case CoreTypes::Char:
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
		case SemanticGroup::SUB:
		{
			switch (coreType)
			{
			case CoreTypes::Bool:
			case CoreTypes::Char:
			case CoreTypes::I1:
			case CoreTypes::U1:
				if (immLoad) USE_INS(SUB_MI_I1)
				else MR_USE_INS(SUB_MR_I1, SUB_RM_I1)
					break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
			case CoreTypes::U2:
			case CoreTypes::U4:
			case CoreTypes::U8:
				if (immLoad) USE_INS(SUB_MI_IU)
				else MR_USE_INS(SUB_MR_IU, SUB_RM_IU)
					break;
			case CoreTypes::R4:
				if (immLoad) THROW("Unsupported SUB M <- I/R <- I for R4");
				if (MR) THROW("Unsupported SUB M <- R for R4");
				USE_INS(SUBSS_RM);
				break;
			case CoreTypes::R8:
				if (immLoad) THROW("Unsupported SUB M <- I/R <- I for R8");
				if (MR) THROW("Unsupported SUB M <- R for R8");
				USE_INS(SUBSD_RM);
				break;
			}
			break;
		}
		case SemanticGroup::MUL:
		{
			switch (coreType)
			{
			case CoreTypes::Bool:
			case CoreTypes::Char:
			case CoreTypes::I1:
			case CoreTypes::U1:
				if (immLoad || MR) THROW("Not supported");
				USE_INS(IMUL_RM_I1);
				break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
			case CoreTypes::U2:
			case CoreTypes::U4:
			case CoreTypes::U8:
				if (immLoad || MR) THROW("Not supported");
				USE_INS(IMUL_RM_IU);
				break;
			case CoreTypes::R4:
				if (immLoad) THROW("Unsupported ADD M <- I/R <- I for R4");
				if (MR) THROW("Unsupported ADD M <- R for R4");
				USE_INS(MULSS_RM);
				break;
			case CoreTypes::R8:
				if (immLoad) THROW("Unsupported ADD M <- I/R <- I for R8");
				if (MR) THROW("Unsupported ADD M <- R for R8");
				USE_INS(MULSD_RM);
				break;
			}
			break;
		}
		case SemanticGroup::MOD:
		case SemanticGroup::DIV:
		{
			switch (coreType)
			{
			case CoreTypes::Bool:
			case CoreTypes::Char:
			case CoreTypes::I1:
				if (immLoad || MR) THROW("Not supported");
				USE_INS(IDIV_RM_I1);
				break;
			case CoreTypes::U1:
				if (immLoad || MR) THROW("Not supported");
				USE_INS(DIV_RM_I1);
				break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
				if (immLoad || MR) THROW("Not supported");
				USE_INS(IDIV_RM_IU);
				break;
			case CoreTypes::U2:
			case CoreTypes::U4:
			case CoreTypes::U8:
				if (immLoad || MR) THROW("Not supported");
				USE_INS(DIV_RM_IU);
				break;
			case CoreTypes::R4:
				if (immLoad) THROW("Unsupported ADD M <- I/R <- I for R4");
				if (MR) THROW("Unsupported ADD M <- R for R4");
				USE_INS(DIVSS_RM);
				break;
			case CoreTypes::R8:
				if (immLoad) THROW("Unsupported ADD M <- I/R <- I for R8");
				if (MR) THROW("Unsupported ADD M <- R for R8");
				USE_INS(DIVSD_RM);
				break;
			}
			break;
		}
		case SemanticGroup::AND:
		{
			switch (coreType)
			{
			case CoreTypes::Bool:
			case CoreTypes::Char:
			case CoreTypes::I1:
			case CoreTypes::U1:
				if (immLoad) USE_INS(AND_MI_I1)
				else MR_USE_INS(AND_MR_I1, AND_RM_I1)
					break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
			case CoreTypes::U2:
			case CoreTypes::U4:
			case CoreTypes::U8:
				if (immLoad) USE_INS(AND_MI_IU)
				else MR_USE_INS(AND_MR_IU, AND_RM_IU)
					break;
			default:
				THROW("Unsupported core type for AND");
			}
			break;
		}
		case SemanticGroup::OR:
		{
			switch (coreType)
			{
			case CoreTypes::Bool:
			case CoreTypes::Char:
			case CoreTypes::I1:
			case CoreTypes::U1:
				if (immLoad) USE_INS(OR_MI_I1)
				else MR_USE_INS(OR_MR_I1, OR_RM_I1)
					break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
			case CoreTypes::U2:
			case CoreTypes::U4:
			case CoreTypes::U8:
				if (immLoad) USE_INS(OR_MI_IU)
				else MR_USE_INS(OR_MR_IU, OR_RM_IU)
					break;
			default:
				THROW("Unsupported core type for OR");
			}
			break;
		}
		case SemanticGroup::XOR:
		{
			switch (coreType)
			{
			case CoreTypes::Bool:
			case CoreTypes::Char:
			case CoreTypes::I1:
			case CoreTypes::U1:
				if (immLoad) USE_INS(XOR_MI_I1)
				else MR_USE_INS(XOR_MR_I1, XOR_RM_I1)
					break;
			case CoreTypes::I2:
			case CoreTypes::I4:
			case CoreTypes::I8:
			case CoreTypes::U2:
			case CoreTypes::U4:
			case CoreTypes::U8:
				if (immLoad) USE_INS(XOR_MI_IU)
				else MR_USE_INS(XOR_MR_IU, XOR_RM_IU)
					break;
			default:
				THROW("Unsupported core type for XOR");
			}
			break;
		}
		case SemanticGroup::CMP:
		{
			switch (coreType)
			{
			case CoreTypes::Bool:
			case CoreTypes::Char:
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
		case NodeKinds::Compare:
		case NodeKinds::BinaryArithmetic: break;
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

		bool immLeft = false;
		if (binary->First->Is(NodeKinds::Constant))
		{
			immLeft = true;
			constant = binary->First->As<ConstantNode>();
		}
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

		//Keep the same
		UInt64 destinationRegMask = regMask;

		Operand source{};
		Operand destination{};

		//For idiv/div and imul for I8/U8
		bool requiresSpecialDestinationRegister = false;
		bool requiresSpecialAllocationForMod = false;
		bool requiresConvert = false;
		bool requiresMOperandForSource = false;
		//For compare
		bool preservingDestination = false;
		SemanticGroup group{};
		switch (binary->Kind)
		{
		case NodeKinds::BinaryArithmetic:
		{
			auto binaryArithmetic = binary->As<BinaryArithmeticNode>();
			if (CoreTypes::IsIntegerLike(coreType))
			{
				switch (binaryArithmetic->Opcode)
				{
				case OpCodes::Mod:
					requiresSpecialAllocationForMod = true;
				case OpCodes::Div:
				{
					requiresMOperandForSource = true;
					requiresSpecialDestinationRegister = true;
					destinationRegMask = MSK_REG(NREG::AX);
					if (CoreTypes::IsSignedInteger(coreType))
						requiresConvert = true;

					//Pre-spill the variable if possible
					AllocateRegisterAndGenerateCodeFor(MSK_REG(NREG::DX), coreType);
					MARK_UNSPILLABLE(NREG::DX);
					break;
				}					
				case OpCodes::Mul:
					requiresMOperandForSource = true;
					if (coreType == CoreTypes::I1 ||
						coreType == CoreTypes::U1)
					{
						requiresSpecialDestinationRegister = true;
						destinationRegMask = MSK_REG(NREG::AX);
					}
					break;
				case OpCodes::Shl:
				case OpCodes::Shr:
					//Special treat for shift
					return CodeGenForShift(localCount, locals, constant, immLeft, binaryArithmetic->Opcode);
					break;
				}
			}

			group = (SemanticGroup)((UInt8)SemanticGroup::ADD + (binaryArithmetic->Opcode - OpCodes::Add));
			break;
		}
		case NodeKinds::Compare:
			preservingDestination = true;
			group = SemanticGroup::CMP; break;
		}

		if (constant != nullptr)
		{
			if (immLeft)
			{
				Operand immOperand = UseImmediate(constant, true, destinationRegMask);
				MARK_UNSPILLABLE(immOperand.Register);

				UInt16 variable = locals[0]->As<LocalVariableNode>()->LocalIndex;
				UInt8 reg = AllocateRegisterAndGenerateCodeFor(variable, regMask, coreType);
				MARK_UNSPILLABLE(reg);

				destination = immOperand;
				source = Operand::FromRegister(reg, coreType);
			}
			else
			{
				UInt16 variable = locals[0]->As<LocalVariableNode>()->LocalIndex;
				UInt8 reg = AllocateRegisterAndGenerateCodeFor(variable, destinationRegMask, coreType);
				MARK_UNSPILLABLE(reg);

				Operand immOperand = UseImmediate(constant, requiresMOperandForSource);
				if (immOperand.IsRegister())
					MARK_UNSPILLABLE(immOperand.Register);

				destination = Operand::FromRegister(reg, coreType);
				source = immOperand;

				if (!preservingDestination)
					INVALIDATE_VAR(variable);
			}
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
				UInt8 reg =
					AllocateRegisterAndGenerateCodeFor(leftVar, destinationRegMask, coreType);

				source = Operand::FromRegister(reg, coreType);
				destination = Operand::FromRegister(reg, coreType);
				//Remove the allocation
				if (!preservingDestination)
					INVALIDATE_VAR(leftVar);
			}
			else
			{
				auto requireAndMark = [&](UInt16 var, UInt64 mask) {
					UInt8 reg = AllocateRegisterAndGenerateCodeFor(var, mask, coreType);
					MARK_UNSPILLABLE(reg);
					return Operand::FromRegister(reg, coreType);
				};

				RTAssert(ST_VAR.has_value());
				auto storingVar = ST_VAR.value();

				destination = requireAndMark(leftVar, destinationRegMask);
				source = requireAndMark(rightVar, regMask);

				if (!preservingDestination)
					INVALIDATE_VAR(leftVar);
			}
		}

		//Choose instruction
		RTAssert(destination.Kind == OperandPreference::Register);

		if (requiresConvert)
		{
			ASM_CV(CDQ, coreType);
		}

		//Update result register
		if (requiresSpecialAllocationForMod)
			RE_REG = NREG::DX;
		else
			RE_REG = destination.Register;

		Instruction instruction = RetriveBinaryInstruction(group, coreType, OperandPreference::Register, source.Kind);

		//Emit instruction
		if (requiresSpecialDestinationRegister)
			Emit(instruction, source);
		else
			Emit(instruction, destination, source);
	}

	void X86NativeCodeGenerator::CodeGenForUnary(UnaryArithmeticNode* unaryNode)
	{
		RegisterConflictSession session{ &mGenContext };
		RE_REG = {};
		auto source = unaryNode->Value;
		auto coreType = unaryNode->Value->TypeInfo->GetCoreType();
		if (unaryNode->Opcode == OpCodes::Neg && CoreTypes::IsFloatLike(coreType))
		{
			//Special handling
			Operand operand = UseOperandFrom(session, source,
				OperandOptions::ForceRegister |
				OperandOptions::InvalidateVariable);

			RTAssert(operand.IsRegister());
			RE_REG = operand.Register;

			Operand dispOperand{ OperandPreference::Displacement };
			dispOperand.M.Displacement = DebugOffset32;
			//Use RIP
			dispOperand.UseRIPAddressing = true;

			//Track displacement
			TRK(Displacement);
			ConstantStorage storage{};

			if (coreType == CoreTypes::R4)
			{
				ASM(XORPS_RM, operand, dispOperand);
				storage.SIMD = (UInt8*)NegR4SSEConstant.data();
			}
			else
			{
				ASM(XORPD_RM, operand, dispOperand);
				storage.SIMD = (UInt8*)NegR8SSEConstant.data();
			}

			ConstantWithType key{ CoreTypes::SIMD128, storage };

			mImmediateFix[CoreTypes::SIMD128].Slots[key].push_back(
				RIPFixUp{
					REP_TRK(Displacement).value(),
					mEmitPage->CurrentOffset(),
					mCurrentBB->Index });
		}
		else
		{
			Operand operand = UseOperandFrom(session, source, 
				OperandOptions::ForceRegister | 
				OperandOptions::InvalidateVariable);

			RTAssert(operand.IsRegister());
			RE_REG = operand.Register;

			SemanticGroup semGroup = (SemanticGroup)((UInt8)SemanticGroup::NOT + unaryNode->Opcode - OpCodes::Not);
			Instruction ins = RetriveUnaryInstruction(semGroup, coreType, operand.Kind);
			Emit(ins, operand);
		}
	}

	void X86NativeCodeGenerator::PreCodeGenForJcc(TreeNode* conditionValue)
	{
		RegisterConflictSession session{ &mGenContext };
		if (!JCC_COND.has_value())
		{
			//Requires additional test instruction
			auto operand = UseOperandFrom(session, conditionValue);
			ASM(TEST_MR_I1, operand, operand);
		}

		//Record Jcc condition
		mJccConditions[mCurrentBB->Index] = JCC_COND;
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

				RCS(session);
				UseOperandFrom(session, returnValue, OperandOptions::ForceRegister, MSK_REG(returnPassReg));
			}
			break;
			case NodeKinds::Constant:
			{
				//Make use of UseImmediate
				UseImmediate(returnValue->As<ConstantNode>(), true, MSK_REG(returnPassReg));
				break;
			}		
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
			return CodeGenForUnary(node->As<UnaryArithmeticNode>());
		case NodeKinds::Convert:
			return CodeGenForConvert(node->As<ConvertNode>());
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
		USE_DISP(source, variable, coreType);

		Emit(ins, Operand::FromRegister(nativeRegister, coreType), source);
	}

	void X86NativeCodeGenerator::EmitLoadR2R(UInt8 toRegister, UInt8 fromRegister, UInt8 coreType)
	{
		auto ins = RetriveBinaryInstruction(
			SemanticGroup::MOV,
			coreType,
			OperandPreference::Register,
			OperandPreference::Register);

		Emit(ins, Operand::FromRegister(toRegister, coreType), Operand::FromRegister(fromRegister, coreType));
	}

	void X86NativeCodeGenerator::EmitStoreR2M(UInt8 nativeRegister, UInt16 variable, UInt8 coreType)
	{
		auto ins = RetriveBinaryInstruction(
			SemanticGroup::MOV,
			coreType,
			OperandPreference::Displacement,
			OperandPreference::Register);

		Operand dest{};
		USE_DISP(dest, variable, coreType);

		Emit(ins, dest, Operand::FromRegister(nativeRegister, coreType));
	}

	void X86NativeCodeGenerator::Emit(Instruction instruction, Operand const& left, Operand const& right)
	{
		constexpr Int32 InvalidPos = -1;
		constexpr bool x64 = RTP::CurrentWidth == RTP::Platform::Bit64;

		//REX Prefix determination
		UInt8 REX = 0u;
		UInt8 Operand16Prefix = 0u;
		{
			Int32 i2Count = 0;
			Int32 operandCount = 0;
			//If operand use extended registers
			bool use64Reg = false;
			auto setPrefixAccordingToUnifiedCoreType = [&](UInt8 unified) {
				switch (unified)
				{
				case CoreTypes::I2:
					i2Count++;
					break;
				case CoreTypes::Ref:
				case CoreTypes::InteriorRef:
					if (!x64) break;
				case CoreTypes::I8:
					if (!(instruction.Flags & NO_REXW_F))
						REX |= 0x48;
					break;
				}
			};

			auto setPrefix = [&](Operand const& operand)
			{
				if (operand.IsEmpty())
					return;
				operandCount++;
				setPrefixAccordingToUnifiedCoreType(operand.UnifiedCoreType);
				operand.ForEachRegister(
					[&](UInt8 reg)
					{
						use64Reg |=
							(NREG64::R8 <= operand.Register && operand.Register <= NREG64::R15) ||
							(NREG64::XMM8 <= operand.Register && operand.Register <= NREG64::XMM15);
					});
			};

			if (instruction.CoreType.has_value())
			{
				//Only instruction without operand can use implication core type
				setPrefixAccordingToUnifiedCoreType(instruction.CoreType.value());
				if (instruction.CoreType.value() == CoreTypes::I2)
					Operand16Prefix = 0x66;
			}
			else
			{
				setPrefix(left);
				setPrefix(right);
			}

			if (i2Count > 0 && (i2Count == operandCount))
				Operand16Prefix = 0x66;

			if (use64Reg)
				REX |= 0x40;
		}

		//Write 66H to use 16bit operand
		if (Operand16Prefix > 0u)
			WritePage(mEmitPage, Operand16Prefix);

		Int32 rexPos = InvalidPos;
		Int32 opcodeRegBytePos = -1;
		//Write REX prefiex
		if (REX > 0u)
		{
			//REX May occur at else where

			Int8 rexOffset = (instruction.Flags & InstructionFlags::REXPositionMSK) >> InstructionFlags::REXPositionShift;
			if (rexOffset > 0)
				WritePage(mEmitPage, instruction.Opcode, rexOffset);

			rexPos = WritePage(mEmitPage, REX);

			//Write the remain
			opcodeRegBytePos = WritePage(mEmitPage, instruction.Opcode + rexOffset, instruction.Length - rexOffset) + instruction.Length - rexOffset - 1;
		}
		else
		{
			//Write opcodes
			opcodeRegBytePos = WritePage(mEmitPage, instruction.Opcode, instruction.Length) + instruction.Length - 1;
		}

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

		auto handleDisplacement = [&](std::optional<Int32> displacement, bool forceDisp32 = false) -> UInt8
		{
			if (forceDisp32)
			{
				Disp32 = displacement;
				return AddressingMode::RegisterAddressingDisp32;
			}

			if (displacement.value_or(0) == 0)
			{
				//Special expression for [reg]
				return AddressingMode::RegisterAddressing;
			}
			else
			{
				//Optimize for code size
				if (std::numeric_limits<Int8>::min() <= displacement &&
					std::numeric_limits<Int8>::max() >= displacement)
				{
					Disp8 = (Int8)displacement.value();
					return AddressingMode::RegisterAddressingDisp8;
				}
				else
				{
					Disp32 = displacement;
					return AddressingMode::RegisterAddressingDisp32;
				}
			}
		};

		auto handleMOperand = [&](const Operand& operand) -> UInt8 {
			if (operand.RefVariable.has_value())
				trackingVariable = operand.RefVariable;

			constexpr UInt8 InvalidSIBBase = 0b101;
			constexpr UInt8 InvalidSIBIndex = 0b100;

			UInt8 modRM = 0;
			switch (operand.Kind)
			{
			case OperandPreference::Displacement:
			{
				if (!operand.M.Base.has_value())
				{
					//Forced to use disp32
					Disp32 = operand.M.Displacement;
					modRM = AddressingMode::RegisterAddressing | RegisterOrMemory::Disp32;
					//Direct access
					if (!operand.UseRIPAddressing)
						SIB = (InvalidSIBIndex << 3) | InvalidSIBBase;
					break;
				}

				constexpr UInt8 EmptySIB = 0b00100000;
				if (operand.M.Displacement.value_or(0) == 0 && (
					operand.M.Base == NREG64::R13 || operand.M.Base == NREG::BP))
				{
					//Special encoding
					modRM = AddressingMode::RegisterAddressingDisp8;
					//Force a disp8 for [R13]/[EBP] encoding to [R13/EBP + 0]
					Disp8 = 0;
				}
				else if (operand.M.Base == NREG64::R12 || operand.M.Base == NREG64::SP)
				{
					modRM = handleDisplacement(operand.M.Displacement);
					modRM |= RegisterOrMemory::SIB;
					//Use SIB
					SIB = EmptySIB | encodeRegIdentity(REXBit::B, operand.M.Base.value());
				}
				else
				{
					modRM = handleDisplacement(operand.M.Displacement, trackingVariable.has_value());
					modRM |= encodeRegIdentity(REXBit::B, operand.M.Base.value_or(RegisterOrMemory::Disp32));
				}

				break;
			}
			case OperandPreference::SIB:
			{
				if (REX > 0 && operand.M.SIB.Index == NREG::SP)
					THROW("Unable to use RSP in SIB as index");
				modRM = handleDisplacement(operand.M.Displacement);
				if (operand.M.Displacement.value_or(0) == 0 && (
					operand.M.SIB.Base == NREG::BP ||
					operand.M.SIB.Base == NREG64::R13))
				{
					//Force a disp8 for [R13/RBP/EBP + RX * 2] encoding to [R13/RBP/EBP + RX * 2 + 0]
					modRM = AddressingMode::RegisterAddressingDisp8;
					Disp8 = 0;
				}
				modRM |= RegisterOrMemory::SIB;
				//SIB registe encoding
				UInt8 sib = 0;
				sib |= encodeRegIdentity(REXBit::B, operand.M.SIB.Base.value_or(InvalidSIBBase));
				sib |= encodeRegIdentity(REXBit::X, operand.M.SIB.Index.value_or(InvalidSIBIndex)) << 3;

				//Scale encoding
				switch (operand.M.SIB.Scale)
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
		case NO_PRD_F:
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
			auto disp = Disp8.value();
			if (IS_TRK(Displacement))			
			{
				REP_TRK(Displacement) = mEmitPage->CurrentOffset();
				REP_TRK_SIZE(Displacement) = sizeof(UInt8);
			}
			WritePage(mEmitPage, disp);
		}
		else if (Disp32.has_value())
		{
			auto disp = Disp32.value();
			if (IS_TRK(Displacement))
			{
				REP_TRK(Displacement) = mEmitPage->CurrentOffset();
				REP_TRK_SIZE(Displacement) = sizeof(UInt32);
			}
			Int32 offset = WritePage(mEmitPage, disp);
			//Add fix up
			if (trackingVariable.has_value())
			{
				mVariableDispFix[trackingVariable.value()][mCurrentBB->Index].push_back(offset);
				if (disp != DebugOffset32)
					mVariableDispOffset[mCurrentBB->Index][offset] = disp;
			}
		}

		if (Immediate.has_value())
		{
			auto&& imm = Immediate.value();
			Int32 size = CoreTypes::GetCoreTypeSize(ImmediateCoreType);

			if (IS_TRK(Immediate))
			{
				REP_TRK(Immediate) = mEmitPage->CurrentOffset();
				REP_TRK_SIZE(Immediate) = size;
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