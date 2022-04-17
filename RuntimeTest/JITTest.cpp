#include "pch.h"
#include "CppUnitTest.h"
#include "../HexRuntime/Runtime/Core/Meta/MetaManager.h"
#include "../HexRuntime/Runtime/RuntimeAlias.h"
#include "../HexRuntime/Runtime/Core/JIT/ILEmitter.h"
#include "../HexRuntime/Runtime/Core/JIT/OpCodes.h"
#include "../HexRuntime/Runtime/Core/Meta/CoreTypes.h"
#include "../HexRuntime/Runtime/Core/JIT/JITContext.h"
#include "../HexRuntime/Runtime/Core/Memory/SegmentHeap.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Frontend/Transformer.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Frontend/SSABuilder.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Frontend/ConstantFolder.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Frontend/FlowGraphPruner.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Backend/SSAReducer.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Backend/Linearizer.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Frontend/Materializer.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Backend/LivenessAnalyzer.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Backend/X86CodeInterpreter.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/JITDebugView.h"
#include "../HexRuntime/Runtime/Core/Object/Object.h"
#include <format>
#include <fstream>
#include <filesystem>

using namespace RTJ;
using namespace RTJ::Hex;
using namespace RTC;
using namespace RT;
using namespace RTM;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Microsoft::VisualStudio::CppUnitTestFramework
{
	template<> inline std::wstring ToString<Hex::PPKind>(const Hex::PPKind& t)
	{
		switch (t)
		{
		case Hex::PPKind::Conditional:
			return L"Conditional";
		case Hex::PPKind::Unconditional:
			return L"Unconditional";
		case Hex::PPKind::Target:
			return L"Target";
		case Hex::PPKind::Sequential:
			return L"Sequential";
		case Hex::PPKind::Ret:
			return L"Ret";
		default:
			return L"Unknown";
		}
	}

	template<> inline std::wstring ToString<Hex::BasicBlock>(Hex::BasicBlock* t)
	{
		if (t == nullptr)
			return L"null";
		return std::format(L"Basic Block - Index: {}", t->Index);
	}
}


namespace RuntimeTest
{
	struct CodeDumper
	{
	public:
		std::wstring Directory;
		HexJITContext* Context = nullptr;

		void* operator()() {
			std::wstring_view methodName = ToStringView(Context->Context->MethDescriptor->GetName());
			auto finalDumpPath = std::format(L"{}/{}.bin", Directory, methodName);

			auto code = (const char*)Context->NativeCode->GetExecuteableCode();
			std::fstream fstream{ finalDumpPath, std::ios::out | std::ios::binary };
			fstream.write(code, Context->NativeCode->GetCodeSize());
			fstream.close();
			return (void*)code;
		}
	};

	template<class FlowT, class...FlowTs>
	Hex::BasicBlock* PassThrough(HexJITContext* context)
	{
		FlowT flow{ context };
		auto bb = flow.PassThrough();
		if constexpr (sizeof...(FlowTs) == 0)
			return bb;
		else
			return PassThrough<FlowTs...>(context);
	}

	void ViewIR(BasicBlock* bb)
	{
		auto content = JITDebugView::ViewIR(bb);
		Logger::WriteMessage(content.c_str());
	}

	void CheckOrCreateDumpCodeDirectory(const char* directory)
	{
		if (std::filesystem::exists(directory))
			return;
		std::filesystem::create_directories(directory);
	}

