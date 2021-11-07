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
		DoubleLinkList<Statement> LayDown(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable = true);
		DoubleLinkList<Statement> LayDownLoad(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable = true);
		DoubleLinkList<Statement> LayDownStore(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable = true);
		DoubleLinkList<Statement> LayDownSingle(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable = true);
		DoubleLinkList<Statement> LayDownDouble(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable = true);
		DoubleLinkList<Statement> LayDownMultiple(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable = true);
		Int32 RequestJITVariable();
	};
}