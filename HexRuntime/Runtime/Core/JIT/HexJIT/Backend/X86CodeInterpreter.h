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
#define NO_PRD_F InstructionFlags::NO_OPRD
#define MAGIC_F(VALUE) ((InstructionFlags::MagicRegUse) | (VALUE << InstructionFlags::MagicRegShift))
#define NO_F 0
#define NO_REXW_F InstructionFlags::REXWNotRequiredFor64

#define I_F InstructionFlags::I_
#define R_F InstructionFlags::R_
#define M_F InstructionFlags::M_
#define RMI_F InstructionFlags::RMI

namespace RTJ::Hex::X86
{
	struct InstructionFlags
	{
	public:
		ETY = UInt16;

		VAL SSE = 0b001;
		VAL AVX = 0b010;
		VAL OpRegEnc = 0b100;

		VAL OperandMSK = 0b111000;
		VAL MR = 0b0000000;
		VAL RM = 0b0001000;
		VAL RI = 0b0010000;
		VAL MI = 0b0011000;

		VAL I_ = 0b0100000;
		VAL R_ = 0b0101000;
		VAL M_ = 0b0110000;
		VAL NO_OPRD = 0b0111000;
		VAL RMI = 0b1000000;

		VAL MagicRegMSK = 0b01110000000;
		VAL MagicRegUse = 0b10000000000;
		VAL MagicRegShift = 7;

		VAL REXWNotRequiredFor64 = 0b1000000000000000;
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
		INS_1(MOV_MI_I1, MI_F | MAGIC_F(0), 0xC6);
		INS_1(MOV_MI_IU, MI_F | MAGIC_F(0), 0xC7);

		INS_3(MOVSD_RM, RM_F | SSE_F, 0xF2, 0x0F, 0x10);
		INS_3(MOVSD_MR, MR_F | SSE_F, 0xF2, 0x0F, 0x11);
		
		INS_3(MOVSS_RM, RM_F | SSE_F, 0xF3, 0x0F, 0x10);
		INS_3(MOVSS_MR, MR_F | SSE_F, 0xF3, 0x0F, 0x11);

		INS_1(ADD_MR_I1, MR_F, 0x00);
		INS_1(ADD_MR_IU, MR_F, 0x01);
		INS_1(ADD_RM_I1, RM_F, 0x02);
		INS_1(ADD_RM_IU, RM_F, 0x03);

		INS_1(ADD_MI_I1, MI_F | MAGIC_F(0), 0x80);
		INS_1(ADD_MI_IU, MI_F | MAGIC_F(0), 0x81);

		INS_3(ADDSD_RM, RM_F | SSE_F, 0xF2, 0x0F, 0x58);
		INS_3(ADDSS_RM, RM_F | SSE_F, 0xF3, 0x0F, 0x58);

		INS_1(SUB_MR_I1, MR_F, 0x28);
		INS_1(SUB_MR_IU, MR_F, 0x29);
		INS_1(SUB_RM_I1, RM_F, 0x2A);
		INS_1(SUB_RM_IU, RM_F, 0x2B);

		INS_1(SUB_MI_I1, MI_F | MAGIC_F(5), 0x80);
		INS_1(SUB_MI_IU, MI_F | MAGIC_F(5), 0x81);

		INS_3(SUBSD_RM, RM_F | SSE_F, 0xF2, 0x0F, 0x5C);
		INS_3(SUBSS_RM, RM_F | SSE_F, 0xF3, 0x0F, 0x5C);

		INS_1(IMUL_RM_I1, M_F | MAGIC_F(4), 0xF6);
		INS_2(IMUL_RM_IU, RM_F, 0x0F, 0xAF);

		INS_3(MULSD_RM, RM_F | SSE_F, 0xF2, 0x0F, 0x59);
		INS_3(MULSS_RM, RM_F | SSE_F, 0xF3, 0x0F, 0x59);

		INS_1(DIV_RM_I1, M_F | MAGIC_F(6), 0xF6);
		INS_1(DIV_RM_IU, M_F | MAGIC_F(6), 0xF7);

