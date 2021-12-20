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
#include <set>

namespace RTJ::Hex
{
	class AllocationContext
	{
		RTMM::SegmentHeap* mHeap = nullptr;
		std::unordered_map<UInt16, UInt8> mLocal2VReg;
		std::unordered_map<UInt8, UInt16> mVReg2Local;
		std::unordered_map<UInt8, UInt8> mVReg2PReg;
		std::unordered_map<UInt16, ConcreteInstruction> mLoadInsOnWatch;
		std::unordered_map<UInt16, ConcreteInstruction> mStoreInsOnWatch;
		UInt64 mRegisterPool = 0xFFFFFFFFu;
	public:
		AllocationContext() = default;
		AllocationContext(AllocationContext const&) = default;
		AllocationContext& operator=(AllocationContext const&) = default;

		void WatchOnLoad(UInt16 variableIndex, UInt8 newVirtualRegister, ConcreteInstruction ins);
		void WatchOnStore(UInt16 variableIndex, UInt8 virtualRegister, ConcreteInstruction ins);
		void UsePhysicalResigster(InstructionOperand& operand);
		std::optional<UInt8> TryPollRegister(UInt64 mask);
		void ReturnRegister(UInt8 physicalRegister);
		void InvalidateVirtualRegister(UInt8 virtualRegister);
		void InvalidateLocalVariable(UInt16 variable);
		void InvalidatePVAllocation(UInt16 variable);
		void TryInvalidatePVAllocation(UInt16 variable);

		std::tuple<std::optional<ConcreteInstruction>, bool> RequestLoad(UInt8 virtualRegister, UInt64 registerMask);
		std::optional<ConcreteInstruction> RequestStore(UInt16 variableIndex, UInt8 virtualRegister);

		ConcreteInstruction CopyInstruction(ConcreteInstruction const& origin);
	};

	/// <summary>
	/// Linear scan register allocation
	/// </summary>
	class LSRA : public IHexJITFlow
	{
		HexJITContext* mContext = nullptr;
		
		Int32 mInsIndex = 0;
		std::vector<ConcreteInstruction> mInstructions; 
		AllocationContext* mRegContext;
		BasicBlock* mCurrentBB;
		NativeCodeInterpreter<Platform::CurrentArchitecture, Platform::CurrentWidth> mInterpreter;
	private:
		/// <summary>
		/// This method invalidates variables yet we'll do a more relaxed one.
		/// </summary>
		void InvalidateWithinBasicBlock();
		/// <summary>
		/// This method invalidates variables strictly according to the liveness
		/// </summary>
		void InvalidateAcrossBaiscBlock();
		/// <summary>
		/// This will merge the AllocationContext from BBIn
		/// </summary>
		void MergeContext();
		std::optional<UInt8> RetriveSpillCandidate();
		void AllocateRegisterFor(ConcreteInstruction instruction);
		
		void ChooseCandidate();
		void ComputeLivenessDuration();
		void AllocateRegisters();

		static LocalVariableNode* GuardedDestinationExtract(StoreNode* store);
		static LocalVariableNode* GuardedSourceExtract(LoadNode* store);
		void UpdateLiveSet(TreeNode* node, std::set<UInt16>& liveSet);
	public:
		LSRA(HexJITContext* context);
		virtual BasicBlock* PassThrough() final;
	};
}