#include "SSAOptimizer.h"
#include "..\..\..\Type\CoreTypes.h"
#include "..\..\OpCodes.h"

RTJ::Hex::SSAOptimizer::SSAOptimizer(HexJITContext* context) :
	mJITContext(context),
	mMemory(context->Memory)
{
	mTraversalSpace = new(mMemory) Int8[sizeof(Int8*) * SpaceCount];
}

RTJ::Hex::TreeNode* RTJ::Hex::SSAOptimizer::FoldUnaryOpConstant(UnaryArithmeticNode* node)
{
#define EVAL_CASE(CORE_TYPE) case CoreTypes::CORE_TYPE: \
	evaluatedNode->CORE_TYPE = -constant->CORE_TYPE; \
	break

	if (node->Value->Is(NodeKinds::Constant))
	{
		auto constant = node->Value->As<ConstantNode>();
		auto evaluatedNode = new(mMemory) ConstantNode(constant->CoreType);
		switch (node->Opcode)
		{
		case OpCodes::Neg:
			switch (constant->CoreType)
			{
				EVAL_CASE(I1);
				EVAL_CASE(I2);
				EVAL_CASE(I4);
				EVAL_CASE(I8);
				EVAL_CASE(R4);
				EVAL_CASE(R8);
			}
			break;
		case OpCodes::Not:
			evaluatedNode->I1 = !constant->I1;
			break;
		}
	}
	else
		return node;

#undef EVAL_CASE
}

void RTJ::Hex::SSAOptimizer::FoldConstant(Statement* stmt)
{
	TraverseTreeBottomUp(mTraversalSpace, SpaceCount, stmt->Now, 
		[&](TreeNode*& node) 
		{
			switch (node->Kind)
			{
			case NodeKinds::BinaryArithmetic:
			{
				node = FoldBinaryOpConstant(node->As<BinaryArithmeticNode>());
				break;
			}
			case NodeKinds::UnaryArithmetic:
			{
				node = FoldUnaryOpConstant(node->As<UnaryArithmeticNode>());
				break;
			}
			}
		});
}

void RTJ::Hex::SSAOptimizer::PruneFlowGraph(BasicBlock* basicBlock)
{
	
}

RTJ::Hex::TreeNode* RTJ::Hex::SSAOptimizer::FoldBinaryOpConstant(BinaryArithmeticNode* node)
{
	auto integerEval = [](auto left, auto right, UInt8 opcode) 
	{
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
		case OpCodes::Mod:
			return left % right;
		case OpCodes::Or:
			return left | right;
		case OpCodes::And:
			return left & right;
		case OpCodes::Xor:
			return left ^ right;
		}
	};

#define EVAL_INTEGER_CASE(CORE_TYPE) case CoreTypes::CORE_TYPE: \
	evaluatedNode->CORE_TYPE = integerEval(left->CORE_TYPE, right->CORE_TYPE, node->Opcode); \
	break

	auto floatEval = [](auto left, auto right, UInt8 opcode)
	{
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

#define EVAL_FLOAT_CASE(CORE_TYPE) case CoreTypes::CORE_TYPE: \
	evaluatedNode->CORE_TYPE = floatEval(left->CORE_TYPE, right->CORE_TYPE, node->Opcode); \
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
			EVAL_INTEGER_CASE(I1);
			EVAL_INTEGER_CASE(I2);
			EVAL_INTEGER_CASE(I4);
			EVAL_INTEGER_CASE(I8);
			EVAL_FLOAT_CASE(R4);
			EVAL_FLOAT_CASE(R8);
		}
		return evaluatedNode;
	}
	else
		return node;

#undef EVAL_INTEGER_CASE
#undef EVAL_FLOAT_CASE
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
			FoldConstant(stmtIterator);
		}
		PruneFlowGraph(bbIterator);
	}
	return bbHead;
}