		INS_1(CDQ, NO_PRD_F, 0x99);
		INS_1(IDIV_RM_I1, M_F | MAGIC_F(7), 0xF6);
		INS_1(IDIV_RM_IU, M_F | MAGIC_F(7), 0xF7);

		INS_3(DIVSD_RM, RM_F | SSE_F, 0xF2, 0x0F, 0x5E);
		INS_3(DIVSS_RM, RM_F | SSE_F, 0xF3, 0x0F, 0x5E);

		INS_1(RET, NO_PRD_F | NO_REXW_F, 0xC3);
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

		INS_3(UCOMISD_RM, RM_F, 0x66, 0x0F, 0x2E);
		INS_2(UCOMISS_RM, RM_F, 0x0F, 0x2E);

		INS_2(SET_EQ_I1, M_F, 0x0F, 0x94);
		INS_2(SET_NE_I1, M_F, 0x0F, 0x95);
		INS_2(SET_GT_I1, M_F, 0x0F, 0x9F);
		INS_2(SET_LT_I1, M_F, 0x0F, 0x9C);
		INS_2(SET_GE_I1, M_F, 0x0F, 0x9D);
		INS_2(SET_LE_I1, M_F, 0x0F, 0x9E);

		INS_1(TEST_MR_I1, MR_F, 0x84);
		INS_1(TEST_MI_I1, MI_F | MAGIC_F(0), 0xF6);

		INS_1(JCC_EQ_I1, I_F, 0x74);
		INS_1(JCC_NE_I1, I_F, 0x75);
		INS_1(JCC_GT_I1, I_F, 0x7F);
		INS_1(JCC_LT_I1, I_F, 0x7C);
		INS_1(JCC_GE_I1, I_F, 0x7D);
		INS_1(JCC_LE_I1, I_F, 0x7E);

		INS_2(JCC_EQ_I4, I_F, 0x0F, 0x84);
		INS_2(JCC_NE_I4, I_F, 0x0F, 0x85);
		INS_2(JCC_GT_I4, I_F, 0x0F, 0x8F);
		INS_2(JCC_LT_I4, I_F, 0x0F, 0x8C);
		INS_2(JCC_GE_I4, I_F, 0x0F, 0x8D);
		INS_2(JCC_LE_I4, I_F, 0x0F, 0x8E);

		INS_1(NOT_I1, M_F | MAGIC_F(2), 0xF6);
		INS_1(NOT_IU, M_F | MAGIC_F(2), 0xF7);

		INS_1(NEG_I1, M_F | MAGIC_F(3), 0xF6);
		INS_1(NEG_IU, M_F | MAGIC_F(3), 0xF7);

		INS_2(XORPS_RM, RM_F, 0x0F, 0x57);
		INS_3(XORPD_RM, RM_F, 0x66, 0x0F, 0x57);

		INS_1(AND_MR_I1, MR_F, 0x20);
		INS_1(AND_MR_IU, MR_F, 0x21);
		INS_1(AND_RM_I1, RM_F, 0x22);
		INS_1(AND_RM_IU, RM_F, 0x23);

		INS_1(AND_MI_I1, MI_F | MAGIC_F(4), 0x80);
		INS_1(AND_MI_IU, MI_F | MAGIC_F(4), 0x81);

		INS_1(OR_MR_I1, MR_F, 0x08);
		INS_1(OR_MR_IU, MR_F, 0x09);
		INS_1(OR_RM_I1, RM_F, 0x0A);
		INS_1(OR_RM_IU, RM_F, 0x0B);

		INS_1(OR_MI_I1, MI_F | MAGIC_F(1), 0x80);
		INS_1(OR_MI_IU, MI_F | MAGIC_F(1), 0x81);

		INS_1(XOR_MR_I1, MR_F, 0x30);
		INS_1(XOR_MR_IU, MR_F, 0x31);
		INS_1(XOR_RM_I1, RM_F, 0x32);
		INS_1(XOR_RM_IU, RM_F, 0x33);

		INS_1(XOR_MI_I1, MI_F | MAGIC_F(6), 0x80);
		INS_1(XOR_MI_IU, MI_F | MAGIC_F(6), 0x81);

