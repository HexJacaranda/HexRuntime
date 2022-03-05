#pragma once
//Union initialization
#pragma warning(disable:26495)

#include "..\..\..\RuntimeAlias.h"
#include "..\..\..\Utility.h"
#include "..\..\Meta\CoreTypes.h"
#include "..\..\Platform\PlatformSpecialization.h"
#include "..\..\..\SmallSet.h"
#include <vector>

namespace RTM
{
	class TypeDescriptor;
	class FieldDescriptor;
	class MethodDescriptor;
}

namespace RTJ::Hex
{
	class RegisterAllocationContext;
}

namespace RTJ
{
	class EmitPage;
}

namespace RTJ::Hex
{
	enum class NodeKinds : UInt8
	{
		Constant,
		LocalVariable,
		Call,
		Load,
		Store,
		Array,
		StaticField,
		InstanceField,
		Convert,
		Cast,
		Box,
		UnBox,
		Compare,
		Duplicate,
		New,
		NewArray,
		BinaryArithmetic,
		UnaryArithmetic,
		Null,
		//SSA introduced

		Phi,
		Use,
		ValueDef,
		ValueUse,
		UndefinedValue,
		//Morph

		MorphedCall,
		OffsetOf,
		ArrayOffsetOf
	};

	struct TreeNode
	{
		/// <summary>
		/// Indicate the type info of this node
		/// </summary>
		RTM::TypeDescriptor* TypeInfo = nullptr;
		/// <summary>
		/// Node kind
		/// </summary>
		NodeKinds Kind;
	public:
		TreeNode(NodeKinds kind) :Kind(kind) {}

		//For convenience

		ForcedInline bool Is(NodeKinds value)const {
			return Kind == value;
		}

		template<class T>
		ForcedInline T* As()const {
			return (T*)this;
		}

		bool CheckEquivalentWith(TreeNode* target)const;
		bool CheckEquivalentWith(RTM::TypeDescriptor* target)const;
		bool CheckUpCastFrom(TreeNode* target)const;
		bool CheckUpCastFrom(RTM::TypeDescriptor* source)const;
		void TypeFrom(TreeNode* target);
		TreeNode* SetType(RTM::TypeDescriptor* type);
	};

	struct UnaryNode : TreeNode {
		UnaryNode(NodeKinds kind) :TreeNode(kind) {}
	};

	struct BinaryNode : TreeNode {
		BinaryNode(NodeKinds kind) :TreeNode(kind) {}
	};

	/// <summary>
	/// To access right bytes of constant
	/// </summary>
	union ConstantStorage
	{
		Boolean Bool;
		Int8 I1;
		Int16 I2;
		Int32 I4;
		Int64 I8;
		UInt8 U1;
		UInt16 U2;
		UInt32 U4;
		UInt64 U8;
		Float R4;
		Double R8;
		void* Pointer;
		UInt8* SIMD;

		static ConstantStorage From(Int8 value)
		{
			ConstantStorage ret{};
			ret.I1 = value;
			return ret;
		}
		static ConstantStorage From(Int16 value)
		{
			ConstantStorage ret{};
			ret.I2 = value;
			return ret;
		}
		static ConstantStorage From(Int32 value)
		{
			ConstantStorage ret{};
			ret.I4 = value;
			return ret;
		}
		static ConstantStorage From(Int64 value)
		{
			ConstantStorage ret{};
			ret.I8 = value;
			return ret;
		}
		static ConstantStorage From(UInt8 value)
		{
			ConstantStorage ret{};
			ret.U1 = value;
			return ret;
		}
		static ConstantStorage From(UInt16 value)
		{
			ConstantStorage ret{};
			ret.U2 = value;
			return ret;
		}
		static ConstantStorage From(UInt32 value)
		{
			ConstantStorage ret{};
			ret.U4 = value;
			return ret;
		}
		static ConstantStorage From(UInt64 value)
		{
			ConstantStorage ret{};
			ret.U8 = value;
			return ret;
		}
	};

	struct ConstantNode : TreeNode
	{
		ConstantNode(UInt8 coreType) :
			TreeNode(NodeKinds::Constant),
			CoreType(coreType) {}

