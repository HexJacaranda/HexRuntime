#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\Platform\Platform.h"
#include "..\..\..\Platform\CallingConventionProviderUse.h"
#include "..\..\..\Meta\TypeDescriptor.h"
#include "..\..\..\Exception\RuntimeException.h"
#include "..\..\..\ExecutionEngine\StackCrawling.h"
#include "..\..\OpCodes.h"
#include "..\..\..\Platform\X86Register.h"
#include "..\..\..\..\Utility.h"
#include "..\..\..\..\Binary.h"
#include "..\EmitPage.h"
#include "..\JITFlow.h"
#include "RegisterAllocationContext.h"

#define INS_1(NAME, FLAG, B1) static constexpr std::array<UInt8, 1> NAME = { B1 }; \
								static constexpr UInt16 NAME##_FLG = FLAG

#define INS_2(NAME, FLAG, B1, B2) static constexpr std::array<UInt8, 2> NAME = { B1, B2 }; \
									static constexpr UInt16 NAME##_FLG = FLAG

#define INS_3(NAME, FLAG, B1, B2, B3) static constexpr std::array<UInt8, 3> NAME = { B1, B2, B3 }; \
										static constexpr UInt16 NAME##_FLG = FLAG

#define AVX_F InstructionFlags::AVX
#define SSE_F InstructionFlags::SSE
#define OP_REG_F InstructionFlags::OpRegEnc
#define RM_F InstructionFlags::RM
#define MR_F InstructionFlags::MR
#define RI_F InstructionFlags::RI
#define MI_F InstructionFlags::MI
#define MAGIC_F(VALUE) ((InstructionFlags::MagicRegUse) | (VALUE << 6))
#define NO_F 0

#define I_F InstructionFlags::I_
#define R_F InstructionFlags::R_
#define M_F InstructionFlags::M_

namespace RTJ::Hex::X86
{
	struct InstructionFlags
	{
	public:
		ETY = UInt16;

		VAL SSE = 0x0001;
		VAL AVX = 0x0002;
		VAL OpRegEnc = 0x0004;

		VAL OperandMSK = 0b111000;
		VAL MR = 0b000000;
		VAL RM = 0b001000;
		VAL RI = 0b010000;
		VAL MI = 0b011000;

		VAL I_ = 0b100000;
		VAL R_ = 0b101000;
		VAL M_ = 0b110000;

		VAL MagicRegMSK = 0b0111000000;
		VAL MagicRegUse = 0b1000000000;
		VAL MagicRegShift = 6;
	};

	/* M = Memory / Register
	*  R = Register
	*  I = Immediate
	*  IU = Universe operand size
	*  IX = Special operand size
	*/
	struct NativePlatformInstruction
	{
		INS_1(MOV_MR_I1, MR_F, 0x88);
		INS_1(MOV_MR_IU, MR_F, 0x89);
		INS_1(MOV_RM_I1, RM_F, 0x8A);
		INS_1(MOV_RM_IU, RM_F, 0x8B);
		INS_1(MOV_RI_I1, RI_F | OP_REG_F, 0xB0);
		INS_1(MOV_RI_IU, RI_F | OP_REG_F, 0xB8);

		INS_3(MOVSD_RM, RM_F | SSE_F, 0xF2, 0x0F, 0x10);
		INS_3(MOVSD_MR, MR_F | SSE_F, 0xF2, 0x0F, 0x11);
		
		INS_3(MOVSS_RM, RM_F | SSE_F, 0xF3, 0x0F, 0x10);
		INS_3(MOVSS_MR, MR_F | SSE_F, 0xF3, 0x0F, 0x11);

		INS_1(ADD_MR_I1, MR_F,0x00);
		INS_1(ADD_MR_IU, MR_F, 0x01);
		INS_1(ADD_RM_I1, RM_F, 0x02);
		INS_1(ADD_RM_IU, RM_F, 0x03);

		INS_1(ADD_MI_I1, MI_F | MAGIC_F(0), 0x80);
		INS_1(ADD_MI_IU, MI_F | MAGIC_F(0), 0x81);

		INS_1(SUB_MI_I1, MI_F | MAGIC_F(5), 0x80);
		INS_1(SUB_MI_IU, MI_F | MAGIC_F(5), 0x81);

