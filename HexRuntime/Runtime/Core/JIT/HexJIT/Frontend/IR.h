#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\..\Utility.h"
#include <vector>
#include <array>

namespace RTJ::Hex
{
	enum class NodeKinds : UInt8
	{
		Constant,
		Argument,
		LocalVariable,
		Call,
		Load,
		Store,
		Array,
		StaticField,
		InstanceField,
		Convert,
		Compare,
		Duplicate,
		New,
		NewArray,
		Return,
		BinaryArithmetic,
		UnaryArithmetic,
		Phi
	};

	struct TreeNode
	{
		NodeKinds Kind;

		TreeNode(NodeKinds kind) :Kind(kind) {}

		//For convenience

		ForcedInline bool Is(NodeKinds value) {
			return Kind == value;
		}
		template<class T>
		ForcedInline T* As() {
			return (T*)this;
		}
	};

	struct UnaryNode : TreeNode {
		UnaryNode(NodeKinds kind) :TreeNode(kind) {}
	};

	struct BinaryNode : TreeNode {
		BinaryNode(NodeKinds kind) :TreeNode(kind) {}
	};

	struct ConstantNode : TreeNode
	{
		ConstantNode(UInt8 coreType)
			:TreeNode(NodeKinds::Constant),
			CoreType(coreType) {}
		UInt8 CoreType = 0;
		union
		{
			Boolean Bool;
			Int8 I1;
			Int16 I2;
			Int32 I4;
			Int64 I8 = 0;
			Float R4;
			Double R8;
			UInt32 StringToken;
		};
	};

	struct LocalVariableNode : TreeNode
	{
		LocalVariableNode(UInt8 valueType, Int16 localIndex)
			:TreeNode(NodeKinds::LocalVariable),
			ValueType(valueType),
			LocalIndex(localIndex) {}
		LocalVariableNode(NodeKinds kind, UInt8 valueType, Int16 localIndex)
			:TreeNode(kind),
			ValueType(valueType),
			LocalIndex(localIndex) {}

		UInt8 ValueType = 0;
		Int16 LocalIndex = 0;
	};

	struct ArgumentNode : LocalVariableNode
	{
		ArgumentNode(UInt8 valueType, Int16 argumentIndex)
			: LocalVariableNode(NodeKinds::Argument, valueType, argumentIndex) {
		}
	};

	struct CallNode : TreeNode
	{
		CallNode(
			UInt32 methodReference,
			TreeNode** arugments,
			Int32 argumentCount)
			:TreeNode(NodeKinds::Call),
			MethodReference(methodReference),
			Arguments(arugments),
			ArgumentCount(argumentCount) {}
		Int32 ArgumentCount;
		TreeNode** Arguments;
		UInt32 MethodReference;
	};

	/// <summary>
	/// Store or Load constant
	/// </summary>
	struct SLMode
	{
		static constexpr UInt8 Direct = 0x00;
		static constexpr UInt8 Indirect = 0x01;
	};

	struct StoreNode : BinaryNode
	{
		StoreNode(TreeNode* destination, TreeNode* source)
			:BinaryNode(NodeKinds::Store),
			Destination(destination),
			Source(source) {}
		TreeNode* Destination = nullptr;
		TreeNode* Source = nullptr;
	};

	/// <summary>
	/// The load node currently is only used in indirect loading for indicating.
	/// The purpose is to reduce memory allocation. If you want to represent direct load,
	/// use corresponding node instead.
	/// </summary>
	struct LoadNode : UnaryNode
	{
		LoadNode(UInt8 loadType, TreeNode* source)
			:UnaryNode(NodeKinds::Load),
			Source(source)
		{}
		/// <summary>
		/// Only allows array element, field, argument and local
		/// </summary>
		TreeNode* Source = nullptr;
		UInt8 LoadType = SLMode::Indirect;
	};

	struct ArrayElementNode : BinaryNode
	{
		ArrayElementNode(TreeNode* array, TreeNode* index)
			:BinaryNode(NodeKinds::Array),
			Array(array),
			Index(index) {}
		TreeNode* Array;
		TreeNode* Index;
	};

	struct StaticFieldNode : TreeNode
	{
		StaticFieldNode(UInt32 fieldReference)
			:TreeNode(NodeKinds::StaticField),
			FieldReference(fieldReference) {}
		UInt32 FieldReference;
	};

	struct InstanceFieldNode : UnaryNode
	{
		InstanceFieldNode(UInt32 fieldReference, TreeNode* source)
			:UnaryNode(NodeKinds::InstanceField),
			FieldReference(fieldReference),
			Source(source) {}
		TreeNode* Source;
		UInt32 FieldReference;
	};

	struct NewNode : TreeNode
	{
		NewNode(UInt32 ctor)
			:TreeNode(NodeKinds::New),
			MethodReference(ctor) {}
		Int32 ArgumentCount = 0;
		TreeNode** Arguments = nullptr;
		UInt32 MethodReference;
	};

