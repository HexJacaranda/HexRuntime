#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\..\LinkList.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"

namespace RTJ::Hex
{
	/// <summary>
	/// Linearize the IR to sequential order of trees that have
	/// at most depth of two
	/// </summary>
	class Linearizer : public IHexJITFlow
	{
		HexJITContext* mJITContext;
		Statement* mCurrentStmt = nullptr;
		Statement* mPreviousStmt = nullptr;
		Statement* mStmtHead = nullptr;
	public:
		Linearizer(HexJITContext* context);
		virtual BasicBlock* PassThrough()final;
	private:
		DoubleLinkList<Statement> LayDown(TreeNode* node, bool requestJITVariable, TreeNode*& generatedLocal);
		DoubleLinkList<Statement> LayDownCall(TreeNode* node, bool requestJITVariable, TreeNode*& generatedLocal);
		DoubleLinkList<Statement> LayDownUnaryOperation(TreeNode* node, bool requestJITVariable, TreeNode*& generatedLocal);
		DoubleLinkList<Statement> LayDownBinaryOperation(TreeNode* node, bool requestJITVariable, TreeNode*& generatedLocal);
		Int32 RequestJITVariable();
	};
}