#include "Runtime/Core/JIT/ILEmitter.h"
#include "Runtime/Core/JIT/OpCodes.h"
#include "Runtime/Core/Type/CoreTypes.h"
#include "Runtime/RuntimeAlias.h"
#include "Runtime/Core/JIT/JITContext.h"
#include "Runtime/Core/JIT/HexJIT/JITMemory.h"
#include "Runtime/Core/JIT/HexJIT/Frontend/Transformer.h"
#include "Runtime/Core/JIT/HexJIT/Frontend/SSABuilder.h"

using namespace RTJ;
using namespace RTC;

void BasicBlockPartition() 
{
	/*LdC 32
	* LdC 4.0
	* Conv I4
	* Div
	* LdC 8
	* Cmp EQ
	* Jcc label
	* LdC 64
	* Ret
	* .label:
	* LdC 32
	* Ret
	*/

	ILEmitter il;
	il.EmitLdC(CoreTypes::I4, 32);
	il.EmitLdC(CoreTypes::R4, 4.0f);
	il.EmitConv(CoreTypes::R4, CoreTypes::I4);
	il.EmitAriOperation(OpCodes::Div, CoreTypes::I4);
	il.EmitLdC(CoreTypes::I4, 8);
	il.EmitCompare(CmpCondition::EQ);

	auto entry = il.EmitJcc(0);

	il.EmitLdC(CoreTypes::I4, 64);
	il.Emit(OpCodes::Ret, 0x10);

	auto update = il.GetOffset();
	il.UpdateEntry(entry, update);

	il.EmitLdC(CoreTypes::I4, 32);
	il.Emit(OpCodes::Ret, 0x10);

	JITContext context;
	context.CodeSegment = il.GetIL();
	context.SegmentLength = il.GetLength();

	Hex::HexJITContext hexContext;
	hexContext.Context = &context;

	Hex::ILTransformer transformer{ &hexContext , new Hex::JITMemory() };
	auto bb = transformer.TransformILFrom();
}

void SSABuild()
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

	ILEmitter il;
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

	JITContext context;
	context.CodeSegment = il.GetIL();
	context.SegmentLength = il.GetLength();
	context.LocalVariables.push_back({ { 0, CoreTypes::I4 } });
	context.LocalVariables.push_back({ { 0, CoreTypes::I4 } });

	Hex::HexJITContext hexContext;
	hexContext.Context = &context;

	Hex::JITMemory hexMemory;
	
	Hex::ILTransformer transformer{ &hexContext , &hexMemory };
	auto bb = transformer.TransformILFrom();

	Hex::SSABuilder ssaBuilder{ &hexContext };
	bb = ssaBuilder.Build();
}

int main()
{
	SSABuild();
}