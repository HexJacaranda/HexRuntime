#include "Materializer.h"
#include "..\JITHelper.h"
#include "..\..\..\Meta\MetaManager.h"

#define POOL (mJITContext->Memory)

RTJ::Hex::Materializer::Materializer(HexJITContext* context) :
	mJITContext(context)
{
}

void RTJ::Hex::Materializer::InsertCall(MorphedCallNode* node)
{
	auto stmt = new(POOL) Statement(node, mCurrentStmt->ILOffset, mCurrentStmt->ILOffset);
	//Insert call
	LinkedList::InsertBefore(mStmtHead, mPreviousStmt, stmt);
}

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphCall(TreeNode* node)
{
	return new(POOL) MorphedCallNode(node, &JITCall::ManagedCallSignature);
}

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphNew(TreeNode* node)
{
	return new(POOL) MorphedCallNode(node, &JITCall::NewObjectSignature);
}

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphNewArray(TreeNode* node)
{
	auto newNode = node->As<NewArrayNode>();
	auto ret = new(POOL) MorphedCallNode(newNode, nullptr);
	if (newNode->DimensionCount == 1)
	{
		//SZArray case
		ret->Signature = &JITCall::NewSZArraySignature;
	}
	else
	{
		ret->Signature = &JITCall::NewArraySignature;
		//Multi-dimensional
	}
	return ret;
}

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphStore(TreeNode* node)
{
	auto storeNode = node->As<StoreNode>();
	if (storeNode->Destination->Is(NodeKinds::InstanceField))
	{
		auto writeBarrierNode = new(POOL) MorphedCallNode(node, &JITCall::WriteBarrierForRefSignature);
		InsertCall(writeBarrierNode);
	}
	return node;
}

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphLoad(TreeNode* node)
{
	auto storeNode = node->As<LoadNode>();
	if (storeNode->Source->Is(NodeKinds::InstanceField))
	{
		auto readBarrierNode = new(POOL) MorphedCallNode(node, &JITCall::ReadBarrierForRefSignature);
		InsertCall(readBarrierNode);
	}
	return node;
}

RTJ::Hex::BasicBlock* RTJ::Hex::Materializer::PassThrough()
{
	auto bbHead = mJITContext->BBs[0];

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
			TraverseTreeBottomUp(
				mJITContext->Traversal.Space, 
				mJITContext->Traversal.Count, 
				mCurrentStmt->Now,
				[&](TreeNode*& node) {
					switch (node->Kind)
					{
					case NodeKinds::Call:
						node = MorphCall(node);
						break;
					case NodeKinds::New:
						node = MorphNew(node);
						break;
					case NodeKinds::NewArray:
						node = MorphNewArray(node);
						break;
					case NodeKinds::Load:
						node = MorphLoad(node);
						break;
					case NodeKinds::Store:
						node = MorphStore(node);
						break;
					}
				});
		}
	}
	return bbHead;
}

#undef POOL