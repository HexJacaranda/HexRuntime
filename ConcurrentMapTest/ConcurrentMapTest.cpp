#include "pch.h"
#include "CppUnitTest.h"
#include "..\HexRuntime\Runtime\RuntimeAlias.h"
#include "..\HexRuntime\Runtime\ConcurrentMap.h"
#include <unordered_map>
#include <mutex>
#include <algorithm>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace RT::Concurrent;

namespace ConcurrentMapTest
{
	TEST_CLASS(ConcurrentMapTest)
	{
	public:
		static void MessageLogKey(ConcurrentMap<int, double>& map)
		{
			std::vector<int> keys;

			map.Walk([&](auto&& hash, auto&& key, auto&& value) {
				keys.push_back(key);
				});

			std::sort(keys.begin(), keys.end());

			for (auto&& key : keys)
				Logger::WriteMessage(std::format("{}", key).c_str());
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

		TEST_METHOD(GetOrAdd)
		{
			ConcurrentMap<int, double> map{ 32 };
			map.TryAdd(2, 3.0);
			Assert::AreEqual(3.0, map.GetOrAdd(2, []() { return 2.0; }));
			Assert::AreEqual(2.0, map.GetOrAdd(3, []() { return 2.0; }));
		}

		TEST_METHOD(AddToMigrateLimit)
		{
			ConcurrentMap<int, double> map{ 32 };
			for (int i = 0; i < 32; ++i)
				map.TryAdd(i, 0);
			MessageLogKey(map);
			Assert::AreEqual(32, map.Count(), L"Map should be of 40 count");
		}

		TEST_METHOD(AddToMigrate)
		{
			ConcurrentMap<int, double> map{ 32 };
			for (int i = 0; i < 40; ++i)
				map.TryAdd(i, 0);

			MessageLogKey(map);
			Assert::AreEqual(40, map.Count(), L"Map should be of 40 count");
		}

		TEST_METHOD(AddDuplicate)
		{
			ConcurrentMap<int, double> map{ 32 };
			map.TryAdd(2, 3.0);
			Assert::IsTrue(!map.TryAdd(2, 4.0));
		}

		TEST_METHOD(AddMultiThreadWith10)
		{
			ConcurrentMap<int, double> map{ 32 };
			ParallelRun(20, [&](int i)
				{
					for (int k = 0; k < 100000; ++k)
						map.TryAdd(i * 100000 + k, 0);
				});
			Assert::AreEqual(2000000, map.Count(), L"Map with 10 threads and 20 each should be of size 200!");
		}

		TEST_METHOD(GetOrAddMultiThread)
		{
			ConcurrentMap<int, double> map{ 32 };
			ParallelRun(20, [&](int i)
				{
					for (int k = 0; k < 100000; ++k)
						map.GetOrAdd(k, []() { return 0.0; });
				});
			Assert::AreEqual(100000, map.Count(), L"Map with 10 threads and 20 each should be of size 200!");
		}

		TEST_METHOD(GetOrAddMultiThreadSTD)
		{
			std::unordered_map<int, double> map{};
			std::mutex lock;
			ParallelRun(20, [&](int i)
				{
					for (int k = 0; k < 100000; ++k)
					{
						std::lock_guard guard{ lock };
						if(map.find(k) == map.end())
							map.insert(std::make_pair(k, 0));
					}
						
				});
			Assert::AreEqual(100000, (int)map.size(), L"Map with 10 threads and 20 each should be of size 200!");
		}

		TEST_METHOD(AddMultiThreadWith10STD)
		{
			std::unordered_map<int, double> map{};
			std::mutex lock;

			ParallelRun(20, [&](int i)
				{
					for (int k = 0; k < 100000; ++k)
					{
						std::lock_guard guard{ lock };
						map.insert(std::make_pair(i * 100000 + k, 0));
					}
				});

			Assert::AreEqual(2000000, (int)map.size(), L"Map with 10 threads and 20 each should be of size 200!");
		}
	};
}