	struct NewArrayNode : TreeNode
	{
		NewArrayNode(UInt32 type, TreeNode** dimensions, Int32 dimensionCount)
			:TreeNode(NodeKinds::NewArray),
			TypeReference(type),
			Dimensions(dimensions),
			DimensionCount(dimensionCount) {}
		Int32 DimensionCount;
		TreeNode** Dimensions;	
		UInt32 TypeReference;
	};

	struct CompareNode : BinaryNode
	{
		CompareNode(UInt8 condition, TreeNode* left, TreeNode* right)
			:BinaryNode(NodeKinds::Compare),
			Condition(condition),
			Left(left),
			Right(right) {}
		TreeNode* Left;
		TreeNode* Right;
		UInt8 Condition;
	};

	struct ReturnNode : UnaryNode
	{
		ReturnNode(TreeNode* ret)
			:UnaryNode(NodeKinds::Return),
			Ret(ret) {}
		TreeNode* Ret;
	};

	struct BinaryArithmeticNode : BinaryNode
	{
		BinaryArithmeticNode(
			TreeNode* left,
			TreeNode* right,
			UInt8 type,
			UInt8 opcode)
			: BinaryNode(NodeKinds::BinaryArithmetic),
			Type(type),
			Left(left),
			Right(right),
			Opcode(opcode) {}
		TreeNode* Left;
		TreeNode* Right;
		UInt8 Type;
		UInt8 Opcode;
	};

	struct UnaryArithmeticNode : UnaryNode
	{
		UnaryArithmeticNode(
			TreeNode* value,
			UInt8 type,
			UInt8 opcode)
			: UnaryNode(NodeKinds::UnaryArithmetic),
			Value(value),
			Type(type),
			Opcode(opcode) {}
		TreeNode* Value;
		UInt8 Type;
		UInt8 Opcode;
	};

	/// <summary>
	/// Convert node for primitive types like i1 to i8.
	/// </summary>
	struct ConvertNode : UnaryNode
	{
		ConvertNode(
			TreeNode* value,
			UInt8 from,
			UInt8 to)
			: UnaryNode(NodeKinds::UnaryArithmetic),
			Value(value),
			From(from),
			To(to) {}
		TreeNode* Value;
		UInt8 From;
		UInt8 To;
	};

	/// <summary>
	/// Used to access child of unary node.
	/// </summary>
	struct UnaryNodeAccessProxy : UnaryNode
	{
		UnaryNodeAccessProxy() : UnaryNode(NodeKinds::UnaryArithmetic) {}
		TreeNode* Value = nullptr;
	};

	/// <summary>
	/// Used to access children of binary node.
	/// </summary>
	struct BinaryNodeAccessProxy : BinaryNode
	{
		BinaryNodeAccessProxy() : BinaryNode(NodeKinds::BinaryArithmetic) {}
		TreeNode* First = nullptr;
		TreeNode* Second = nullptr;
	};

	/// <summary>
	/// Used to access children of multiple node
	/// </summary>
	struct MultipleNodeAccessProxy : TreeNode
	{
		MultipleNodeAccessProxy() : TreeNode(NodeKinds::Call) {}
		Int32 Count = 0;
		TreeNode** Values = nullptr;
	};

	template<size_t StackDepth, class Fn>
	static void TraverseTree(TreeNode* source, Fn&& action)
	{
		//Store reference for modifying pointer
		std::array<TreeNode**, StackDepth> stack{};
		Int32 index = 0;

		auto pushStack = [&](TreeNode** value) {
			if (index < stack.size())
				stack[index++] = value;
		};
		auto popStack = [&]() {
			return stack[--index];
		};
		//Push stack first
		pushStack(&source);

		while (index > 0)
		{
			auto&& current = *popStack();
			//Do custom action
			action(current);
			switch (current->Kind)
			{
			//Binary access
			case NodeKinds::Store:
			case NodeKinds::Array:
			case NodeKinds::Compare:
			case NodeKinds::BinaryArithmetic:

			{
				BinaryNodeAccessProxy* proxy = (BinaryNodeAccessProxy*)current;
				pushStack(&proxy->First);
				pushStack(&proxy->Second);
				break;
			}
			//Unary access
			case NodeKinds::Convert:
			case NodeKinds::InstanceField:
			case NodeKinds::Return:
			case NodeKinds::UnaryArithmetic:
			case NodeKinds::Duplicate:
			{
				UnaryNodeAccessProxy* proxy = (UnaryNodeAccessProxy*)current;
				pushStack(&proxy->Value);
				break;
			}
			//Multiple access 
			case NodeKinds::Call:
			case NodeKinds::New:
			case NodeKinds::NewArray:
			{
				MultipleNodeAccessProxy* proxy = (MultipleNodeAccessProxy*)current;
				for (Int32 i = 0; i < proxy->Count; ++i)
					pushStack(&proxy->Values[i]);
				break;
			}
			}
		}
	}

