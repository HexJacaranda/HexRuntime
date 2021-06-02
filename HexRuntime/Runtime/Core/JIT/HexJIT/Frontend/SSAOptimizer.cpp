#include "SSAOptimizer.h"
#include "..\..\..\Type\CoreTypes.h"
#include "..\..\OpCodes.h"

RTJ::Hex::SSAOptimizer::SSAOptimizer(HexJITContext* context) :
	mJITContext(context),
	mMemory(context->Memory)
{
	mTraversalSpace = new(mMemory) Int8[sizeof(Int8*) * SpaceCount];
}

void RTJ::Hex::SSAOptimizer::DoConstantFolding(Statement* stmt)
{
	TraverseTreeBottomUp(mTraversalSpace, SpaceCount, stmt->Now, [&](TreeNode*& node) {
		switch (node->Kind)
		{
		case NodeKinds::BinaryArithmetic:
		{
			node = FoldBinaryOpConstant(node->As<BinaryArithmeticNode>());
			break;
		}
		case NodeKinds::UnaryArithmetic:
		{

		}
		}
		});
}

RTJ::Hex::TreeNode* RTJ::Hex::SSAOptimizer::FoldBinaryOpConstant(BinaryArithmeticNode* node)
{
	auto eval = [](auto left, auto right, UInt8 opcode) {
		switch (opcode)
		{
		case OpCodes::Add:
			return left + right;
		case OpCodes::Sub:
			return left - right;
		case OpCodes::Mul:
			return left * right;
		case OpCodes::Div:
			return left / right;
		}
	};

#define EVAL_CASE(CORE_TYPE) case CoreTypes::CORE_TYPE: \
	evaluatedNode->CORE_TYPE = eval(left->CORE_TYPE, right->CORE_TYPE, node->Opcode); \
	break

	if (node->Left->Is(NodeKinds::Constant) &&
		node->Right->Is(NodeKinds::Constant))
	{
		auto left = node->Left->As<ConstantNode>();
		auto right = node->Right->As<ConstantNode>();
		auto coreType = left->CoreType;
		//Cannot fold non-primitive constant
		if (!CoreTypes::IsPrimitive(coreType)) 
			return node;
		auto evaluatedNode = new (mMemory) ConstantNode(left->CoreType);
		switch (left->CoreType)
		{
			EVAL_CASE(I1);
			EVAL_CASE(I2);
			EVAL_CASE(I4);
			EVAL_CASE(I8);
			EVAL_CASE(R4);
			EVAL_CASE(R8);
		}
		return evaluatedNode;
	}
	else
		return node;

#undef EVAL_CASE
}

RTJ::Hex::BasicBlock* RTJ::Hex::SSAOptimizer::Optimize()
{
	auto bbHead = mJITContext->BBs[0];

	for (BasicBlock* bbIterator = bbHead;
		bbIterator != nullptr;
		bbIterator = bbIterator->Next)
	{
		for (Statement* stmtIterator = bbIterator->Now;
			stmtIterator != nullptr && stmtIterator->Now != nullptr;
			stmtIterator = stmtIterator->Next)
		{
			DoConstantFolding(stmtIterator);
		}
	}
	return nullptr;
}