		INS_1(PUSH_R_IU, R_F | OP_REG_F | NO_REXW_F, 0x50);
		INS_1(POP_R_IU, R_F | OP_REG_F | NO_REXW_F, 0x58);
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
		MOD,
		AND,
		OR,
		XOR,
		NOT,
		NEG,
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

	struct RIPFixUp
	{
		Int32 Offset;
		Int32 RIPBase;
		Int32 BasicBlock;
	};

	struct ConstantWithType
	{
		UInt8 CoreType;
		ConstantStorage Storage;
	};

	struct ConstantEqualer
	{
		bool operator()(ConstantWithType const& left, ConstantWithType const& right) const {
			if (left.CoreType != right.CoreType) return false;
			Int32 size = CoreTypes::GetCoreTypeSize(left.CoreType);
			switch (size)
			{
				case sizeof(UInt8): return left.Storage.U1 == right.Storage.U1;
				case sizeof(UInt16): return left.Storage.U2 == right.Storage.U2;
				case sizeof(UInt32): return left.Storage.U4 == right.Storage.U4;
				case sizeof(UInt64): return left.Storage.U8 == right.Storage.U8;
				default:
					return !std::memcmp(left.Storage.SIMD, right.Storage.SIMD, size);
			}
		}
	};

	struct ConstantHasher
	{
		std::size_t operator()(ConstantWithType const& value) const {
			if (CoreTypes::IsSIMD(value.CoreType))
				return ComputeHashCode(value.Storage.SIMD, CoreTypes::GetCoreTypeSize(value.CoreType));

			return ComputeHashCode(value.Storage);
		}
	};

	struct OperandOptions
	{
		ETY = UInt8;
		VAL None = 0b00000000;
		VAL ForceMask = 0b00000011;
		/// <summary>
		/// Force to use register
		/// </summary>
		VAL ForceRegister = 0b00000001;
		/// <summary>
		/// Force to use memory
		/// </summary>
		VAL ForceMemory = 0b00000010;
		/// <summary>
		/// Automantically determine which to use
		/// </summary>
		VAL AutoRMI = 0b00000000;
		/// <summary>
		/// Only allocate register yet do not read variable from memory
		/// </summary>
		VAL AllocateOnly = 0b00000100;
		/// <summary>
		/// AddressOf semantic
		/// </summary>
		VAL AddressOf = 0b00001000;
		/// <summary>
		/// Invalidate variable if possible
		/// </summary>
		VAL InvalidateVariable = 0b00010000;
	};

	struct ImmediateSegment
	{
		Int32 BaseOffset;
		//Constant -> Uses
		std::unordered_map<ConstantWithType, std::vector<RIPFixUp>, ConstantHasher, ConstantEqualer> Slots;
	};

	/// <summary>
	/// Code generation must be context-aware for optimization
	/// </summary>
	struct GenerationContext
	{
		std::optional<UInt16> StoringVariable;
		std::optional<Operand> StoringDestination;
		std::optional<UInt8> ResultRegister;
		Tracking ImmediateTrack;
		Tracking DisplacementTrack;
		UInt64 RegisterMask = std::numeric_limits<UInt64>::max();	
		std::optional<UInt8> JccCondition;
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

	static void RewritePageImmediate(EmitPage* emitPage, Int32 offset, UInt8 coreType, ConstantStorage const& constant)
	{
		UInt8* address = emitPage->GetRaw() + offset;
		switch (coreType)
		{
		case CoreTypes::I1:
		case CoreTypes::U1:
			Binary::WriteByLittleEndianness(address, constant.U1); break;
		case CoreTypes::I2:
		case CoreTypes::U2:
			Binary::WriteByLittleEndianness(address, constant.U2); break;
		case CoreTypes::I4:
		case CoreTypes::U4:
			Binary::WriteByLittleEndianness(address, constant.U4); break;
		case CoreTypes::I8:
		case CoreTypes::U8:
			Binary::WriteByLittleEndianness(address, constant.U8); break;
		case CoreTypes::R2:
			Binary::WriteByLittleEndianness(address, constant.U2); break;
		case CoreTypes::R4:
			Binary::WriteByLittleEndianness(address, constant.U4); break;
		case CoreTypes::R8:
			Binary::WriteByLittleEndianness(address, constant.U8); break;
		default:
		{
			//TODO: should adapt specific type write by little endianness?
			std::memcpy(address, constant.SIMD, CoreTypes::GetCoreTypeSize(coreType));
			break;
		}
		}	
	}

