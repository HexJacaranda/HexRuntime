#include "Transformer.h"
#include "..\..\OpCodes.h"
#include "..\..\..\..\Utility.h"
#include "..\..\..\Exception\RuntimeException.h"
#include <unordered_map>
#include <assert.h>

ForcedInline RT::Int32 RTJ::Hex::ILTransformer::GetOffset() const
{
	return mCodePtr - mJITContext->Context->CodeSegment;
}

ForcedInline RT::Int32 RTJ::Hex::ILTransformer::GetPreviousOffset() const
{
	return mPreviousCodePtr - mJITContext->Context->CodeSegment;
}

ForcedInline RTJ::JITContext* RTJ::Hex::ILTransformer::GetRawContext() const
{
	return mJITContext->Context;
}

ForcedInline void RTJ::Hex::ILTransformer::DecodeInstruction(_RE_ UInt8& opcode)
{
	mPreviousCodePtr = mCodePtr;
	opcode = *mCodePtr;
	mCodePtr++;
	if (mCodePtr >= mCodePtrBound)
		RTE::Throw(Text("Malformed IL"));

	//Opcode with bae
	switch (opcode)
	{
		case OpCodes::Call:
		case OpCodes::CallVirt:
		case OpCodes::StFld:
		case OpCodes::LdFld:
		case OpCodes::LdFldA:
		case OpCodes::Ret:
		{
			mBaeIn = (mCodePtr[0] & 0xF0) >> 4;
			mBaeOut = mCodePtr[0] & 0x0F;
			mCodePtr++;
			break;
		}
	}
}