	void SetUpMethod(
		AssemblyContext* assembly,
		HexJITContext* context,
		std::wstring_view const& typeName,
		std::wstring_view const& name)
	{
		auto type = Meta::MetaData->GetTypeFromFQN(typeName);
		auto table = type->GetMethodTable();
		for (auto&& method : table->GetMethods())
		{
			auto methodName = method->GetName();
			if (name == methodName->GetContent())
			{
				context->Context->MethDescriptor = method;
				context->Context->Assembly = assembly;
				break;
			}
		}

		auto method = context->Context->MethDescriptor;
		for (auto&& local : method->GetLocalVariables())
			context->LocalAttaches.push_back({ &local, 0 });

		for (auto&& argument : method->GetSignature()->GetArguments())
			context->ArgumentAttaches.push_back({ &argument, 0 });

		Assert::IsNotNull(context->Context->MethDescriptor, L"Method not found");
	}

#define SET_UP_METHOD_FOR(NAME) \
	void SetUpMethod(std::wstring_view const& name) \
	{ \
		RuntimeTest::SetUpMethod(Assembly, Context, L"[JIT][Test]"#NAME, name);\
	} 
		

	TEST_CLASS(JITTest)
	{
		Hex::HexJITContext* context = nullptr;
		static RTM::AssemblyContext* assembly;
	public:
		template<class FlowT, class...FlowTs>
		Hex::BasicBlock* PassThrough()
		{
			return RuntimeTest::PassThrough<FlowT, FlowTs...>(context);
		}

		void* DumpNativeCode(const char* name)
		{
			auto code = (const char*)context->NativeCode->GetExecuteableCode();
			std::fstream fstream{ name, std::ios::out | std::ios::binary };
			fstream.write(code, context->NativeCode->GetCodeSize());
			fstream.close();
			return (void*)code;
		}

		void SetUpMethod(std::wstring_view name)
		{
			auto type = Meta::MetaData->GetTypeFromDefinitionToken(assembly, 0u);
			auto table = type->GetMethodTable();
			for (auto&& method : table->GetMethods())
			{
				auto methodName = method->GetName();
				if (name == methodName->GetContent())
				{
					context->Context->MethDescriptor = method;
					context->Context->Assembly = assembly;
					break;
				}
			}

			auto method = context->Context->MethDescriptor;
			for (auto&& local : method->GetLocalVariables())
				context->LocalAttaches.push_back({ &local, 0 });

			for (auto&& argument : method->GetSignature()->GetArguments())
				context->ArgumentAttaches.push_back({ &argument, 0 });

			Assert::IsNotNull(context->Context->MethDescriptor, L"Method not found");
		}

		TEST_CLASS_INITIALIZE(InitializeMetaManager)
		{
			Meta::MetaData = new Meta::MetaManager();
			assembly = Meta::MetaData->StartUp(Text("JIT"));
		}

		TEST_CLASS_CLEANUP(CleanUpMetaManager)
		{
			Meta::MetaData->ShutDown();
			delete Meta::MetaData;
		}

		TEST_METHOD_INITIALIZE(InitializeContext)
		{
			context = new Hex::HexJITContext();
			context->Context = new JITContext();
			context->Memory = new Memory::SegmentHeap();

			context->Traversal.Count = 4096;
			context->Traversal.Space = (Int8*)context->Memory->Allocate(sizeof(void*) * context->Traversal.Count);
		}

		TEST_METHOD_CLEANUP(CleanUpContext)
		{
			delete context->Memory;
			delete context;
		}

		TEST_METHOD(ILTransformingTest)
		{
			SetUpMethod(L"PreTest");
			auto bb = PassThrough<Hex::ILTransformer>();
			ViewIR(bb);

			Assert::IsNotNull(bb, L"Basic block is null");
			Assert::IsNotNull(bb->BranchConditionValue, L"Basic block condition value is null");
		}

		TEST_METHOD(LinearizingTest)
		{
			SetUpMethod(L"LinearizeTest");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);

			Assert::AreEqual(4, (Int32)context->LocalAttaches.size(), L"Two JIT variables expected");
			auto assertArgument = [](TreeNode* arg) {
				if (!arg->Is(NodeKinds::Load))
					Assert::Fail(L"Argument node after linearization should be of type Load");
				auto load = arg->As<LoadNode>();
				switch (load->Source->Kind)
				{
				case NodeKinds::Constant:
				case NodeKinds::LocalVariable:
					break;
				default:
					Assert::Fail(L"The loading value of argument should be either constant or variable.");
				}
			};

			ForeachStatement(bb,
				[&](TreeNode* node, bool isCond)
				{
					if (!isCond)
					{
						if (!node->Is(NodeKinds::Store) &&
							!node->Is(NodeKinds::MorphedCall))
							Assert::Fail(L"Nodes other than Store/MorphedCall occurs");
						if (node->Is(NodeKinds::MorphedCall))
						{
							//Check argument
							auto call = node->As<MorphedCallNode>();
							ForeachInlined(call->Arguments, call->ArgumentCount, assertArgument);

							if (call->Origin->Is(NodeKinds::Call))
							{
								auto originCall = call->Origin->As<CallNode>();
								ForeachInlined(originCall->Arguments, originCall->ArgumentCount, assertArgument);
							}
						}
					}
				});
		}

		TEST_METHOD(SSABuildingTest)
		{
			SetUpMethod(L"SSABuildTest");
			auto bb = PassThrough<ILTransformer, SSABuilder>();
			ViewIR(bb);
		}

		TEST_METHOD(SSAOptimizingTest)
		{
			SetUpMethod(L"SSAOptimizationTest");
			auto bb = PassThrough<ILTransformer, SSABuilder, ConstantFolder, FlowGraphPruner>();
			ViewIR(bb);

			Assert::AreEqual(PPKind::Sequential, bb->BranchKind, L"First bb should be sequential");
			Assert::IsNull(bb->BranchedBB, L"Branch should be next BB");
		}

		TEST_METHOD(SSAReducingTest)
		{
			SetUpMethod(L"SSAOptimizationTest");
			auto bb = PassThrough<ILTransformer, SSABuilder, SSAReducer>();
			ViewIR(bb);

			for (BasicBlock* bbIterator = bb;
				bbIterator != nullptr;
				bbIterator = bbIterator->Next)
			{
				for (Statement* stmtIterator = bbIterator->Now;
					stmtIterator != nullptr && stmtIterator->Now != nullptr;
					stmtIterator = stmtIterator->Next)
				{
					TraverseTree(
						context->Traversal.Space,
						context->Traversal.Count,
						stmtIterator->Now,
						[](TreeNode*& node)
						{
							switch (node->Kind)
							{
							case NodeKinds::Use:
							case NodeKinds::ValueDef:
							case NodeKinds::ValueUse:
							case NodeKinds::Phi:
								Assert::Fail(L"SSA node should not exist");
							}
						});
				}
			}
		}

		TEST_METHOD(CodeGenTest1)
		{
			using Fn = int(*)();

			SetUpMethod(L"CodeGenTest1");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenTest1.bin");
			Assert::AreEqual(5, method());
		}

		TEST_METHOD(CodeGenTest2)
		{
			using Fn = int(*)(int, int, int, int);

			SetUpMethod(L"CodeGenTest2");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenTest2.bin");
			Assert::AreEqual(5, method(0, 0, 2, 3));
		}

		TEST_METHOD(CodeGenTest3)
		{
			using Fn = int(__fastcall*)(int, int, int, int);

			SetUpMethod(L"CodeGenTest3");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenTest3.bin");
			Int32 ret = method(0, 0, 2, 3);
			Assert::AreEqual(0, ret);
		}

		TEST_METHOD(CodeGenFloatImm)
		{
			using Fn = float(__fastcall*)();
			SetUpMethod(L"CodeGenFloatImm");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenFloatImm.bin");
			Float ret = method();
			Assert::AreEqual(3.14f, ret);
		}

		TEST_METHOD(CodeGenDoubleImm)
		{
			using Fn = double(__fastcall*)();
			SetUpMethod(L"CodeGenDoubleImm");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenDoubleImm.bin");
			auto ret = method();
			Assert::AreEqual(2.718, ret);
		}

		TEST_METHOD(CodeGenIntAdd)
		{
			using Fn = int(__fastcall*)(int);
			SetUpMethod(L"CodeGenIntAdd");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenIntAdd.bin");
			auto ret = method(1);
			Assert::AreEqual(0, ret);
		}

		TEST_METHOD(CodeGenIntSub)
		{
			using Fn = int(__fastcall*)(int);
			SetUpMethod(L"CodeGenIntSub");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenIntSub.bin");
			auto ret = method(1);
			Assert::AreEqual(2, ret);
		}

		TEST_METHOD(CodeGenIntMul)
		{
			using Fn = int(__fastcall*)(int);
			SetUpMethod(L"CodeGenIntMul");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenIntMul.bin");
			auto ret = method(1);
			Assert::AreEqual(3, ret);
		}

		TEST_METHOD(CodeGenIntDiv)
		{
			using Fn = int(__fastcall*)(int);
			SetUpMethod(L"CodeGenIntDiv");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenIntDiv.bin");
			auto ret = method(5);
			Assert::AreEqual(2, ret);
		}

		TEST_METHOD(CodeGenFloatAdd)
		{
			using Fn = float(__fastcall*)(float);
			SetUpMethod(L"CodeGenFloatAdd");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenFloatAdd.bin");
			auto ret = method(2.0f);
			Assert::AreEqual(0.0f, ret);
		}

		TEST_METHOD(CodeGenFloatSub)
		{
			using Fn = float(__fastcall*)(float);
			SetUpMethod(L"CodeGenFloatSub");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenFloatSub.bin");
			auto ret = method(4.0f);
			Assert::AreEqual(2.0f, ret);
		}

		TEST_METHOD(CodeGenFloatMul)
		{
			using Fn = float(__fastcall*)(float);
			SetUpMethod(L"CodeGenFloatMul");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenFloatMul.bin");
			auto ret = method(2.0f);
			Assert::AreEqual(4.0f, ret);
		}

		TEST_METHOD(CodeGenFloatDiv)
		{
			using Fn = float(__fastcall*)(float);
			SetUpMethod(L"CodeGenFloatDiv");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenFloatDiv.bin");
			auto ret = method(5.0f);
			Assert::AreEqual(2.5f, ret);
		}

		TEST_METHOD(CodeGenDoubleAdd)
		{
			using Fn = Double(__fastcall*)(Double);
			SetUpMethod(L"CodeGenDoubleAdd");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenDoubleAdd.bin");
			auto ret = method(2.0);
			Assert::AreEqual(0.0, ret);
		}

		TEST_METHOD(CodeGenDoubleSub)
		{
			using Fn = Double(__fastcall*)(Double);
			SetUpMethod(L"CodeGenDoubleSub");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenDoubleSub.bin");
			auto ret = method(4.0);
			Assert::AreEqual(2.0, ret);
		}

		TEST_METHOD(CodeGenDoubleMul)
		{
			using Fn = Double(__fastcall*)(Double);
			SetUpMethod(L"CodeGenDoubleMul");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenDoubleMul.bin");
			auto ret = method(2.0);
			Assert::AreEqual(4.0, ret);
		}

		TEST_METHOD(CodeGenDoubleDiv)
		{
			using Fn = Double(__fastcall*)(Double);
			SetUpMethod(L"CodeGenDoubleDiv");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenDoubleDiv.bin");
			auto ret = method(5.0);
			Assert::AreEqual(2.5, ret);
		}

		TEST_METHOD(CodeGenIntMod)
		{
			using Fn = Int32(__fastcall*)(Int32, Int32);
			SetUpMethod(L"CodeGenIntMod");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenIntMod.bin");
			auto ret = method(7, 3);
			Assert::AreEqual(1, ret);
		}

		TEST_METHOD(CodeGenJcc)
		{
			using Fn = Int32(__fastcall*)(Int32, Int32);
			SetUpMethod(L"CodeGenJcc");
			auto bb = PassThrough<ILTransformer, Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenJcc.bin");
			auto ret = method(2, 2);
			Assert::AreEqual(4, ret);
			ret = method(2, 3);
			Assert::AreEqual(3, ret);
		}

		TEST_METHOD(CodeGenJcc1)
		{
			using Fn = Int32(__fastcall*)(Int32, Int32);
			SetUpMethod(L"CodeGenJcc1");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenJcc1.bin");
			auto ret = method(2, 2);
			Assert::AreEqual(4, ret);
			ret = method(2, 3);
			Assert::AreEqual(3, ret);
		}

		TEST_METHOD(CodeGenBoolAnd)
		{
			using Fn = bool(__fastcall*)(bool, bool);
			SetUpMethod(L"CodeGenBoolAnd");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenBoolAnd.bin");
			auto ret = method(true, false);
			Assert::AreEqual(false, ret);
		}

		TEST_METHOD(CodeGenBoolOr)
		{
			using Fn = bool(__fastcall*)(bool, bool);
			SetUpMethod(L"CodeGenBoolOr");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenBoolOr.bin");
			auto ret = method(false, true);
			Assert::AreEqual(true, ret);
		}

		TEST_METHOD(CodeGenBoolXor)
		{
			using Fn = bool(__fastcall*)(bool, bool);
			SetUpMethod(L"CodeGenBoolXor");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenBoolXor.bin");
			auto ret = method(false, true);
			Assert::AreEqual(true, ret);
		}

		TEST_METHOD(CodeGenIntNeg)
		{
			using Fn = Int32(__fastcall*)(Int32);
			SetUpMethod(L"CodeGenIntNeg");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenIntNeg.bin");
			auto ret = method(-1);
			Assert::AreEqual(1, ret);
		}

		TEST_METHOD(CodeGenFloatNeg)
		{
			using Fn = Float(__fastcall*)(Float);
			SetUpMethod(L"CodeGenFloatNeg");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenFloatNeg.bin");
			auto ret = method(3.14f);
			Assert::AreEqual(-3.14f, ret);
		}

		TEST_METHOD(CodeGenDoubleNeg)
		{
			using Fn = Double(__fastcall*)(Double);
			SetUpMethod(L"CodeGenDoubleNeg");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenDoubleNeg.bin");
			auto ret = method(2.718);
			Assert::AreEqual(-2.718, ret);
		}

		TEST_METHOD(CodeGenPreserveArg)
		{
			using Fn = bool(__fastcall*)(bool, bool);
			SetUpMethod(L"CodeGenPreserveArg");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenPreserveArg.bin");
			auto ret = method(false, true);
			Assert::AreEqual(true, ret);
		}

		TEST_METHOD(CodeGenI16ToI32)
		{
			using Fn = Int32(__fastcall*)(Int16);
			SetUpMethod(L"CodeGenI16ToI32");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenI16ToI32.bin");
			auto ret = method(16);
			Assert::AreEqual(16, ret);
		}

		TEST_METHOD(CodeGenI8ToI32)
		{
			using Fn = Int32(__fastcall*)(Int8);
			SetUpMethod(L"CodeGenI8ToI32");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenI8ToI32.bin");
			auto ret = method(16);
			Assert::AreEqual(16, ret);
		}

		TEST_METHOD(CodeGenDoubleToI32)
		{
			using Fn = Int32(__fastcall*)(Double);
			SetUpMethod(L"CodeGenDoubleToI32");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenDoubleToI32.bin");
			auto ret = method(3.2);
			Assert::AreEqual(3, ret);
		}

		TEST_METHOD(CodeGenI32ToDouble)
		{
			using Fn = Double(__fastcall*)(Int32);
			SetUpMethod(L"CodeGenI32ToDouble");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenI32ToDouble.bin");
			auto ret = method(2);
			Assert::AreEqual(2.0, ret);
		}

		TEST_METHOD(CodeGenI32ToI16)
		{
			using Fn = Int16(__fastcall*)(Int32);
			SetUpMethod(L"CodeGenI32ToI16");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenI32ToI16.bin");
			auto ret = method(0xFFFF);
			Assert::AreEqual((Int16)0xFFFF, ret);
		}

		TEST_METHOD(CodeGenRightShift)
		{
			using Fn = Int32(__fastcall*)(Int32);
			SetUpMethod(L"CodeGenRightShift");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenRightShift.bin");
			auto ret = method(16);
			Assert::AreEqual(4, ret);
		}

		TEST_METHOD(CodeGenUnsignedRightShift)
		{
			using Fn = UInt32(__fastcall*)(UInt32);
			SetUpMethod(L"CodeGenUnsignedRightShift");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenUnsignedRightShift.bin");
			auto ret = method(16u);
			Assert::AreEqual(4u, ret);
		}

		TEST_METHOD(CodeGenLeftShift)
		{
			using Fn = Int32(__fastcall*)(Int32);
			SetUpMethod(L"CodeGenLeftShift");
			auto bb = PassThrough<ILTransformer>();
			ViewIR(bb);
			PassThrough<Morpher, Linearizer>();
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>();

			Fn method = (Fn)DumpNativeCode("CodeGenLeftShift.bin");
			auto ret = method(4);
			Assert::AreEqual(16, ret);
		}
	};

