#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "IR.h"
#include <vector>
#include <unordered_map>

namespace RTJ::Hex
{
	/// <summary>
	/// Build the SSA form of FIR
	/// </summary>
	class SSABuilder : public IHexJITFlow
	{
		BasicBlock* mTarget = nullptr;
		HexJITContext* mJITContext;

		using DefinitionMap = std::vector<std::unordered_map<Int16, TreeNode*>>;
		DefinitionMap mLocalDefinition;
		DefinitionMap mArgumentDefinition;
	private:
		/// <summary>
		/// Decide which variables should be tracked by SSA. And this affects our node generation
		/// of LoadNode
		/// </summary>
		void DecideSSATrackability();
		bool IsVariableTrackable(LocalVariableNode* local);
		void WriteVariable(LocalVariableNode* local, Int32 blockIndex, TreeNode* value);
		TreeNode* ReadVariable(LocalVariableNode* local, Int32 blockIndex);
		TreeNode* ReadVariableLookUp(LocalVariableNode* local, Int32 blockIndex);
		TreeNode* AddPhiOperands(LocalVariableNode* local, Int32 blockIndex, SSA::PhiNode* phiNode);
		TreeNode* TrySimplifyChoice(SSA::PhiNode* origin, TreeNode* node);
		TreeNode* TryRemoveRedundantPhiNode(SSA::PhiNode* phiNode);
	public:
		SSABuilder(HexJITContext* jitContext);
		virtual BasicBlock* PassThrough() final;
	};
}