#include "SSAOptimizer.h"
#include "..\..\..\Meta\CoreTypes.h"
#include "..\..\OpCodes.h"
#include "..\..\..\Exception\RuntimeException.h"

RTJ::Hex::SSAOptimizer::SSAOptimizer(HexJITContext* context) :
	mJITContext(context),
	mMemory(context->Memory)
{

}

RTJ::Hex::TreeNode* RTJ::Hex::SSAOptimizer::Fold(UnaryArithmeticNode* node)
{
#define EVAL_NEG_CASE(CORE_TYPE) case CoreTypes::CORE_TYPE: \
	evaluatedNode->CORE_TYPE = -constant->CORE_TYPE; \
	break

#define EVAL_NOT_CASE(CORE_TYPE) case CoreTypes::CORE_TYPE: \
	evaluatedNode->CORE_TYPE = ~constant->CORE_TYPE; \
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
				EVAL_NEG_CASE(I1);
				EVAL_NEG_CASE(I2);
				EVAL_NEG_CASE(I4);
				EVAL_NEG_CASE(I8);
				EVAL_NEG_CASE(R4);
				EVAL_NEG_CASE(R8);
			}
			break;
		case OpCodes::Not:
			switch (constant->CoreType)
			{
				EVAL_NOT_CASE(I1);
				EVAL_NOT_CASE(I2);
				EVAL_NOT_CASE(I4);
				EVAL_NOT_CASE(I8);
			}
			break;
		}
		return evaluatedNode;
	}
	else
		return node;

#undef EVAL_CASE
}

RTJ::Hex::TreeNode* RTJ::Hex::SSAOptimizer::Fold(BinaryArithmeticNode* node)
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

		//Should never reach here
		return left ^ right;
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
		//Should never reach here
		return left;
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

RTJ::Hex::TreeNode* RTJ::Hex::SSAOptimizer::Fold(CompareNode* node) {

#define COMPARE_OP_IN_CORE_TYPE(CORE_TYPE, OP) \
	case CoreTypes::CORE_TYPE: \
		evaluatedNode->I1 = !!(left->CORE_TYPE OP right->CORE_TYPE); \
		break

#define COMPARE_OP(OP) \
	switch(left->CoreType) \
	{ \
		COMPARE_OP_IN_CORE_TYPE(I1, OP);\
		COMPARE_OP_IN_CORE_TYPE(I2, OP);\
		COMPARE_OP_IN_CORE_TYPE(I4, OP);\
		COMPARE_OP_IN_CORE_TYPE(I8, OP);\
		COMPARE_OP_IN_CORE_TYPE(R4, OP);\
		COMPARE_OP_IN_CORE_TYPE(R8, OP);\
	} break


	if (node->Left->Is(NodeKinds::Constant) &&
		node->Right->Is(NodeKinds::Constant))
	{
		auto left = node->Left->As<ConstantNode>();
		auto right = node->Right->As<ConstantNode>();
		auto evaluatedNode = new(mMemory) ConstantNode(CoreTypes::I1);
		switch (node->Condition)
		{
		case CmpCondition::EQ:
			COMPARE_OP(== );
		case CmpCondition::GE:
			COMPARE_OP(>= );
		case CmpCondition::GT:
			COMPARE_OP(> );
		case CmpCondition::LE:
			COMPARE_OP(<= );
		case CmpCondition::LT:
			COMPARE_OP(< );
		case CmpCondition::NE:
			COMPARE_OP(!= );
		}
		return evaluatedNode;
	}
	else
		return node;
#undef COMPARE_OP_IN_CORE_TYPE
#undef COMPARE_OP
}

RTJ::Hex::TreeNode* RTJ::Hex::SSAOptimizer::Fold(SSA::ValueUse* node)
{
	auto value = node->Def->Value;
	//Decrement the count
	if (value->Is(NodeKinds::Constant))
		node->Def->Count--;
	return value;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSAOptimizer::Fold(SSA::ValueDef* def)
{
	if (def->Value->Is(NodeKinds::Constant))
		return def->Value;
	else
		return def;
}

RTJ::Hex::TreeNode* RTJ::Hex::SSAOptimizer::Fold(TreeNode* node)
{
	switch (node->Kind)
	{
	case NodeKinds::BinaryArithmetic:
		return Fold(node->As<BinaryArithmeticNode>());
	case NodeKinds::UnaryArithmetic:
		return Fold(node->As<UnaryArithmeticNode>());
	case NodeKinds::Compare:
		return Fold(node->As<CompareNode>());
	case NodeKinds::ValueDef:
		return Fold(node->As<SSA::ValueDef>());
	case NodeKinds::ValueUse:
		return Fold(node->As<SSA::ValueUse>());
	default:
		return node;
	}
}

void RTJ::Hex::SSAOptimizer::FoldConstant(TreeNode*& stmtRoot)
{
	TraverseTreeBottomUp(mJITContext->Traversal.Space, mJITContext->Traversal.Count, stmtRoot,
		[&](TreeNode*& node) { node = Fold(node); });
}

void RTJ::Hex::SSAOptimizer::PruneFlowGraph(BasicBlock* basicBlock)
{
	//Only for conditional now
	if (basicBlock->BranchKind == PPKind::Conditional)
	{
		//Ref
		auto&& conditionValue = basicBlock->BranchConditionValue;
		if (!(conditionValue->Is(NodeKinds::Compare) || conditionValue->Is(NodeKinds::Constant)))
			THROW("Condition expression type mismatch.");
		//Fold constant first
		if (!conditionValue->Is(NodeKinds::Constant))
			FoldConstant(conditionValue);

		if (conditionValue->Is(NodeKinds::Constant))
		{
			//If this is true, then it becomes an unconditional jump
			auto constant = conditionValue->As<ConstantNode>();
			//Use sequential by default
			BasicBlock* target = basicBlock->Next;
			if (!constant->I1)
			{
				target = basicBlock->BranchedBB;
				basicBlock->BranchKind = PPKind::Unconditional;
				//Set branch bb
				basicBlock->BranchedBB = basicBlock->Next;
			}
			//Remove BBIn from target BB
			auto me = std::find(target->BBIn.begin(), target->BBIn.end(), basicBlock);
			if (me != target->BBIn.end())
				target->BBIn.erase(me);
		}		
	}
}

RTJ::Hex::BasicBlock* RTJ::Hex::SSAOptimizer::PassThrough()
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
			FoldConstant(stmtIterator->Now);
		}
		PruneFlowGraph(bbIterator);
	}
	return bbHead;
}
