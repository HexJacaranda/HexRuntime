#include "pch.h"
#include <filesystem>
#include "CppUnitTest.h"
#include "../HexRuntime/Runtime/RuntimeAlias.h"
#include "../HexRuntime/Runtime/Core/Meta/TypeStructuredName.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace RT;
using namespace RTM;
using namespace RTC;

namespace RuntimeTest
{
	TEST_CLASS(TSNTest)
	{
	public:
		TEST_METHOD(TypeNameParsingTest)
		{
			TypeNameParser parser{ L"[Core][TEST.TEST]A<Canon>.B<Canon, Canon>" };
			auto name = parser.Parse();

			Assert::AreEqual(L"Core", std::wstring{ name->GetReferenceAssembly() }.c_str());
			Assert::AreEqual(L"TEST.TEST", std::wstring{ name->GetNamespace() }.c_str());
			Assert::AreEqual(2, (Int32)name->GetTypeNodes().size());
		}

		TEST_METHOD(TypeNameInstantiatingTest)
		{
			TypeNameParser parser{ L"[Core][global]A<Canon>.B<Canon, Canon>" };
			auto name = parser.Parse();

			auto instantiatedName = name->InstantiateWith({ L"[Core][global]Int32" ,L"[Core][global]Int32", L"[Core][global]Int32" });
			Assert::AreEqual(L"[Core][global]A<[Core][global]Int32>.B<[Core][global]Int32, [Core][global]Int32>", instantiatedName->GetTypeName().data());
		}
	};
}