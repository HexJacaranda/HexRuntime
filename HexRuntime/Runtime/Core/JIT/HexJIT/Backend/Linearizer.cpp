#include "Linearizer.h"

#define POOL (mJITContext->Memory)
#define LAY_DOWN_TO(ORIGIN_SEQ, REPLACE , EXPR) \
	auto _statements = EXPR; \
	if (!_statements.IsEmpty()) \
	{ \
		ORIGIN_SEQ.Append(_statements); \
		auto loadNode = new (POOL) LoadNode(SLMode::Direct, localNode); \
		REPLACE = loadNode; \
	} \

RTJ::Hex::Linearizer::Linearizer(HexJITContext* context) : mJITContext(context)
{
}

RTJ::Hex::BasicBlock* RTJ::Hex::Linearizer::PassThrough()
{
	auto&& bbHead = mJITContext->BBs[0];

	for (BasicBlock* bbIterator = bbHead;
		bbIterator != nullptr;
		bbIterator = bbIterator->Next)
	{
		mStmtHead = bbIterator->Now;

		for (mCurrentStmt = bbIterator->Now;
			mCurrentStmt != nullptr && mCurrentStmt->Now != nullptr;
			mPreviousStmt = mCurrentStmt,
			mCurrentStmt = mCurrentStmt->Next)
		{
			TreeNode* _ = nullptr;
			LayDown(mCurrentStmt->Now, false, _);
		}
	}
	return bbHead;
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::LayDown(TreeNode* node, bool requestJITVariable, TreeNode*& generatedLocal)
{
	switch (node->Kind)
	{
	case NodeKinds::Load:
		return {};
	case NodeKinds::Call:
		return LayDownCall(node, requestJITVariable, generatedLocal);

	default:
		break;
	}
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::LayDownCall(TreeNode* node, bool requestJITVariable, TreeNode*& generatedLocal)
{
	CallNode* call = node->As<CallNode>();
	ObservableArray<TreeNode*> arguments = { call->Arguments, call->ArgumentCount };

	/*
	* First we need to sort out the evaluation order of arguments
	* 1. Method call
	* 2. Non-trivial expression (complex computation)
	* 3. Trivial expression (direct read on variable or array or sth)
	* 4. Constant
	*/
	DoubleLinkList<Statement> methodSequence{};
	DoubleLinkList<Statement> computationSequence{};

	for (auto&& argument : arguments)
	{
		TreeNode* localNode = nullptr;
		if (argument->Kind == NodeKinds::Call)
		{
			LAY_DOWN_TO(methodSequence, argument, LayDownCall(argument, true, localNode));
		}
		else
		{
			LAY_DOWN_TO(computationSequence, argument, LayDown(argument, true, localNode));
		}
	}

	if (requestJITVariable)
	{
		Int32 index = RequestJITVariable();
		generatedLocal = new (POOL) LocalVariableNode(index);
	}

	//Join
	methodSequence.Append(computationSequence);
	return methodSequence;
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::LayDownUnaryOperation(TreeNode* node, bool requestJITVariable, TreeNode*& generatedLocal)
{
	return DoubleLinkList<Statement>();
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::LayDownBinaryOperation(TreeNode* node, bool requestJITVariable, TreeNode*& generatedLocal)
{
	return DoubleLinkList<Statement>();
}

RT::Int32 RTJ::Hex::Linearizer::RequestJITVariable()
{
	Int32 index = mJITContext->ArgumentAttaches.size();
	mJITContext->ArgumentAttaches.push_back({ nullptr, LocalAttachedFlags::JITGenerated });
	return index;
}

#undef POOL
