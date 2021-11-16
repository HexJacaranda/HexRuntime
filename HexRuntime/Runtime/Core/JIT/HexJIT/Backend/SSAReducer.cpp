#include "SSAReducer.h"

RTJ::Hex::TreeNode* RTJ::Hex::SSAReducer::Reduce(SSA::ValueUse* use)
{
	return use->Def->Value;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSAReducer::Reduce(SSA::ValueDef* def)
{
	return def->Value;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSAReducer::Reduce(SSA::PhiNode* phi)
{
	return phi->OriginValue;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSAReducer::Reduce(SSA::Use* use)
{
	return use->Value;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSAReducer::Reduce(TreeNode* node)
{
	switch (node->Kind)
	{
	case NodeKinds::Use:
		return Reduce(node->As<SSA::Use>());
	case NodeKinds::ValueUse:
		return Reduce(node->As<SSA::ValueUse>());
	case NodeKinds::ValueDef:
		return Reduce(node->As<SSA::ValueDef>());
	case NodeKinds::Phi:
		return Reduce(node->As<SSA::PhiNode>());
	default:
		return node;
	}
}

RTJ::Hex::SSAReducer::SSAReducer(HexJITContext* context) :mContext(context)
{
}

RTJ::Hex::BasicBlock* RTJ::Hex::SSAReducer::PassThrough()
{
	for (BasicBlock* bbIterator = mContext->BBs.front();
		bbIterator != nullptr;
		bbIterator = bbIterator->Next)
	{
		for (Statement* stmtIterator = bbIterator->Now;
			stmtIterator != nullptr && stmtIterator->Now != nullptr;
			stmtIterator = stmtIterator->Next)
		{
			TraverseTreeBottomUp(
				mContext->Traversal.Space,
				mContext->Traversal.Count,
				stmtIterator->Now,
				[&](TreeNode*& node) { node = Reduce(node); });
		}
		TraverseTreeBottomUp(
			mContext->Traversal.Space,
			mContext->Traversal.Count,
			bbIterator->BranchConditionValue,
			[&](TreeNode*& node) { node = Reduce(node); });
	}
	return mContext->BBs.front();
}