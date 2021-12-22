#include "Materializer.h"
#include "..\JITHelper.h"
#include "..\..\..\Meta\MetaManager.h"
#include <ranges>

#define POOL (mJITContext->Memory)

RTJ::Hex::Morpher::Morpher(HexJITContext* context) :
	mJITContext(context)
{
}

void RTJ::Hex::Morpher::InsertCall(MorphedCallNode* node)
{
	auto stmt = new(POOL) Statement(node, mCurrentStmt->ILOffset, mCurrentStmt->ILOffset);
	//Insert call
	LinkedList::InsertBefore(mStmtHead, mPreviousStmt, stmt);
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(CallNode* node)
{
	auto methodConstant = new (POOL) ConstantNode(CoreTypes::Ref);
	methodConstant->Pointer = node->Method;

	auto managedMethod = node->Method;
	void* nativeMethod = nullptr;
	Platform::PlatformCallingConvention* callingConv = nullptr;
	if (managedMethod->IsStatic() || managedMethod->IsFinal())
	{
		nativeMethod = &JITCall::ManagedDirectCall;
		callingConv = CALLING_CONV_OF(ManagedDirectCall);
	}
	else if (managedMethod->IsVirtual())
	{
		nativeMethod = &JITCall::ManagedVirtualCall;
		callingConv = CALLING_CONV_OF(ManagedVirtualCall);
	}

	return (new (POOL) MorphedCallNode(node, nativeMethod, callingConv, methodConstant))
		->SetType(node->TypeInfo);
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(NewNode* node)
{
	auto methodConstant = new (POOL) ConstantNode(CoreTypes::Ref);
	methodConstant->Pointer = node->Method;

	return (new (POOL) MorphedCallNode(node, &JITCall::NewObject, CALLING_CONV_OF(NewObject), methodConstant))
		->SetType(node->TypeInfo);
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(NewArrayNode* node)
{
	if (node->DimensionCount == 1)
	{
		//Single-dimensional
		auto arguments = new (POOL) TreeNode * [2]
		{ 
			new (POOL) ConstantNode(node->ElementType), 
			node->Dimension 
		};
		return (new (POOL) MorphedCallNode(node, &JITCall::NewSZArray, CALLING_CONV_OF(NewSZArray), arguments, 2))
			->SetType(node->TypeInfo);
	}
	else
	{
		//Multi-dimensional
		//TODO: Maybe we should generate scoped stack allocation for the third arguments
		auto arguments = new (POOL) TreeNode * [3]
		{
			new (POOL) ConstantNode(node->ElementType),
			new (POOL) ConstantNode(node->DimensionCount),
			nullptr
		};

		return (new (POOL) MorphedCallNode(node, &JITCall::NewArrayFast, CALLING_CONV_OF(NewArray), arguments, 2))
			->SetType(node->TypeInfo);
	}
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(StoreNode* node)
{
	if (node->Destination->Is(NodeKinds::InstanceField))
	{
		//A trick that utilizes the sequential layout of StoreNode
		auto arguments = &node->Destination;
			
	   /* new (POOL) TreeNode* [2]
		{
			node->Destination,
			node->Source
		};*/

		auto writeBarrierNode = new (POOL) MorphedCallNode(node, &JITCall::WriteBarrierForRef, CALLING_CONV_OF(WriteBarrierForRef), arguments, 2);
		InsertCall(writeBarrierNode);
	}
	return node;
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(LoadNode* node)
{
	if (node->Source->Is(NodeKinds::InstanceField))
	{
		auto readBarrierNode = new(POOL) MorphedCallNode(node, &JITCall::ReadBarrierForRef, CALLING_CONV_OF(ReadBarrierForRef), node);
		readBarrierNode->SetType(node->TypeInfo);
		return readBarrierNode;
	}
	return node;
}

RTJ::Hex::BasicBlock* RTJ::Hex::Morpher::PassThrough()
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
						node = Morph(node->As<CallNode>());
						break;
					case NodeKinds::New:
						node = Morph(node->As<NewNode>());
						break;
					case NodeKinds::NewArray:
						node = Morph(node->As<NewArrayNode>());
						break;
					case NodeKinds::Load:
						node = Morph(node->As<LoadNode>());
						break;
					case NodeKinds::Store:
						node = Morph(node->As<StoreNode>());
						break;
					}
				});
		}
	}
	return bbHead;
}

#undef POOL