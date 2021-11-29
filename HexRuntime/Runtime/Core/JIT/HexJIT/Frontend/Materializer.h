#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"

namespace RTJ::Hex
{
	/// <summary>
	/// Responsible for replacing high-level nodes with primitive low-level nodes
	/// </summary>
	class Morpher : public IHexJITFlow
	{
		Statement* mStmtHead = nullptr;
		Statement* mCurrentStmt = nullptr;
		Statement* mPreviousStmt = nullptr;
		HexJITContext* mJITContext;
	public:
		Morpher(HexJITContext* context);
	private:
		void InsertCall(MorphedCallNode* node);
		TreeNode* Morph(CallNode* node);
		TreeNode* Morph(NewNode* node);
		TreeNode* Morph(NewArrayNode* node);
		TreeNode* Morph(StoreNode* node);
		TreeNode* Morph(LoadNode* node);
	public:
		BasicBlock* PassThrough();
	};
}