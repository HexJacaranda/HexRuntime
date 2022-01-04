#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"
#include "..\..\..\..\Bit.h"
#include "ConcreteInstruction.h"
#include "NativeCodeInterpreter.h"
#include <unordered_map>
#include <optional>
#include <tuple>
#include <queue>
#include <set>

namespace RTJ::Hex
{
	struct RegisterAllocationChain
	{
		UInt16 Variable;
		UInt8  PhysicalRegister;
	};
}

template<>
struct std::hash<RTJ::Hex::RegisterAllocationChain>
{
	size_t operator()(RTJ::Hex::RegisterAllocationChain const& key)
	{
		return std::hash<RT::UInt32>()(*(RT::UInt32*)&key);
	}
};

namespace RTJ::Hex
{
	using InterpreterT = NativeCodeInterpreter<Platform::CurrentArchitecture, Platform::CurrentWidth>;

	class AllocationContext
	{
		RTMM::SegmentHeap* mHeap = nullptr;
		std::unordered_map<UInt16, UInt8> mLocal2VReg;
		std::unordered_map<UInt8, UInt16> mVReg2Local;
		std::unordered_map<UInt8, UInt8> mVReg2PReg;
		std::unordered_map<UInt16, UInt8> mLocal2PReg;
		InterpreterT* mInterpreter;
		UInt64 mRegisterPool = 0xFFFFFFFFu;
	public:
		AllocationContext(RTMM::SegmentHeap* heap, InterpreterT* interpreter);
		AllocationContext() = default;
		AllocationContext(AllocationContext const&) = default;
		AllocationContext& operator=(AllocationContext const&) = default;

		void WatchOnLoad(UInt16 variableIndex, UInt8 newVirtualRegister, ConcreteInstruction ins);
		void WatchOnStore(UInt16 variableIndex, UInt8 virtualRegister, ConcreteInstruction ins);
		void UsePhysicalResigster(InstructionOperand& operand, UInt64& usedMask);
		std::optional<UInt8> TryPollRegister(UInt64 mask);
		void ReturnRegister(UInt8 physicalRegister);
		void InvalidateVirtualRegister(UInt8 virtualRegister);
		void InvalidateLocalVariable(UInt16 variable);
		void InvalidatePVAllocation(UInt16 variable);
		std::optional<UInt16> GetLocal(UInt8 virtualRegister);
		std::optional<UInt8> GetVirtualRegister(UInt16 variable);
		std::optional<UInt8> GetPhysicalRegister(UInt16 variable);
		std::optional<RegisterAllocationChain> GetAlloactionChainOf(UInt16 variable);
		
		std::tuple<ConcreteInstruction, ConcreteInstruction>
			RequestSpill(UInt8 oldVirtualRegister, UInt16 oldVariable, UInt8 newVirtualRegister, UInt16 newVariable);
		std::tuple<std::optional<ConcreteInstruction>, bool> RequestLoad(UInt8 virtualRegister, UInt64 registerMask);

		void LoadFromMergeContext(RegisterAllocationChain const& chain);
		void InvalidateLocalVariableExcept(VariableSet const& set);
	public:
		ETY = UInt8;
		VAL ReservedVirtualRegister = 0xFFu;
	};

	/// <summary>
	/// Linear scan register allocation
	/// </summary>
	class LSRA : public IHexJITFlow
	{
		HexJITContext* mContext = nullptr;
		AllocationContext* mRegContext;
		std::vector<VariableSet> mVariableLanded;
		/// <summary>
		/// BB sorted in topological order in reverse.
		/// </summary>
		std::vector<BasicBlock*> mSortedBB;
		InterpreterT mInterpreter;
	private:
		/// <summary>
		/// This method invalidates variables yet we'll do a more relaxed one.
		/// </summary>
		void InvalidateWithinBasicBlock(BasicBlock* bb, Int32 livenessIndex);
		/// <summary>
		/// This method invalidates variables strictly according to the liveness
		/// </summary>
		void InvalidateAcrossBaiscBlock();
		/// <summary>
		/// This will merge the AllocationContext from BBIn
		/// </summary>
		void MergeContext(BasicBlock* bb);
		std::optional<std::tuple<UInt8, UInt16>> RetriveSpillCandidate(BasicBlock* bb, UInt64 mask, Int32 livenessIndex);
		void AllocateRegisterFor(BasicBlock* bb, Int32 livenessIndex, ConcreteInstruction instruction);
		
		void BuildTopologciallySortedBB(BasicBlock* bb, BitSet& visited);
		void BuildTopologciallySortedBB();
		void ChooseCandidate();

		void BuildLivenessDuration();
		/// <summary>
		/// Build the main part of liveness
		/// </summary>
		void BuildLivenessDurationPhaseOne();
		/// <summary>
		/// Build the rest
		/// </summary>
		void BuildLivenessDurationPhaseTwo();
		
		void AllocateRegisters();

		static LocalVariableNode* GuardedDestinationExtract(StoreNode* store);
		static LocalVariableNode* GuardedSourceExtract(LoadNode* store);
		void UpdateLiveSet(TreeNode* node, BasicBlock* currentBB, VariableSet& liveSet);
	public:
		LSRA(HexJITContext* context);
		virtual BasicBlock* PassThrough() final;
	};
}