	template<class Fn>
	static void TraverseTreeBottomUp(Int8* stackSpace, Int32 upperBound, TreeNode*& source, Fn&& action)
	{
		using NodeReference = TreeNode**;
		NodeReference* stack = (NodeReference*)stackSpace;

		Int32 index = 0;
		Int32 previousIndex = 0;

		auto pushStack = [&](TreeNode** value) {
			if (index < upperBound)
				stack[index++] = value;
		};
		
		pushStack(&source);
		//Push them all to stack 
		for (Int32 i = 0; i < index; ++i)
		{
			auto&& current = *stack[i];
			switch (current->Kind)
			{
				//Binary access
			case NodeKinds::Store:
			case NodeKinds::Array:
			case NodeKinds::Compare:
			case NodeKinds::BinaryArithmetic:

			{
				BinaryNodeAccessProxy* proxy = (BinaryNodeAccessProxy*)current;
				pushStack(&proxy->First);
				pushStack(&proxy->Second);
				break;
			}
			//Unary access
			case NodeKinds::Convert:
			case NodeKinds::InstanceField:
			case NodeKinds::Return:
			case NodeKinds::UnaryArithmetic:
			case NodeKinds::Duplicate:
			{
				UnaryNodeAccessProxy* proxy = (UnaryNodeAccessProxy*)current;
				pushStack(&proxy->Value);
				break;
			}
			//Multiple access 
			case NodeKinds::Call:
			case NodeKinds::New:
			case NodeKinds::NewArray:
			{
				MultipleNodeAccessProxy* proxy = (MultipleNodeAccessProxy*)current;
				for (Int32 i = 0; i < proxy->Count; ++i)
					pushStack(&proxy->Values[i]);
				break;
			}
			}
		}

		while (index-- > 0)
			std::forward<Fn>(action)(*stack[index]);
	}

	struct Statement
	{
		Statement(TreeNode* target, Int32 offset, Int32 endOffset)
			:Now(target),
			ILOffset(offset),
			EndOffset(endOffset) {}
		//IL sequential connection
	public:
		Int32 ILOffset;
		Int32 EndOffset;
		Statement* Next = nullptr;
		Statement* Prev = nullptr;
		TreeNode* Now;
	};

	/// <summary>
	/// PP kind indicates the property of partition point.
	/// PPKind::Target should always be ordered after other kinds
	/// in case there are many PPs with same offsets to distinguish
	/// the BB it should belong to
	/// </summary>
	enum class PPKind
	{
		Conditional,
		Unconditional,
		Ret,
		Target,
		Try,
		/// <summary>
		/// Catch block may always have previous try block
		/// in its BBIn
		/// </summary>
		Catch,
		/// <summary>
		/// Finally block always have previous try block
		/// int its BBIn
		/// </summary>
		Finally
	};

	struct BasicBlock
	{
		//IL sequential connection
	public:
		/// <summary>
		/// This can also be used in sequential access of next BB.
		/// </summary>
		BasicBlock* Next = nullptr;
		BasicBlock* Prev = nullptr;
		Statement* Now = nullptr;
		/// <summary>
		/// Basic block index.
		/// </summary>
		Int32 Index = 0;
	public:
		PPKind BranchKind;
		/// <summary>
		/// Stores the condition expression.
		/// </summary>
		TreeNode* BranchConditionValue = nullptr;
		BasicBlock* BranchedBB = nullptr;
	public:
		std::vector<BasicBlock*> BBIn;
	};

	struct BasicBlockPartitionPoint
	{
		BasicBlockPartitionPoint(PPKind kind, Int32 ilOffset, TreeNode* value)
			:Kind(kind), ILOffset(ilOffset), Value(value) {}
		BasicBlockPartitionPoint* Next = nullptr;

		PPKind Kind;
		Int32 ILOffset;

		//Record the target of jump.
		Int32 TargetILOffset = 0;
		TreeNode* Value = nullptr;
	};

	namespace SSA
	{
		/// <summary>
		/// Phi node to choose branch
		/// </summary>
		struct PhiNode : TreeNode
		{
			BasicBlock* Belonging = nullptr;
			std::vector<TreeNode*> Choices;
			TreeNode* OriginValue = nullptr;
		public:
			PhiNode(TreeNode* value):
				TreeNode(NodeKinds::Phi),
				OriginValue(value) {
			}
			PhiNode(BasicBlock* belongs) :
				TreeNode(NodeKinds::Phi),
				Belonging(belongs) {
			}
			bool IsRemoved()const {
				return Belonging == nullptr;
			}
		};
	}
}