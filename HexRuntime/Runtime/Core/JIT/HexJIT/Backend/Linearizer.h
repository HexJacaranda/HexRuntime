#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\..\LinkList.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"

namespace RTJ::Hex
{
	using StmtList = DoubleLinkList<Statement>;
	using FlattenResult = std::tuple<StmtList, TreeNode*>;

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
		UInt8 mStatus = 0;
	public:
		Linearizer(HexJITContext* context);
		virtual BasicBlock* PassThrough()final;
	private:
		FlattenResult Flatten(TreeNode* parent, TreeNode* node, bool shouldKeep = false);
		FlattenResult Flatten(TreeNode* parent, LoadNode* node, bool shouldKeep = false);
		FlattenResult Flatten(TreeNode* parent, StoreNode* node, bool shouldKeep = false);
		FlattenResult Flatten(TreeNode* parent, MorphedCallNode* node, bool shouldKeep = false);
		FlattenResult Flatten(TreeNode* parent, MultipleNodeAccessProxy* node, bool shouldKeep = false);
		FlattenResult Flatten(TreeNode* parent, OffsetOfNode* node, bool shouldKeep = false);
		FlattenResult Flatten(TreeNode* parent, LocalVariableNode* node, bool shouldKeep = false);
		FlattenResult Flatten(TreeNode* parent, ArrayOffsetOfNode* node, bool shouldKeep = false);
		Int32 GetCurrentOffset()const;
		Int32 RequestJITVariable(RTM::Type* type);
	};
}