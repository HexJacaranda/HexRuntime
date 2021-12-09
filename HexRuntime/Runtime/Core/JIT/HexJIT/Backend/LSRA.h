#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"
#include "ConcreteInstruction.h"
#include "NativeCodeInterpreter.h"
#include <unordered_map>
#include <bitset>

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
		Int16 Index;
		UInt8 Register;
		UInt8 VirtualRegister;
	public:
		Int16 GetLocal()const {
			return Index;
		}
		Int16 GetArgument()const {
			return Index & 0x7FFF;
		}
	};

	/// <summary>
	/// Linear scan register allocation
	/// </summary>
	class LSRA : public IHexJITFlow
	{
		HexJITContext* mContext = nullptr;
		Int32 mLivenessIndex = 0;
		std::array<std::unordered_map<Int16, std::vector<Liveness>>, 2>  mLiveness;
		std::vector<ConcreteInstruction> mInstructions;
		std::unordered_map<Int16, RegisterAllocationState> mStateMapping;
		NativeCodeInterpreter<Platform::CurrentArchitecture, Platform::CurrentWidth> mInterpreter;
	private:
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