#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"

namespace RTJ::Hex
{
	struct MorphStatus
	{
		ETY = UInt8;
		VAL AddressTaken = 0x01;
		VAL MainLoadChain = 0x02;
	};
	/// <summary>
	/// Responsible for replacing high-level nodes with primitive low-level nodes
	/// </summary>
	class Morpher : public IHexJITFlow
	{
		Statement* mStmtHead = nullptr;
		Statement* mCurrentStmt = nullptr;
		Statement* mPreviousStmt = nullptr;
		HexJITContext* mJITContext;
		UInt8 mStatus = 0;
	public:
		Morpher(HexJITContext* context);
	private:
		void Set(UInt8 flag);
		void Unset(UInt8 flag);
		bool Has(UInt8 flag);

		template<class Fn>
		void With(UInt8 flag, Fn&& action) {
			bool hasFlag = Has(flag);
			if (!hasFlag)
				Set(flag);
			std::forward<Fn>(action)();
			if (!hasFlag)
				Unset(flag);
		}

		template<class Fn>
		void Use(UInt8 flag, Fn&& action) {
			std::forward<Fn>(action)();
			Unset(flag);
		}

		LoadNode* ChangeToAddressTaken(LoadNode* origin);

		void InsertCall(MorphedCallNode* node);
		TreeNode* Morph(TreeNode* node);
		TreeNode* Morph(CallNode* node);
		TreeNode* Morph(NewNode* node);
		TreeNode* Morph(NewArrayNode* node);
		TreeNode* Morph(StoreNode* node);
		TreeNode* Morph(LoadNode* node);
		TreeNode* Morph(ArrayElementNode* node);
		TreeNode* Morph(InstanceFieldNode* node);
	public:
		BasicBlock* PassThrough();
	};
}