#include "SSABuilder.h"
#include "IR.h"
#include "..\..\..\Type\CoreTypes.h"

#define IMPORT_LOCAL(LOCAL, INDEX, KIND) \
	auto INDEX = LOCAL->LocalIndex; \
	auto KIND = LOCAL->Kind

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

bool RTJ::Hex::SSABuilder::IsVariableTrackable(LocalVariableNode* local)
{
	IMPORT_LOCAL(local, index, kind);
	if (kind == NodeKinds::LocalVariable)
		return mJITContext->LocalSSATrackability[index] == SSATrackability::Ok;
	else
		return mJITContext->ArgumentSSATrackability[index] == SSATrackability::Ok;
}

void RTJ::Hex::SSABuilder::WriteVariable(LocalVariableNode* local, Int32 blockIndex, TreeNode* value)
{
	IMPORT_LOCAL(local, variableIndex, kind);
	if (kind == NodeKinds::LocalVariable)
		mLocalDefinition[variableIndex][blockIndex] = value;
	else
		mArgumentDefinition[variableIndex][blockIndex] = value;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::ReadVariable(LocalVariableNode* local, Int32 blockIndex)
{
	IMPORT_LOCAL(local, variableIndex, kind);
	TreeNode* ret = nullptr;
	if (kind == NodeKinds::LocalVariable)
		ret = mLocalDefinition[variableIndex][blockIndex];
	else
		ret = mArgumentDefinition[variableIndex][blockIndex];

	if (ret != nullptr)
		return ret;
	return ReadVariableLookUp(local, blockIndex);
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::ReadVariableLookUp(LocalVariableNode* local, Int32 blockIndex)
{
	IMPORT_LOCAL(local, variableIndex, kind);

	BasicBlock* block = mJITContext->BBs[blockIndex];
	TreeNode* value = nullptr;
	if (block->BBIn.size() == 1 && block->BBIn[0] != nullptr)
		value = ReadVariable(local, block->BBIn[0]->Index);
	else
	{
		auto phi = mJITContext->Memory->New<SSA::PhiNode>(block);		
		WriteVariable(local, blockIndex, phi);
		value = AddPhiOperands(local, blockIndex, phi);
	}
	WriteVariable(local, blockIndex, value);
	return value;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::TrySimplifyChoice(SSA::PhiNode* origin, TreeNode* node)
{
	if (origin == node)
		return nullptr;
	if (node->Is(NodeKinds::Phi))
	{
		auto phiChoice = node->As<SSA::PhiNode>();
		if (phiChoice->IsEmpty() || phiChoice->IsRemoved())
			return nullptr;
		if (phiChoice->IsCollapsed())
			//Replace with collapsed value
			return phiChoice->CollapsedValue;
	}
	//Non-trival case
	return node;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::AddPhiOperands(LocalVariableNode* local, Int32 blockIndex, SSA::PhiNode* phiNode)
{
	IMPORT_LOCAL(local, variableIndex, kind);
	BasicBlock* block = mJITContext->BBs[blockIndex];
	for (auto&& predecessor : block->BBIn)
	{
		auto choice = TrySimplifyChoice(phiNode, ReadVariable(local, predecessor->Index));
		if (choice != nullptr)
			phiNode->Choices.push_back(choice);
	}
		
	return TryRemoveRedundantPhiNode(phiNode);
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::TryRemoveRedundantPhiNode(SSA::PhiNode* phiNode)
{
	//May be directly collapsed
	if (phiNode->Choices.size() == 1)
		return (phiNode->CollapsedValue = phiNode->Choices[0]);

	//Deal with duplicated case
	TreeNode* sameNode = nullptr;
	for (auto&& choice : phiNode->Choices)
	{
		if (choice == sameNode)
			continue;
		if (sameNode != nullptr)
		{
			//Non-trivial case, merged two or more values that may not collapse
			sameNode = nullptr;
			break;
		}
		sameNode = choice;
	}

	//If this collapsed
	if (phiNode->Choices.size() == 1 || sameNode != nullptr)
		return (phiNode->CollapsedValue = sameNode);

	//Still not trivial
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

	auto readWrite = [&](TreeNode*& node, Int32 bbIndex) {
		//The store node is always a stmt
		if (node->Kind == NodeKinds::Store)
		{
			auto store = node->As<StoreNode>();
			auto kind = store->Destination->Kind;
			auto index = -1;

			LocalVariableNode* local = nullptr;
			if (kind == NodeKinds::LocalVariable || kind == NodeKinds::Argument)
			{
				local = store->Destination->As<LocalVariableNode>();
				index = local->LocalIndex;
			}
				
			if (index != -1 && IsVariableTrackable(local))
				WriteVariable(local, bbIndex, store->Source);
		}

		TraverseTree(
			mJITContext->Traversal.Space,
			mJITContext->Traversal.Count,
			node, 
			[&](TreeNode*& value) {
				if (!value->Is(NodeKinds::Load))
					return;

				auto load = value->As<LoadNode>();
				auto kind = load->Source->Kind;
				auto index = -1;

				LocalVariableNode* local = nullptr;
				if (kind == NodeKinds::LocalVariable || kind == NodeKinds::Argument)
				{
					local = load->Source->As<LocalVariableNode>();
					index = local->LocalIndex;
				}

				if (index != -1 && IsVariableTrackable(local))
					value = ReadVariable(local, bbIndex);
			});
	};

	//Traverse to find every write or read for local variables marked trackable.
	for (BasicBlock* bbIterator = mTarget; 
		bbIterator != nullptr; 
		bbIterator = bbIterator->Next)
	{
		for (Statement* stmtIterator = bbIterator->Now;
			stmtIterator != nullptr && stmtIterator->Now != nullptr;
			stmtIterator = stmtIterator->Next)
		{
			readWrite(stmtIterator->Now, bbIterator->Index);
		}
		//Flow control value may also involve r/w
		if (bbIterator->BranchConditionValue != nullptr)
			readWrite(bbIterator->BranchConditionValue, bbIterator->Index);
	}
	return mTarget;
}
