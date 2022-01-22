#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\Platform\Platform.h"
#include "..\..\..\Platform\PlatformSpecialization.h"
#include "..\..\..\Meta\TypeDescriptor.h"
#include "..\..\..\Exception\RuntimeException.h"
#include "..\..\OpCodes.h"
#include "..\..\..\Platform\X86Register.h"
#include "RegisterAllocationContext.h"
#include "..\EmitPage.h"
#include "..\JITFlow.h"

#define INS_1(NAME, B1) static constexpr std::array<UInt8, 1> NAME = { B1 }
#define INS_2(NAME, B1, B2) static constexpr std::array<UInt8, 2> NAME = { B1, B2 }
#define INS_3(NAME, B1, B2, B3) static constexpr std::array<UInt8, 3> NAME = { B1, B2, B3 }

namespace RTJ::Hex::X86
{
	/* M = Memory / Register
	*  R = Register
	*  I = Immediate
	*  IU = Universe operand size
	*  IX = Special operand size
	*/
	struct NativePlatformInstruction
	{
		INS_1(MOV_MR_I1, 0x88);
		INS_1(MOV_MR_IU, 0x89);
		INS_1(MOV_RM_I1, 0x8A);
		INS_1(MOV_RM_IU, 0x8B);
		INS_1(MOV_RI_I1, 0xB0);
		INS_1(MOV_RI_IU, 0xB8);

		INS_3(MOVSD_RM, 0xF2, 0x0F, 0x10);
		INS_3(MOVSD_MR, 0xF2, 0x0F, 0x11);
		
		INS_3(MOVSS_RM, 0xF3, 0x0F, 0x10);
		INS_3(MOVSS_MR, 0xF3, 0x0F, 0x11);
	};

	struct Instruction
	{
		std::array<UInt8, 3> Opcode;
		UInt8 Length : 4;
		UInt8 OperandSize : 4;
	};

	enum class OperandPreference
	{
		Register,
		Immediate,
		SIB,
		Displacement
	};

	static bool IsAdvancedAddressing(OperandPreference preference) {
		return preference == OperandPreference::Displacement || preference == OperandPreference::SIB;
	}

	enum class SemanticGroup
	{
		MOV,
		ADD,
		SUB,
		MUL,
		DIV
	};

	struct Operand
	{
		OperandPreference Kind;
		union
		{
			UInt8 Register;
			Int64 Immediate;
			struct
			{
				UInt8 Base;
				UInt8 Scale;
				UInt8 Index;
				Int32 Offset;
			} SIB;

			struct
			{
				UInt8 Base;
				Int32 Offset;
			} Displacement;
		};

		bool IsAdvancedAddressing()const {
			return X86::IsAdvancedAddressing(Kind);
		}
		static Operand FromRegister(UInt8 value)
		{
			Operand ret{ OperandPreference::Register };
			ret.Register = value;
			return ret;
		}
	};

	class X86NativeCodeGenerator : public IHexJITFlow
	{
		HexJITContext* mContext = nullptr;
		std::vector<VariableSet> mVariableLanded;
		std::vector<BasicBlock*> mSortedBB;
		BasicBlock* mCurrentBB = nullptr;
		RegisterAllocationContext* mRegContext = nullptr;
		Int32 mLiveness = 0;
		EmitPage* mEmitPage = nullptr;
	private:
		void EmitBinary(Instruction instruction, Operand const& left, Operand const& right, EmitPage* page = nullptr);
		Instruction RetriveBinaryInstruction(SemanticGroup semGroup, UInt8 coreType, OperandPreference destPreference, OperandPreference sourcePreference);
		void EmitLoadM2R(UInt8 nativeRegister, UInt16 variable, UInt8 coreType, EmitPage* page = nullptr);
		void EmitLoadR2R(UInt8 toRegister, UInt8 fromRegister, UInt8 coreType, EmitPage* page = nullptr);
		void EmitStoreR2M(UInt8 nativeRegister, UInt16 variable, UInt8 coreType, EmitPage* page = nullptr);

		bool IsValueUnusedAfter(Int32 livenessIndex, UInt16 variableIndex) const;
		UInt8 AllocateRegisterAndGenerateCodeFor(UInt16 variable, UInt64 mask, UInt8 coreType, bool requireLoading = true);
		std::optional<std::tuple<UInt8, UInt16>> RetriveSpillCandidate(UInt64 mask, Int32 livenessIndex);
		void MarkVariableOnFly(UInt16 variable, BasicBlock* bb = nullptr);
		void MarkVariableLanded(UInt16 variable, BasicBlock* bb = nullptr);

		void LandVariableFor(UInt16 variable, BasicBlock* bb = nullptr);

		void CodeGenFor(MorphedCallNode* call, TreeNode* destination);
		void CodeGenFor(StoreNode* store);
		void CodeGenForBranch(BasicBlock* basicBlock);
		Operand CodeGenFor(BinaryArithmeticNode* binaryNode);
		Operand CodeGenFor(TreeNode* node);

		void MergeContext();
		void Prepare();
		void StartCodeGeneration();
	public:
		X86NativeCodeGenerator(HexJITContext* context);
		virtual BasicBlock* PassThrough() final;
	};
}