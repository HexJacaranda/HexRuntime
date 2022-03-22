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


namespace Microsoft::VisualStudio::CppUnitTestFramework
{
	template<> inline std::wstring ToString<Type>(Type* t)
	{
		if (t == nullptr)
			return L"null";
		return t->GetFullQualifiedName()->GetContent();
	}
}

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
			Assert::AreEqual(L"[Core][global]Int8", int8Type->GetFullQualifiedName()->GetContent());
			Assert::AreEqual(1, int8Type->GetSize());

			auto int16Type = meta.GetIntrinsicTypeFromCoreType(CoreTypes::I2);
			Assert::AreEqual(L"[Core][global]Int16", int16Type->GetFullQualifiedName()->GetContent());
			Assert::AreEqual(2, int16Type->GetSize());

			auto int32Type = meta.GetIntrinsicTypeFromCoreType(CoreTypes::I4);
			Assert::AreEqual(L"[Core][global]Int32", int32Type->GetFullQualifiedName()->GetContent());
			Assert::AreEqual(4, int32Type->GetSize());

			auto int64Type = meta.GetIntrinsicTypeFromCoreType(CoreTypes::I8);
			Assert::AreEqual(L"[Core][global]Int64", int64Type->GetFullQualifiedName()->GetContent());
			Assert::AreEqual(8, int64Type->GetSize());

			auto arrayType = meta.GetIntrinsicTypeFromCoreType(CoreTypes::Array);
			Assert::AreEqual(L"[Core][global]Array<Canon>", arrayType->GetFullQualifiedName()->GetContent());
		}

		TEST_METHOD(InstantiatingRefTest)
		{
			MetaManager meta{};
			auto int32Type = meta.GetIntrinsicTypeFromCoreType(CoreTypes::I4);
			auto refInt32Type = meta.InstantiateRefType(int32Type);

			Assert::AreEqual(CoreTypes::InteriorRef, refInt32Type->GetCoreType());
			Assert::AreEqual(L"[Core][global]Interior<[Core][global]Int32>", refInt32Type->GetFullQualifiedName()->GetContent());
		}

		TEST_METHOD(InstantiatingArrayTest)
		{
			MetaManager meta{};
			auto int32Type = meta.GetIntrinsicTypeFromCoreType(CoreTypes::I4);
			auto arrayInt32Type = meta.InstantiateArrayType(int32Type);

			Assert::AreEqual(CoreTypes::Array, arrayInt32Type->GetCoreType());
			Assert::AreEqual(L"[Core][global]Array<[Core][global]Int32>", arrayInt32Type->GetFullQualifiedName()->GetContent());
		}

		TEST_METHOD(InstantiatingTest)
		{
			MetaManager meta{};
			auto context = meta.StartUp(Text("MetaTest"));
			auto defToken = context->Entries[L"[MetaTest][global]SimpleGeneric<Canon>"];
			auto canonical = meta.GetTypeFromDefinitionToken(context, defToken);
			auto int32Type = meta.GetIntrinsicTypeFromCoreType(CoreTypes::I4);

			auto instantiated = meta.Instantiate(canonical, { int32Type });

			Assert::AreEqual(L"[MetaTest][global]SimpleGeneric<[Core][global]Int32>", instantiated->GetFullQualifiedName()->GetContent());

			{
				auto table = instantiated->GetFieldTable();
				auto fields = table->GetFields();
				Assert::AreEqual(2, fields.Count);

				auto&& X = fields[0];
				Assert::AreEqual(int32Type, X.GetType());

				auto&& Y = fields[1];
				auto arrayInt32Type = meta.InstantiateArrayType(int32Type);
				Assert::AreEqual(arrayInt32Type, Y.GetType());
			}	

			{
				auto size = CoreTypes::GetCoreTypeSize(CoreTypes::Ref) * 2;
				Assert::AreEqual(size, instantiated->GetSize());
			}
		}
	};
}
