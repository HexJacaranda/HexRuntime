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
		HexJITContext* mJITContext;
	public:
		Materializer(HexJITContext* context);
	private:
		TreeNode* MorphCall(TreeNode* node, Statement*& first, Statement* currentStmt);
		TreeNode* MorphNew(TreeNode* node, Statement*& first, Statement* currentStmt);
		TreeNode* MorphNewArray(TreeNode* node, Statement*& first, Statement* currentStmt);
		TreeNode* MorphStore(TreeNode* node, Statement*& first, Statement* currentStmt);
		TreeNode* MorphLoad(TreeNode* node, Statement*& first, Statement* currentStmt);
	public:
		BasicBlock* Materialize();
	};
}