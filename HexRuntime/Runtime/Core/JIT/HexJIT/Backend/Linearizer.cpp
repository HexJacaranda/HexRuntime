#include "Linearizer.h"
#include "..\..\..\Exception\RuntimeException.h"

#define POOL (mJITContext->Memory)
#define FLATTEN(TARGET) auto&& [stmts, transformed] = Flatten(node, (TARGET)); \
						(TARGET) = transformed

#define FLATTEN_KEEP(TARGET) auto&& [stmts, transformed] = Flatten(node, (TARGET), true); \
						(TARGET) = transformed

#define REQ_JIT_VARIABLE(TYPE) (new (POOL) LocalVariableNode(RequestJITVariable(TYPE)))->SetType((TYPE));
#define NEW_LOAD_VALUE(LOCAL_VARIABLE) (new (POOL) LoadNode(AccessMode::Value, LOCAL_VARIABLE))->SetType(LOCAL_VARIABLE->TypeInfo)

RTJ::Hex::Linearizer::Linearizer(HexJITContext* context) : mJITContext(context)
{
}

RTJ::Hex::BasicBlock* RTJ::Hex::Linearizer::PassThrough()
{
	auto&& bbHead = mJITContext->BBs[0];

	auto flatten = [&](TreeNode*& root) {
		auto&& [stmts, transformed] = Flatten(nullptr, root);
		if (!stmts.IsEmpty())
			LinkedList::AppendRangeTwoWay(mStmtHead, mPreviousStmt, stmts.GetHead(), stmts.GetTail());
		root = transformed;
	};

	for (BasicBlock* bbIterator = bbHead;
		bbIterator != nullptr;
		bbIterator = bbIterator->Next)
	{ 
		mStmtHead = bbIterator->Now;
		mPreviousStmt = nullptr;

		for (mCurrentStmt = bbIterator->Now;
			mCurrentStmt != nullptr && mCurrentStmt->Now != nullptr;
			mPreviousStmt = mCurrentStmt,
			mCurrentStmt = mCurrentStmt->Next)
		{
			flatten(mCurrentStmt->Now);
		}
		if (bbIterator->BranchConditionValue != nullptr)
			flatten(bbIterator->BranchConditionValue);

		bbIterator->Now = mStmtHead;
	}
	return bbHead;
}

RTJ::Hex::FlattenResult RTJ::Hex::Linearizer::Flatten(TreeNode* parent, MorphedCallNode* node, bool shouldKeep)
{
	if (node->Origin->Is(NodeKinds::Call))
		return Flatten(parent, node->Origin->As<MultipleNodeAccessProxy>());
	else
		return Flatten(parent, node->As<MultipleNodeAccessProxy>());
}

RTJ::Hex::FlattenResult RTJ::Hex::Linearizer::Flatten(TreeNode* parent, MultipleNodeAccessProxy* node, bool shouldKeep)
{
	StmtList methodStmts{};
	StmtList computationStmts{};
	StmtList simpleStmts{};

	ForeachInlined(node->Values, node->Count, 
		[&](TreeNode*& argument) 
		{
			auto&& [stmts, transformed] = Flatten(node, argument);

			if (argument->Is(NodeKinds::MorphedCall))
				methodStmts.Append(stmts);
			else if (
				ValueIs(argument, NodeKinds::Constant) ||
				ValueIs(argument, NodeKinds::LocalVariable))
			{
				simpleStmts.Append(stmts);
			}
			else
				computationStmts.Append(stmts);

			argument = transformed;
		});	

	StmtList finalChain{};
	finalChain.Append(methodStmts);
	finalChain.Append(computationStmts);
	finalChain.Append(simpleStmts);

	if (shouldKeep)
		return { finalChain, node };
	else
	{
		auto local = REQ_JIT_VARIABLE(node->TypeInfo);
		auto newLoad = NEW_LOAD_VALUE(local);
		auto stmt = new (POOL) Statement(new (POOL) StoreNode(local, node), GetCurrentOffset(), GetCurrentOffset());
		finalChain.Append(stmt);
		
		return { finalChain, newLoad };
	}
}

RTJ::Hex::FlattenResult RTJ::Hex::Linearizer::Flatten(TreeNode* parent, OffsetOfNode* node, bool shouldKeep)
{
	RTAssert(parent != nullptr);
	StmtList retStmts{};

	FLATTEN(node->Base);
	retStmts.Append(stmts);

	if (shouldKeep || (parent != nullptr && parent->Is(NodeKinds::Store)))
		return { retStmts, node };
	else if (parent->Is(NodeKinds::Load))
	{
		auto loadParent = parent->As<LoadNode>();

		auto local = REQ_JIT_VARIABLE(parent->TypeInfo);
		auto newLoad = NEW_LOAD_VALUE(local);
		auto stmt = new (POOL) Statement(new (POOL) StoreNode(local, loadParent), GetCurrentOffset(), GetCurrentOffset());

		//Update 
		retStmts.Append(stmt);
		return { retStmts, newLoad };
	}
	else
		THROW("Inpossible parent node type");
}

