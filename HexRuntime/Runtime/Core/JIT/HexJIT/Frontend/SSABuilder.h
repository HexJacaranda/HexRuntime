#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "IR.h"
#include <vector>

namespace RTJ::Hex
{
	/// <summary>
	/// Build the SSA form of FIR
	/// </summary>
	class SSABuilder
	{
		std::vector<std::vector<TreeNode*>> mCurrentDefinition;
		std::vector<BasicBlock*> mBBs;
	private:
		void WriteVariable(Int16 variableIndex, Int32 blockIndex, TreeNode* value);
		TreeNode* ReadVariable(Int16 variableIndex, Int32 blockIndex);
		TreeNode* ReadVariableLookUp(Int16 variableIndex, Int32 blockIndex);
		SSA::PhiNode* AddPhiOperands(Int16 variableIndex, Int32 blockIndex, SSA::PhiNode* phiNode);
		SSA::PhiNode* TryRemoveRedundantPhiNode(SSA::PhiNode* phiNode);
	public:
	};
}