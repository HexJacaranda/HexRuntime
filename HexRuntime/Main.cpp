#include "Runtime/Core/JIT/ILEmitter.h"
#include "Runtime/Core/JIT/OpCodes.h"
#include "Runtime/Core/Meta/CoreTypes.h"
#include "Runtime/RuntimeAlias.h"
#include "Runtime/Core/JIT/JITContext.h"
#include "Runtime/Core/JIT/HexJIT/JITMemory.h"
#include "Runtime/Core/JIT/HexJIT/Frontend/Transformer.h"
#include "Runtime/Core/JIT/HexJIT/Frontend/SSABuilder.h"
#include "Runtime/Core/JIT/HexJIT/Frontend/SSAOptimizer.h"
#include "Runtime/Core/InteriorPointer.h"


using namespace RTJ;
using namespace RTC;
using namespace RT;

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
	JITContext context;

	Hex::JITMemory hexMemory;

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

int main()
{
	ILEmitter il;
	PrepareBackIL(il);
	SSABuildAndOptimize(il);
	//for (int i = 0; i < 100000; ++i)
	//	SSABuildAndOptimize(il);
}