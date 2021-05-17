#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\..\Utility.h"
#include <vector>

namespace RTJ::Hex
{
	//Standard procedure for releasing pointer
	template<class T>
	constexpr static inline void ReleaseNode(T*& target) {
		if (target != nullptr) {
			delete target;
			target = nullptr;
		}
	}

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

		virtual ~TreeNode() = default;
	};

	struct UnaryNode : TreeNode {
		UnaryNode(NodeKinds kind) :TreeNode(kind) {}
	};

	struct BinaryNode : TreeNode {
		BinaryNode(NodeKinds kind) :TreeNode(kind) {}
	};

	struct ConstantNode : UnaryNode
	{
		ConstantNode(UInt8 coreType)
			:UnaryNode(NodeKinds::Constant),
			CoreType(coreType) {}
		UInt8 CoreType = 0;
		union
		{
			Int8 I1;
			Int16 I2;
			Int32 I4;
			Int64 I8;
			/// <summary>
			/// For pointer
			/// </summary>
			UInt32 X86Ref;

			/// <summary>
			/// For pointer
			/// </summary>
			UInt64 X64Ref;

			/// <summary>
			/// For interior pointer
			/// </summary>
			Int32 X86InteriorRef[2];
			/// <summary>
			/// For interior pointer
			/// </summary>
			Int64 X64InteriorRef[2];
		};
	};

	struct ArgumentNode : UnaryNode
	{
		ArgumentNode(UInt8 valueType, Int16 argumentIndex)
			:UnaryNode(NodeKinds::Argument),
			ValueType(valueType),
			ArgumentIndex(argumentIndex) {}
		UInt8 ValueType = 0;
		Int16 ArgumentIndex = 0;
	};

	struct LocalVariableNode : UnaryNode
	{
		LocalVariableNode(UInt8 valueType, Int16 localIndex)
			:UnaryNode(NodeKinds::LocalVariable),
			ValueType(valueType),
			LocalIndex(localIndex) {}
		UInt8 ValueType = 0;
		Int16 LocalIndex = 0;
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

		UInt32 MethodReference;
		TreeNode** Arguments;
		Int32 ArgumentCount;

		virtual ~CallNode() {
			if (Arguments != nullptr)
			{
				for (Int32 i = 0; i < ArgumentCount; ++i)
					delete Arguments[i];
				Arguments = nullptr;
			}
		}
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

		virtual ~StoreNode() {
			ReleaseNode(Destination);
			ReleaseNode(Source);
		}
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
		UInt8 LoadType = SLMode::Indirect;
		/// <summary>
		/// Only allows array element, field, argument and local
		/// </summary>
		TreeNode* Source = nullptr;
		virtual ~LoadNode() {
			ReleaseNode(Source);
		}
	};

	struct ArrayElementNode : BinaryNode
	{
		ArrayElementNode(TreeNode* array, TreeNode* index)
			:BinaryNode(NodeKinds::Array),
			Array(array),
			Index(index) {}
		TreeNode* Array;
		TreeNode* Index;
		virtual ~ArrayElementNode() {
			ReleaseNode(Array);
			ReleaseNode(Index);
		}
	};

	struct StaticFieldNode : UnaryNode
	{
		StaticFieldNode(UInt32 fieldReference)
			:UnaryNode(NodeKinds::StaticField),
			FieldReference(fieldReference) {}
		UInt32 FieldReference;
	};

	struct InstanceFieldNode : UnaryNode
	{
		InstanceFieldNode(UInt32 fieldReference, TreeNode* source)
			:UnaryNode(NodeKinds::InstanceField),
			FieldReference(fieldReference),
			Source(source) {}
		UInt32 FieldReference;
		TreeNode* Source;
		virtual ~InstanceFieldNode() {
			ReleaseNode(Source);
		}
	};

	struct NewNode : UnaryNode
	{
		NewNode(UInt32 ctor)
			:UnaryNode(NodeKinds::New),
			MethodReference(ctor) {}
		UInt32 MethodReference;
	};

	struct NewArrayNode : TreeNode
	{
		NewArrayNode(UInt32 type, TreeNode** dimensions, UInt32 dimensionCount)
			:TreeNode(NodeKinds::NewArray),
			TypeReference(type),
			Dimensions(dimensions),
			DimensionCount(dimensionCount) {}
		UInt32 TypeReference;
		TreeNode** Dimensions;
		UInt32 DimensionCount;
		virtual ~NewArrayNode() {
			if (Dimensions != nullptr)
			{
				for (UInt32 i = 0; i < DimensionCount; ++i)
					delete Dimensions[i];
				Dimensions = nullptr;
			}
		}
	};

	struct CompareNode : BinaryNode
	{
		CompareNode(UInt8 condition, TreeNode* left, TreeNode* right)
			:BinaryNode(NodeKinds::Compare),
			Condition(condition),
			Left(left),
			Right(right) {}
		UInt8 Condition;
		TreeNode* Left;
		TreeNode* Right;
		virtual ~CompareNode() {
			ReleaseNode(Left);
			ReleaseNode(Right);
		}
	};

	struct ReturnNode : UnaryNode
	{
		ReturnNode(TreeNode* ret)
			:UnaryNode(NodeKinds::Return),
			Ret(ret) {}
		TreeNode* Ret;
		virtual ~ReturnNode() {
			ReleaseNode(Ret);
		}
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
		virtual ~BinaryArithmeticNode() {
			ReleaseNode(Left);
			ReleaseNode(Right);
		}
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
		UInt8 Type;
		TreeNode* Value;
		UInt8 Opcode;
		virtual ~UnaryArithmeticNode() {
			ReleaseNode(Value);
		}
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
		virtual ~ConvertNode() {
			ReleaseNode(Value);
		}
	};

	/// <summary>
	/// Duplicate node is actually for resource management.
	/// When met with duplicate node, we won't release its child
	/// </summary>
	struct DuplicateNode : UnaryNode
	{
		DuplicateNode(TreeNode* target)
			:UnaryNode(NodeKinds::Duplicate),
			Target(target) {}
		/// <summary>
		/// Ref from current duplicate node, increment count
		/// </summary>
		/// <returns></returns>
		ForcedInline DuplicateNode* Duplicate() {
			Count++;
			return this;
		}
		TreeNode* Target;
		/// <summary>
		/// For releasing itself.
		/// </summary>
		Int32 Count = 0;
	};

	struct Statement
	{
		Statement(TreeNode* target, Int32 offset)
			:Now(target),
			ILOffset(offset) {}
		//IL sequential connection
	public:
		Int32 ILOffset;
		Statement* Next = nullptr;
		Statement* Prev = nullptr;
		TreeNode* Now;
	};

	struct BasicBlock
	{
		//IL sequential connection
	public:
		BasicBlock* Next = nullptr;
		BasicBlock* Prev = nullptr;
		Statement* Now = nullptr;
		/// <summary>
		/// Basic block index.
		/// </summary>
		Int32 Index = 0;
	public:
		//Branch type that comes out��conditional or unconditional or sequential��
		Int8 BranchType;
		/// <summary>
		/// Stores the condition expression.
		/// </summary>
		TreeNode* BranchConditionValue = nullptr;
	public:
		std::vector<BasicBlock*> BBIn;
	};

	struct BasicBlockPartitionPoint
	{
		BasicBlockPartitionPoint(Int32 ilOffset, TreeNode* value)
			:ILOffset(ilOffset), Value(value) {}
		BasicBlockPartitionPoint* Next = nullptr;
		Int32 ILOffset;
		TreeNode* Value;
		BasicBlock* Source = nullptr;

		/// <summary>
		/// Is the target of basic block.
		/// </summary>
		/// <returns></returns>
		bool IsTargetPP()const {
			return Value == nullptr;
		}
	};

	namespace SSA
	{
		struct DeclareNode
		{

		};

		struct AssignNode
		{

		};

		/// <summary>
		/// Phi node to choose branch
		/// </summary>
		struct PhiNode : TreeNode
		{
			BasicBlock* Belonging;
			std::vector<void*> Choices;
		public:
			PhiNode(BasicBlock* belonging)
				:TreeNode(NodeKinds::Phi), Belonging(belonging) {
			}
		};
	}
}