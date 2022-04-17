#include "Materializer.h"
#include "..\JITHelper.h"
#include "..\..\..\Meta\MetaManager.h"
#include "..\..\..\Exception\RuntimeException.h"
#include <ranges>

#define POOL (mJITContext->Memory)
#define MORPH_ARG(HELPER) (new (POOL) MorphedCallNode(node, &JITCall::HELPER, CALLING_CONV_OF(HELPER), argument))
#define MORPH_ARGS(HELPER, NUMS) (new (POOL) MorphedCallNode(node, &JITCall::HELPER, CALLING_CONV_OF(HELPER), arguments, NUMS))
#define MORPH_FIELD(EXPR) EXPR = Morph(EXPR)

RTJ::Hex::Morpher::Morpher(HexJITContext* context) :
	mJITContext(context)
{
}

void RTJ::Hex::Morpher::Set(UInt8 flag)
{
	mStatus |= flag;
}

void RTJ::Hex::Morpher::Unset(UInt8 flag)
{
	mStatus &= ~flag;
}

bool RTJ::Hex::Morpher::Has(UInt8 flag)
{
	return mStatus & flag;
}

RTJ::Hex::LoadNode* RTJ::Hex::Morpher::ChangeToAddressTaken(LoadNode* origin)
{
	auto loadAddress = new (POOL) LoadNode(AccessMode::Address, origin->Source);
	auto ref = Meta::MetaData->InstantiateRefType(origin->Source->TypeInfo);
	loadAddress->SetType(ref);
	return loadAddress;
}

