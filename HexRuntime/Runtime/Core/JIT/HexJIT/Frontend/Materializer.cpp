#include "Materializer.h"
#include "..\JITHelper.h"
#include "..\..\..\Meta\MetaManager.h"
#include <ranges>

#define POOL (mJITContext->Memory)
#define MORPH_ARG(HELPER) (new (POOL) MorphedCallNode(node, &JITCall::HELPER, CALLING_CONV_OF(HELPER), argument))
#define MORPH_ARGS(HELPER, NUMS) (new (POOL) MorphedCallNode(node, &JITCall::HELPER, CALLING_CONV_OF(HELPER), arguments, NUMS))

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

	auto argument = new (POOL) LoadNode(SLMode::Direct, methodConstant);

	return (new (POOL) MorphedCallNode(node, nativeMethod, callingConv, argument))
		->SetType(node->TypeInfo);
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(NewNode* node)
{
	auto methodConstant = new (POOL) ConstantNode(CoreTypes::Ref);
	methodConstant->Pointer = node->Method;
	auto argument = new (POOL) LoadNode(SLMode::Indirect, methodConstant);
	return MORPH_ARG(NewObject)->SetType(node->TypeInfo);
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(NewArrayNode* node)
{
	if (node->DimensionCount == 1)
	{
		//Single-dimensional
		auto arguments = new (POOL) TreeNode * [2]
		{ 
			new (POOL) LoadNode(SLMode::Direct, new (POOL) ConstantNode(node->ElementType)),
			node->Dimension
		};
		return MORPH_ARGS(NewSZArray, 2)->SetType(node->TypeInfo);
	}
	else
	{
		//Multi-dimensional
		//TODO: Maybe we should generate scoped stack allocation for the third arguments
		auto arguments = new (POOL) TreeNode * [3]
		{
			new (POOL) LoadNode(SLMode::Direct, new (POOL) ConstantNode(node->ElementType)),
			new (POOL) LoadNode(SLMode::Direct, new (POOL) ConstantNode(node->DimensionCount)),
			nullptr
		};

		return MORPH_ARGS(NewArrayFast, 2)->SetType(node->TypeInfo);
	}
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(StoreNode* node)
{
	if (node->Destination->Is(NodeKinds::OffsetOf) &&
		CoreTypes::IsRef(node->Destination->TypeInfo->GetCoreType()))
	{
		auto fieldRef = new (POOL) LoadNode(SLMode::Indirect, node->Destination);

		auto arguments = new (POOL) TreeNode * [2]{
			fieldRef,
			node->Source
		};

		//Replace store directly with write barrier since store is always a stmt
		return MORPH_ARGS(WriteBarrierForRef, 2);
	}
	return node;
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(LoadNode* node)
{
	if (node->Source->Is(NodeKinds::OffsetOf) &&
		CoreTypes::IsRef(node->Source->TypeInfo->GetCoreType()))
	{
		auto argument = new (POOL) LoadNode(SLMode::Indirect, node->Source);
		return MORPH_ARG(ReadBarrierForRef)->SetType(node->TypeInfo);
	}
	return node;
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(ArrayElementNode* node)
{
	auto arrayOffset = new (POOL) ArrayOffsetOfNode(node->Array, node->Index, sizeof(RTO::ArrayObject), node->TypeInfo->GetLayoutSize());
	arrayOffset->SetType(node->TypeInfo);
	return arrayOffset;
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(InstanceFieldNode* node)
{
	auto offset = new (POOL) OffsetOfNode(node->Source->As<LocalVariableNode>(), node->Field->GetOffset());
	offset->SetType(node->TypeInfo);
	return offset;
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
					case NodeKinds::InstanceField:
						node = Morph(node->As<InstanceFieldNode>());
						break;
					case NodeKinds::Array:
						node = Morph(node->As<ArrayElementNode>());
						break;
					}
				});
		}
	}
	return bbHead;
}

#undef POOL