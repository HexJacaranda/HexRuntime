#include "Runtime/Core/JIT/ILEmitter.h"
#include "Runtime/Core/JIT/OpCodes.h"
#include "Runtime/Core/Meta/CoreTypes.h"
#include "Runtime/RuntimeAlias.h"
#include "Runtime/Core/JIT/JITContext.h"
#include "Runtime/Core/Memory/SegmentMemory.h"
#include "Runtime/Core/JIT/HexJIT/Frontend/Transformer.h"
#include "Runtime/Core/JIT/HexJIT/Frontend/SSABuilder.h"
#include "Runtime/Core/JIT/HexJIT/Frontend/SSAOptimizer.h"
#include "Runtime/Core/InteriorPointer.h"
#include "Runtime/Core/Interlocked.h"
#include "Runtime/Core/Meta/MetaManager.h"

#undef _DEBUG

#include "Runtime/ConcurrentMap.h"


using namespace RTJ;
using namespace RTC;
using namespace RT;
using namespace RTM;
using namespace RT::Concurrent;

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

void JITTest()
{
	ILEmitter il;
	PrepareBackIL(il);
	SSABuildAndOptimize(il);
	for (int i = 0; i < 100000; ++i)
		SSABuildAndOptimize(il);
}

void TSTest()
{
	MetaData = new MetaManager();
	auto context = MetaData->StartUp(Text("HexRT.Core"));
	auto type = MetaData->GetTypeFromToken(context, 1);
}

template<class Fn>
static void ParallelRun(int threadCount, Fn&& action)
{
	std::vector<std::thread> threads{};

	for (int i = 0; i < threadCount; ++i)
		threads.push_back(std::thread(std::forward<Fn>(action), i));

	for (auto&& thread : threads)
		thread.join();
}

int main()
{
	ConcurrentMap<int, double> map{ 32 };
	ParallelRun(20, [&](int i)
		{
			for (int k = 0; k < 100000; ++k)
				map.GetOrAdd(k, []() { return 0.0; });
		});

	//ConcurrentMap<int, double> map{ 32 };
	//std::vector<std::thread> threads{};

	//for (int i = 0; i < 20; ++i)
	//{
	//	threads.push_back(std::thread(
	//		[&, i]()
	//		{
	//			for (int k = 0; k < 100000; ++k)
	//				map.TryAdd(i * 100000 + k, 0);
	//		}
	//	));
	//}

	//for (auto&& thread : threads)
	//	thread.join();
}