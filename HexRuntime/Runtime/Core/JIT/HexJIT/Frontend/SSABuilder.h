#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "IR.h"
#include <vector>
#include <unordered_map>

namespace RTJ::Hex
{
	/// <summary>
	/// Build the SSA form of FIR
	/// </summary>
	class SSABuilder
	{
		BasicBlock* mTarget = nullptr;
		HexJITContext* mJITContext;
		std::vector<std::unordered_map<Int16, TreeNode*>> mLocalDefinition;
		std::vector<std::unordered_map<Int16, TreeNode*>> mArgumentDefinition;
	private:
		/// <summary>
		/// Decide which variables should be tracked by SSA. And this affects our node generation
		/// of LoadNode
		/// </summary>
		void DecideSSATrackability();
		bool IsVariableTrackable(NodeKinds kind, Int16 variableIndex);
		void WriteVariable(NodeKinds kind, Int16 variableIndex, Int32 blockIndex, TreeNode* value);
		TreeNode* ReadVariable(NodeKinds kind, Int16 variableIndex, Int32 blockIndex);
		TreeNode* ReadVariableLookUp(NodeKinds kind, Int16 variableIndex, Int32 blockIndex);
		TreeNode* AddPhiOperands(NodeKinds kind, Int16 variableIndex, Int32 blockIndex, SSA::PhiNode* phiNode);
		TreeNode* TrySimplifyChoice(SSA::PhiNode* origin, TreeNode* node);
		TreeNode* TryRemoveRedundantPhiNode(SSA::PhiNode* phiNode);
	public:
		SSABuilder(HexJITContext* jitContext);
		BasicBlock* Build();
	};
}