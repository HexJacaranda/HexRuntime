#include "pch.h"
#include "CppUnitTest.h"
#include "..\HexRuntime\Runtime\RuntimeAlias.h"
#include "..\HexRuntime\Runtime\ConcurrentMap.h"
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace RT::Concurrent;

namespace ConcurrentMapTest
{
	TEST_CLASS(ConcurrentMapTest)
	{
	public:
		TEST_METHOD(Add)
		{
			ConcurrentMap<int, double> map{ 32 };
			map.TryAdd(2, 3.0);
			map.TryAdd(3, 4.0);
			double value = 0.0;
			Assert::IsTrue(map.TryGet(2, value));
			Assert::AreEqual(3.0, value);
			Assert::IsTrue(map.TryGet(3, value));
			Assert::AreEqual(4.0, value);
		}

		TEST_METHOD(AddDuplicate)
		{
			ConcurrentMap<int, double> map{ 32 };
			map.TryAdd(2, 3.0);
			Assert::IsTrue(!map.TryAdd(2, 4.0));
		}
	};
}