		INS_3(ADDSD_RM, RM_F | SSE_F, 0xF2, 0x0F, 0x58);
		INS_3(ADDSS_RM, RM_F | SSE_F, 0xF3, 0x0F, 0x58);

		INS_1(RET, NO_F, 0xC3);
		INS_1(JMP_I1, I_F, 0xEB);
		INS_1(JMP_I4, I_F, 0xE9);
		INS_1(JMP_M_I8, M_F, 0xFF);

		INS_1(CMP_MI_I1, MI_F | MAGIC_F(7), 0x80);
		INS_1(CMP_MI_IU, MI_F | MAGIC_F(7), 0x81);
		INS_1(CMP_MR_I1, MR_F, 0x38);
		INS_1(CMP_MR_IU, MR_F, 0x39);
		INS_1(CMP_RM_I1, RM_F, 0x3A);
		INS_1(CMP_RM_IU, RM_F, 0x3B);

		INS_3(COMISD_RM, RM_F, 0x66, 0x0F, 0x2F);
		INS_2(COMISS_RM, RM_F, 0x0F, 0x2F);

		INS_1(JCC_EQ_I1, I_F, 0x74);
		INS_1(JCC_NE_I1, I_F, 0x75);
		INS_1(JCC_GT_I1, I_F, 0x7F);
		INS_1(JCC_LT_I1, I_F, 0x7C);
		INS_1(JCC_GE_I1, I_F, 0x7D);
		INS_1(JCC_LE_I1, I_F, 0x7E);

		INS_1(PUSH_R_IU, R_F | OP_REG_F, 0x50);
		INS_1(POP_R_IU, R_F | OP_REG_F, 0x58);
	};

	struct Instruction
	{
		const UInt8* Opcode;
		UInt8 Length;
		UInt8 CoreType;
		UInt16 Flags;
		UInt8 GetMagicRegisterValue()const {
			return (InstructionFlags::MagicRegMSK & Flags) >> InstructionFlags::MagicRegShift;
		}
	};

	enum class OperandPreference : UInt8
	{
		Register,
		Immediate,
		SIB,
		Displacement,
		Empty
	};

	class AddressingMode
	{
	public:
		ETY = UInt8;
		VAL RegisterAddressing = 0b00000000;
		VAL RegisterAddressingDisp8 = 0b01000000;
		VAL RegisterAddressingDisp32 = 0b10000000;
		VAL Register = 0b11000000;
	};

	class RegisterOrMemory
	{
	public:
		ETY = UInt8;
		VAL SIB = 0b00000100;
		VAL Disp32 = 0b00000101;
	};

	class REXBit
	{
	public:
		ETY = UInt8;
		VAL B = 0;
		VAL X = 1;
		VAL R = 2;
		VAL W = 3;
	};

	static bool IsAdvancedAddressing(OperandPreference preference) {
		return preference == OperandPreference::Displacement || preference == OperandPreference::SIB;
	}

	enum class SemanticGroup : UInt8
	{
		MOV,
		ADD,
		SUB,
		MUL,
		DIV,
		AND,
		OR,
		XOR,
		CMP
	};

	struct Operand
	{
		OperandPreference Kind;
		union 
		{
			UInt8 ImmediateCoreType;
			//This is used to ref the variable when Kind is displacement
			std::optional<UInt16> RefVariable;
		};
		bool UseRIPAddressing = false;
		union
		{
			UInt8 Register;
			ConstantStorage Immediate;
			struct
			{
				union
				{
					std::optional<UInt8> Base;
					struct
					{
						std::optional<UInt8> Base;
						std::optional<UInt8> Index;
						UInt8 Scale;							
					} SIB;
				};
				std::optional<Int32> Displacement;
			} M;
		};

		bool IsAdvancedAddressing()const {
			return X86::IsAdvancedAddressing(Kind);
		}
		bool IsRegister()const {
			return Kind == OperandPreference::Register;
		}
		bool IsImmediate()const {
			return Kind == OperandPreference::Immediate;
		}
		static Operand FromRegister(UInt8 value)
		{
			Operand ret{ OperandPreference::Register };
			ret.Register = value;
			return ret;
		}
		template<class ImmediateT>
		static Operand FromImmediate(ImmediateT value, UInt8 coreType)
		{
			Operand ret{ OperandPreference::Immediate };
			ret.Immediate = ConstantStorage::From(value);
			ret.ImmediateCoreType = coreType;
			return ret;
		}
		static Operand FromDisplacement(UInt8 base, Int32 offset)
		{
			Operand ret{ OperandPreference::Displacement };
			ret.M.Base = base;
			ret.M.Displacement = offset;
			return ret;
		}
		static Operand Empty()
		{
			return { OperandPreference::Empty };
		}
	};

