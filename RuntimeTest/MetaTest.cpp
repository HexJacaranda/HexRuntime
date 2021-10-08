#include "pch.h"
#include <filesystem>
#include "CppUnitTest.h"
#include "../HexRuntime/Runtime/RuntimeAlias.h"
#include "../HexRuntime/Runtime/Core/Meta/CoreTypes.h"
#include "../HexRuntime/Runtime/Core/Meta/MetaManager.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace RT;
using namespace RTM;
using namespace RTC;

namespace RuntimeTest
{
	TEST_CLASS(MetaTest)
	{
	public:
		
		TEST_METHOD(MetaManagerLoadingTest)
		{
			MetaManager meta{};
			auto context = meta.StartUp(Text("Core"));
		}

		TEST_METHOD(IntrinsicTypeLoadingTest)
		{
			MetaManager meta{};

			auto int8Type = meta.GetIntrinsicTypeFromCoreType(CoreTypes::I1);
			Assert::AreEqual(L"[System]Int8", int8Type->GetFullQualifiedName()->GetContent());
			Assert::AreEqual(1, int8Type->GetSize());

			auto int16Type = meta.GetIntrinsicTypeFromCoreType(CoreTypes::I2);
			Assert::AreEqual(L"[System]Int16", int16Type->GetFullQualifiedName()->GetContent());
			Assert::AreEqual(2, int16Type->GetSize());

			auto int32Type = meta.GetIntrinsicTypeFromCoreType(CoreTypes::I4);
			Assert::AreEqual(L"[System]Int32", int32Type->GetFullQualifiedName()->GetContent());
			Assert::AreEqual(4, int32Type->GetSize());

			auto int64Type = meta.GetIntrinsicTypeFromCoreType(CoreTypes::I8);
			Assert::AreEqual(L"[System]Int64", int64Type->GetFullQualifiedName()->GetContent());
			Assert::AreEqual(8, int64Type->GetSize());

			auto arrayType = meta.GetIntrinsicTypeFromCoreType(CoreTypes::Array);
			Assert::AreEqual(L"[System]Array<Canon>", arrayType->GetFullQualifiedName()->GetContent());
		}
	};
}
