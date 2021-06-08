#include "SSABuilder.h"
#include "IR.h"
#include "..\..\..\Type\CoreTypes.h"

void RTJ::Hex::SSABuilder::DecideSSATrackability()
{
	/*
	 * Usually we won't track any type modified with pointer or
	 * marked not primitive. But the nullability of reference
	 * is involved in tracking for eliminating redundant checks
	 */

	auto isTrackable = [](TypeRepresentation const& type) {
		if (type.CoreType < CoreTypes::Struct || type.CoreType == CoreTypes::Ref)
			return SSATrackability::Ok;
		return SSATrackability::Forbidden;
	};
	auto rawContext = mJITContext->Context;
	if (rawContext->LocalVariables.size())
		mJITContext->LocalSSATrackability = std::move(
			std::vector<SSATrackability>(rawContext->LocalVariables.size()));
	if (rawContext->Arguments.size())
		mJITContext->ArgumentSSATrackability = std::move(
			std::vector<SSATrackability>(rawContext->Arguments.size()));

	for (Int32 i = 0; i < rawContext->LocalVariables.size(); ++i)
		mJITContext->LocalSSATrackability[i] = isTrackable(rawContext->LocalVariables[i].Type);

	for (Int32 i = 0; i < rawContext->Arguments.size(); ++i)
		mJITContext->ArgumentSSATrackability[i] = isTrackable(rawContext->Arguments[i].Type);
}

bool RTJ::Hex::SSABuilder::IsVariableTrackable(NodeKinds kind, Int16 variableIndex)
{
	return mJITContext->LocalSSATrackability[variableIndex] == SSATrackability::Ok;
}

void RTJ::Hex::SSABuilder::WriteVariable(NodeKinds kind, Int16 variableIndex, Int32 blockIndex, TreeNode* value)
{
	if (kind == NodeKinds::LocalVariable)
		mLocalDefinition[variableIndex][blockIndex] = value;
	else
		mArgumentDefinition[variableIndex][blockIndex] = value;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::ReadVariable(NodeKinds kind, Int16 variableIndex, Int32 blockIndex)
{
	TreeNode* ret = nullptr;
	if (kind == NodeKinds::LocalVariable)
		ret = mLocalDefinition[variableIndex][blockIndex];
	else
		ret = mArgumentDefinition[variableIndex][blockIndex];

	if (ret != nullptr)
		return ret;
	return ReadVariableLookUp(kind, variableIndex, blockIndex);
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::ReadVariableLookUp(NodeKinds kind, Int16 variableIndex, Int32 blockIndex)
{
	BasicBlock* block = mJITContext->BBs[blockIndex];
	TreeNode* value = nullptr;
	if (block->BBIn.size() == 1 && block->BBIn[0] != nullptr)
		value = ReadVariable(kind, variableIndex, block->BBIn[0]->Index);
	else
	{
		auto phi = new(mJITContext->Memory) SSA::PhiNode(block);		
		WriteVariable(kind, variableIndex, blockIndex, phi);
		value = AddPhiOperands(kind, variableIndex, blockIndex, phi);
	}
	WriteVariable(kind, variableIndex, blockIndex, value);
	return value;
}

RTJ::Hex::SSA::PhiNode* RTJ::Hex::SSABuilder::AddPhiOperands(NodeKinds kind, Int16 variableIndex, Int32 blockIndex, SSA::PhiNode* phiNode)
{
	BasicBlock* block = mJITContext->BBs[blockIndex];
	for (auto&& predecessor : block->BBIn)
		phiNode->Choices.push_back(ReadVariable(kind, variableIndex, predecessor->Index));
	return TryRemoveRedundantPhiNode(phiNode);
}

RTJ::Hex::SSA::PhiNode* RTJ::Hex::SSABuilder::TryRemoveRedundantPhiNode(SSA::PhiNode* phiNode)
{
	return phiNode;
}

RTJ::Hex::SSABuilder::SSABuilder(HexJITContext* jitContext) :
	mJITContext(jitContext),
	mTarget(jitContext->BBs[0]) 
{

}

RTJ::Hex::BasicBlock* RTJ::Hex::SSABuilder::Build()
{
	//Initialize
	DecideSSATrackability();
	mArgumentDefinition = std::move(
		std::vector<std::unordered_map<Int16, TreeNode*>>(mJITContext->Context->Arguments.size()));
	mLocalDefinition = std::move(
		std::vector<std::unordered_map<Int16, TreeNode*>>(mJITContext->Context->LocalVariables.size()));

	//Traverse to find every write or read for local variables marked trackable.
	for (BasicBlock* bbIterator = mTarget; 
		bbIterator != nullptr; 
		bbIterator = bbIterator->Next)
	{
		for (Statement* stmtIterator = bbIterator->Now;
			stmtIterator != nullptr && stmtIterator->Now != nullptr;
			stmtIterator = stmtIterator->Next)
		{
			auto node = stmtIterator->Now;
			//The store node is always a stmt
			if (node->Kind == NodeKinds::Store)
			{
				auto store = node->As<StoreNode>();
				auto kind = store->Destination->Kind;
				auto index = -1;

				if (kind == NodeKinds::LocalVariable || kind == NodeKinds::Argument)
					index = store->Destination->As<LocalVariableNode>()->LocalIndex;

				if (index != -1 && IsVariableTrackable(kind, index))
					WriteVariable(kind, index, bbIterator->Index, store->Source);
			}

			TraverseTree<256>(node, [&](TreeNode*& value) {
				if (!value->Is(NodeKinds::Load))
					return;

				auto load = value->As<LoadNode>();
				auto kind = load->Source->Kind;
				auto index = -1;

				if (kind == NodeKinds::LocalVariable || kind == NodeKinds::Argument)
					index = load->Source->As<LocalVariableNode>()->LocalIndex;

				if (index != -1 && IsVariableTrackable(kind, index))
					value = ReadVariable(kind, index, bbIterator->Index);
			});		
		}
	}
	return mTarget;
}
