#include "IR.h"
#include "..\..\Meta\TypeDescriptor.h"
#include "..\OpCodes.h"
#include <iostream>
#include <string>

bool RTJ::Hex::TreeNode::CheckEquivalentWith(TreeNode* target) const
{
	return TypeInfo == target->TypeInfo;
}

bool RTJ::Hex::TreeNode::CheckEquivalentWith(RTM::TypeDescriptor* target) const
{
	return TypeInfo == target;
}

bool RTJ::Hex::TreeNode::CheckUpCastFrom(TreeNode* target) const
{
	return CheckUpCastFrom(target->TypeInfo);
}

bool RTJ::Hex::TreeNode::CheckUpCastFrom(RTM::TypeDescriptor* source) const
{
	if (TypeInfo == nullptr && source == nullptr)
		return true;
	if (TypeInfo == nullptr)
		return false;
	return TypeInfo->IsAssignableFrom(source);
}

void RTJ::Hex::TreeNode::TypeFrom(TreeNode* target)
{
	TypeInfo = target->TypeInfo;
}

RTJ::Hex::TreeNode* RTJ::Hex::TreeNode::SetType(RTM::TypeDescriptor* type)
{
	TypeInfo = type;
	return this;
}

void RTJ::Hex::JITDebug::PrintNode(std::wstringstream& output, std::wstring const& prefix, TreeNode* node, bool isLast)
{
	if (node != nullptr)
	{
		output << prefix;
		output << (isLast ? L"©¸©¤©¤" : L"©À©¤©¤");

		PrintNodeContent(output, node);

		auto nextPrefix = prefix + (isLast ? L"    " : L"©¦   ");
		switch (node->Kind)
		{
			//Binary access
			CASE_BINARY
			{
				BinaryNodeAccessProxy * proxy = (BinaryNodeAccessProxy*)node;
				PrintNode(output, nextPrefix, proxy->First, false);
				PrintNode(output, nextPrefix, proxy->Second, true);
				break;
			}
			//Unary access
			CASE_UNARY
			{
				UnaryNodeAccessProxy * proxy = (UnaryNodeAccessProxy*)node;
				PrintNode(output, nextPrefix, proxy->Value, true);
				break;
			}
			case NodeKinds::MorphedCall:
			{
				auto call = node->As<MorphedCallNode>();
				PrintNode(output, nextPrefix, call->Origin, false);
			}
			//Multiple access 
			CASE_MULTIPLE
			{
				MultipleNodeAccessProxy * proxy = (MultipleNodeAccessProxy*)node;
				ForeachInlined(proxy->Values, proxy->Count,
					[&](TreeNode* node, Int32 index)
					{
						if (index < proxy->Count - 1)
						{
							PrintNode(output, nextPrefix, node, false);
						}
						else
						{
							PrintNode(output, nextPrefix, node, true);
						}
					});
				break;
			}
		}
	}
	else
	{
		output << prefix;
		output << (isLast ? L"©¸©¤©¤" : L"©À©¤©¤") << "empty stmt" << std::endl;
	}
}