void RTJ::Hex::Morpher::InsertCall(MorphedCallNode* node)
{
	auto stmt = new (POOL) Statement(node, mCurrentStmt->ILOffset, mCurrentStmt->ILOffset);
	//Insert call
	LinkedList::InsertBefore(mStmtHead, mPreviousStmt, stmt);
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(TreeNode* node)
{
	switch (node->Kind)
	{
	case NodeKinds::Call:
		return Morph(node->As<CallNode>());
	case NodeKinds::New:
		return Morph(node->As<NewNode>());
	case NodeKinds::NewArray:
		return Morph(node->As<NewArrayNode>());
	case NodeKinds::Array:
		return Morph(node->As<ArrayElementNode>());
	case NodeKinds::InstanceField:
		return Morph(node->As<InstanceFieldNode>());
	case NodeKinds::Store:
		return Morph(node->As<StoreNode>());
	case NodeKinds::Load:
		return Morph(node->As<LoadNode>());
	case NodeKinds::StaticField:
		THROW("Not supported");
		return Morph(node->As<StaticFieldNode>());
	default:
		TraverseChildren(node, [&](TreeNode*& child) {
			child = Morph(child);
		});
		return node;
	}
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

	auto argument = new (POOL) LoadNode(AccessMode::Value, methodConstant);

	ForeachInlined(node->Arguments, node->ArgumentCount, 
		[&](TreeNode*& node) 
		{
			node = Morph(node);
		});

	return (new (POOL) MorphedCallNode(node, nativeMethod, callingConv, argument))
		->SetType(node->TypeInfo);
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(NewNode* node)
{
	auto arguments = new (POOL) TreeNode * [2]{
		new (POOL) ConstantNode(node->TypeInfo),
		new (POOL) ConstantNode(node->Method)
	};

	ForeachInlined(node->Arguments, node->ArgumentCount,
		[&](TreeNode*& node)
		{
			node = Morph(node);
		});

	return MORPH_ARGS(NewObject, 2)->SetType(node->TypeInfo);
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(NewArrayNode* node)
{
	ForeachInlined(node->Dimensions, node->DimensionCount,
		[&](TreeNode*& node)
		{
			node = Morph(node);
		});

	if (node->DimensionCount == 1)
	{
		//Single-dimensional
		auto arguments = new (POOL) TreeNode * [2]
		{ 
			new (POOL) ConstantNode(node->ElementType),
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
			new (POOL) ConstantNode(node->ElementType),
			new (POOL) ConstantNode(node->DimensionCount),
			nullptr
		};

		return MORPH_ARGS(NewArrayFast, 2)->SetType(node->TypeInfo);
	}
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(StoreNode* node)
{
	switch (node->Mode)
	{
	case AccessMode::Value:
	{
		auto destination = node->Destination;
		if (destination->Is(NodeKinds::InstanceField) &&
			CoreTypes::IsCategoryRef(destination->TypeInfo->GetCoreType()))
		{
			MORPH_FIELD(node->Destination);
			MORPH_FIELD(node->Source);
			auto fieldRef = new (POOL) LoadNode(AccessMode::Address, node->Destination);

			auto arguments = new (POOL) TreeNode * [2]{
				fieldRef,
				node->Source
			};

			//Replace store directly with write barrier since store is always a stmt
			return MORPH_ARGS(WriteBarrierForRef, 2);
		}
		break;
	}
	case AccessMode::Content:
	{
		auto destinationType = node->Destination->TypeInfo;
		auto refValueType = destinationType->GetFirstTypeArgument();
		if (CoreTypes::IsCategoryRef(refValueType->GetCoreType()))
		{
			MORPH_FIELD(node->Destination);
			MORPH_FIELD(node->Source);
			auto arguments = new (POOL) TreeNode * [2]{
				node->Destination,
				node->Source
			};

			//Replace store directly with write barrier since store is always a stmt
			return MORPH_ARGS(WriteBarrierForRef, 2);
		}
		break;
	}
	}

	MORPH_FIELD(node->Destination);
	MORPH_FIELD(node->Source);

	return node;
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(LoadNode* node)
{
	auto source = node->Source;
	auto sourceType = source->TypeInfo;

	//Should collapse the load chain if possible
	if (source->Is(NodeKinds::Load))
	{
		auto sourceLoad = source->As<LoadNode>();
		switch (node->Mode)
		{
		case AccessMode::Value:
			node->Source = sourceLoad->Source;
			break;
		case AccessMode::Address:
		case AccessMode::Content:
			if (sourceLoad->Mode == AccessMode::Value)
				node->Source = sourceLoad->Source;
			break;
		}
	}

	switch (node->Mode)
	{
	case AccessMode::Address:
	{
		//Only 
		With(MorphStatus::AddressTaken, [&]() {
			MORPH_FIELD(node->Source);
		});
		break;
	}	
	case AccessMode::Value:
		//Need to promote struct load to address form
		if (sourceType->IsStruct() && Has(MorphStatus::AddressTaken))
		{
			node = ChangeToAddressTaken(node);
			Use(MorphStatus::AddressTaken, [&]() {
				MORPH_FIELD(node->Source);
			});

			return node;
		}

		//Need to replace it with read barrier
		if (CoreTypes::IsRef(sourceType->GetCoreType()) &&
			(source->Is(NodeKinds::InstanceField) || source->Is(NodeKinds::Array)))
		{
			auto argument = new (POOL) LoadNode(AccessMode::Address, node->Source);
			return MORPH_ARG(ReadBarrierForRef)->SetType(node->TypeInfo);
		}

		MORPH_FIELD(node->Source);
		break;
	case AccessMode::Content:
		MORPH_FIELD(node->Source);
		break;
	}

	return node;
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(ArrayElementNode* node)
{
	MORPH_FIELD(node->Array);
	MORPH_FIELD(node->Index);

	auto arrayOffset = new (POOL) ArrayOffsetOfNode(node->Array, node->Index, sizeof(RTO::ArrayObject), node->TypeInfo->GetLayoutSize());
	arrayOffset->SetType(node->TypeInfo);
	return arrayOffset;
}

RTJ::Hex::TreeNode* RTJ::Hex::Morpher::Morph(InstanceFieldNode* node)
{
	Use(MorphStatus::AddressTaken, [&]() {
		MORPH_FIELD(node->Source);
	});

	auto source = node->Source;
	if (source->Is(NodeKinds::Load))
	{
		auto sourceCoreType = source->TypeInfo->GetCoreType();
		if (CoreTypes::IsStruct(sourceCoreType) || sourceCoreType == CoreTypes::InteriorRef)
		{
			if (ValueIs(source, NodeKinds::OffsetOf))
			{
				auto originOffset = ValueAs<OffsetOfNode>(source);
				auto offset = new (POOL) OffsetOfNode(
					node->Source->As<LocalVariableNode>(),
					node->Field->GetOffset());

				offset->SetType(node->TypeInfo);
				//Collapse offset
				offset->Base = originOffset->Base;
				offset->Offset += originOffset->Offset;

				return offset;
			}
			else if (ValueIs(source, NodeKinds::ArrayOffsetOf))
			{
				auto originOffset = ValueAs<ArrayOffsetOfNode>(source);
				originOffset->SetType(node->TypeInfo);
				originOffset->BaseOffset += node->Field->GetOffset();

				return originOffset;
			}
		}
	}

	auto offset = new (POOL) OffsetOfNode(
		node->Source->As<LocalVariableNode>(),
		node->Field->GetOffset());

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
			MORPH_FIELD(mCurrentStmt->Now);
		}

		if (bbIterator->BranchConditionValue != nullptr)
		{
			MORPH_FIELD(bbIterator->BranchConditionValue);
		}
	}
	return bbHead;
}

#undef POOL