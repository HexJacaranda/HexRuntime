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
		JITMemory* mMemory;
		HexJITContext* mJITContext;
		static constexpr Int32 SpaceCount = 1024;
		Int8* mTraversalSpace;
	public:
		SSAOptimizer(HexJITContext* context);
	private:
		/// <summary>
		/// Assuming the
		/// </summary>
		TreeNode* FoldBinaryOpConstant(BinaryArithmeticNode* node);
		void DoConstantFolding(Statement* stmt);
	public:
		BasicBlock* Optimize();
	};
}