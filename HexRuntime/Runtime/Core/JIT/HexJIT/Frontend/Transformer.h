#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\JITContext.h"
#include "IR.h"
#include "EvaluationStack.h"

namespace RTJ::Hex
{
	/// <summary>
	/// Transform the opcode to basic block expression tree.
	/// </summary>
	class ILTransformer
	{
		JITContext mJITContext;
		const UInt8* mCodePtr = nullptr;
		const UInt8* mCodePtrBound = nullptr;
		const UInt8* mPreviousCodePtr = nullptr;
		EvaluationStack mEvalStack;
		// Current instruction BAE state

		UInt8 mBaeIn = 0;
		UInt8 mBaeOut = 0;

		// Fast streaming
		template<class T> ForcedInline T ReadAs() {
			T ret = *(T*)mCodePtr;
			mCodePtr += sizeof(T);
			return ret;
		}

		template<class T> ForcedInline T PeekAs()const {
			T ret = *(T*)mCodePtr;
			return ret;
		}
		/// <summary>
		/// Get offset of now
		/// </summary>
		/// <returns></returns>
		ForcedInline Int32 GetOffset()const;

		/// <summary>
		/// Get offset of previous instruction
		/// </summary>
		/// <returns></returns>
		ForcedInline Int32 GetPreviousOffset()const;

		/// <summary>
		/// Decode the instruction at current memory
		/// </summary>
		/// <param name="opcode">opcode value</param>
		/// <param name="in">Bae group bringing in value</param>
		/// <param name="out">Bae group bringing out value</param>
		ForcedInline void DecodeInstruction(_RE_ UInt8& opcode);
		CallNode* GenerateCall();
		TreeNode* GenerateLoadLocalVariable(UInt8 SLMode);
		TreeNode* GenerateLoadArgument(UInt8 SLMode);
		TreeNode* GenerateLoadField(UInt8 SLMode);
		TreeNode* GenerateLoadArrayElement(UInt8 SLMode);
		TreeNode* GenerateLoadString();
		TreeNode* GenerateLoadConstant();
		StoreNode* GenerateStoreField();
		StoreNode* GenerateStoreArgument();
		StoreNode* GenerateStoreLocal();
		StoreNode* GenerateStoreArrayElement();
		StoreNode* GenerateStoreToAddress();
		NewNode* GenerateNew();
		NewArrayNode* GenerateNewArray();
		CompareNode* GenerateCompare();
		DuplicateNode* GenerateDuplicate();
		ReturnNode* GenerateReturn();
		BinaryArithmeticNode* GenerateBinaryArithmetic(UInt8 opcode);
		UnaryArithmeticNode* GenerateUnaryArtithmetic(UInt8 opcode);
		ConvertNode* GenerateConvert();

		void GenerateJccPP(BasicBlockPartitionPoint*& partitions);
		void GenerateJmpPP(BasicBlockPartitionPoint*& partitions);

		/// <summary>
		/// The key standard for generating a statement is that eval stack is empty(balanced).
		/// Also there is case that unbalanced eval stack is unacceptable. For example, a call 
		/// to certian method with void return type must be a statement
		/// </summary>
		/// <param name="value"></param>
		/// <param name="isBalancedCritical"></param>
		/// <returns></returns>
		Statement* TryGenerateStatement(TreeNode* value, bool isBalancedCritical = false);
		/// <summary>
		/// Firstly translate IL to a single basic block then partition it according to the
		/// information we collected
		/// </summary>
		/// <returns></returns>
		Statement* TransformToUnpartitionedStatements(_RE_ BasicBlockPartitionPoint*& partitions);
		/// <summary>
		/// Correctly partition the chained statements according to partition information.
		/// </summary>
		/// <param name="unpartitionedStmt"></param>
		/// <param name="partitions"></param>
		/// <returns></returns>
		BasicBlock* PartitionToBB(Statement* unpartitionedStmt, BasicBlockPartitionPoint* partitions);
	public:
		ILTransformer(JITContext const&);
		BasicBlock* TransformILFrom();
		
	};
}