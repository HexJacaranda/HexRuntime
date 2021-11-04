#pragma once
#include "..\..\..\RuntimeAlias.h"
#include "..\..\..\Utility.h"
#include "..\..\Memory\SegmentMemory.h"
#include "..\JITContext.h"
#include "Frontend\IR.h"
#include <vector>
#include <type_traits>

namespace RTJ::Hex
{
	class LocalAttachedFlags
	{
	public:
		UNDERLYING_TYPE(UInt32);
		VALUE(Trackable) = 0x00000001;
		VALUE(JITGenerated) = 0x00000002;
	};

	template<class T>
	concept LocalOrArgument = requires 
	{ 
		std::is_same_v<T, RTME::ArgumentMD> || std::is_same_v<T, RTME::LocalVariableMD>;
	};

	template<LocalOrArgument T>
	struct LocalVariableAttached
	{
		T* Origin;
		UInt32 Flags;
	public:
		bool IsTrackable()const { 
			return Flags & LocalAttachedFlags::Trackable; 
		};
		bool IsJITGenerated()const {
			return Flags & LocalAttachedFlags::JITGenerated;
		}
	};

	using LocalAttached = LocalVariableAttached<RTME::LocalVariableMD>;
	using ArgumentAttached = LocalVariableAttached<RTME::ArgumentMD>;

	struct HexJITContext 
	{
		/// <summary>
		/// Allocator
		/// </summary>
		RTMM::SegmentMemory* Memory;
		JITContext* Context;
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
		/// Shared space for evaluation stack and tree traversal
		/// </summary>
		struct 
		{
			Int8* Space;
			Int32 Count;
		} Traversal;
	};
}