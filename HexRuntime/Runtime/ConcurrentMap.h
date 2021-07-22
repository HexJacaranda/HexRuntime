#pragma once
#include "RuntimeAlias.h"
#include <atomic>
#include <thread>
#include <utility>

namespace RT
{
	namespace Concurrent
	{
		template<class T>
		class ConcurrentReference
		{
			struct Internal
			{
				std::atomic<Int32> InternalCount;
				T Value;

				template<class...Args>
				Internal(Args&&...args) :
					Value(std::forward<Args>(args)...),
					InternalCount(0) {}

				void Release()
				{
					if (InternalCount.fetch_add(1, std::memory_order_acq_rel) == -1)
						delete this;
				}
			};

			struct External
			{
				Int32 ExternalCount;
				Internal* InternalPtr;

				void Release()
				{
					if (!InternalPtr)
						return;
					auto external = ExternalCount;
					if (InternalPtr->InternalCount.fetch_sub(external, std::memory_order_acq_rel) == external - 1)
						delete InternalPtr;
					else
						InternalPtr->Release();
				}
			};

			std::atomic<External> mExternal;
		public:
			class Guard
			{
				friend class ConcurrentReference<T>;
				Internal* mInternal;
			public:
				Guard(Internal* value) :mInternal(value) {}
				Guard(Guard&& value) :mInternal(value.mInternal) {
					value.mInternal = nullptr;
				}
				Guard(Guard const&) = delete;
				Guard& operator=(Guard&& value)
				{
					if (mInternal)
						mInternal->Release();
					mInternal = value.mInternal;
					value.mInternal = nullptr;
					return *this;
				}
				Guard& operator=(Guard const&) = delete;
				bool IsValid()const { return !!mInternal; }
				T* operator->() const { return &mInternal->Value; };
				T& operator*() { return mInternal->Value; };
				T const& operator*()const { return mInternal->Value; };
				~Guard()
				{
					if (mInternal)
						mInternal->Release();
				}
			};
		public:
			ConcurrentReference()
			{
				External newOne{ 1,nullptr };
				mExternal.store(newOne, std::memory_order_relaxed);
			}
			~ConcurrentReference()
			{
				auto external = mExternal.load(std::memory_order_acquire);
				external.Release();
			}

			Guard Acquire()
			{
				External newExternal;
				auto old = mExternal.load(std::memory_order_relaxed);
				do
				{
					newExternal = old;
					++newExternal.ExternalCount;
				} while (!mExternal.compare_exchange_weak(old, newExternal));

				return Guard{ newExternal.InternalPtr };
			}

			template<class...Args>
			void Emplace(Args&&...args)
			{
				Internal* newOne = new Internal(std::forward<Args>(args)...);
				External newExternal{ 1, newOne };
				External oldOne = mExternal.load(std::memory_order_relaxed);
				while (!mExternal.compare_exchange_weak(oldOne, newExternal));
				
				oldOne.Release();
			}

			template<class...Args>
			void TryEmplace(Guard & oldGuard,Args&&...args)
			{
				Internal* newOne = new Internal(std::forward<Args>(args)...);
				External newExternal{ 1, newOne };
				External oldOne = mExternal.load(std::memory_order_relaxed);

				bool casSuccessful = false;
				while (oldOne.InternalPtr == oldGuard.mInternal &&
					!(casSuccessful = mExternal.compare_exchange_weak(oldOne, newExternal)));

				if (casSuccessful)
					oldOne.Release();
				else
					delete newOne;

				return casSuccessful;
			}

			void Drop()
			{
				External empty{ 0, nullptr };
				External old = mExternal.load(std::memory_order_relaxed);
				while (!mExternal.compare_exchange_weak(old, empty));
				old.Release();
			}
		};

		template<class TKey, class TValue>
		struct ConcurrentKeyValue
		{
			TKey Key;
			TValue Value;
		};
		
		template<class TKey, class TValue>
		struct ConcurrentEntry
		{
			Int32 Hash;
			ConcurrentReference<ConcurrentKeyValue<TKey, TValue>*> Data;
		};

		template<class TKey, class TValue>
		struct ConcurrentTable
		{
			ConcurrentReference<ConcurrentTable<TKey, TValue>*> OldVersion;
			Int32 Capacity;
			ConcurrentEntry<TKey, TValue>* Entries;
		};

		template<class TKey, class TValue, class HasherT, class ComparatorT>
		class ConcurrentMap
		{
			using Table = ConcurrentTable<TKey, TValue>;
			using Entry = ConcurrentEntry<TKey, TValue>;
		private:
			ConcurrentReference<Table*> mCurrent;

			Int32 GetHash(Int32 index)
			{
				Table* table = mCurrent.load(std::memory_order_acquire);
				return table->Entries[index].Hash;
			}

		public:
			ConcurrentMap(Int32 initialCapacity)
			{
				Table* newTable = new Table
				{
					OldVersion = nullptr,
					Capacity = initialCapacity,
					Entries = new Entry[initialCapacity]
				};

				mCurrent.store(newTable, std::memory_order_relaxed);
			}

			bool TryAdd(TKey const& key, TValue const& value)
			{
				Int32 hashValue = HasherT()(key);
				ComparatorT compare{};

				auto table = mCurrent.Acquire();
				
				
			}

			bool TryGet(TKey const& key, TValue& outValue) 
			{
				Int32 hashValue = HasherT()(key);
				Int32 index = hashValue;
				ComparatorT compare{};

				auto table = mCurrent.Acquire();
				auto oldOne = table->OldVersion.Acquire();
				if (oldOne.IsValid())
					table = std::move(oldOne);

				while (true)
				{
					index &= table->Capacity - 1;
					auto guard = table->Entries[index].Data.Acquire();
					if (!guard.IsValid())
						return false;
					Int32 hash = GetHash();
					if (hashValue == hash && compare(key, guard->Key) == 0)
					{
						outValue = guard->Value;
						return true;
					}
					index++;   
				}
			}

			void Remove(TKey const& key)
			{

			}
		};
	}
}