RTJ::Hex::FlattenResult RTJ::Hex::Linearizer::Flatten(TreeNode* parent, LocalVariableNode* node, bool shouldKeep)
{
	StmtList retStmts{};
	if (parent == nullptr) {
		return { retStmts, node };
	}

	if (shouldKeep || (parent != nullptr && parent->Is(NodeKinds::Store)))
		return { retStmts, node };
	else if (parent->Is(NodeKinds::Load))
	{	
		auto loadParent = parent->As<LoadNode>();
		if (loadParent->Mode == AccessMode::Value)
			return { retStmts, parent };

		auto local = REQ_JIT_VARIABLE(parent->TypeInfo);
		auto newLoad = NEW_LOAD_VALUE(local);
		auto stmt = new (POOL) Statement(new (POOL) StoreNode(local, loadParent), GetCurrentOffset(), GetCurrentOffset());

		//Update 
		retStmts.Append(stmt);
		return { retStmts, newLoad };
	}
	else
		THROW("Impossible parent node type");
}

RTJ::Hex::FlattenResult RTJ::Hex::Linearizer::Flatten(TreeNode* parent, ArrayOffsetOfNode* node, bool shouldKeep)
{
	RTAssert(parent != nullptr);

	StmtList retStmts{};
	{
		FLATTEN(node->Array);
		retStmts.Append(stmts);
	}
	{
		FLATTEN(node->Index);
		retStmts.Append(stmts);
	}

	if (shouldKeep || (parent != nullptr && parent->Is(NodeKinds::Store)))
		return { retStmts, node };
	else if (parent->Is(NodeKinds::Load))
	{
		auto loadParent = parent->As<LoadNode>();

		auto local = REQ_JIT_VARIABLE(parent->TypeInfo);
		auto newLoad = NEW_LOAD_VALUE(local);
		auto stmt = new (POOL) Statement(new (POOL) StoreNode(local, loadParent), GetCurrentOffset(), GetCurrentOffset());

		//Update 
		retStmts.Append(stmt);
		return { retStmts, newLoad };
	}
	else
		THROW("Impossible parent node type");
}

RTJ::Hex::FlattenResult RTJ::Hex::Linearizer::Flatten(TreeNode* parent, TreeNode* node, bool shouldKeep)
{
	switch (node->Kind)
	{
	case NodeKinds::Store:
		return Flatten(parent, node->As<StoreNode>(), shouldKeep);
	case NodeKinds::Load:
		return Flatten(parent, node->As<LoadNode>(), shouldKeep);
	case NodeKinds::MorphedCall:
		return Flatten(parent, node->As<MorphedCallNode>(), shouldKeep);
	case NodeKinds::ArrayOffsetOf:
		return Flatten(parent, node->As<ArrayOffsetOfNode>(), shouldKeep);
	case NodeKinds::OffsetOf:
		return Flatten(parent, node->As<OffsetOfNode>(), shouldKeep);
	case NodeKinds::LocalVariable:
		return Flatten(parent, node->As<LocalVariableNode>(), shouldKeep);
	case NodeKinds::Constant:
	case NodeKinds::Null:
		return { {}, node };	
	default:
	{
		//Other compound operation
		StmtList retStmts{};

		TraverseChildren(node,
			[&](TreeNode*& child)
			{
				FLATTEN(child);
				retStmts.Append(stmts);
			});

		if (shouldKeep)
			return { retStmts, node };
		else
		{
			auto local = REQ_JIT_VARIABLE(node->TypeInfo);
			auto newLoad = NEW_LOAD_VALUE(local);
			auto stmt = new (POOL) Statement(new (POOL) StoreNode(local, node), GetCurrentOffset(), GetCurrentOffset());
			retStmts.Append(stmt);

			return { retStmts, newLoad };
		}
	}
	break;
	}
}

RTJ::Hex::FlattenResult RTJ::Hex::Linearizer::Flatten(TreeNode* parent, LoadNode* node, bool shouldKeep)
{
	if(parent != nullptr && parent->Is(NodeKinds::Store))
	{
		FLATTEN_KEEP(node->Source);

		auto store = parent->As<StoreNode>();
		if (store->Destination == node)
			return { stmts, node->Source };
		else
			return { stmts, node };
	}

	if (shouldKeep)
	{
		FLATTEN_KEEP(node->Source);
		return { stmts, node };
	}

	return Flatten(node, node->Source);
}

RTJ::Hex::FlattenResult RTJ::Hex::Linearizer::Flatten(TreeNode* parent, StoreNode* node, bool shouldKeep)
{
	auto retStmts = StmtList{};

	{
		FLATTEN(node->Destination);
		retStmts.Append(stmts);
	}

	{
		if (node->Destination->Is(NodeKinds::LocalVariable))
		{
			FLATTEN_KEEP(node->Source);
			retStmts.Append(stmts);
		}
		else
		{
			FLATTEN(node->Source);
			retStmts.Append(stmts);
		}		
	}

	return { retStmts , node };
}

RT::Int32 RTJ::Hex::Linearizer::GetCurrentOffset() const
{
	return mCurrentStmt->ILOffset;
}

RT::Int32 RTJ::Hex::Linearizer::RequestJITVariable(RTM::Type* type)
{
	Int32 index = mJITContext->LocalAttaches.size();
	mJITContext->LocalAttaches.push_back({ type, LocalAttachedFlags::JITGenerated });
	return index;
}

#undef POOL