		ConstantNode(Boolean value) :
			TreeNode(NodeKinds::Constant),
			Bool(value),
			CoreType(CoreTypes::I1) {}

		ConstantNode(Int8 value) :
			TreeNode(NodeKinds::Constant),
			I1(value),
			CoreType(CoreTypes::I1) {}

		ConstantNode(Int16 value) :
			TreeNode(NodeKinds::Constant),
			I2(value),
			CoreType(CoreTypes::I2) {}

		ConstantNode(Int32 value) :
			TreeNode(NodeKinds::Constant),
			I4(value),
			CoreType(CoreTypes::I4) {}

		ConstantNode(Int64 value) :
			TreeNode(NodeKinds::Constant),
			I8(value),
			CoreType(CoreTypes::I8) {}

		ConstantNode(void* value) :
			TreeNode(NodeKinds::Constant),
			Pointer(value),
			CoreType(CoreTypes::Ref) {}

		UInt8 CoreType = 0;
		union
		{
			ConstantStorage Storage;
			Boolean Bool;
			Int8 I1;
			Int16 I2;
			Int32 I4;
			Int64 I8;
			UInt8 U1;
			UInt16 U2;
			UInt32 U4;
			UInt64 U8;
			Float R4;
			Double R8;
			UInt32 StringToken;
			void* Pointer;
		};
	};

	struct LocalVariableNode : TreeNode
	{
		static constexpr UInt16 ArgumentFlag = 0x8000u;
		LocalVariableNode(UInt16 localIndex)
			:TreeNode(NodeKinds::LocalVariable),
			LocalIndex(localIndex) {}

		UInt16 LocalIndex = 0;
	
		inline bool IsArgument()const {
			return LocalIndex & ArgumentFlag;
		}
		static inline bool IsArgument(UInt16 value) {
			return value & ArgumentFlag;
		}
		static inline UInt16 GetIndex(UInt16 value) {
			return value & 0x7FFFu;
		}
		inline UInt16 GetIndex()const {
			return LocalIndex & 0x7FFFu;
		}
	};

	struct ArgumentNode : LocalVariableNode
	{
		ArgumentNode(UInt16 localIndex) :
			LocalVariableNode(localIndex | ArgumentFlag) {}
	};

	struct CallNode : TreeNode
	{
		CallNode(
			RTM::MethodDescriptor* method,
			TreeNode** arugments,
			Int32 argumentCount)
			:TreeNode(NodeKinds::Call),
			Method(method),
			Arguments(arugments),
			ArgumentCount(argumentCount) {}

		CallNode(
			RTM::MethodDescriptor* method,
			TreeNode* arugment)
			:TreeNode(NodeKinds::Call),
			Method(method),
			Argument(arugment),
			ArgumentCount(1) {}

		Int32 ArgumentCount;
		union
		{
			TreeNode** Arguments;
			TreeNode* Argument;
		};
		RTM::MethodDescriptor* Method;
	};

	enum class AccessMode
	{
		Value, //Load the origin value
		Address, //Load the address
		Content, //Load the content from address
	};

	struct StoreNode : BinaryNode
	{
		StoreNode(
			TreeNode* destination, 
			TreeNode* source,
			AccessMode mode = AccessMode::Value)
			:BinaryNode(NodeKinds::Store),
			Destination(destination),
			Source(source),
			Mode(mode) {}
		TreeNode* Destination = nullptr;
		TreeNode* Source = nullptr;
		AccessMode Mode;
	};

	/// <summary>
	/// The load node currently is only used in indirect loading for indicating.
	/// The purpose is to reduce memory allocation. If you want to represent direct load,
	/// use corresponding node instead.
	/// </summary>
	struct LoadNode : UnaryNode
	{
		LoadNode(AccessMode mode, TreeNode* source)
			:UnaryNode(NodeKinds::Load),
			Source(source),
			Mode(mode)
		{}
		/// <summary>
		/// Only allows array element, field, argument and local
		/// </summary>
		TreeNode* Source = nullptr;
		AccessMode Mode = AccessMode::Value;
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
		StaticFieldNode(RTM::FieldDescriptor* field)
			:TreeNode(NodeKinds::StaticField),
			Field(field) {}
		RTM::FieldDescriptor* Field;
	};

