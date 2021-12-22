#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\..\LinkList.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"

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
		DoubleLinkList<Statement> Flatten(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable = true);
		DoubleLinkList<Statement> FlattenLoad(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable = true);
		DoubleLinkList<Statement> FlattenStore(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable = true);
		DoubleLinkList<Statement> FlattenUnary(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable = true);
		DoubleLinkList<Statement> FlattenBinary(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable = true);
		DoubleLinkList<Statement> FlattenMultiple(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable = true);
		DoubleLinkList<Statement> FlattenMorphedCall(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable = true);
		Int32 RequestJITVariable(RTM::Type* type);
	};
}