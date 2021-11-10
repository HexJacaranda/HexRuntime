#pragma once
#include "..\..\..\RuntimeAlias.h"
#include "..\..\..\Utility.h"
#include "..\..\Memory\SegmentMemory.h"
#include "..\..\Meta\TypeDescriptor.h"
#include "..\..\Meta\AssemblyContext.h"
#include "..\..\Meta\MetaManager.h"
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
		union 
		{
			T* Origin = nullptr;
			RTM::Type* JITVariableType;
		};
		
		UInt32 Flags = 0;
		Int32 UseCount = 0;
	public:
		LocalVariableAttached(RTM::Type* type, UInt32 flags)
			:JITVariableType(type), Flags(flags) {}

		LocalVariableAttached(T* origin, UInt32 flags)
			:Origin(origin), Flags(flags) {}

		bool IsTrackable()const { 
			return Flags & LocalAttachedFlags::Trackable; 
		};
		bool IsJITGenerated()const {
			return Flags & LocalAttachedFlags::JITGenerated;
		}
		RTM::Type* GetType(RTM::AssemblyContext* context)const {
			if (IsJITGenerated())
				return JITVariableType;
			else
				return Meta::MetaData->GetTypeFromToken(context, Origin->TypeRefToken);
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