#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"
#include "ConcreteInstruction.h"
#include "NativeCodeInterpreter.h"
#include <unordered_map>
#include <optional>
#include <tuple>
#include <queue>

namespace RTJ::Hex
{
	struct Liveness
	{
		Int32 From;
		Int32 To;
	};

	/// <summary>
	/// Triple tuple between (Local/Argument, PlatformRegister, VirtualRegister)
	/// </summary>
	struct RegisterAllocationState
	{
		ETY = UInt8;
		VAL OnHold = 0xFF;

		UInt16 Index;
		UInt8 Register;
		UInt8 VirtualRegister;
	public:
		RegisterAllocationState() :
			Index(0), 
			Register(OnHold), 
			VirtualRegister(OnHold) {}
		RegisterAllocationState(UInt16 index, UInt8 virtualRegister) :
			Index(index), 
			Register(OnHold), 
			VirtualRegister(virtualRegister) {}
		Int16 GetLocal()const {
			return (Int16)(Index & 0x7FFF);
		}
		Int16 GetArgument()const {
			return (Int16)(Index & 0x7FFF);
		}
	};

	/// <summary>
	/// Linear scan register allocation
	/// </summary>
	class LSRA : public IHexJITFlow
	{
		HexJITContext* mContext = nullptr;
		Int32 mLivenessIndex = 0;
		std::unordered_map<UInt16, std::queue<Liveness>> mLiveness;
		
		Int32 mInsIndex = 0;
		std::vector<ConcreteInstruction> mInstructions;
		std::unordered_map<UInt16, UInt8> mLocal2VReg;
		std::unordered_map<UInt8, UInt16> mVReg2Local;
		std::unordered_map<UInt8, UInt8> mVReg2PReg;
		std::unordered_map<UInt16, ConcreteInstruction> mLoadInsOnWatch;
		std::unordered_map<UInt16, ConcreteInstruction> mStoreInsOnWatch;
		UInt64 mRegisterPool;

		NativeCodeInterpreter<Platform::CurrentArchitecture, Platform::CurrentWidth> mInterpreter;
	private:
		std::optional<UInt8> TryPollRegister(UInt64 mask);
		void WatchOnLoad(UInt16 variableIndex, UInt8 newVirtualRegister, ConcreteInstruction instruction);
		std::optional<ConcreteInstruction> RequestLoad(UInt8 virtualRegister, UInt64 registerMask);
		void WatchOnStore(UInt16 variableIndex, UInt8 virtualRegister);
		void RequestStore(UInt16 variableIndex, UInt8 virtualRegister);
		void UsePhysicalResigster(InstructionOperand& operand);
		void InvalidateVirtualRegister(UInt8 virtualRegister);
		void InvalidateLocalVariable(UInt8 variable);
		void InvalidateContextWithLiveness();

		void AllocateRegisterFor(ConcreteInstruction instruction);
		void UpdateLivenessFor(TreeNode* node);

		void ChooseCandidate();
		void ComputeLivenessDuration();
		void AllocateRegisters();

		static LocalVariableNode* GuardedDestinationExtract(StoreNode* store);
		static LocalVariableNode* GuardedSourceExtract(LoadNode* store);
		ConcreteInstruction CopyInstruction(ConcreteInstruction const& origin, Int32 operandsCount);
	public:
		LSRA(HexJITContext* context);
		virtual BasicBlock* PassThrough() final;
	};
}