#include "Linearizer.h"
#include "..\..\..\Exception\RuntimeException.h"

#define POOL (mJITContext->Memory)
#define LAY_DOWN_TO(ORIGIN_SEQ, REPLACE , EXPR) \
		{ \
			auto _statements = EXPR; \
			if (!_statements.IsEmpty()) \
			{ \
				ORIGIN_SEQ.Append(_statements); \
				auto loadNode = new (POOL) LoadNode(SLMode::Direct, localNode); \
				REPLACE = loadNode; \
			} \
		}

RTJ::Hex::Linearizer::Linearizer(HexJITContext* context) : mJITContext(context)
{
}

RTJ::Hex::BasicBlock* RTJ::Hex::Linearizer::PassThrough()
{
	auto&& bbHead = mJITContext->BBs[0];
	auto layDown = [&](TreeNode*& root) {
		TreeNode* _ = nullptr;
		auto stmts = Flatten(root, _, false);
		if (!stmts.IsEmpty())
			LinkedList::AppendRangeTwoWay(mStmtHead, mPreviousStmt, stmts.GetHead(), stmts.GetTail());
	};

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
			layDown(mCurrentStmt->Now);
		}
		if (bbIterator->BranchConditionValue != nullptr)
			layDown(bbIterator->BranchConditionValue);
	}
	return bbHead;
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::Flatten(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable)
{
	switch (node->Kind)
	{
	case NodeKinds::Store:
	case NodeKinds::Array:
	case NodeKinds::Compare:
	case NodeKinds::BinaryArithmetic:
		return FlattenBinary(node, generatedLocal, requestJITVariable);
	case NodeKinds::Convert:
	case NodeKinds::Cast:
	case NodeKinds::Box:
	case NodeKinds::UnBox:
	case NodeKinds::InstanceField:
	case NodeKinds::UnaryArithmetic:
	case NodeKinds::Duplicate:
		return FlattenUnary(node, generatedLocal, requestJITVariable);
	case NodeKinds::Load:
		return FlattenLoad(node, generatedLocal, requestJITVariable);
	case NodeKinds::Call:
	case NodeKinds::New:
	case NodeKinds::NewArray:
		return FlattenMultiple(node, generatedLocal, requestJITVariable);
	case NodeKinds::MorphedCall:
		return FlattenMorphedCall(node, generatedLocal, requestJITVariable);
	default:
		break;
	}
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::FlattenLoad(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable)
{
	auto load = node->As<LoadNode>();
	auto source = load->Source;

	switch (source->Kind)
	{
	case NodeKinds::Constant:
	case NodeKinds::Null:
	case NodeKinds::LocalVariable:
		return {};
	default:
		Flatten(source, generatedLocal, requestJITVariable);
	}
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::FlattenStore(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable)
{
	RT::DoubleLinkList<RTJ::Hex::Statement> ret{};

	auto store = node->As<StoreNode>();
	auto&& destination = store->Destination;
	switch (destination->Kind)
	{
	case NodeKinds::LocalVariable:
		break;
	default:
	{
		TreeNode* localNode = nullptr;
		/* The destination of store node is special, we need to 
		*  preserve the semantic of somewhat address-related
		*  operation.
		*
		*  For example, the destination ought to be a InstanceField
		*  or something else, but if we generate JIT variable for 
		*  InstanceField, then finally it will have nothing to do with 
		*  the origin field. So we either choose to make a address-like 
		*  node of InstanceField or just ignore and use the laid down 
		*  origin InstanceFiled. But if we really do the former one, it 
		*  will cause problems for other JIT flows.
		*/
		LAY_DOWN_TO(ret, destination, Flatten(destination, localNode, false));
	}
	}

	auto&& source = store->Source;
	switch (source->Kind)
	{
	case NodeKinds::LocalVariable:
		break;
	case NodeKinds::MorphedCall:
	{
		TreeNode* localNode = nullptr;
		LAY_DOWN_TO(ret, source, FlattenMorphedCall(source, localNode, false));
		break;
	}
	default:
	{
		TreeNode* localNode = nullptr;
		LAY_DOWN_TO(ret, source, Flatten(source, localNode));
	}
	}

	return ret;
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::FlattenUnary(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable)
{
	auto single = node->As<UnaryNodeAccessProxy>();

	RT::DoubleLinkList<RTJ::Hex::Statement> ret{};
	TreeNode* localNode = nullptr;
	LAY_DOWN_TO(ret, single->Value, Flatten(single->Value, localNode));

	if (requestJITVariable)
	{
		if (single->TypeInfo == nullptr)
			THROW("Cannot require JIT variable from void");
		generatedLocal = new (POOL) LocalVariableNode(RequestJITVariable(single->TypeInfo));
		ret.Append(new (POOL) Statement(new (POOL) StoreNode(generatedLocal, node), mCurrentStmt->ILOffset, mCurrentStmt->ILOffset));
	}

	return ret;
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::FlattenBinary(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable)
{
	auto single = node->As<BinaryNodeAccessProxy>();

	RT::DoubleLinkList<RTJ::Hex::Statement> ret{};
	TreeNode* localNode = nullptr;
	LAY_DOWN_TO(ret, single->First, Flatten(single->First, localNode));
	LAY_DOWN_TO(ret, single->Second, Flatten(single->Second, localNode));

	if (requestJITVariable)
	{
		if (single->TypeInfo == nullptr)
			THROW("Cannot require JIT variable from void");
		generatedLocal = new (POOL) LocalVariableNode(RequestJITVariable(single->TypeInfo));
		ret.Append(new (POOL) Statement(new (POOL) StoreNode(generatedLocal, node), mCurrentStmt->ILOffset, mCurrentStmt->ILOffset));
	}

	return ret;
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::FlattenMultiple(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable)
{
	auto call = node->As<CallNode>();

	/*
	* First we need to sort out the evaluation order of arguments
	* 1. Method call
	* 2. Non-trivial expression (complex computation)
	* 3. Trivial expression (direct read on variable or array or sth)
	* 4. Constant
	*/
	DoubleLinkList<Statement> methodSequence{};
	DoubleLinkList<Statement> computationSequence{};

	ForeachInlined(call->Arguments, call->ArgumentCount,
		[&](TreeNode*& argument)
		{
			TreeNode* localNode = nullptr;
			switch (argument->Kind)
			{
			case NodeKinds::MorphedCall:
			{
				LAY_DOWN_TO(methodSequence, argument, FlattenMorphedCall(argument, localNode));
				break;
			}
			case NodeKinds::Call:
			case NodeKinds::New:
			case NodeKinds::NewArray:
			{
				LAY_DOWN_TO(methodSequence, argument, FlattenMultiple(argument, localNode));
				break;
			}
			default:
			{
				LAY_DOWN_TO(computationSequence, argument, Flatten(argument, localNode));
				break;
			}
			}
		});

	//Join
	methodSequence.Append(computationSequence);

	if (requestJITVariable)
	{
		if (call->TypeInfo == nullptr)
			THROW("Cannot require JIT variable from void");
		generatedLocal = new (POOL) LocalVariableNode(RequestJITVariable(call->TypeInfo));

		//Append itself
		methodSequence.Append(new (POOL) Statement(new (POOL) StoreNode(generatedLocal, node), mCurrentStmt->ILOffset, mCurrentStmt->ILOffset));
	}

	return methodSequence;
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::FlattenMorphedCall(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable)
{
	MorphedCallNode* call = node->As<MorphedCallNode>();
	DoubleLinkList<Statement> ret{};

	TreeNode* localNode = nullptr;
	ret.Append(FlattenMultiple(node, localNode, false));

	if (requestJITVariable)
	{
		if (call->TypeInfo == nullptr)
			THROW("Cannot require JIT variable from void");
		generatedLocal = new (POOL) LocalVariableNode(RequestJITVariable(call->TypeInfo));
	}
	
	//Lay down origin value
	LAY_DOWN_TO(ret, call->Origin, Flatten(call->Origin, localNode));

	return ret;
}

RT::Int32 RTJ::Hex::Linearizer::RequestJITVariable(RTM::Type* type)
{
	Int32 index = mJITContext->LocalAttaches.size();
	mJITContext->LocalAttaches.push_back({ type, LocalAttachedFlags::JITGenerated });
	return index;
}

#undef POOL
