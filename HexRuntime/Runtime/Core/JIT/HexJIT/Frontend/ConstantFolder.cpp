#include "ConstantFolder.h"
#include "..\..\..\Meta\CoreTypes.h"
#include "..\..\OpCodes.h"
#include "..\..\..\Exception\RuntimeException.h"

#define POOL mMemory

namespace RTJ::Hex
{
	ConstantFolder::ConstantFolder(HexJITContext* context) :
		mJITContext(context),
		mMemory(context->Memory)
	{

	}

	TreeNode* ConstantFolder::Fold(UnaryArithmeticNode* node)
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
			auto evaluatedNode = new (POOL) ConstantNode(constant->CoreType);
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
				case CoreTypes::Bool:
					evaluatedNode->Bool = !constant->Bool;
					break;

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

	bool EvalBinary(bool left, bool right, UInt8 opcode) {
		switch (opcode)
		{
		case OpCodes::Or:
			return left || right;
		case OpCodes::And:
			return left && right;
		case OpCodes::Xor:
			return !!(left ^ right);
		default:
			THROW("Unsupported boolean operation");
		}
	}

	template<class T> requires(std::is_integral_v<T>)
		T EvalBinary(T left, T right, UInt8 opcode) {
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
		case OpCodes::Shl:
			return left << right;
		case OpCodes::Shr:
			return left >> right;
		default:
			THROW("Unsupported integer operation");
		}
	}

	template<class T> requires(std::is_floating_point_v<T>)
		T EvalBinary(T left, T right, UInt8 opcode) {
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
			return std::fmod(left, right);
		default:
			THROW("Unsupported float operation");
		}
	}

	TreeNode* ConstantFolder::Fold(BinaryArithmeticNode* node)
	{

#define EVAL_CASE(CORE_TYPE) case CoreTypes::CORE_TYPE: \
	evaluatedNode->CORE_TYPE = EvalBinary(left->CORE_TYPE, right->CORE_TYPE, node->Opcode); \
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
			auto evaluatedNode = new (POOL) ConstantNode(left->CoreType);
			switch (left->CoreType)
			{
				EVAL_CASE(Bool);
				EVAL_CASE(Char);
				EVAL_CASE(I1);
				EVAL_CASE(I2);
				EVAL_CASE(I4);
				EVAL_CASE(I8);
				EVAL_CASE(U1);
				EVAL_CASE(U2);
				EVAL_CASE(U4);
				EVAL_CASE(U8);
				EVAL_CASE(R4);
				EVAL_CASE(R8);
			}
			return evaluatedNode;
		}
		else
			return node;

#undef EVAL_CASE
	}

	TreeNode* ConstantFolder::Fold(CompareNode* node) {

#define COMPARE_OP_IN_CORE_TYPE(CORE_TYPE, OP) \
	case CoreTypes::CORE_TYPE: \
		evaluatedNode->I1 = !!(left->CORE_TYPE OP right->CORE_TYPE); \
		break

#define COMMON_COMPARE_CORE_TYPE(OP) \
		COMPARE_OP_IN_CORE_TYPE(I1, OP);\
		COMPARE_OP_IN_CORE_TYPE(I2, OP);\
		COMPARE_OP_IN_CORE_TYPE(I4, OP);\
		COMPARE_OP_IN_CORE_TYPE(I8, OP);\
		COMPARE_OP_IN_CORE_TYPE(U1, OP);\
		COMPARE_OP_IN_CORE_TYPE(U2, OP);\
		COMPARE_OP_IN_CORE_TYPE(U4, OP);\
		COMPARE_OP_IN_CORE_TYPE(U8, OP);\
		COMPARE_OP_IN_CORE_TYPE(R4, OP);\
		COMPARE_OP_IN_CORE_TYPE(R8, OP);\

#define COMPARE_OP(OP) \
	switch(left->CoreType) \
	{ \
		COMMON_COMPARE_CORE_TYPE(OP) \
	} break

#define COMPARE_EQ \
	switch(left->CoreType) \
	{ \
		COMPARE_OP_IN_CORE_TYPE(Bool, ==);\
		COMPARE_OP_IN_CORE_TYPE(Char, ==);\
		COMMON_COMPARE_CORE_TYPE(==); \
	} break

#define COMPARE_NE \
	switch(left->CoreType) \
	{ \
		COMPARE_OP_IN_CORE_TYPE(Bool, !=);\
		COMPARE_OP_IN_CORE_TYPE(Char, !=);\
		COMMON_COMPARE_CORE_TYPE(!=); \
	} break


		if (node->Left->Is(NodeKinds::Constant) &&
			node->Right->Is(NodeKinds::Constant))
		{
			auto left = node->Left->As<ConstantNode>();
			auto right = node->Right->As<ConstantNode>();
			auto evaluatedNode = new (POOL) ConstantNode(CoreTypes::Bool);
			switch (node->Condition)
			{
			case CmpCondition::EQ:
				COMPARE_EQ;
			case CmpCondition::GE:
				COMPARE_OP(>= );
			case CmpCondition::GT:
				COMPARE_OP(> );
			case CmpCondition::LE:
				COMPARE_OP(<= );
			case CmpCondition::LT:
				COMPARE_OP(< );
			case CmpCondition::NE:
				COMPARE_NE;
			}
			return evaluatedNode;
		}
		else
			return node;
#undef COMPARE_OP_IN_CORE_TYPE
#undef COMMON_COMPARE_CORE_TYPE
#undef COMPARE_OP
#undef COMPARE_EQ
#undef COMPARE_NE
	}

	TreeNode* ConstantFolder::Fold(SSA::ValueUse* node)
	{
		auto value = node->Def->Value;
		//Decrement the count
		if (value->Is(NodeKinds::Constant))
			node->Def->Count--;
		return value;
	}

	TreeNode* ConstantFolder::Fold(SSA::ValueDef* def)
	{
		if (def->Value->Is(NodeKinds::Constant))
			return def->Value;
		else
			return def;
	}

	TreeNode* ConstantFolder::Fold(TreeNode* node)
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

	void ConstantFolder::FoldConstant(TreeNode*& stmtRoot)
	{
		TraverseTreeBottomUp(mJITContext->Traversal.Space, mJITContext->Traversal.Count, stmtRoot,
			[&](TreeNode*& node) { node = Fold(node); });
	}

	BasicBlock* ConstantFolder::PassThrough()
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

			if (bbIterator->BranchConditionValue != nullptr)
				FoldConstant(bbIterator->BranchConditionValue);
		}
		return bbHead;
	}
}