	struct InstanceFieldNode : UnaryNode
	{
		InstanceFieldNode(RTM::FieldDescriptor* field, TreeNode* source)
			:UnaryNode(NodeKinds::InstanceField),
			Field(field),
			Source(source) {}
		TreeNode* Source;
		RTM::FieldDescriptor* Field;
	};

	struct NewNode : TreeNode
	{
		NewNode(
			RTM::MethodDescriptor* method,
			TreeNode** arugments,
			Int32 argumentCount)
			:TreeNode(NodeKinds::New),
			Method(method),
			Arguments(arugments),
			ArgumentCount(argumentCount) {}

		NewNode(
			RTM::MethodDescriptor* method,
			TreeNode* arugment)
			:TreeNode(NodeKinds::New),
			Method(method),
			Argument(arugment),
			ArgumentCount(1) {}
		Int32 ArgumentCount = 0;
		union
		{
			TreeNode** Arguments = nullptr;
			TreeNode* Argument;
		};
		RTM::MethodDescriptor* Method;
	};

	struct NewArrayNode : TreeNode
	{
		NewArrayNode(RTM::TypeDescriptor* elementType, TreeNode** dimensions, Int32 dimensionCount)
			:TreeNode(NodeKinds::NewArray),
			ElementType(elementType),
			Dimensions(dimensions),
			DimensionCount(dimensionCount) {}

		NewArrayNode(RTM::TypeDescriptor* elementType, TreeNode* dimension)
			:TreeNode(NodeKinds::NewArray),
			ElementType(elementType),
			Dimension(dimension),
			DimensionCount(1) {}

		Int32 DimensionCount;
		union
		{
			TreeNode** Dimensions;
			TreeNode* Dimension;
		};
		RTM::TypeDescriptor* ElementType;
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

	struct BinaryArithmeticNode : BinaryNode
	{
		BinaryArithmeticNode(
			TreeNode* left,
			TreeNode* right,
			UInt8 opcode)
			: BinaryNode(NodeKinds::BinaryArithmetic),
			Left(left),
			Right(right),
			Opcode(opcode) {}
		TreeNode* Left;
		TreeNode* Right;
		UInt8 Opcode;
	};

	struct UnaryArithmeticNode : UnaryNode
	{
		UnaryArithmeticNode(
			TreeNode* value,
			UInt8 opcode)
			: UnaryNode(NodeKinds::UnaryArithmetic),
			Value(value),
			Opcode(opcode) {}
		TreeNode* Value;
		UInt8 Opcode;
	};

	/// <summary>
	/// Convert node for primitive types like i1 to i8.
	/// </summary>
	struct ConvertNode : UnaryNode
	{
		ConvertNode(
			TreeNode* value,
			UInt8 to)
			: UnaryNode(NodeKinds::UnaryArithmetic),
			Value(value),
			To(to) {}
		TreeNode* Value;
		UInt8 To;
	};

	struct CastNode : UnaryNode
	{
		CastNode(
			TreeNode* value)
			: UnaryNode(NodeKinds::Cast),
			Value(value){}
		TreeNode* Value;
	};

	struct BoxNode : UnaryNode
	{
		BoxNode(
			TreeNode* value)
			: UnaryNode(NodeKinds::Box),
			Value(value){}
		TreeNode* Value;
	};

	struct UnBoxNode : UnaryNode
	{
		UnBoxNode(
			TreeNode* value)
			: UnaryNode(NodeKinds::UnBox),
			Value(value) {}
		TreeNode* Value;
	};

	struct NullNode : TreeNode
	{
		NullNode(): TreeNode(NodeKinds::Null) {}
		static NullNode& Instance()
		{
			static NullNode node;
			return node;
		}
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
		union 
		{
			TreeNode** Values = nullptr;
			TreeNode* Value;
		};	
	};

