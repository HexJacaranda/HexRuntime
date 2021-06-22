#include "Materializer.h"

#define POOL (mJITContext->Memory)

RTJ::Hex::Materializer::Materializer(HexJITContext* context) :
	mJITContext(context)
{
}

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphCall(TreeNode* node)
{
	return nullptr;
}

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphNew(TreeNode* node)
{
	auto newNode = node->As<NewNode>();
	return nullptr;
}

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphNewArray(TreeNode* node)
{
	auto newNode = node->As<NewArrayNode>();
	if (newNode->DimensionCount == 1)
	{
		//SZArray case
	}
	else
	{
		//Multi-dimensional
	}
	return nullptr;
}

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphStore(TreeNode* node)
{
	auto storeNode = node->As<StoreNode>();
	if (storeNode->Destination->Is(NodeKinds::InstanceField))
	{

	}
	else if (storeNode->Source->Is(NodeKinds::InstanceField))
	{

	}
	return nullptr;
}

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphLoad(TreeNode* node)
{
	auto storeNode = node->As<LoadNode>();
	if (storeNode->Source->Is(NodeKinds::InstanceField))
	{

	}
	return nullptr;
}

RTJ::Hex::BasicBlock* RTJ::Hex::Materializer::Materialize()
{
	auto bbHead = mJITContext->BBs[0];

	for (BasicBlock* bbIterator = bbHead;
		bbIterator != nullptr;
		bbIterator = bbIterator->Next)
	{
		mStmtHead = bbIterator->Now;

		for (mCurrentStmt = bbIterator->Now;
			mCurrentStmt != nullptr && mCurrentStmt->Now != nullptr;
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