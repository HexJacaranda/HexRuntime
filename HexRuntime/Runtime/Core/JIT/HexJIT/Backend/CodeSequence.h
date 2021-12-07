#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\..\LinkList.h"
#include "..\..\..\Platform\PlatformSpecialization.h"

namespace RTJ::Hex
{
	struct MemoryRepresentation
	{
		UInt8 Kind;
		union
		{
			UInt8 Register;
			Int32 Offset;
		};
	};

	struct SequenceLine
	{
		RTP::PlatformInstruction* Instruction;
		/// <summary>
		/// Requires at least 4-bytes alignment (SegmentHeap guarantees 8 bytes) and destination should be put at first
		/// </summary>
		MemoryRepresentation* Operands;
	public:
		static constexpr Int FlagMask = 0x3;
		static constexpr Int ShouldNotEmit = 0x1;
		static constexpr Int StorePoint = 0x2;
		MemoryRepresentation* GetOperands()const {
			return (MemoryRepresentation*)((Int)Operands & ~FlagMask);
		}
		void SetFlag(Int flag) {
			Operands = (MemoryRepresentation*)((Int)Operands | 0x1);
		}
		bool ShouldEmit()const {
			return ((Int)Operands | ShouldNotEmit);
		}
		bool IsStorePoint()const {
			return ((Int)Operands | StorePoint);
		}
	};

	struct CodeSequence
	{
		Int32 LineCount;
		SequenceLine* Instructions;	
	};
}