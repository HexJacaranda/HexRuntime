#include "Transformer.h"
#include "..\..\OpCodes.h"
#include "..\..\..\..\Utility.h"
#include "..\..\..\Exception\RuntimeException.h"
#include "..\..\..\Meta\MetaManager.h"
#include "..\..\..\Meta\MethodDescriptor.h"
#include <unordered_map>
#include <assert.h>

#define POOL (mJITContext->Memory)


ForcedInline RT::Int32 RTJ::Hex::ILTransformer::GetOffset() const
{
	return mCodePtr - mILMD->IL;
}

ForcedInline RT::Int32 RTJ::Hex::ILTransformer::GetPreviousOffset() const
{
	return mPreviousCodePtr - mILMD->IL;
}

ForcedInline RTJ::JITContext* RTJ::Hex::ILTransformer::GetRawContext() const
{
	return mJITContext->Context;
}

ForcedInline RTM::AssemblyContext* RTJ::Hex::ILTransformer::GetAssembly() const
{
	return GetRawContext()->Assembly;
}

ForcedInline void RTJ::Hex::ILTransformer::DecodeInstruction(UInt8& opcode)
{
	mPreviousCodePtr = mCodePtr;
	opcode = *mCodePtr;
	mCodePtr++;
	if (mCodePtr > mCodePtrBound)
		THROW("Malformed IL");
}