	static bool ValueIs(TreeNode* value, NodeKinds kind)
	{
		if (value == nullptr)
			return false;
		
		return ((UnaryNodeAccessProxy*)value)->Value->Is(kind);
	}

	template<class T>
	static T* ValueAs(TreeNode* value)
	{
		if (value == nullptr)
			return nullptr;
		return ((UnaryNodeAccessProxy*)value)->Value->As<T>();
	}

	/*--------------------------------Morphed Section--------------------------------*/

	/// <summary>
	/// MorphedCallNode often represents a pure native call to JIT runtime method.
	/// But there may be a origin managed call accompanied.
	/// </summary>
	struct MorphedCallNode : TreeNode
	{
		MorphedCallNode(TreeNode* origin, void* nativeEntry, RTP::PlatformCallingConvention* callingConv, TreeNode** arguments, Int32 count) :
			TreeNode(NodeKinds::MorphedCall),
			Arguments(arguments),
			ArgumentCount(count),
			Origin(origin),
			NativeEntry(nativeEntry),
			CallingConvention(callingConv)		
		{}

		MorphedCallNode(TreeNode* origin, void* nativeEntry, RTP::PlatformCallingConvention* callingConv, TreeNode* argument) :
			TreeNode(NodeKinds::MorphedCall),
			Argument(argument),
			ArgumentCount(1),
			Origin(origin),
			NativeEntry(nativeEntry),
			CallingConvention(callingConv)
		{}

		Int32 ArgumentCount = 0;
		union 
		{
			TreeNode* Argument;
			TreeNode** Arguments = nullptr;
		};		
		RTP::PlatformCallingConvention* CallingConvention = nullptr;
		void* NativeEntry;
		TreeNode* Origin;	
	};

	struct OffsetOfNode : UnaryNode
	{
		OffsetOfNode(TreeNode* base, Int32 offset) :
			UnaryNode(NodeKinds::OffsetOf),
			Base(base),
			Offset(offset) {}

		TreeNode* Base;
		Int32 Offset;
	};

	struct ArrayOffsetOfNode : BinaryNode
	{
		ArrayOffsetOfNode(TreeNode* base, TreeNode* index, Int32 offset, Int32 scale) :
			BinaryNode(NodeKinds::OffsetOf),
			Array(base),
			Index(index),
			BaseOffset(offset),
			Scale(scale) {}

		TreeNode* Array;
		TreeNode* Index;
		Int32 BaseOffset;
		Int32 Scale;
	};

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
		/// <summary>
		/// Initial and most natural kind
		/// </summary>
		Sequential,
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
		/// in its BBIn
		/// </summary>
		Finally
	};

	using VariableSet = SmallSet<UInt16>;
	using LivenessMapT = std::vector<VariableSet>;

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
		PPKind BranchKind = PPKind::Sequential;
		/// <summary>
		/// Stores the condition expression.
		/// </summary>
		union 
		{
			TreeNode* BranchConditionValue = nullptr;
			TreeNode* ReturnValue;
		};		
		BasicBlock* BranchedBB = nullptr;

		RegisterAllocationContext* RegisterContext = nullptr;
		/// <summary>
		/// Liveness
		/// </summary>
		/// <returns></returns>
		VariableSet VariablesLiveIn;
		VariableSet VariablesLiveOut;
		VariableSet VariablesUse;
		VariableSet VariablesDef;
		LivenessMapT Liveness;

		Int32 PhysicalOffset = 0;
		Int32 NativeCodeSize = 0;

		EmitPage* NativeCode = nullptr;
	public:
		std::vector<BasicBlock*> BBIn;
	public:
		template<class Fn>
		void ForeachSuccessor(Fn&& action)
		{
			auto next = Next;
			auto branch = BranchedBB;

			if (next != nullptr)
				std::forward<Fn>(action)(next);

			//Prevent duplicate access
			if (branch != nullptr && branch != next)
				std::forward<Fn>(action)(branch);
		}
		bool IsUnreachable()const {
			return BBIn.empty() && Prev != nullptr;
		}

