#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\Memory\SegmentMemory.h"
#include "..\HexJITContext.h"


namespace RTJ::Hex
{
	class SSAOptimizer
	{
		RTMM::SegmentMemory* mMemory;
		HexJITContext* mJITContext;	
		//Fold binary
		TreeNode* FoldBinaryOpConstant(BinaryArithmeticNode* node);
		//Fold unary
		TreeNode* FoldUnaryOpConstant(UnaryArithmeticNode* node);
		//Fold comapre constant
		TreeNode* FoldCompareConstant(CompareNode* node);
		//Call for each root node of stmt
		void FoldConstant(TreeNode*& stmtRoot);
		//Prune the unnecessary BBIns to possibly eliminate dead code
		void PruneFlowGraph(BasicBlock* basicBlock);
	public:
		SSAOptimizer(HexJITContext* context);
		BasicBlock* Optimize();
	};
}