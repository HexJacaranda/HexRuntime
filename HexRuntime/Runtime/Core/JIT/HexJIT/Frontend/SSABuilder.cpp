#include "SSABuilder.h"
#include "IR.h"

void RTJ::Hex::SSABuilder::WriteVariable(Int16 variableIndex, Int32 blockIndex, TreeNode* value)
{
	mCurrentDefinition[variableIndex][blockIndex] = value;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::ReadVariable(Int16 variableIndex, Int32 blockIndex)
{
	auto ret = mCurrentDefinition[variableIndex][blockIndex];
	if (ret != nullptr)
		return ret;
	return ReadVariableLookUp(variableIndex, blockIndex);
}

RTJ::Hex::TreeNode* RTJ::Hex::SSABuilder::ReadVariableLookUp(Int16 variableIndex, Int32 blockIndex)
{
	BasicBlock* block = mBBs[blockIndex];
	TreeNode* value = nullptr;
	if (block->BBIn.size() == 1)
		value = ReadVariable(variableIndex, block->Index);
	else
	{
		auto phi = new SSA::PhiNode(block);		
		WriteVariable(variableIndex, blockIndex, phi);
		value = AddPhiOperands(variableIndex, blockIndex, phi);
	}
	WriteVariable(variableIndex, blockIndex, value);
	return value;
}

RTJ::Hex::SSA::PhiNode* RTJ::Hex::SSABuilder::AddPhiOperands(Int16 variableIndex, Int32 blockIndex, SSA::PhiNode* phiNode)
{
	BasicBlock* block = mBBs[blockIndex];
	for (auto&& predecessor : block->BBIn)
		phiNode->Choices.push_back(ReadVariable(variableIndex, predecessor->Index));
	return TryRemoveRedundantPhiNode(phiNode);
}

RTJ::Hex::SSA::PhiNode* RTJ::Hex::SSABuilder::TryRemoveRedundantPhiNode(SSA::PhiNode* phiNode)
{
	return nullptr;
}
