#include "LSRA.h"
#include "..\..\..\Exception\RuntimeException.h"

void RTJ::Hex::LSRA::ChooseCandidate()
{
	/* Trackable flag will be reused here. And our criteria of candidates are:
	* 1. Local size <= sizeof(void*)
	* 2. Use count > 0
	*/

	for (auto&& local : mContext->LocalAttaches)
	{
		if (local.UseCount > 0 && local.JITVariableType->GetSize() <= sizeof(void*))
			local.Flags |= LocalAttachedFlags::Trackable;
		else
			local.Flags &= ~LocalAttachedFlags::Trackable;
	}

	for (auto&& argument : mContext->ArgumentAttaches)
	{
		if (argument.UseCount > 0 && argument.JITVariableType->GetSize() <= sizeof(void*))
			argument.Flags |= LocalAttachedFlags::Trackable;
		else
			argument.Flags &= ~LocalAttachedFlags::Trackable;
	}
}

void RTJ::Hex::LSRA::ComputeLivenessDuration()
{
	auto bbHead = mContext->BBs.front();
	for (BasicBlock* bbIterator = bbHead; 
		bbIterator != nullptr; 
		bbIterator = bbIterator->Next)
	{
		for (Statement* stmtIterator = bbIterator->Now;
			stmtIterator != nullptr;
			stmtIterator = stmtIterator->Next)
		{
			/* Here we can assume that all the nodes are flattened by Linearizer.
			* All the depth should not be greater than 3 (maybe 4?)
			*/
			UpdateLivenessFor(stmtIterator->Now);
			mLivenessIndex++;
		}

		if (bbIterator->BranchConditionValue != nullptr)
			UpdateLivenessFor(bbIterator->BranchConditionValue);
	}

	//Reset index for allocation
	mLivenessIndex = 0;
}

void RTJ::Hex::LSRA::AllocateRegisters()
{
	ForeachStatement(mContext->BBs.front(), [&](auto now, bool _) {
		Int32 before = mInstructions.size();
		mInterpreter.Interpret(mInstructions, now);
		Int32 after = mInstructions.size();
		AllocateRegisterFor(before, after);
	});
}

void RTJ::Hex::LSRA::AllocateRegisterFor(Int32 seqBeginIndex, Int32 seqEndIndex)
{
	for (Int32 i = seqBeginIndex; i < seqEndIndex; ++i)
	{
		auto&& instruction = mInstructions[i];

	}
}

void RTJ::Hex::LSRA::UpdateLivenessFor(TreeNode* node)
{
	auto update = [&](Int16 index, NodeKinds kind) {
		if (kind == NodeKinds::LocalVariable && !mContext->LocalAttaches[index].IsTrackable())
			return;

		if (kind == NodeKinds::Argument && !mContext->ArgumentAttaches[index].IsTrackable())
			return;

		Int32 mapIndex = kind == NodeKinds::LocalVariable ? 0 : 1;
		auto&& livenessList = mLiveness[mapIndex][index];

		if (livenessList.size() == 0)
			livenessList.push_back({ mLivenessIndex, mLivenessIndex + 1 });
		else
		{
			auto&& previous = livenessList.back();
			if (previous.To == mLivenessIndex)
				previous.To++;
			else
				livenessList.push_back({ mLivenessIndex, mLivenessIndex + 1 });
		}
	};

	switch (node->Kind)
	{
	case NodeKinds::Store:
	{
		if (auto variable = GuardedDestinationExtract(node->As<StoreNode>()))
			update(variable->LocalIndex, variable->Kind);
		break;
	}
	case NodeKinds::Load:
	{
		if (auto variable = GuardedSourceExtract(node->As<LoadNode>()))
			update(variable->LocalIndex, variable->Kind);
		break;
	}
	case NodeKinds::MorphedCall:
	{
		auto call = node->As<MorphedCallNode>();
		ForeachInlined(call->Arguments, call->ArgumentCount,
			[&](auto node) { UpdateLivenessFor(node); });

		auto origin = call->Origin;
		if (origin->Is(NodeKinds::Call))
		{
			auto managedCall = origin->As<CallNode>();
			ForeachInlined(managedCall->Arguments, managedCall->ArgumentCount,
				[&](auto node) { UpdateLivenessFor(node); });
		}
		break;
	}
	default:
		THROW("Should not reach here");
		break;
	}
}

RTJ::Hex::LocalVariableNode* RTJ::Hex::LSRA::GuardedDestinationExtract(StoreNode* store)
{
	auto dest = store->Destination;

	if (dest->Is(NodeKinds::LocalVariable) ||
		dest->Is(NodeKinds::Argument))
	{
		return dest->As<LocalVariableNode>();
	}
	return nullptr;
}

RTJ::Hex::LocalVariableNode* RTJ::Hex::LSRA::GuardedSourceExtract(LoadNode* store)
{
	auto source = store->Source;

	if (source->Is(NodeKinds::LocalVariable) ||
		source->Is(NodeKinds::Argument))
	{
		return source->As<LocalVariableNode>();
	}
	return nullptr;
}

RTJ::Hex::LSRA::LSRA(HexJITContext* context) : mContext(context), mInterpreter(context->Memory)
{
}

RTJ::Hex::BasicBlock* RTJ::Hex::LSRA::PassThrough()
{
	ComputeLivenessDuration();
	AllocateRegisters();
	return mContext->BBs.front();
}