	RTM::AssemblyContext* JITTest::assembly = nullptr;

	TEST_CLASS(JITTest2)
	{
		HexJITContext* Context = nullptr;
		static AssemblyContext* Assembly;
		CodeDumper Dump;
	public:
		TEST_CLASS_INITIALIZE(InitializeMetaManager)
		{
			Meta::MetaData = new Meta::MetaManager();
			Assembly = Meta::MetaData->StartUp(Text("JIT"));
			CheckOrCreateDumpCodeDirectory("JITTest2");
		}

		TEST_CLASS_CLEANUP(CleanUpMetaManager)
		{
			Meta::MetaData->ShutDown();
			delete Meta::MetaData;
		}

		TEST_METHOD_INITIALIZE(InitializeContext)
		{
			Context = new Hex::HexJITContext();
			Context->Context = new JITContext();
			Context->Memory = new Memory::SegmentHeap();

			Context->Traversal.Count = 256;
			Context->Traversal.Space = (Int8*)Context->Memory->Allocate(sizeof(void*) * Context->Traversal.Count);

			Dump = CodeDumper{ L"JITTest2", Context };
		}

		TEST_METHOD_CLEANUP(CleanUpContext)
		{
			delete Context->Memory;
			delete Context;
		}

		SET_UP_METHOD_FOR(JITTest2)

		TEST_METHOD(TestStructLocal)
		{
			SetUpMethod(L"TestStructLocal");
			using Fn = Float(__fastcall*)();
			auto bb = PassThrough<ILTransformer, Morpher>(Context);
			ViewIR(bb);
			PassThrough<Linearizer>(Context);
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>(Context);

			Fn method = (Fn)Dump();
			auto ret = method();

			Assert::AreEqual(1.0f, ret);
		}

		TEST_METHOD(TestLocalStructAddress)
		{
			SetUpMethod(L"TestLocalStructAddress");
			using Fn = Float(__fastcall*)();
			auto bb = PassThrough<ILTransformer>(Context);
			ViewIR(bb);
			PassThrough<Morpher>(Context);
			ViewIR(bb);
			PassThrough<Linearizer>(Context);
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>(Context);

			Fn method = (Fn)Dump();
			auto ret = method();

			Assert::AreEqual(2.0f, ret);
		}

		TEST_METHOD(TestNestLocalStruct)
		{
			SetUpMethod(L"TestNestLocalStruct");
			using Fn = Float(__fastcall*)();
			auto bb = PassThrough<ILTransformer>(Context);
			ViewIR(bb);
			PassThrough<Morpher>(Context);
			ViewIR(bb);
			PassThrough<Linearizer>(Context);
			ViewIR(bb);
			PassThrough<FlowGraphPruner, LivenessAnalyzer, X86::X86NativeCodeGenerator>(Context);

			Fn method = (Fn)Dump();
			auto ret = method();

			Assert::AreEqual(2.0f, ret);
		}
	};

	RTM::AssemblyContext* JITTest2::Assembly = nullptr;
}