	struct Tracking
	{
		std::optional<Int32> Offset;
		Int8 Size;

		ETY = Int8;
		VAL RequestTracking = -1;
	};

	struct JumpFixUp
	{
		Int32 Offset;
		UInt8 Size;
		Int32 BasicBlock;
	};

	struct ImmediateFixUp
	{
		Int32 Offset;
		Int32 RIPBase;
		Int32 BasicBlock;
		ConstantStorage Storage;
	};

	struct ImmediateSegment
	{
		Int32 BaseOffset;
		std::vector<ImmediateFixUp> Slots;
	};

	/// <summary>
	/// Code generation must be context-aware for optimization
	/// </summary>
	struct GenerationContext
	{
		std::optional<UInt16> StoringVariable;
		std::optional<UInt8> ResultRegister;
		Tracking ImmediateTrack;
		Tracking DisplacementTrack;
		UInt64 RegisterMask = std::numeric_limits<UInt64>::max();
	};

	/// <summary>
	/// For preventing spilling unusable registers in context
	/// </summary>
	class RegisterConflictSession
	{
		UInt64 mRestoreBits = 0ull;
		GenerationContext* mGenContextRef = nullptr;
	public:
		RegisterConflictSession(GenerationContext* context);
		void Borrow(UInt8 nativeReg);
		void Return(UInt8 nativeReg);
		~RegisterConflictSession();
	};

	template<class T>
	static Int32 WritePage(EmitPage* emitPage, T value) {
		UInt8* address = emitPage->Prepare(sizeof(T));
		Int32 ret = emitPage->CurrentOffset();
		*(T*)address = value;
		emitPage->Commit(sizeof(T));
		return ret;
	}

	template<class T>
	static Int32 WritePageImmediate(EmitPage* emitPage, T value) {
		UInt8* address = emitPage->Prepare(sizeof(T));
		Int32 ret = emitPage->CurrentOffset();
		Binary::WriteByLittleEndianness(address, value);
		emitPage->Commit(sizeof(T));
		return ret;
	}

	static Int32 WritePage(EmitPage* emitPage, const UInt8* value, Int32 length) {
		UInt8* address = emitPage->Prepare(length);
		Int32 ret = emitPage->CurrentOffset();
		std::memcpy(address, value, length);
		emitPage->Commit(length);
		return ret;
	}

	static void RewritePage(EmitPage* emitPage, Int32 offset, UInt8 value) {
		UInt8* address = emitPage->GetRaw();
		address[offset] = value;
	}

	template<class T>
	static void RewritePageImmediate(EmitPage* emitPage, Int32 offset, T value) {
		UInt8* address = emitPage->GetRaw() + offset;
		Binary::WriteByLittleEndianness(address, value);
	}

	static RTP::PlatformCallingConvention* GenerateCallingConvFor(RTMM::PrivateHeap* heap, RTM::MethodSignatureDescriptor* signature);

	class X86NativeCodeGenerator : public IHexJITFlow
	{
		HexJITContext* mContext = nullptr;
		std::vector<VariableSet> mVariableLanded;
		std::vector<BasicBlock*> mSortedBB;
		BasicBlock* mCurrentBB = nullptr;
		RegisterAllocationContext* mRegContext = nullptr;
		Int32 mLiveness = 0;
		EmitPage* mEmitPage = nullptr;
		GenerationContext mGenContext;

		EmitPage* mProloguePage = nullptr;
		EmitPage* mEpiloguePage = nullptr;
		//[Variable] -> <BB, Offsets>
		std::unordered_map<UInt16, std::unordered_map<Int32, std::vector<Int32>>> mVariableDispFix;
		//[BB] -> Local Page Offset
		std::unordered_map<Int32, JumpFixUp> mJmpOffsetFix;
		//For immediate memory fix up (currently only for float and double)
		std::unordered_map<UInt8, ImmediateSegment> mImmediateFix;

