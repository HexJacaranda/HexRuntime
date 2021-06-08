#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "..\JITMemory.h"

namespace RTJ::Hex
{
	/// <summary>
	/// Responsible for common optimizations like: 1. constant folding 2. constant propagation
	/// 3. range check 4. nullability check
	/// </summary>
	class SSAOptimizer
	{
		static constexpr Int32 SpaceCount = 1024;
		JITMemory* mMemory;
		HexJITContext* mJITContext;	
		Int8* mTraversalSpace;
		/// <summary>
		/// Fold binary
		/// </summary>
		/// <param name="node"></param>
		/// <returns></returns>
		TreeNode* FoldBinaryOpConstant(BinaryArithmeticNode* node);
		/// <summary>
		/// Fold unary
		/// </summary>
		TreeNode* FoldUnaryOpConstant(UnaryArithmeticNode* node);
		/// <summary>
		/// Call for each root node of stmt
		/// </summary>
		void FoldConstant(TreeNode*& stmtRoot);
		/// <summary>
		/// Prune the unnecessary BBIns to possibly eliminate dead code
		/// </summary>
		void PruneFlowGraph(BasicBlock* basicBlock);
	public:
		SSAOptimizer(HexJITContext* context);
		BasicBlock* Optimize();
	};
}