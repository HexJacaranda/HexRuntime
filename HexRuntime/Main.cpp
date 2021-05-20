#include "Runtime/Core/JIT/ILEmitter.h"
#include "Runtime/Core/JIT/OpCodes.h"
#include "Runtime/Core/Type/CoreTypes.h"
#include "Runtime/RuntimeAlias.h"
#include "Runtime/Core/JIT/JITContext.h"
#include "Runtime/Core/JIT/HexJIT/Frontend/Transformer.h"

using namespace RTJ;
using namespace RTC;

int main()
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

	Hex::ILTransformer transformer{ context };
	auto bb = transformer.TransformILFrom();
}