void RTJ::Hex::JITDebug::PrintNodeContent(std::wstringstream& output, TreeNode* node)
{
	if (node == nullptr)
	{
		output << L"null" << std::endl;
		return;
	}

	switch (node->Kind)
	{
	case NodeKinds::Load:
	{
		auto load = node->As<LoadNode>();
		output << L"load";
		if (load->LoadType == SLMode::Indirect)
			output << L" (address)";
		output << std::endl;
		break;
	}
	case NodeKinds::Use:
		output << L"use (ssa)" << std::endl;  break;
	case NodeKinds::ValueDef:
		output << L"value def (ssa)" << std::endl; break;
	case NodeKinds::ValueUse:
		output << L"value use (ssa)" << std::endl; break;
	case NodeKinds::Convert:
		output << L"convert" << std::endl; break;
	case NodeKinds::Cast:
		output << L"cast" << std::endl; break;
	case NodeKinds::Box:
		output << L"box" << std::endl; break;
	case NodeKinds::UnBox:
		output << L"unbox" << std::endl; break;
	case NodeKinds::InstanceField:
		output << L"instance field" << std::endl; break;
	case NodeKinds::UnaryArithmetic:
	{
		auto unary = node->As<UnaryArithmeticNode>();
		const wchar_t* opText = nullptr;
		switch (unary->Opcode)
		{
		case OpCodes::Xor:
			opText = L"xor"; break;
		case OpCodes::Not:
			opText = L"xor"; break;
		case OpCodes::Neg:
			opText = L"neg"; break;
		default:
			opText = L"unknown unary operator"; break;
		}
		output << opText << std::endl; break;
	}
	case NodeKinds::Duplicate:
		output << L"dup" << std::endl; break;
	case NodeKinds::Phi:
		output << L"phi (ssa)" << std::endl; break;
	case NodeKinds::OffsetOf:
		output << L"offset of" << std::endl; break;
	case NodeKinds::Store:
		output << L"store" << std::endl; break;
	case NodeKinds::Array:
		output << L"array index" << std::endl; break;
	case NodeKinds::Compare:
	{
		auto cmp = node->As<CompareNode>();
		const wchar_t* opText = nullptr;
		switch (cmp->Condition)
		{
		case CmpCondition::EQ:
			opText = L"=="; break;
		case CmpCondition::NE:
			opText = L"!="; break;
		case CmpCondition::GT:
			opText = L">"; break;
		case CmpCondition::LT:
			opText = L"<"; break;
		case CmpCondition::GE:
			opText = L">="; break;
		case CmpCondition::LE:
			opText = L"<="; break;
		default:
			opText = L"unknown condition"; break;
		};
		output << L"compare " << opText << std::endl; break;
	}
	case NodeKinds::BinaryArithmetic:
	{
		auto binary = node->As<BinaryArithmeticNode>();
		const wchar_t* opText = nullptr;
		switch (binary->Opcode)
		{
		case OpCodes::Add:
			opText = L"add"; break;
		case OpCodes::Sub:
			opText = L"sub"; break;
		case OpCodes::Mul:
			opText = L"mul"; break;
		case OpCodes::Div:
			opText = L"div"; break;
		case OpCodes::And:
			opText = L"and"; break;
		case OpCodes::Or:
			opText = L"or"; break;
		case OpCodes::Xor:
			opText = L"xor"; break;
		default:
			opText = L"unknown binary operator"; break;
		}
		output << opText << std::endl; break;
	}
	case NodeKinds::ArrayOffsetOf:
		output << L"array offset of" << std::endl; break;
	case NodeKinds::Call:
		output << L"call" << std::endl; break;
	case NodeKinds::New:
		output << L"new" << std::endl; break;
	case NodeKinds::NewArray:
		output << L"new array" << std::endl; break;
	case NodeKinds::Null:
		output << L"null-value" << std::endl; break;
	case NodeKinds::UndefinedValue:
		output << L"undefined value (ssa)" << std::endl; break;
	case NodeKinds::Constant:
	{
		auto constant = node->As<ConstantNode>();
		switch (constant->CoreType)
		{
		case CoreTypes::Bool:
			output << (constant->Bool ? L"true" : L"false") << std::endl; break;
		case CoreTypes::Char:
			std::wcout << (wchar_t)constant->I2 << std::endl; break;
		case CoreTypes::I1:
			output << L"0x" << std::hex << constant->I1 << std::endl; break;
		case CoreTypes::U1:
			output << L"0x" << std::hex << constant->U1 << std::endl; break;
		case CoreTypes::I2:
			output << L"0x" << std::hex << constant->I2 << std::endl; break;
		case CoreTypes::U2:
			output << L"0x" << std::hex << constant->U2 << std::endl; break;
		case CoreTypes::I4:
			output << L"0x" << std::hex << constant->I4 << std::endl; break;
		case CoreTypes::U4:
			output << L"0x" << std::hex << constant->U4 << std::endl; break;
		case CoreTypes::I8:
			output << L"0x" << std::hex << constant->I8 << std::endl; break;
		case CoreTypes::U8:
			output << L"0x" << std::hex << constant->U8 << std::endl; break;
		case CoreTypes::R4:
			output << constant->R4 << std::endl; break;
		case CoreTypes::R8:
			output << constant->R8 << std::endl; break;
		case CoreTypes::String:
			output << L"String token " << constant->StringToken << std::endl; break;
		}
		break;
	}
	case NodeKinds::LocalVariable:
	{
		auto local = node->As<LocalVariableNode>();
		output << (local->IsArgument() ? L"argument " : L"local ") << local->GetIndex() << std::endl;
		break;
	}
	case NodeKinds::MorphedCall:
	{
		auto call = node->As<MorphedCallNode>();
		output << "morphed call - " << "0x" << call->NativeEntry << std::endl;
		break;
	}
	default:
		output << L"Unsupported IR" << std::endl;
		break;
	}
}

std::wstring RTJ::Hex::JITDebug::PrintIR(BasicBlock* basicBlock)
{
	std::wstringstream output{};
	while (basicBlock != nullptr)
	{
		output << L"Basic Block - " << basicBlock->Index << L" ";
		if (basicBlock->IsUnreachable())
			output << L"(Unreachable)";
		output << std::endl;

		Statement* stmt = basicBlock->Now;
		while (stmt != nullptr)
		{
			PrintNode(output, L"", stmt->Now, false);
			stmt = stmt->Next;
		}
		
		switch (basicBlock->BranchKind)
		{
		case PPKind::Conditional:
			output << L"jcc " << "BB - " << basicBlock->BranchedBB->Index << std::endl;
			PrintNode(output, L"", basicBlock->BranchConditionValue, false);
			break;
		case PPKind::Unconditional:
			output << L"jmp " << "BB - " << basicBlock->BranchedBB->Index << std::endl;
			break;
		case PPKind::Ret:
			output << L"ret" << std::endl;
			PrintNode(output, L"", basicBlock->BranchConditionValue, false);
		}

		output << std::endl;
		basicBlock = basicBlock->Next;
	}
	return output.str();
}