RTJ::Hex::CallNode* RTJ::Hex::ILTransformer::GenerateCall()
{
	TreeNode** arguments = nullptr;
	UInt16 argumentsCount = mBaeIn;
	if (mBaeIn > 0)
	{	
		//Overceed 14 arguments, Extended uint16 will be used
		if (mBaeIn == 0xF)
			argumentsCount = ReadAs<UInt16>();
		arguments = new(mMemory) TreeNode * [argumentsCount];
		for (int i = 0; i < argumentsCount; ++i)
			arguments[i] = mEvalStack.Pop();
	}
	//Read method reference token
	UInt32 methodRef = ReadAs<UInt32>();
	return new(mMemory) CallNode(methodRef, arguments, argumentsCount);
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateLoadLocalVariable(UInt8 SLMode)
{
	auto localIndex = ReadAs<Int16>();
	auto local = new(mMemory) LocalVariableNode(GetRawContext()->LocalVariables[localIndex].Type.CoreType, localIndex);
	//Keep uniformity for convenience of traversal in SSA building
	return new(mMemory) LoadNode(SLMode, local);
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateLoadArgument(UInt8 SLMode)
{
	auto argumentIndex = ReadAs<Int16>();
	auto argument = new(mMemory) ArgumentNode(GetRawContext()->Arguments[argumentIndex].Type.CoreType, argumentIndex);
	//Keep uniformity for convenience of traversal in SSA building
	return new(mMemory) LoadNode(SLMode, argument);
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateLoadField(UInt8 SLMode)
{
	TreeNode* field = nullptr;
	if (mBaeIn == 0)
		field = new(mMemory) StaticFieldNode(ReadAs<UInt32>());
	else if (mBaeIn == 1)
		field = new(mMemory) InstanceFieldNode(ReadAs<UInt32>(), mEvalStack.Pop());
	else
	{
		RTE::Throw(Text("Invalid balance group."));
		//Should never reach here
	}

	if (SLMode == SLMode::Indirect)
		return new(mMemory) LoadNode(SLMode::Indirect, field);
	else
		return field;
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateLoadArrayElement(UInt8 SLMode)
{
	auto index = mEvalStack.Pop();
	auto array = mEvalStack.Pop();
	auto arrayElement = new(mMemory) ArrayElementNode(array, index);
	if (SLMode == SLMode::Indirect)
		return new(mMemory) LoadNode(SLMode::Indirect, arrayElement);
	else
		return arrayElement;
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateLoadString()
{
	auto stringToken = ReadAs<UInt32>();
	auto ret = new(mMemory) ConstantNode(CoreTypes::String);
	ret->StringToken = stringToken;
	return ret;
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateLoadConstant()
{
	UInt8 coreType = ReadAs<UInt8>();
	Int32 coreTypeSize = CoreTypes::SizeOfCoreType[coreType];
	auto ret = new(mMemory) ConstantNode(coreType);
	switch (coreTypeSize)
	{
	case 1: ret->I1 = ReadAs<Int8>(); break;
	case 2: ret->I2 = ReadAs<Int16>(); break;
	case 4: ret->I4 = ReadAs<Int32>(); break;
	case 8: ret->I8 = ReadAs<Int64>(); break;
	case -1:
		//Should never reach here.
		break;
	}
	return ret;
}

RTJ::Hex::StoreNode* RTJ::Hex::ILTransformer::GenerateStoreField()
{
	if (mBaeIn == 1)
		//Static field store
		return new(mMemory) StoreNode(new(mMemory) StaticFieldNode(ReadAs<UInt32>()), mEvalStack.Pop());
	else if (mBaeIn == 2)
	{
		//Instance field store
		auto value = mEvalStack.Pop();
		auto object = mEvalStack.Pop();
		return new(mMemory) StoreNode(new(mMemory) InstanceFieldNode(ReadAs<UInt32>(), object), value);
	}
	else
	{
		RTE::Throw(Text("Invalid balance group"));
		//Should never reach here
	}
}

RTJ::Hex::StoreNode* RTJ::Hex::ILTransformer::GenerateStoreArgument()
{
	auto argumentIndex = ReadAs<Int16>();
	auto coreType = GetRawContext()->Arguments[argumentIndex].Type.CoreType;
	return new(mMemory) StoreNode(
		new(mMemory) ArgumentNode(coreType, argumentIndex),
		mEvalStack.Pop());
}

RTJ::Hex::StoreNode* RTJ::Hex::ILTransformer::GenerateStoreLocal()
{
	auto localIndex = ReadAs<Int16>();
	auto coreType = GetRawContext()->LocalVariables[localIndex].Type.CoreType;
	return new(mMemory) StoreNode(
		new(mMemory) LocalVariableNode(coreType, localIndex),
		mEvalStack.Pop());
}

RTJ::Hex::StoreNode* RTJ::Hex::ILTransformer::GenerateStoreArrayElement()
{
	auto value = mEvalStack.Pop();
	auto index = mEvalStack.Pop();
	auto array = mEvalStack.Pop();

	return new(mMemory) StoreNode(
		new(mMemory) ArrayElementNode(array, index),
		value);
}

RTJ::Hex::StoreNode* RTJ::Hex::ILTransformer::GenerateStoreToAddress()
{
	auto value = mEvalStack.Pop();
	auto address = mEvalStack.Pop();
	return new(mMemory) StoreNode(address, value);
}

RTJ::Hex::NewNode* RTJ::Hex::ILTransformer::GenerateNew()
{
	return new(mMemory) NewNode(ReadAs<UInt32>());
}

RTJ::Hex::NewArrayNode* RTJ::Hex::ILTransformer::GenerateNewArray()
{
	TreeNode** dimensions = nullptr;
	Int32 dimensionCount = mBaeIn;
	if (mBaeIn > 0)
	{
		//Overceed 14 dimensions, Extended uint32 will be used
		if (mBaeIn == 0xF)
			dimensionCount = ReadAs<Int32>();
		dimensions = new(mMemory) TreeNode * [dimensionCount];
		for (int i = 0; i < dimensionCount; ++i)
			dimensions[i] = mEvalStack.Pop();
	}
	//Read method reference token
	UInt32 typeRef = ReadAs<UInt32>();
	return new(mMemory) NewArrayNode(typeRef, dimensions, dimensionCount);
}

RTJ::Hex::CompareNode* RTJ::Hex::ILTransformer::GenerateCompare()
{
	auto right = mEvalStack.Pop();
	auto left = mEvalStack.Pop();
	return new(mMemory) CompareNode(ReadAs<UInt8>(), left, right);
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateDuplicate()
{
	return mEvalStack.Top();
}

RTJ::Hex::ReturnNode* RTJ::Hex::ILTransformer::GenerateReturn(BasicBlockPartitionPoint*& partitions)
{
	TreeNode* ret = nullptr;
	if (mBaeIn == 1)
		ret = mEvalStack.Pop();
	auto currentPoint = new(mMemory) BasicBlockPartitionPoint(PPKind::Ret, GetOffset(), ret);
	LinkedList::AppendOneWayOrdered(partitions, currentPoint,
		[&](BasicBlockPartitionPoint* x, InsertOption& option) {
			if (currentPoint->ILOffset == x->ILOffset && x->Kind == PPKind::Target)
				option = InsertOption::Before;
			return currentPoint->ILOffset <= x->ILOffset;
	});
	return new(mMemory) ReturnNode(ret);
}

RTJ::Hex::BinaryArithmeticNode* RTJ::Hex::ILTransformer::GenerateBinaryArithmetic(UInt8 opcode)
{
	auto right = mEvalStack.Pop();
	auto left = mEvalStack.Pop();
	UInt8 kind = ReadAs<UInt8>();
	return new(mMemory) BinaryArithmeticNode(left, right, kind, opcode);
}

RTJ::Hex::UnaryArithmeticNode* RTJ::Hex::ILTransformer::GenerateUnaryArtithmetic(UInt8 opcode)
{
	auto value = mEvalStack.Pop();
	UInt8 kind = ReadAs<UInt8>();
	return new(mMemory) UnaryArithmeticNode(value, kind, opcode);
}

RTJ::Hex::ConvertNode* RTJ::Hex::ILTransformer::GenerateConvert()
{
	auto value = mEvalStack.Pop();
	UInt8 from = ReadAs<UInt8>();
	UInt8 to = ReadAs<UInt8>();
	return new(mMemory) ConvertNode(value, from, to);
}

RTJ::Hex::TreeNode* RTJ::Hex::ILTransformer::GenerateJccPP(BasicBlockPartitionPoint*& partitions)
{
	auto value = mEvalStack.Pop();
	auto jccOffset = ReadAs<Int32>();
	auto currentPoint = new(mMemory) BasicBlockPartitionPoint(PPKind::Conditional, GetOffset(), value);
	currentPoint->TargetILOffset = jccOffset;

	auto branchedPoint = new(mMemory) BasicBlockPartitionPoint(PPKind::Target, GetOffset(), nullptr);
	//Append current point into list
	LinkedList::AppendOneWayOrdered(partitions, currentPoint,
		[&](BasicBlockPartitionPoint* x, InsertOption& option) {
			if (currentPoint->ILOffset == x->ILOffset && x->Kind == PPKind::Target)
				option = InsertOption::Before;
			return currentPoint->ILOffset <= x->ILOffset;
		});

	//Append branched to list
	LinkedList::AppendOneWayOrdered(partitions, branchedPoint,
		[&](BasicBlockPartitionPoint* x, InsertOption&) {
			return branchedPoint->ILOffset <= x->ILOffset;
		});
	return value;
}

void RTJ::Hex::ILTransformer::GenerateJmpPP(BasicBlockPartitionPoint*& partitions)
{
	auto jmpOffset = ReadAs<Int32>();
	auto currentPoint = new(mMemory) BasicBlockPartitionPoint(PPKind::Unconditional, GetOffset(), nullptr);
	currentPoint->TargetILOffset = jmpOffset;

	auto branchedPoint = new(mMemory) BasicBlockPartitionPoint(PPKind::Target, GetOffset(), nullptr);
	//Append current point into list
	LinkedList::AppendOneWayOrdered(partitions, currentPoint,
		[&](BasicBlockPartitionPoint* x, InsertOption& option) {
			if (currentPoint->ILOffset == x->ILOffset && x->Kind == PPKind::Target)
				option = InsertOption::Before;
			return currentPoint->ILOffset <= x->ILOffset;
		});

	//Append branched to list
	LinkedList::AppendOneWayOrdered(partitions, branchedPoint,
		[&](BasicBlockPartitionPoint* x, InsertOption&) {
			return branchedPoint->ILOffset <= x->ILOffset;
		});
}

RTJ::Hex::Statement* RTJ::Hex::ILTransformer::TryGenerateStatement(TreeNode* value, Int32 beginOffset, bool isBalancedCritical)
{
	//Is eval stack already balanced?
	if (mEvalStack.IsBalanced())
		return new(mMemory) Statement(value, beginOffset, GetOffset());
	else
	{
		if (!isBalancedCritical)
			return nullptr;
		//Should never reach here, malformed IL
		RTE::Throw(Text("Malformed IL."));
	}
}

RTJ::Hex::Statement* RTJ::Hex::ILTransformer::TransformToUnpartitionedStatements(_RE_ BasicBlockPartitionPoint*& partitions)
{
	Statement* stmtHead = nullptr;
	Statement* stmtPrevious = nullptr;
	Statement* stmtCurrent = nullptr;

#define IL_TRY_GEN_STMT_CRITICAL(OP, CRITICAL) auto node = OP; \
							stmtCurrent = TryGenerateStatement(node, \
																stmtPrevious == nullptr ? 0 : stmtPrevious->EndOffset,\
																CRITICAL); \
							if (stmtCurrent == nullptr) { mEvalStack.Push(node); } \
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
			IL_TRY_GEN_STMT_CRITICAL(GenerateCall(), mBaeOut == 0);
			break;
		}
		case OpCodes::Ret:
		{
			IL_TRY_GEN_STMT_CRITICAL(GenerateReturn(partitions), true);
			break;
		}

		case OpCodes::Jcc:
		{
			IL_TRY_GEN_STMT_CRITICAL(GenerateJccPP(partitions), true);
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
			mEvalStack.Push(GenerateLoadArgument(SLMode::Direct));
			break;
		case OpCodes::LdArgA:
			mEvalStack.Push(GenerateLoadArgument(SLMode::Indirect));
			break;
		case OpCodes::LdLoc:
			mEvalStack.Push(GenerateLoadLocalVariable(SLMode::Direct));
			break;
		case OpCodes::LdLocA:
			mEvalStack.Push(GenerateLoadLocalVariable(SLMode::Indirect));
			break;
		case OpCodes::LdElem:
			mEvalStack.Push(GenerateLoadArrayElement(SLMode::Direct));
			break;
		case OpCodes::LdElemA:
			mEvalStack.Push(GenerateLoadArrayElement(SLMode::Indirect));
			break;
		case OpCodes::LdStr:
			mEvalStack.Push(GenerateLoadString());
			break;
		case OpCodes::LdC:
			mEvalStack.Push(GenerateLoadConstant());
			break;
		case OpCodes::LdNull:
			mEvalStack.Push(&NullNode::Instance());
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
			mEvalStack.Push(GenerateBinaryArithmetic(instruction));
			break;

		//---------------------------------------------------
		//Unary arithmetic instructions
		//---------------------------------------------------

		case OpCodes::Not:
		case OpCodes::Neg:
			mEvalStack.Push(GenerateUnaryArtithmetic(instruction));
			break;

		//---------------------------------------------------
		//Convert instruction
		//---------------------------------------------------

		case OpCodes::Conv:
			mEvalStack.Push(GenerateConvert());
			break;

		//------------------------------------------------------
		//Special instructions
		//------------------------------------------------------

		case OpCodes::Dup:
			mEvalStack.Push(GenerateDuplicate());
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

			IL_TRY_GEN_STMT_CRITICAL(mEvalStack.Pop(), true);
			break;
		}

		case OpCodes::Cmp:
			mEvalStack.Push(GenerateCompare());
			break;

		//------------------------------------------------------
		//GC related opcodes.
		//------------------------------------------------------
		case OpCodes::New:
			mEvalStack.Push(GenerateNew());
			break;
		case OpCodes::NewArr:
			mEvalStack.Push(GenerateNewArray());
			break;

		default:
			//You should never reach here
			RTE::Throw(Text("Unrecognized IL Opcode."));
			break;
		}
	}
	if (!mEvalStack.IsBalanced())
	{
		//Malformed IL
		RTE::Throw(Text("Malformed IL."));
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
		auto ret = mMemory->New<BasicBlock>();
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
			return basicBlockMap[offset] = mMemory->New<BasicBlock>();
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
		
		//Should not be target PP
		while (ppOfSameOffset != nullptr &&
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
				RTE::Throw(Text("Unknown PP kind"));
			}

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
			auto iteratorOfStmt = beginOfStmts->Next;
			auto previousOfIterator = beginOfStmts;
			//Traverse until we meet the next stmt belonging to another basic block.
			while (iteratorOfStmt != nullptr &&
				iteratorOfStmt->ILOffset < partitionPoint->ILOffset)
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
		if (basicBlockPrevious == nullptr ||
			basicBlockPrevious->BranchKind == PPKind::Conditional)
			//Logically sequential to the previous basic block if there is no return. 	
			basicBlockCurrent->BBIn.push_back(basicBlockPrevious);

		//Then we append this to our list.
		LinkedList::AppendTwoWay(basicBlockHead, basicBlockPrevious, basicBlockCurrent);

		//Update partition point to next one of new basic block.
		partitionPoint = ppOfSameOffset;
	}
	return basicBlockHead;
}

RTJ::Hex::ILTransformer::ILTransformer(
	HexJITContext* context
)
	:mJITContext(context),
	mMemory(context->Memory) {
	mCodePtr = mJITContext->Context->CodeSegment;
	mPreviousCodePtr = mJITContext->Context->CodeSegment;
	mCodePtrBound = mJITContext->Context->CodeSegment + mJITContext->Context->SegmentLength;
}

RTJ::Hex::BasicBlock* RTJ::Hex::ILTransformer::TransformILFrom()
{
	BasicBlockPartitionPoint* partitions = nullptr;
	Statement* stmts = TransformToUnpartitionedStatements(partitions);
	return PartitionToBB(stmts, partitions);
}


