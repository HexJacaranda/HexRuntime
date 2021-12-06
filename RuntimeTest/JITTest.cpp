#include "pch.h"
#include "CppUnitTest.h"
#include "../HexRuntime/Runtime/Core/Meta/MetaManager.h"
#include "../HexRuntime/Runtime/RuntimeAlias.h"
#include "../HexRuntime/Runtime/Core/JIT/ILEmitter.h"
#include "../HexRuntime/Runtime/Core/JIT/OpCodes.h"
#include "../HexRuntime/Runtime/Core/Meta/CoreTypes.h"
#include "../HexRuntime/Runtime/Core/JIT/JITContext.h"
#include "../HexRuntime/Runtime/Core/Memory/SegmentMemory.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Frontend/Transformer.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Frontend/SSABuilder.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Frontend/SSAOptimizer.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Backend/SSAReducer.h"
#include "../HexRuntime/Runtime/Core/JIT/HexJIT/Backend/Linearizer.h"

#include <format>

using namespace RTJ;
using namespace RTC;
using namespace RT;
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
	TEST_CLASS(JITTest)
	{
		Hex::HexJITContext* context = nullptr;
		static RTM::AssemblyContext* assembly;
	public:
		template<class FlowT, class...FlowTs>
		Hex::BasicBlock* PassThrough()
		{
			FlowT flow{ context };
			auto bb = flow.PassThrough();
			if constexpr (sizeof...(FlowTs) == 0)
				return bb;
			else
				return PassThrough<FlowTs...>();
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
			assembly = Meta::MetaData->StartUp(Text("..\\..\\RuntimeTest\\TestIL\\JIT"));
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
			context->Memory = new Memory::SegmentMemory();

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

			Assert::IsNotNull(bb, L"Basic block is null");
			Assert::IsNotNull(bb->BranchConditionValue, L"Basic block condition value is null");
		}

		TEST_METHOD(LinearizingTest)
		{
			SetUpMethod(L"LinearizeTest");
			auto bb = PassThrough<Hex::ILTransformer, Hex::Linearizer>();
			Assert::AreEqual(2, (Int32)context->LocalAttaches.size(), L"Two JIT variables expected");

			Hex::ForeachStatement(bb, [](Hex::TreeNode* node, bool isCond) {
				if (!isCond)
				{
					if (!node->Is(Hex::NodeKinds::Store) &&
						!node->Is(Hex::NodeKinds::MorphedCall))
						Assert::Fail(L"Nodes other than Store/MorphedCall occurs");
				}
				});
		}

		TEST_METHOD(SSABuildingTest)
		{
			SetUpMethod(L"SSABuildTest");
			auto bb = PassThrough<Hex::ILTransformer, Hex::SSABuilder>();
		}

		TEST_METHOD(SSAOptimizingTest)
		{
			SetUpMethod(L"SSAOptimizationTest");
			auto bb = PassThrough<Hex::ILTransformer, Hex::SSABuilder, Hex::SSAOptimizer>();

			Assert::AreEqual(Hex::PPKind::Unconditional, bb->BranchKind, L"First bb should be unconditional");
			Assert::AreEqual(bb->BranchedBB, bb->Next, L"Branch should be next BB");
		}

		TEST_METHOD(SSAReducingTest)
		{
			SetUpMethod(L"SSAOptimizationTest");
			auto bb = PassThrough<Hex::ILTransformer, Hex::SSABuilder, Hex::SSAOptimizer, Hex::SSAReducer>();

			for (Hex::BasicBlock* bbIterator = bb;
				bbIterator != nullptr;
				bbIterator = bbIterator->Next)
			{
				for (Hex::Statement* stmtIterator = bbIterator->Now;
					stmtIterator != nullptr && stmtIterator->Now != nullptr;
					stmtIterator = stmtIterator->Next)
				{
					Hex::TraverseTree(
						context->Traversal.Space,
						context->Traversal.Count,
						stmtIterator->Now,
						[](Hex::TreeNode*& node)
						{
							switch (node->Kind)
							{
							case Hex::NodeKinds::Use:
							case Hex::NodeKinds::ValueDef:
							case Hex::NodeKinds::ValueUse:
							case Hex::NodeKinds::Phi:
								Assert::Fail(L"SSA node should not exist");
							}
						});
				}
			}
		}
	};

	RTM::AssemblyContext* JITTest::assembly = nullptr;
}