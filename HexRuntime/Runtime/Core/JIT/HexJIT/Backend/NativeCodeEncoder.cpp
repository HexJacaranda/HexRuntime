#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "ConcreteInstruction.h"

namespace RTJ::Hex
{
	enum PatchKind : UInt8
	{
		BranchRelativeOffset,
		BranchAbsoluteOffset
	};

	struct Patch
	{
		Int32 Offset;
		PatchKind Kind;
		UInt8 PatchSize;
		Int32 BasicBlockIndex;
	};

	template<class Fn>
	concept PatchGenerationEventT = requires (Patch patch, Fn && action)
	{
		patch = std::forward<Fn>(action)(patch);
	};

	template<UInt32 Architecture, UInt32 Width>
	class NativeCodeEncoder
	{
	public:
		template<PatchGenerationEventT Fn>
		void Encode(ConcreteInstruction const&, Fn&& patchEvent)
		{

		}
	};

#define SIZE_I1 sizeof(UInt8)
#define SIZE_I2 sizeof(UInt16)
#define SIZE_I4 sizeof(UInt32)
#define SIZE_I8 sizeof(UInt64)

#define PATCH_BR_REL(BBINDEX, SIZE) std::forward<Fn>(patchEvent)( \
				Patch { mPage->CurrentOffset(), PatchKind::BranchRelativeOffset, SIZE, BBINDEX })

#define PATCH_BR_ABS(BBINDEX, SIZE) std::forward<Fn>(patchEvent)( \
				Patch { mPage->CurrentOffset(), PatchKind::BranchAbsoluteOffset, SIZE, BBINDEX })
}