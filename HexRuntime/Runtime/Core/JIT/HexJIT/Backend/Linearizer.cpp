#include "Linearizer.h"
#include "..\..\..\Exception\RuntimeException.h"

#define POOL (mJITContext->Memory)
#define FLATTEN_AND_UPDATE(ORIGIN_SEQ, REPLACE, EXPR) \
		{ \
			auto _statements = EXPR; \
			if (!_statements.IsEmpty()) \
			{ \
				ORIGIN_SEQ.Append(_statements); \
			} \
			if (localNode != nullptr) \
			{	\
				auto loadNode = new (POOL) LoadNode(SLMode::Direct, localNode); \
				REPLACE = loadNode; \
			} \
		}

#define FLATTEN(ORIGIN_SEQ, EXPR) \
		{ \
			auto _statements = EXPR; \
			if (!_statements.IsEmpty()) \
				ORIGIN_SEQ.Append(_statements); \
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

		bbIterator->Now = mStmtHead;
	}
	return bbHead;
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::Flatten(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable)
{
	switch (node->Kind)
	{
	case NodeKinds::Compare:
	case NodeKinds::BinaryArithmetic:
		return FlattenBinary(node, generatedLocal, requestJITVariable);
	case NodeKinds::Convert:
	case NodeKinds::Cast:
	case NodeKinds::Box:
	case NodeKinds::UnBox:
	case NodeKinds::UnaryArithmetic:
	case NodeKinds::Duplicate:
		return FlattenUnary(node, generatedLocal, requestJITVariable);
	case NodeKinds::Call:
	case NodeKinds::New:
	case NodeKinds::NewArray:
		return FlattenMultiple(node, generatedLocal, requestJITVariable);
	case NodeKinds::Store:
		return Flatten(node->As<StoreNode>());
	case NodeKinds::Load:
		return Flatten(node->As<LoadNode>(), generatedLocal, requestJITVariable);
	case NodeKinds::MorphedCall:
		return Flatten(node->As<MorphedCallNode>(), generatedLocal, requestJITVariable);
	case NodeKinds::OffsetOf:
		return Flatten(node->As<OffsetOfNode>(), generatedLocal, requestJITVariable);
	case NodeKinds::ArrayOffsetOf:
		return Flatten(node->As<ArrayOffsetOfNode>(), generatedLocal, requestJITVariable);
	default:
		break;
	}
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::Flatten(ArrayOffsetOfNode* node, TreeNode*& generatedLocal, bool requestJITVariable)
{
	DoubleLinkList<Statement> ret{};

	{
		TreeNode* localNode = nullptr;
		FLATTEN_AND_UPDATE(ret, node->Array, Flatten(node->Array, localNode));
	}

	{
		TreeNode* localNode = nullptr;
		FLATTEN_AND_UPDATE(ret, node->Index, Flatten(node->Index, localNode));
	}
	
	if (requestJITVariable)
	{
		auto refType = Meta::MetaData->InstantiateRefType(node->TypeInfo);
		generatedLocal = new LocalVariableNode(RequestJITVariable(refType));
		generatedLocal->SetType(refType);
	}

	return ret;
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::Flatten(OffsetOfNode* node, TreeNode*& generatedLocal, bool requestJITVariable)
{
	DoubleLinkList<Statement> ret{};

	TreeNode* localNode = nullptr;
	FLATTEN_AND_UPDATE(ret, node->Base, Flatten(node->Base, localNode));

	if (requestJITVariable)
	{
		auto refType = Meta::MetaData->InstantiateRefType(node->TypeInfo);
		generatedLocal = new LocalVariableNode(RequestJITVariable(refType));
		generatedLocal->SetType(refType);
	}

	return ret;
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::Flatten(LoadNode* node, TreeNode*& generatedLocal, bool requestJITVariable)
{
	auto source = node->Source;

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

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::Flatten(StoreNode* node)
{
	RT::DoubleLinkList<RTJ::Hex::Statement> ret{};
	bool requestSourceVariable = true;

	auto&& destination = node->Destination;
	switch (destination->Kind)
	{
	case NodeKinds::LocalVariable:
		//Should not generate for directly store
		requestSourceVariable = false;
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
		FLATTEN(ret, Flatten(destination, localNode, false));
	}
	}

	auto&& source = node->Source;
	switch (source->Kind)
	{
	case NodeKinds::MorphedCall:
	{
		TreeNode* localNode = nullptr;
		FLATTEN(ret, Flatten(source->As<MorphedCallNode>(), localNode, false));
		break;
	}
	default:
	{
		TreeNode* localNode = nullptr;
		FLATTEN_AND_UPDATE(ret, source, Flatten(source, localNode, requestSourceVariable));
	}
	}

	return ret;
}

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::FlattenUnary(TreeNode* node, TreeNode*& generatedLocal, bool requestJITVariable)
{
	auto single = node->As<UnaryNodeAccessProxy>();

	RT::DoubleLinkList<RTJ::Hex::Statement> ret{};
	TreeNode* localNode = nullptr;
	FLATTEN_AND_UPDATE(ret, single->Value, Flatten(single->Value, localNode));

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

	{
		TreeNode* localNode = nullptr;
		FLATTEN_AND_UPDATE(ret, single->First, Flatten(single->First, localNode));
	}

	{
		TreeNode* localNode = nullptr;
		FLATTEN_AND_UPDATE(ret, single->Second, Flatten(single->Second, localNode));
	}
	
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
				FLATTEN_AND_UPDATE(methodSequence, argument, Flatten(argument->As<MorphedCallNode>(), localNode));
				break;
			}
			default:
			{
				FLATTEN_AND_UPDATE(computationSequence, argument, Flatten(argument, localNode));
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

RT::DoubleLinkList<RTJ::Hex::Statement> RTJ::Hex::Linearizer::Flatten(MorphedCallNode* node, TreeNode*& generatedLocal, bool requestJITVariable)
{
	DoubleLinkList<Statement> ret{};

	TreeNode* localNode = nullptr;
	ret.Append(FlattenMultiple(node, localNode, false));
	
	//Flatten origin value
	if (node->Is(NodeKinds::Call))
		FLATTEN_AND_UPDATE(ret, node->Origin, Flatten(node->Origin, localNode, false));

	if (requestJITVariable)
	{
		if (node->TypeInfo == nullptr)
			THROW("Cannot require JIT variable from void");
		generatedLocal = new (POOL) LocalVariableNode(RequestJITVariable(node->TypeInfo));

		//Append itself
		ret.Append(new (POOL) Statement(new (POOL) StoreNode(generatedLocal, node), mCurrentStmt->ILOffset, mCurrentStmt->ILOffset));
	}

	return ret;
}

RT::Int32 RTJ::Hex::Linearizer::RequestJITVariable(RTM::Type* type)
{
	Int32 index = mJITContext->LocalAttaches.size();
	mJITContext->LocalAttaches.push_back({ type, LocalAttachedFlags::JITGenerated });
	return index;
}

#undef POOL
