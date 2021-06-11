#include "Runtime/Core/JIT/ILEmitter.h"
#include "Runtime/Core/JIT/OpCodes.h"
#include "Runtime/Core/Type/CoreTypes.h"
#include "Runtime/RuntimeAlias.h"
#include "Runtime/Core/JIT/JITContext.h"
#include "Runtime/Core/JIT/HexJIT/JITMemory.h"
#include "Runtime/Core/JIT/HexJIT/Frontend/Transformer.h"
#include "Runtime/Core/JIT/HexJIT/Frontend/SSABuilder.h"
#include "Runtime/Core/JIT/HexJIT/Frontend/SSAOptimizer.h"

using namespace RTJ;
using namespace RTC;

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
	il.Emit(OpCodes::Ret, 0x10);

	auto update = il.GetOffset();
	il.UpdateEntry(entry, update);

	il.EmitLoad(OpCodes::LdLoc, 1);
	il.Emit(OpCodes::Ret, 0x10);
}

void SSABuildAndOptimize(ILEmitter const& il)
{
	JITContext context;
	context.CodeSegment = il.GetIL();
	context.SegmentLength = il.GetLength();
	context.LocalVariables.push_back({ { 0, CoreTypes::I4 } });
	context.LocalVariables.push_back({ { 0, CoreTypes::I4 } });

	Hex::JITMemory hexMemory;

	Hex::HexJITContext hexContext;
	hexContext.Context = &context;
	hexContext.Memory = &hexMemory;
	
	
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
	PrepareIL(il);
	for (int i = 0; i < 100000; ++i)
		SSABuildAndOptimize(il);
}