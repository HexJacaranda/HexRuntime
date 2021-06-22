#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "IR.h"

namespace RTJ::Hex
{
	/// <summary>
	/// Responsible for replacing high-level nodes with primitive low-level nodes
	/// </summary>
	class Materializer
	{
		Statement* mStmtHead = nullptr;
		Statement* mCurrentStmt = nullptr;
		HexJITContext* mJITContext;
	public:
		Materializer(HexJITContext* context);
	private:
		TreeNode* MorphCall(TreeNode* node);
		TreeNode* MorphNew(TreeNode* node);
		TreeNode* MorphNewArray(TreeNode* node);
		TreeNode* MorphStore(TreeNode* node);
		TreeNode* MorphLoad(TreeNode* node);
	public:
		BasicBlock* Materialize();
	};
}