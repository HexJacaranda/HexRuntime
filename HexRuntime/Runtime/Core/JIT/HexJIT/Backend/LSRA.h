#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"
#include "ConcreteInstruction.h"
#include "NativeCodeInterpreter.h"
#include <unordered_map>
#include <bitset>
#include <optional>

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
		VAL SpilledRegister = 0xFF;

		UInt16 Index;
		UInt8 Register;
		UInt8 VirtualRegister;
	public:
		RegisterAllocationState(UInt16 index, UInt8 physicalRegister, UInt8 virtualRegister) :
			Index(index), Register(physicalRegister), VirtualRegister(virtualRegister) {}
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
		std::unordered_map<UInt16, std::vector<Liveness>> mLiveness;
		std::vector<ConcreteInstruction> mInstructions;

		std::unordered_map<UInt16, RegisterAllocationState> mStateMapping;
		//Requires multiple
		std::unordered_map<UInt8, UInt16> mVirtualRegisterToLocal;

		UInt64 mRegisterPool;
		NativeCodeInterpreter<Platform::CurrentArchitecture, Platform::CurrentWidth> mInterpreter;
	private:
		RegisterAllocationState* TryGetRegisterState(UInt16 localIndex)const;
		RegisterAllocationState* TryGetRegisterStateFromVirtualRegister(UInt16 virtualRegister)const;
		std::optional<UInt8> TryPollRegister(UInt64 mask);
		
		bool TryInheritFromContext(UInt16 variableIndex, UInt8 newVirtualRegister);
		bool TryAlloacteRegister(UInt16 variableIndex, UInt8 virtualRegister, UInt64 registerRequirement);
		void SpillVariableFor(UInt16 variableIndex, UInt8 virtualRegister);

		void ChooseCandidate();
		void ComputeLivenessDuration();
		void AllocateRegisters();
		void AllocateRegisterFor(Int32 seqBeginIndex, Int32 seqEndIndex);
		void UpdateLivenessFor(TreeNode* node);
		static LocalVariableNode* GuardedDestinationExtract(StoreNode* store);
		static LocalVariableNode* GuardedSourceExtract(LoadNode* store);
	public:
		LSRA(HexJITContext* context);
		virtual BasicBlock* PassThrough() final;
	};
}