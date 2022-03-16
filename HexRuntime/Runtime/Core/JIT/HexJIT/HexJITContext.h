#pragma once
#include "..\..\..\RuntimeAlias.h"
#include "..\..\..\Utility.h"
#include "..\..\Memory\SegmentHeap.h"
#include "..\..\Meta\TypeDescriptor.h"
#include "..\..\Meta\AssemblyContext.h"
#include "..\..\Meta\MetaManager.h"
#include "..\JITContext.h"
#include "EmitPage.h"
#include "..\IR.h"
#include <vector>
#include <type_traits>

namespace RTJ::Hex
{
	class LocalAttachedFlags
	{
	public:
		ETY = UInt32;
		VAL Trackable = 0x00000001;
		VAL JITGenerated = 0x00000002;
		VAL AddressTaken = 0x00000004;
		VAL ShouldGenerateLayout = 0x00000008;
		VAL Unused = 0x80000000;
	};

	template<class T>
	concept LocalOrArgument = requires 
	{
		std::is_same_v<RTM::MethodArgumentDescriptor, T> || std::is_same_v<RTM::MethodLocalVariableDescriptor, T>;
	};

	struct LocalVariableAttachedBase
	{
		UInt32 Flags = 0;
		Int32 Offset = InvalidOffset;
		static constexpr Int32 InvalidOffset = std::numeric_limits<Int32>::max();
		bool IsTrackable()const {
			return Flags & LocalAttachedFlags::Trackable;
		};
		bool IsJITGenerated()const {
			return Flags & LocalAttachedFlags::JITGenerated;
		}
		LocalVariableAttachedBase(UInt32 flags): Flags(flags){}
	};

	template<LocalOrArgument T>
	struct LocalVariableAttached : public LocalVariableAttachedBase
	{
		union 
		{
			T* Origin = nullptr;
			RTM::Type* JITVariableType;
		};
	public:
		LocalVariableAttached(RTM::Type* type, UInt32 flags)
			:JITVariableType(type), LocalVariableAttachedBase(flags) {}

		LocalVariableAttached(T* origin, UInt32 flags)
			:Origin(origin), LocalVariableAttachedBase(flags) {}

		RTM::Type* GetType()const {
			if (IsJITGenerated())
				return JITVariableType;
			else
				return Origin->GetType();
		}
	};

	using LocalAttached = LocalVariableAttached<RTM::MethodLocalVariableDescriptor>;
	using ArgumentAttached = LocalVariableAttached<RTM::MethodArgumentDescriptor>;

	struct HexJITContext 
	{
		/// <summary>
		/// Allocator
		/// </summary>
		RTMM::SegmentHeap* Memory = nullptr;
		JITContext* Context = nullptr;
		/// <summary>
		/// The attached local info
		/// </summary>
		std::vector<LocalAttached> LocalAttaches;
		/// <summary>
		/// The attached argument info
		/// </summary>
		std::vector<ArgumentAttached> ArgumentAttaches;
		/// <summary>
		/// Index to basic block
		/// </summary>
		std::vector<BasicBlock*> BBs;
		/// <summary>
		/// Topologically sorted basic block
		/// </summary>
		std::vector<BasicBlock*> TopologicallySortedBBs;
		/// <summary>
		/// Shared space for evaluation stack and tree traversal
		/// </summary>
		struct 
		{
			Int8* Space;
			Int32 Count;
		} Traversal;

		EmitPage* NativeCode = nullptr;
		RTM::Type* GetLocalType(UInt16 localIndex)
		{
			UInt16 index = LocalVariableNode::GetIndex(localIndex);
			if (LocalVariableNode::IsArgument(localIndex))
				return ArgumentAttaches[index].GetType();
			else
				return LocalAttaches[index].GetType();
		}

		LocalVariableAttachedBase* GetLocalBase(UInt16 localIndex)
		{
			UInt16 index = LocalVariableNode::GetIndex(localIndex);
			if (LocalVariableNode::IsArgument(localIndex))
				return &ArgumentAttaches[index];
			else
				return &LocalAttaches[index];
		}
	};
}