		//For debugging
		static constexpr Int32 DebugOffset32 = 0x7F7F7F7F;
		static constexpr Int16 DebugOffset16 = 0x7F7F;
		static constexpr Int8 DebugOffset8 = 0x7F;
		static constexpr Int32 EpilogueBBIndex = -1;
		static constexpr Int32 CodeAlignment = 8;
	private:
		void Emit(Instruction instruction, Operand const& left, Operand const& right);
		void Emit(Instruction instruction, Operand const& operand);
		void Emit(Instruction instruction);

		Instruction RetriveBinaryInstruction(SemanticGroup semGroup, UInt8 coreType, OperandPreference destPreference, OperandPreference sourcePreference);
		void EmitLoadM2R(UInt8 nativeRegister, UInt16 variable, UInt8 coreType);
		void EmitLoadR2R(UInt8 toRegister, UInt8 fromRegister, UInt8 coreType);
		void EmitStoreR2M(UInt8 nativeRegister, UInt16 variable, UInt8 coreType);

		bool IsValueUnusedAfter(Int32 livenessIndex, UInt16 variableIndex) const;
		UInt8 AllocateRegisterAndGenerateCodeFor(UInt16 variable, UInt64 mask, UInt8 coreType, bool requireLoading = true);

		/// <summary>
		/// This should only be called by UseImmediate otherwise you may leak the register
		/// </summary>
		/// <param name="mask"></param>
		/// <param name="coreType"></param>
		/// <returns></returns>
		UInt8 AllocateRegisterAndGenerateCodeFor(UInt64 mask, UInt8 coreType);
		Operand UseImmediate(
			ConstantNode* constant,
			bool forceRegister = false,
			UInt64 additionalMask = std::numeric_limits<UInt64>::max());
		void SpillAndGenerateCodeFor(UInt16 variable, UInt8 coreType);
		std::tuple<UInt16, UInt8> RetriveSpillCandidate(UInt64 mask, Int32 livenessIndex);
		UInt16 RetriveLongLived(UInt16 left, UInt16 right);

		RTP::PlatformCallingConvention* GetCallingConv()const;

		void MarkVariableOnFly(UInt16 variable, BasicBlock* bb = nullptr);
		void MarkVariableLanded(UInt16 variable, BasicBlock* bb = nullptr);
		void LandVariableFor(UInt16 variable, BasicBlock* bb = nullptr);

		void CodeGenFor(MorphedCallNode* call, TreeNode* destination);
		void CodeGenFor(StoreNode* store);
		void CodeGenForBranch(BasicBlock* basicBlock, Int32 estimatedOffset);
		void CodeGenForReturn(BasicBlock* basicBlock, Int32 estimatedOffset);
		void CodeGenForJmp(BasicBlock* basicBlock, Int32 estimatedOffset);
		void CodeGenForJcc(BasicBlock* basicBlock, Int32 estimatedOffset);

		void CodeGenForBinary(BinaryNodeAccessProxy* binaryNode);
		void CodeGenFor(UnaryArithmeticNode* unaryNode);
		void PreCodeGenForJcc(TreeNode* conditionValue);
		void PreCodeGenForRet(TreeNode* returnValue);
		void CodeGenFor(TreeNode* node);

		void MergeContext();
		void Prepare();
		void Finalize();
		void FixUpDisplacement();
		void FixUpImmediateDisplacement(EmitPage* finalPage);

		void GenerateBranch();
		void AssemblyCode();
		Int32 ComputeAlignedImmediateLayout();
		std::vector<UInt8> GetUsedNonVolatileRegisters()const;
		/// <summary>
		/// Prologue code for preserve register and allocating stack space
		/// </summary>
		void GeneratePrologue(RTEE::StackFrameInfo* info, std::vector<UInt8> const& nonVolatileRegisters);
		/// <summary>
		/// Epilogue code for restore register and deallocating stack space
		/// </summary>
		void GenerateEpilogue(RTEE::StackFrameInfo* info, std::vector<UInt8> const& nonVolatileRegisters);
		/// <summary>
		/// Set up the RegisterAllocationContext from calling convention
		/// </summary>
		void InitializeContextFromCallingConv();		
		void StartCodeGeneration();
	public:
		X86NativeCodeGenerator(HexJITContext* context);
		virtual BasicBlock* PassThrough() final;
	};
}