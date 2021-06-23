#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "IR.h"

namespace RTJ::Hex
{
	class EvaluationStack
	{
		/// <summary>
		/// Evaluation stack for tree construction
		/// </summary>
		TreeNode** mEvaluationStack = nullptr;
		/// <summary>
		/// Indicator
		/// </summary>
		Int32 mIndex = 0;
		/// <summary>
		/// Indicate max depth of evaluation stack
		/// </summary>
		Int32 mEvaluationStackDepth;
	public:
		EvaluationStack(Int8* space, Int32 upper);
		~EvaluationStack();
	public:
		TreeNode* Push(TreeNode*);
		TreeNode* Pop();
		TreeNode* Top()const;
		bool IsBalanced();
	};
}
