#include "Materializer.h"

#define POOL (mJITContext->Memory)

RTJ::Hex::Materializer::Materializer(HexJITContext* context) :
	mJITContext(context)
{
}

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphCall(TreeNode* node, Statement*& first, Statement* currentStmt)
{
	return nullptr;
}

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphNew(TreeNode* node, Statement*& first, Statement* currentStmt)
{
	auto newNode = node->As<NewNode>();
	return nullptr;
}

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphNewArray(TreeNode* node, Statement*& first, Statement* currentStmt)
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

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphStore(TreeNode* node, Statement*& first, Statement* currentStmt)
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

RTJ::Hex::TreeNode* RTJ::Hex::Materializer::MorphLoad(TreeNode* node, Statement*& first, Statement* currentStmt)
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
		for (Statement* stmtIterator = bbIterator->Now;
			stmtIterator != nullptr && stmtIterator->Now != nullptr;
			stmtIterator = stmtIterator->Next)
		{
			TraverseTreeBottomUp(nullptr, 0, stmtIterator->Now,
				[&](TreeNode*& node) {
					switch (node->Kind)
					{
					case NodeKinds::Call:
						node = MorphCall(node, bbIterator->Now, stmtIterator);
						break;
					case NodeKinds::New:
						node = MorphNew(node, bbIterator->Now, stmtIterator);
						break;
					case NodeKinds::NewArray:
						node = MorphNewArray(node, bbIterator->Now, stmtIterator);
						break;
					case NodeKinds::Load:
						node = MorphLoad(node, bbIterator->Now, stmtIterator);
						break;
					case NodeKinds::Store:
						node = MorphStore(node, bbIterator->Now, stmtIterator);
						break;
					}
				});
		}
	}
	return bbHead;
}

#undef POOL