		static bool ReachableFn(BasicBlock* bb) {
			return !bb->IsUnreachable();
		}
	};

	struct BasicBlockPartitionPoint
	{
		BasicBlockPartitionPoint(PPKind kind, Int32 ilOffset, TreeNode* value)
			:Kind(kind), ILOffset(ilOffset), Value(value) {}
		BasicBlockPartitionPoint* Next = nullptr;

		PPKind Kind;
		//For target kind PP, this should be its targeted offset
		Int32 ILOffset;
		Int32 TargetILOffset = 0;
		//The value of conditional jump
		TreeNode* Value = nullptr;
	};


	/*--------------------------------SSA Section--------------------------------*/
	namespace SSA
	{
		/// <summary>
		/// Use node
		/// </summary>
		struct Use : UnaryNode
		{
			TreeNode* Value;
			Use* Prev = nullptr;
		public:
			Use(TreeNode* value) : UnaryNode(NodeKinds::Use), Value(value) {}
		};

		/// <summary>
		/// Predeclaration
		/// </summary>
		struct ValueUse;

		struct ValueDef : UnaryNode
		{
			TreeNode* Value;
			TreeNode* Origin;
			ValueUse* UseChain = nullptr;
			Int32 Count = 0;
		public:
			ValueDef(TreeNode* value, TreeNode* origin) :
				UnaryNode(NodeKinds::ValueDef),
				Value(value),
				Origin(origin) {}
		};

		struct ValueUse : UnaryNode
		{
			ValueDef* Def;
			ValueUse* Prev = nullptr;
		public:
			ValueUse(ValueDef* def) :
				UnaryNode(NodeKinds::ValueUse),
				Def(def) {}
		};

		struct UndefinedValueNode : TreeNode
		{
		public:
			UndefinedValueNode() : TreeNode(NodeKinds::UndefinedValue) {}
			static UndefinedValueNode& Instance()
			{
				static UndefinedValueNode node;
				return node;
			}
		};

		/// <summary>
		/// Phi node to choose branch
		/// </summary>
		struct PhiNode : TreeNode
		{
			/// <summary>
			/// As a unary node when traversing
			/// </summary>
			TreeNode* OriginValue;
			/// <summary>
			/// Belonging basic block
			/// </summary>
			BasicBlock* Belonging;
			/// <summary>
			/// Choices
			/// </summary>
			std::vector<TreeNode*> Choices;
		public:
			PhiNode(BasicBlock* belongs, TreeNode* originValue) :
				TreeNode(NodeKinds::Phi),
				Belonging(belongs),
				OriginValue(originValue)
			{
			}
			bool IsRemoved()const {
				return Belonging == nullptr;
			}
			bool IsEmpty()const {
				return Choices.size() == 0;
			}
		};
	}

#define CASE_UNARY \
			case NodeKinds::Load: \
			case NodeKinds::Convert: \
			case NodeKinds::Cast: \
			case NodeKinds::Box: \
			case NodeKinds::UnBox: \
			case NodeKinds::InstanceField: \
			case NodeKinds::UnaryArithmetic: \
			case NodeKinds::Duplicate: \
			case NodeKinds::Phi: \
			case NodeKinds::OffsetOf:

#define CASE_BINARY \
			case NodeKinds::Store: \
			case NodeKinds::Array: \
			case NodeKinds::Compare: \
			case NodeKinds::BinaryArithmetic: \
			case NodeKinds::ArrayOffsetOf:

#define CASE_MULTIPLE \
			case NodeKinds::Call: \
			case NodeKinds::New: \
			case NodeKinds::NewArray:

#define CASE_POTENTIAL_CIRCULAR_REF \
			case NodeKinds::Use: \
			case NodeKinds::ValueDef: \
			case NodeKinds::ValueUse:

	template<class Fn>
	static void TraverseTree(Int8* stackSpace, Int32 upperBound, TreeNode*& source, Fn&& action)
	{
		using NodeReference = TreeNode**;
		NodeReference* stack = (NodeReference*)stackSpace;

		Int32 index = 0;

		auto pushStack = [&](NodeReference value) {
			if (index < upperBound)
				stack[index++] = value;
		};

		auto popStack = [&]() -> NodeReference {
			return stack[--index];
		};

		//Push stack first
		pushStack(&source);

		while (index > 0)
		{
			auto&& current = *popStack();
			//Do custom action
			std::forward<Fn>(action)(current);
			switch (current->Kind)
			{
			//Binary access
			CASE_BINARY
			{
				BinaryNodeAccessProxy* proxy = (BinaryNodeAccessProxy*)current;
				pushStack(&proxy->First);
				pushStack(&proxy->Second);
				break;
			}
			//Unary access
			CASE_UNARY
			{
				UnaryNodeAccessProxy* proxy = (UnaryNodeAccessProxy*)current;
				pushStack(&proxy->Value);
				break;
			}
			case NodeKinds::MorphedCall:
			{
				auto call = current->As<MorphedCallNode>();
				pushStack(&call->Origin);
			}
			//Multiple access 
			CASE_MULTIPLE
			{
				MultipleNodeAccessProxy* proxy = (MultipleNodeAccessProxy*)current;
				ForeachInlined(proxy->Values, proxy->Count, 
					[&](TreeNode*& node) 
					{
						pushStack(&node);
					});
				break;
			}
			CASE_POTENTIAL_CIRCULAR_REF
			{

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

		auto pushStack = [&](NodeReference value) {
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
			CASE_BINARY
			{
				BinaryNodeAccessProxy* proxy = (BinaryNodeAccessProxy*)current;
				pushStack(&proxy->First);
				pushStack(&proxy->Second);
				break;
			}
			//Unary access
			CASE_UNARY
			{
				UnaryNodeAccessProxy* proxy = (UnaryNodeAccessProxy*)current;
				pushStack(&proxy->Value);
				break;
			}
			case NodeKinds::MorphedCall:
			{
				auto call = current->As<MorphedCallNode>();
				pushStack(&call->Origin);
			}
			//Multiple access 
			CASE_MULTIPLE
			{
				MultipleNodeAccessProxy* proxy = (MultipleNodeAccessProxy*)current;
				ForeachInlined(proxy->Values, proxy->Count,
					[&](TreeNode*& node)
					{
						pushStack(&node);
					});
				break;
			}
			CASE_POTENTIAL_CIRCULAR_REF
			{

			}
			}
		}

		while (--index >= 0)
			std::forward<Fn>(action)(*stack[index]);
	}

	template<class Fn>
	static void ForeachStatement(BasicBlock* bbHead, Fn&& action)
	{
		for (BasicBlock* bbIterator = bbHead;
			bbIterator != nullptr;
			bbIterator = bbIterator->Next)
		{
			for (Statement* currentStmt = bbIterator->Now;
				currentStmt != nullptr && currentStmt->Now != nullptr;
				currentStmt = currentStmt->Next)
			{
				std::forward<Fn>(action)(currentStmt->Now, false);
			}

			if (bbIterator->BranchConditionValue != nullptr)
				std::forward<Fn>(action)(bbIterator->BranchConditionValue, true);
		}
	}

	template<class Fn>
	static void TraverseChildren(TreeNode* source, Fn&& action)
	{
		switch (source->Kind)
		{
			//Binary access
			CASE_BINARY
			{
				BinaryNodeAccessProxy * proxy = (BinaryNodeAccessProxy*)source;
				std::forward<Fn>(action)(proxy->First);
				std::forward<Fn>(action)(proxy->Second);
				break;
			}		
			CASE_UNARY
			{
				UnaryNodeAccessProxy * proxy = (UnaryNodeAccessProxy*)source;
				std::forward<Fn>(action)(proxy->Value);
				break;
			}
			case NodeKinds::MorphedCall:
			{
				auto call = source->As<MorphedCallNode>();
				std::forward<Fn>(action)(call->Origin);
			}
			//Multiple access 
			CASE_MULTIPLE
			{
				MultipleNodeAccessProxy * proxy = (MultipleNodeAccessProxy*)source;
				ForeachInlined(proxy->Values, proxy->Count,
					[&](TreeNode*& node)
					{
						std::forward<Fn>(action)(node);
					});
				break;
			}
		}
	}
}