	static void RewritePageImmediate(EmitPage* emitPage, Int32 offset, UInt8* value, Int32 length) {
		UInt8* address = emitPage->GetRaw() + offset;
		std::memcpy(address, value, length);
	}

	static RTP::PlatformCallingConvention* GenerateCallingConvFor(RTMM::PrivateHeap* heap, RTM::MethodSignatureDescriptor* signature);

	class X86NativeCodeGenerator : public IHexJITFlow
	{
		HexJITContext* mContext = nullptr;
		std::vector<VariableSet> mVariableLanded;
		std::vector<BasicBlock*> mSortedBB;
		BasicBlock* mCurrentBB = nullptr;
		Statement* mCurrentStmt = nullptr;
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
		std::unordered_map<Int32, std::optional<UInt8>> mJccConditions;

		//For debugging
		static constexpr Int32 DebugOffset32 = 0x7F7F7F7F;
		static constexpr Int16 DebugOffset16 = 0x7F7F;
		static constexpr Int8 DebugOffset8 = 0x7F;

		static constexpr Int32 EpilogueBBIndex = -1;
		static constexpr Int32 CodeAlignment = 8;
		static constexpr std::array<UInt8, 16> NegR4SSEConstant = { 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80 };
		static constexpr std::array<UInt8, 16> NegR8SSEConstant = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00 ,0x00, 0x00, 0x00, 0x00, 0x00, 0x80 };
	private:
		void Emit(Instruction instruction, Operand const& left, Operand const& right);
		void Emit(Instruction instruction, Operand const& operand);
		void Emit(Instruction instruction);

		Instruction RetriveBinaryInstruction(SemanticGroup semGroup, UInt8 coreType, OperandPreference destPreference, OperandPreference sourcePreference);
		Instruction RetriveUnaryInstruction(SemanticGroup semGroup, UInt8 coreType, OperandPreference preference);
		void EmitLoadM2R(UInt8 nativeRegister, UInt16 variable, UInt8 coreType);
		void EmitLoadR2R(UInt8 toRegister, UInt8 fromRegister, UInt8 coreType);
		void EmitStoreR2M(UInt8 nativeRegister, UInt16 variable, UInt8 coreType);

		bool IsVariableUnusedFromNow(UInt16 variable) const;
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

		Operand UseOperandFrom(
			RegisterConflictSession& session,
			TreeNode* target,
			UInt8 options = OperandOptions::None,
			std::optional<UInt64> mask = {});

		void SpillAndGenerateCodeFor(UInt16 variable, UInt8 coreType);
		std::tuple<UInt16, UInt8> RetriveSpillCandidate(UInt64 mask, Int32 livenessIndex);
		UInt16 RetriveLongLived(UInt16 left, UInt16 right);

		RTP::PlatformCallingConvention* GetCallingConv()const;

		void MarkVariableOnFly(UInt16 variable, BasicBlock* bb = nullptr);
		void MarkVariableLanded(UInt16 variable, BasicBlock* bb = nullptr);
		void LandVariableFor(UInt16 variable, BasicBlock* bb = nullptr);
		/// <summary>
		/// Special care for arguments on register
		/// </summary>
		/// <param name="variable"></param>
		void InvalidateVaraible(UInt16 variable);

		void CodeGenFor(MorphedCallNode* call, TreeNode* destination);
		void CodeGenFor(StoreNode* store);
		void CodeGenForBranch(BasicBlock* basicBlock, Int32 estimatedOffset);
		void CodeGenForReturn(BasicBlock* basicBlock, Int32 estimatedOffset);
		void CodeGenForJmp(BasicBlock* basicBlock, Int32 estimatedOffset);
		void CodeGenForJcc(BasicBlock* basicBlock, Int32 estimatedOffset);
		void CodeGenForBooleanStore(TreeNode* expression);
		bool ShouldStoreBoolean(UInt16 variable);
		bool IsUsedByAdjacentJcc(UInt16 variable);

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