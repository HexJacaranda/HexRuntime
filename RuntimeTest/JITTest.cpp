#include "pch.h"
#include "CppUnitTest.h"
#include "../HexRuntime/Runtime/RuntimeAlias.h"
#include "../HexRuntime/Runtime/Core/JIT/ILEmitter.h"
#include "../HexRuntime/Runtime/Core/JIT/OpCodes.h"
#include "../HexRuntime/Runtime/Core/Meta/CoreTypes.h"
#include "../HexRuntime/Runtime/Core/JIT/JITContext.h"
#include "../HexRuntime/Runtime/Core/Memory/SegmentMemory.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Frontend/Transformer.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Frontend/SSABuilder.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Frontend/SSAOptimizer.h"

using namespace RTJ;
using namespace RTC;
using namespace RT;

namespace RuntimeTest
{
	TEST_CLASS(JITTest)
	{
	public:
		void PrepareIL(ILEmitter& il)
		{
			/*LdC 2
			* StLoc 0
			* LdC 3
			* StLoc 1
			* LdLoc 0
			* LdLoc 1
			* Cmp EQ
			* Jcc label
			* LdLoc 0
			* Ret
			* .label:
			* LdLoc 1
			* Ret
			*/

			il.EmitLdC(CoreTypes::I4, 2);
			il.EmitStore(OpCodes::StLoc, 0);
			il.EmitLdC(CoreTypes::I4, 3);
			il.EmitStore(OpCodes::StLoc, 1);

			il.EmitLoad(OpCodes::LdLoc, 0);
			il.EmitLoad(OpCodes::LdLoc, 1);
			il.EmitCompare(CmpCondition::EQ);

			auto entry = il.EmitJcc(0);
			il.EmitLoad(OpCodes::LdLoc, 0);
			il.EmitRet(0x10);

			auto update = il.GetOffset();
			il.UpdateEntry(entry, update);

			il.EmitLoad(OpCodes::LdLoc, 1);
			il.EmitRet(0x10);
		}

		void PrepareBackIL(ILEmitter& il)
		{
			il.EmitLdC(CoreTypes::I4, 2);
			il.EmitStore(OpCodes::StLoc, 0);

			auto offset = il.GetLength();

			il.EmitLdC(CoreTypes::I4, 3);
			il.EmitStore(OpCodes::StLoc, 1);

			il.EmitLoad(OpCodes::LdLoc, 0);
			il.EmitLoad(OpCodes::LdLoc, 1);
			il.EmitCompare(CmpCondition::EQ);
			il.EmitJcc(offset);
			il.EmitRet(0x00);
		}

		void SSABuildAndOptimize(ILEmitter const& il)
		{
			JITContext context{};

			Memory::SegmentMemory hexMemory;

			Hex::HexJITContext hexContext;
			hexContext.Context = &context;
			hexContext.Memory = &hexMemory;

			hexContext.Traversal.Count = 4096;
			hexContext.Traversal.Space = (Int8*)hexMemory.Allocate(sizeof(void*) * hexContext.Traversal.Count);

			Hex::ILTransformer transformer{ &hexContext };
			auto bb = transformer.TransformILFrom();

			Hex::SSABuilder ssaBuilder{ &hexContext };
			bb = ssaBuilder.Build();

			Hex::SSAOptimizer ssaOptimizer{ &hexContext };
			bb = ssaOptimizer.Optimize();
		}

		TEST_METHOD(FrontendOptimizeTest)
		{
			ILEmitter il;
			PrepareBackIL(il);
			SSABuildAndOptimize(il);
		}
	};
}