RTJ::Hex::CallNode* RTJ::Hex::ILTransformer::GenerateCall()
{
	//Read method reference tokenas
	auto methodRef = ReadAs<MDToken>();
	auto method = Meta::MetaData->GetMethodFromToken(GetAssembly(), methodRef);
	auto returnType = method->GetReturnType();

	CallNode* ret = nullptr;
	auto argumentsCount = method->GetArguments().Count;
	if (argumentsCount <= 1)
	{
		if (argumentsCount == 0)
			ret = new (POOL) CallNode(method, nullptr, argumentsCount);
		else
			ret = new (POOL) CallNode(method, Pop());
	}
	else
	{
		TreeNode** arguments = new (POOL) TreeNode * [argumentsCount];
		for (int i = 0; i < argumentsCount; ++i)
			arguments[i] = Pop();

		ret = new (POOL) CallNode(method, arguments, argumentsCount);
	}

	//Set node type info
	ret->TypeInfo = returnType;
	return ret;
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateLoadLocalVariable(AccessMode mode)
{
	auto localIndex = ReadAs<Int16>();
	auto&& locals = GetRawContext()->MethDescriptor->GetLocalVariables();
	auto localType = locals[localIndex].GetType();

	auto local = new (POOL) LocalVariableNode(localIndex);
	local->SetType(localType);

	if (localIndex >= locals.Count)
		THROW("Local variable index out of range.");

	//Different node type if this is a address-taken load
	auto retType = local->TypeInfo;
	if (mode == AccessMode::Address)
		retType = Meta::MetaData->InstantiateRefType(retType);

	//Keep uniformity for convenience of traversal in SSA building
	return (new (POOL) LoadNode(mode, local))
		->SetType(retType);
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateLoadArgument(AccessMode mode)
{
	auto localIndex = ReadAs<Int16>();
	auto&& locals = GetRawContext()->MethDescriptor->GetArguments();
	auto localType = locals[localIndex].GetType();

	auto local = new (POOL) ArgumentNode(localIndex);
	local->SetType(localType);

	if (localIndex >= locals.Count)
		THROW("Argument index out of range.");

	//Different node type if this is a address-taken load
	auto retType = local->TypeInfo;
	if (mode == AccessMode::Address)
		retType = Meta::MetaData->InstantiateRefType(retType);

	//Keep uniformity for convenience of traversal in SSA building
	return (new (POOL) LoadNode(mode, local))
		->SetType(retType);
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateLoadField(AccessMode mode)
{
	TreeNode* field = nullptr;
	auto fieldToken = ReadAs<MDToken>();
	auto&& fieldDescriptor = RTM::MetaData->GetFieldFromToken(GetAssembly(), fieldToken);

	if (fieldDescriptor->IsStatic())
		field = new (POOL) StaticFieldNode(fieldDescriptor);
	else
		field = new (POOL) InstanceFieldNode(fieldDescriptor, Pop());

	field->TypeInfo = fieldDescriptor->GetType();

	//Different node type if this is a address-taken load
	auto retType = field->TypeInfo;
	if (mode == AccessMode::Address)
		retType = Meta::MetaData->InstantiateRefType(retType);

	return (new (POOL) LoadNode(mode, field))
		->SetType(retType);
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateLoadArrayElement(AccessMode mode)
{
	auto index = Pop();
	auto array = Pop();

	auto elementType = array->TypeInfo->GetFirstTypeArgument();
	auto arrayElement = (new (POOL) ArrayElementNode(array, index))
		->SetType(elementType);

	//Different node type if this is a address-taken load
	auto retType = elementType;
	if (mode == AccessMode::Address)
		retType = Meta::MetaData->InstantiateRefType(retType);

	return (new (POOL) LoadNode(mode, arrayElement))
		->SetType(retType);
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateLoadString()
{
	auto stringToken = ReadAs<UInt32>();
	auto ret = new (POOL) ConstantNode(CoreTypes::String);
	ret->StringToken = stringToken;
	ret->TypeInfo = Meta::MetaData->GetIntrinsicTypeFromCoreType(CoreTypes::String);
	return ret;
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateLoadIndirectly()
{
	auto origin = Pop();
	
	auto type = origin->TypeInfo;
	if (type->GetCoreType() != CoreTypes::InteriorRef)
		THROW("Only interior<> supported");

	auto contentType = type->GetFirstTypeArgument();

	return (new (POOL) LoadNode(AccessMode::Content, origin))
		->SetType(contentType);
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateLoadConstant()
{
	UInt8 coreType = ReadAs<UInt8>();
	Int32 coreTypeSize = CoreTypes::GetCoreTypeSize(coreType);
	auto ret = new (POOL) ConstantNode(coreType);
	switch (coreTypeSize)
	{
	case 1: ret->I1 = ReadAs<Int8>(); break;
	case 2: ret->I2 = ReadAs<Int16>(); break;
	case 4: ret->I4 = ReadAs<Int32>(); break;
	case 8: ret->I8 = ReadAs<Int64>(); break;
	case -1:
		THROW("Unknown constant size.");
		//Should never reach here.
		break;
	}
	//Set type info
	ret->TypeInfo = Meta::MetaData->GetIntrinsicTypeFromCoreType(coreType);
	return ret;
}

RTJ::Hex::StoreNode* RTJ::Hex::ILTransformer::GenerateStoreField()
{
	TreeNode* value = Pop();
	MDToken fieldToken = ReadAs<MDToken>();
	auto field = RTM::MetaData->GetFieldFromToken(GetAssembly(), fieldToken);
	auto fieldType = field->GetType();

	if (!fieldType->IsAssignableFrom(value->TypeInfo))
		THROW("Type check failed.");

	TreeNode* destination = nullptr;
	if (field->IsStatic())
	{
		//Static field store
		destination = new (POOL) StaticFieldNode(field);
	}
	else
	{
		//Instance field store
		auto object = Pop();
		destination = new (POOL) InstanceFieldNode(field, object);
	}
	return new (POOL) StoreNode(destination->SetType(fieldType), value);
}

RTJ::Hex::StoreNode* RTJ::Hex::ILTransformer::GenerateStoreArgument()
{
	TreeNode* value = Pop();

	auto argumentIndex = ReadAs<Int16>();
	auto arguments = GetRawContext()->MethDescriptor->GetArguments();
	auto type = arguments[argumentIndex].GetType();

	if (!value->CheckEquivalentWith(type))
		THROW("Type check failed.");

	auto argument = new (POOL) ArgumentNode(argumentIndex);
	return new (POOL) StoreNode(argument->SetType(type), value);
}

RTJ::Hex::StoreNode* RTJ::Hex::ILTransformer::GenerateStoreLocal()
{
	TreeNode* value = Pop();
	auto localIndex = ReadAs<Int16>();
	auto type = Meta::MetaData->GetTypeFromToken(
		GetAssembly(),
		mILMD->LocalVariables[localIndex].TypeRefToken);

	if (!type->IsAssignableFrom(value->TypeInfo))
		THROW("Type check failed.");

	auto local = new (POOL) LocalVariableNode(localIndex);
	return new (POOL) StoreNode(local->SetType(type), value);
}

RTJ::Hex::StoreNode* RTJ::Hex::ILTransformer::GenerateStoreArrayElement()
{
	auto value = Pop();
	auto index = Pop();
	auto array = Pop();

	auto elementNode = new (POOL) ArrayElementNode(array, index);
	return new (POOL) StoreNode(
		elementNode->SetType(array->TypeInfo->GetTypeArguments()[0]),
		value);
}

RTJ::Hex::StoreNode* RTJ::Hex::ILTransformer::GenerateStoreToAddress()
{
	auto value = Pop();
	auto address = Pop();
	return new (POOL) StoreNode(address, value, AccessMode::Content);
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateNew()
{
	//Read method reference tokenas
	auto methodRef = ReadAs<MDToken>();
	auto method = Meta::MetaData->GetMethodFromToken(GetAssembly(), methodRef);
	auto type = method->GetReturnType();

	auto argumentsCount = method->GetArguments().Count;
	if (argumentsCount <= 1)
	{
		if (argumentsCount == 0)
			return (new (POOL) NewNode(method, nullptr, argumentsCount))->SetType(type);
		return (new (POOL) NewNode(method, Pop()))->SetType(type);
	}
	else
	{
		TreeNode** arguments = new (POOL) TreeNode * [argumentsCount];
		for (int i = 0; i < argumentsCount; ++i)
			arguments[i] = Pop();

		return (new (POOL) NewNode(method, arguments, argumentsCount))->SetType(type);
	}
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateNewArray()
{
	Int32 dimensionCount = ReadAs<Int32>();
	//Read type reference token
	auto typeRef = ReadAs<MDToken>();
	auto elementType = Meta::MetaData->GetTypeFromToken(GetAssembly(), typeRef);
	auto arrayType = Meta::MetaData->InstantiateArrayType(elementType);
	if (dimensionCount == 1)
	{
		return (new (POOL) NewArrayNode(elementType, Pop()))
			->SetType(arrayType);
	}
	else
	{
		TreeNode** dimensions = new (POOL) TreeNode * [dimensionCount];
		for (int i = 0; i < dimensionCount; ++i)
			dimensions[i] = Pop();
		return (new (POOL) NewArrayNode(elementType, dimensions, dimensionCount))
			->SetType(arrayType);
	}
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateCompare()
{
	auto right = Pop();
	auto left = Pop();

	if (!left->CheckEquivalentWith(right))
		THROW("Type check failed.");

	auto ret = new (POOL) CompareNode(ReadAs<UInt8>(), left, right);
	ret->TypeInfo = Meta::MetaData->GetIntrinsicTypeFromCoreType(CoreTypes::Bool);

	return ret;
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateDuplicate()
{
	return mEvalStack.Top();
}

void RTJ::Hex::ILTransformer::GenerateReturn(BasicBlockPartitionPoint*& partitions)
{
	TreeNode* ret = nullptr;
	auto returnType = GetRawContext()->MethDescriptor->GetReturnType();
	if (returnType != nullptr)
	{
		ret = Pop();
		if (!returnType->IsAssignableFrom(ret->TypeInfo))
			THROW("Incompatible conversion when returning");	
	}
	auto currentPoint = new (POOL) BasicBlockPartitionPoint(PPKind::Ret, GetOffset(), ret);
	LinkedList::AppendOneWayOrdered(partitions, currentPoint,
		[&](BasicBlockPartitionPoint* x, InsertOption& option) {
			if (currentPoint->ILOffset == x->ILOffset && x->Kind == PPKind::Target)
				option = InsertOption::Before;
			return currentPoint->ILOffset <= x->ILOffset;
	});
}

RTJ::Hex::BinaryArithmeticNode* RTJ::Hex::ILTransformer::GenerateBinaryArithmetic(UInt8 opcode)
{
	auto right = Pop();
	auto left = Pop();
	if (left->TypeInfo != right->TypeInfo)
		THROW("Unconsistency of binary operators");

	auto node = new (POOL) BinaryArithmeticNode(left, right, opcode);
	node->TypeInfo = left->TypeInfo;
	return node;
}

RTJ::Hex::UnaryArithmeticNode* RTJ::Hex::ILTransformer::GenerateUnaryArtithmetic(UInt8 opcode)
{
	auto value = Pop();
	auto node = new (POOL) UnaryArithmeticNode(value, opcode);
	node->TypeInfo = value->TypeInfo;
	return node;
}

RTJ::Hex::ConvertNode* RTJ::Hex::ILTransformer::GenerateConvert()
{
	auto value = Pop();
	UInt8 to = ReadAs<UInt8>();
	if (CoreTypes::IsValidCoreType(to))
		THROW("Invalid cast between primitive types");

	auto ret = new (POOL) ConvertNode(value, to);
	ret->TypeInfo = Meta::MetaData->GetIntrinsicTypeFromCoreType(to);
	
	return ret;
}

RTJ::Hex::CastNode* RTJ::Hex::ILTransformer::GenerateCast()
{
	auto value = Pop();
	MDToken typeRef = ReadAs<MDToken>();
	auto type = Meta::MetaData->GetTypeFromToken(GetAssembly(), typeRef);

	if (type->IsStruct())
		THROW("Invalid type of cast");

	auto node = new (POOL) CastNode(value);
	node->TypeInfo = type;
	return node;
}

RTJ::Hex::UnBoxNode* RTJ::Hex::ILTransformer::GenerateUnBox()
{
	auto value = Pop();
	MDToken typeRef = ReadAs<MDToken>();
	auto targetType = Meta::MetaData->GetTypeFromToken(GetAssembly(), typeRef);

	if (!targetType->IsStruct())
		THROW("Invalid type of unbox");

	auto node = new (POOL) UnBoxNode(value);
	node->TypeInfo = targetType;
	return node;
}

RTJ::Hex::BoxNode* RTJ::Hex::ILTransformer::GenerateBox()
{
	auto value = Pop();
	auto objectType = Meta::MetaData->GetIntrinsicTypeFromCoreType(CoreTypes::Object);

	auto node = new (POOL) BoxNode(value);
	node->TypeInfo = objectType;
	return node;
}

void RTJ::Hex::ILTransformer::Push(TreeNode* value)
{
	mEvalStack.Push(value);
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::Pop()
{
	return mEvalStack.Pop();
}


void RTJ::Hex::ILTransformer::GenerateJccPP(BasicBlockPartitionPoint*& partitions)
{
	auto value = Pop();
	auto jccOffset = ReadAs<Int32>();
	auto currentPoint = new (POOL) BasicBlockPartitionPoint(PPKind::Conditional, GetOffset(), value);
	currentPoint->TargetILOffset = jccOffset;

	auto branchedPoint = new (POOL) BasicBlockPartitionPoint(PPKind::Target, jccOffset, nullptr);
	//Append current point into list
	LinkedList::AppendOneWayOrdered(partitions, currentPoint,
		[&](BasicBlockPartitionPoint* x, InsertOption& option) {
			if (currentPoint->ILOffset == x->ILOffset && x->Kind == PPKind::Target)
				option = InsertOption::Before;
			return currentPoint->ILOffset <= x->ILOffset;
		});

	//Append branched to list
	LinkedList::AppendOneWayOrdered(partitions, branchedPoint,
		[&](BasicBlockPartitionPoint* x, InsertOption& option) {	
			if (branchedPoint->ILOffset == x->ILOffset && x->Kind != PPKind::Target)
				option = InsertOption::After;
			else
				option = InsertOption::Before;
			return branchedPoint->ILOffset <= x->ILOffset;
		});
}

void RTJ::Hex::ILTransformer::GenerateJmpPP(BasicBlockPartitionPoint*& partitions)
{
	auto jmpOffset = ReadAs<Int32>();
	auto currentPoint = new (POOL) BasicBlockPartitionPoint(PPKind::Unconditional, GetOffset(), nullptr);
	currentPoint->TargetILOffset = jmpOffset;

	auto branchedPoint = new (POOL) BasicBlockPartitionPoint(PPKind::Target, jmpOffset, nullptr);
	//Append current point into list
	LinkedList::AppendOneWayOrdered(partitions, currentPoint,
		[&](BasicBlockPartitionPoint* x, InsertOption& option) {
			if (currentPoint->ILOffset == x->ILOffset && x->Kind == PPKind::Target)
				option = InsertOption::Before;
			return currentPoint->ILOffset <= x->ILOffset;
		});

	//Append branched to list
	LinkedList::AppendOneWayOrdered(partitions, branchedPoint,
		[&](BasicBlockPartitionPoint* x, InsertOption& option) {
			if (branchedPoint->ILOffset == x->ILOffset && x->Kind != PPKind::Target)
				option = InsertOption::After;
			else
				option = InsertOption::Before;
			return branchedPoint->ILOffset <= x->ILOffset;
		});
}

RTJ::Hex::Statement* RTJ::Hex::ILTransformer::TryGenerateStatement(TreeNode* value, Int32 beginOffset, bool isBalancedCritical)
{
	//Is eval stack already balanced?
	if (mEvalStack.IsBalanced())
		return new (POOL) Statement(value, beginOffset, GetOffset());
	else
	{
		if (!isBalancedCritical)
			return nullptr;
		//Should never reach here, malformed IL
		THROW("Malformed IL.");
	}
}

RTJ::Hex::Statement* RTJ::Hex::ILTransformer::TransformToUnpartitionedStatements(BasicBlockPartitionPoint*& partitions)
{
	Statement* stmtHead = nullptr;
	Statement* stmtPrevious = nullptr;
	Statement* stmtCurrent = nullptr;

#define IL_TRY_GEN_STMT_CRITICAL(OP, CRITICAL) auto node = OP; \
							stmtCurrent = TryGenerateStatement(node, \
																stmtPrevious == nullptr ? 0 : stmtPrevious->EndOffset,\
																CRITICAL); \
							if (stmtCurrent == nullptr) { Push(node); } \
							else LinkedList::AppendTwoWay(stmtHead, stmtPrevious, stmtCurrent)

	//Try generate statement for operation and thread it if possible
#define IL_TRY_GEN_STMT(OP) IL_TRY_GEN_STMT_CRITICAL(OP, false)

	UInt8 instruction = OpCodes::Nop;
	while (mCodePtr < mCodePtrBound)
	{
		DecodeInstruction(instruction);
		switch (instruction)
		{
		//------------------------------------------------------
		//control flow related instructions
		//------------------------------------------------------
		case OpCodes::Call:
		case OpCodes::CallVirt:
		{
			auto callNode = GenerateCall();
			if (callNode->Method->GetReturnType() == nullptr)
			{
				IL_TRY_GEN_STMT_CRITICAL(callNode, true);
			}
			else
				Push(callNode);
			break;
		}
		case OpCodes::Ret:
		{
			GenerateReturn(partitions);
			IL_TRY_GEN_STMT_CRITICAL(nullptr, true);
			break;
		}

		case OpCodes::Jcc:
		{
			GenerateJccPP(partitions);
			IL_TRY_GEN_STMT_CRITICAL(nullptr, true);
			break;
		}
		case OpCodes::Jmp:
		{
			GenerateJmpPP(partitions);
			IL_TRY_GEN_STMT_CRITICAL(nullptr, true);
			break;
		}


		//------------------------------------------------------
		//Store load related instructions
		//------------------------------------------------------

		case OpCodes::StArg:
		{
			IL_TRY_GEN_STMT(GenerateStoreArgument());
			break;
		}
		case OpCodes::StElem:
		{
			IL_TRY_GEN_STMT(GenerateStoreArrayElement());
			break;
		}
		case OpCodes::StFld:
		{
			IL_TRY_GEN_STMT(GenerateStoreField());
			break;
		}
		case OpCodes::StLoc:
		{
			IL_TRY_GEN_STMT(GenerateStoreLocal());
			break;
		}
		case OpCodes::StTA:
		{
			IL_TRY_GEN_STMT(GenerateStoreToAddress());
			break;
		}

		case OpCodes::LdArg:
			Push(GenerateLoadArgument(AccessMode::Value));
			break;
		case OpCodes::LdArgA:
			Push(GenerateLoadArgument(AccessMode::Address));
			break;
		case OpCodes::LdLoc:
			Push(GenerateLoadLocalVariable(AccessMode::Value));
			break;
		case OpCodes::LdLocA:
			Push(GenerateLoadLocalVariable(AccessMode::Address));
			break;
		case OpCodes::LdElem:
			Push(GenerateLoadArrayElement(AccessMode::Value));
			break;
		case OpCodes::LdElemA:
			Push(GenerateLoadArrayElement(AccessMode::Address));
			break;
		case OpCodes::LdFld:
			Push(GenerateLoadField(AccessMode::Value));
			break;
		case OpCodes::LdFldA:
			Push(GenerateLoadField(AccessMode::Address));
			break;
		case OpCodes::LdStr:
			Push(GenerateLoadString());
			break;
		case OpCodes::LdC:
			Push(GenerateLoadConstant());
			break;
		case OpCodes::LdNull:
			Push(&NullNode::Instance());
			break;
		case OpCodes::LdInd:
			Push(GenerateLoadIndirectly());
			break;

		//---------------------------------------------------
		//Binary arithmetic instructions
		//---------------------------------------------------

		case OpCodes::Add:
		case OpCodes::Sub:
		case OpCodes::Mul:
		case OpCodes::Div:
		case OpCodes::Mod:
		case OpCodes::And:
		case OpCodes::Or:
		case OpCodes::Xor:
			Push(GenerateBinaryArithmetic(instruction));
			break;

		//---------------------------------------------------
		//Unary arithmetic instructions
		//---------------------------------------------------

		case OpCodes::Not:
		case OpCodes::Neg:
			Push(GenerateUnaryArtithmetic(instruction));
			break;

		//---------------------------------------------------
		//Convert instruction
		//---------------------------------------------------

		case OpCodes::Conv:
			Push(GenerateConvert());
			break;

		//---------------------------------------------------
		//Cast instruction
		//---------------------------------------------------

		case OpCodes::Cast:
			Push(GenerateCast());
			break;

		case OpCodes::Box:
			Push(GenerateBox());
			break;

		case OpCodes::UnBox:
			Push(GenerateUnBox());
			break;

		//------------------------------------------------------
		//Special instructions
		//------------------------------------------------------

		case OpCodes::Dup:
			Push(GenerateDuplicate());
			break;
		case OpCodes::Pop:
		{
			// Pop is a special instruction that may be introduced in scenario
			// where the result of expression is discarded (usually method call with return). 
			// So there is no need for a real 'PopNode' to exist. We will just treat this as
			// a way to balance eval stack.

			// In another word, pop means eval stack must be balanced after this operation
			// and this does impose limitations on some special and unregulated IL arrangements
			// For example:
			// ------------------
			// New System::String
			// New System::Object
			// Pop
			// Pop
			// Ret
			// ------------------
			// is not allowed.

			IL_TRY_GEN_STMT_CRITICAL(Pop(), true);
			break;
		}

		case OpCodes::Cmp:
			Push(GenerateCompare());
			break;

		//------------------------------------------------------
		//GC related opcodes.
		//------------------------------------------------------
		case OpCodes::New:
			Push(GenerateNew());
			break;
		case OpCodes::NewArr:
			Push(GenerateNewArray());
			break;

		default:
			//You should never reach here
			THROW("Unrecognized IL Opcode.");
			break;
		}
	}
	if (!mEvalStack.IsBalanced())
	{
		//Malformed IL
		THROW("Malformed IL.");
	}
	return stmtHead;

#undef IL_TRY_GEN_STMT
#undef IL_TRY_GEN_STMT_CRITICAL
}

RTJ::Hex::BasicBlock* RTJ::Hex::ILTransformer::PartitionToBB(Statement* unpartitionedStmt, BasicBlockPartitionPoint* partitions)
{
	//If no partition points, return directly
	if (partitions == nullptr || unpartitionedStmt == nullptr)
	{
		auto ret = POOL->New<BasicBlock>();
		ret->Now = unpartitionedStmt;
		return ret;
	}
	//Notice: Partitions are ordered by IL offset

	std::unordered_map<Int32, BasicBlock*> basicBlockMap;
	auto getBBFromMap = [&](Int32 offset) {
		auto value = basicBlockMap.find(offset);
		if (value != basicBlockMap.end())
			return value->second;
		else
			return basicBlockMap[offset] = POOL->New<BasicBlock>();
	};
	BasicBlock* basicBlockHead = nullptr;
	BasicBlock* basicBlockPrevious = nullptr;
	BasicBlock* basicBlockCurrent = nullptr;

	Statement* beginOfStmts = unpartitionedStmt;
	Int32 basicBlockIndex = 0;

	/*A BB may be described by two parts: Target(s) and its ending control flow.
	* Target PP may be multiple and they should be of the same offset (BB1).
	* May contain no PP but be partitioned by PPs from other BBs (BB2) 
	* There may not be any leading target for BB (BB4).
	* And there may not be any ending control flow (BB3).
	* A comprehensive case: (BB1: Target -> Target -> Jcc) -> (BB2:) -> (BB3: Target) -> (BB4: Jmp)
	*/

	auto partitionPoint = partitions;
	while (partitionPoint != nullptr)
	{		
		auto basicBlockBeginOffset = beginOfStmts->ILOffset;
		//Get the offset of basic block according to the beginning stmt
		basicBlockCurrent = getBBFromMap(basicBlockBeginOffset);
		//Set basic block index
		basicBlockCurrent->Index = basicBlockIndex;
		basicBlockIndex++;
		//Add index to BB mapping
		mJITContext->BBs.push_back(basicBlockCurrent);

		//Firstly we will introduce all the features for basic block.	
		//Partition points with same offset.
		auto ppOfSameOffset = partitionPoint;
		auto ilOffset = basicBlockBeginOffset;

		//Set the properties of current BB
 
		/*Ignore leading PP(s) of current BB
		* But carefully consider the scenario of BB2 given in example, the offset of current stmt is
		* the one to trust. Otherwise you may mistakenly consume target PPs of the next BB (BB3 in example).
		*/
		while (ppOfSameOffset != nullptr &&
			ppOfSameOffset->ILOffset == basicBlockBeginOffset &&
			ppOfSameOffset->Kind == PPKind::Target)
		{
			ppOfSameOffset = ppOfSameOffset->Next;
		}
		//If not null, update the il offset for ending control flow
		if (ppOfSameOffset != nullptr)
			ilOffset = ppOfSameOffset->ILOffset;
		
		/* For this variable, simply imagine the boundary of two BBs. (Control Flow PP as 'A') -> | -> (Target PP as 'B') 
		* the existence of boundary should satisfy that 
		* 1. there should be at least one PP.
		* 2. if there are two, they should share the same offset as the start offset of next BB
		*/ 
		Int32 nextBBStartOffset = -1;
		//Should not be target PP (Only one ending flow)		
		if (ppOfSameOffset != nullptr &&
			ppOfSameOffset->ILOffset == ilOffset &&
			ppOfSameOffset->Kind != PPKind::Target)
		{
			switch (ppOfSameOffset->Kind)
			{
			case PPKind::Conditional:
				//Record condition value
				basicBlockCurrent->BranchConditionValue = ppOfSameOffset->Value;
				//Branched BB
				basicBlockCurrent->BranchedBB = getBBFromMap(ppOfSameOffset->TargetILOffset);
				//Set branch kind
				basicBlockCurrent->BranchKind = ppOfSameOffset->Kind;
				//Add current BB to BBIn
				basicBlockCurrent->BranchedBB->BBIn.push_back(basicBlockCurrent);
				break;
			case PPKind::Unconditional:
				//Branched BB
				basicBlockCurrent->BranchedBB = getBBFromMap(ppOfSameOffset->TargetILOffset);
				//Set branch kind
				basicBlockCurrent->BranchKind = ppOfSameOffset->Kind;
				//Add current BB to BBIn
				basicBlockCurrent->BranchedBB->BBIn.push_back(basicBlockCurrent);
				break;
			case PPKind::Ret:
				basicBlockCurrent->BranchConditionValue = ppOfSameOffset->Value;
				//Set branch kind
				basicBlockCurrent->BranchKind = ppOfSameOffset->Kind;
				break;
			default:
				THROW("Unknown PP kind");
			}
			//Record the next bb start offset
			nextBBStartOffset = ppOfSameOffset->ILOffset;
			ppOfSameOffset = ppOfSameOffset->Next;
		}

		if (ppOfSameOffset == nullptr)
		{
			//No more partitions available

			//Hang the rest ones to current basic block.
			basicBlockCurrent->Now = beginOfStmts;
		}
		else
		{
			if (nextBBStartOffset == -1)
			{
				if (ppOfSameOffset->Kind != PPKind::Target)
					THROW("Illegal PP sequence");
				nextBBStartOffset = ppOfSameOffset->ILOffset;
			}

			auto iteratorOfStmt = beginOfStmts->Next;
			auto previousOfIterator = beginOfStmts;
			//Traverse until we meet the next stmt belonging to another basic block.
			while (iteratorOfStmt != nullptr &&
				iteratorOfStmt->ILOffset < nextBBStartOffset)
			{
				previousOfIterator = iteratorOfStmt;
				iteratorOfStmt = iteratorOfStmt->Next;
			} 

			//Break the connection
			previousOfIterator->Next = nullptr;
			beginOfStmts->Prev = nullptr;
			if (iteratorOfStmt != nullptr) 
				iteratorOfStmt->Prev = nullptr;

			//Hang it to basic block.
			basicBlockCurrent->Now = beginOfStmts;

			//Update the location where we should start next time.
			beginOfStmts = iteratorOfStmt;
		}
		
		//Append to linked list will change the previous BB to current
		if (basicBlockPrevious != nullptr && (
			basicBlockPrevious->BranchKind == PPKind::Conditional ||
			basicBlockPrevious->BranchKind == PPKind::Sequential))
			//Logically sequential to the previous basic	
			basicBlockCurrent->BBIn.push_back(basicBlockPrevious);

		//Then we append this to our list.
		LinkedList::AppendTwoWay(basicBlockHead, basicBlockPrevious, basicBlockCurrent);

		//Update partition point to next one of new basic block.
		partitionPoint = ppOfSameOffset;
	}
	return basicBlockHead;
}

RTJ::Hex::ILTransformer::ILTransformer(HexJITContext* context) :
	mJITContext(context),
	mEvalStack(context->Traversal.Space, context->Traversal.Count) 
{
	mILMD = mJITContext->Context->MethDescriptor->GetIL();
	mCodePtr = mPreviousCodePtr = mILMD->IL;
	mCodePtrBound = mCodePtr + mILMD->CodeLength;
}

RTJ::Hex::BasicBlock* RTJ::Hex::ILTransformer::PassThrough()
{
	BasicBlockPartitionPoint* partitions = nullptr;
	Statement* stmts = TransformToUnpartitionedStatements(partitions);
	return PartitionToBB(stmts, partitions);
}

#undef POOL

