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

		using DefinitionMap = std::vector<std::unordered_map<Int16, SSA::Use*>>;
		DefinitionMap mLocalDefinition;
		DefinitionMap mArgumentDefinition;
		std::unordered_map<SSA::PhiNode*, SSA::Use*> mPhiUsage;
	private:
		/// <summary>
		/// Decide which variables should be tracked by SSA. And this affects our node generation
		/// of LoadNode
		/// </summary>
		void DecideSSATrackability();
		/// <summary>
		///  Locals or arguments may have not value assigned before, so we will
		///  do the basic initialization for them
		/// </summary>
		void InitializeLocalsAndArguments();
		bool IsVariableTrackable(LocalVariableNode* local);
		/// <summary>
		/// Increment use count if it's a use node
		/// </summary>
		/// <param name="node"></param>
		void CountUse(TreeNode* node);

		/// <summary>
		/// Checke whether it's a Use node with UndefinedValue (Now only argument reading)
		/// </summary>
		/// <param name="node"></param>
		/// <returns></returns>
		static bool IsUseUndefined(TreeNode* node);
		
		TreeNode* TrackPhiUsage(TreeNode* phi);
		void UpdatePhiUsage(SSA::PhiNode* origin, TreeNode* target);

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