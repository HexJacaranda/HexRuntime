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

		enum EntryState
		{
			NotYet,
			Set,
			Migrated,
			Dropped
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
			std::atomic<Int64> State;
			ConcurrentReference<ConcurrentKeyValue<TKey, TValue>*> Data;
		};

		static inline void BreakState(Int64 stateValue, Int32& hash, Int32& state)
		{
			hash = stateValue & 0xFFFFFFFF;
			state = (stateValue & 0xFFFFFFFF00000000) >> 32;
		}

		static inline Int64 TieUpState(Int32 hash, Int32 state)
		{
			return (state << 32) | hash;
		}

		template<class TKey, class TValue>
		struct ConcurrentTable
		{
			ConcurrentReference<ConcurrentTable<TKey, TValue>*> OldVersion;
			Int32 Capacity;
			std::atomic<Int32> Count;
			ConcurrentEntry<TKey, TValue>* Entries;
		};

		template<class TKey, class TValue, class HasherT, class ComparatorT>
		class ConcurrentMap
		{
			using Table = ConcurrentTable<TKey, TValue>;

			template<class T>
			using Guard = typename ConcurrentReference<T>::Guard;

			using Entry = ConcurrentEntry<TKey, TValue>;

			enum class ResizeState
			{
				NotStarted,
				TablePreparing,
				ProgressReady,
				ProgressDone
			};

		private:
			ConcurrentReference<Table*> mCurrent;
			std::atomic<ResizeState> mResizeState;

			bool EnsureKeyAbsenceInOldTable(Guard<Table*>& guard, TKey const& key, Int32 hashValue, ComparatorT && compare)
			{
				auto oldGuard = guard->OldVersion.Acquire();

				//If there is no old table, return true
				if (!oldGuard.IsValid())
					return true;

				Int32 index = hashValue;
				Int32 reprobeLimit = (table->Capacity - 1) >> 1;
				while (true)
				{
					index &= (oldGuard->Capacity - 1);
					auto&& entry = oldGuard->Entries[index];

					Int64 stateValue = entry.State.load(std::memory_order_acquire);
					Int32 hash, state;
					BreakState(stateValue, hash, state);

					if (state == Set && hash == hashValue)
					{
						auto dataGuard = entry.Data.Acquire();
						if (compare(dataGuard->Key, key) == 0)
							return false;
					}

					//Maybe we should be migration-aware

					index++;
					if (reprobeLimit-- == 0)
						break;
				}

				return true;
			}

			void Migrate()
			{
				ResizeState resizeState = mResizeState.load(std::memory_order_relaxed);

				if (resizeState == ResizeState::NotStarted
					&& mResizeState.compare_exchange_weak(resizeState, ResizeState::TablePreparing))
				{
					//Winner is responsible for creating new table
					auto table = mCurrent.Acquire();
					Int32 capacity = table->Capacity << 1;
					Table* newTable = new Table
					{
						Capacity = capacity,
						Entries = new Entry[capacity],
						Count = 0
					};

					newTable->OldVersion.Emplace(table);
					mCurrent.Emplace(newTable);

					//Ready
					mResizeState.store(ResizeState::ProgressReady, std::memory_order_release);
				}
				else
				{
					//Wait for new table
					while ((resizeState = mResizeState.load(std::memory_order_acquire)) == ResizeState::TablePreparing)
						std::this_thread::yield();

					//Is migration over?
					if (resizeState != ResizeState::ProgressReady)
						return;
				}
				auto table = mCurrent.Acquire();
				auto oldTable = table->OldVersion.Acquire();

				//Should we randomly partition the migration area?
				for (Int32 i = 0; i < oldTable->Capacity; ++i)
				{

				}
			}
		public:
			ConcurrentMap(Int32 initialCapacity)
			{
				Table* newTable = new Table
				{
					Capacity = initialCapacity,
					Entries = new Entry[initialCapacity],
					Count = 0
				};

				mCurrent.Emplace(newTable);
			}

			bool TryAdd(TKey const& key, TValue const& value)
			{
				Int32 hashValue = HasherT()(key);
				Int32 index = hashValue;

				ComparatorT compare{};

				auto table = mCurrent.Acquire();

				if (!EnsureKeyAbsenceInOldTable(table, key, hashValue, compare))
					return false;

				Int32 reprobeLimit = (table->Capacity - 1) >> 1;
				while (true)
				{
					index &= (table->Capacity - 1);
					auto&& entry = table->Entries[index];

					Int64 stateValue = entry.State.load(std::memory_order_acquire);

					Int32 hash, state;
					BreakState(stateValue, hash, state);

					Int64 newState = TieUpState(hashValue, Set);
					if (state == NotYet &&
						entry.State.compare_exchange_weak(
							stateValue,
							newState,
							std::memory_order_acq_rel,
							std::memory_order_relaxed))
					{
						//Emplace the value now
						entry.Data.Emplace(new ConcurrentKeyValue{ key, value });
					}
					else
					{
						BreakState(stateValue, hash, state);

						//Need to be sure that it's a different key
						auto dataGuard = entry.Data.Acquire();
						while (!dataGuard.IsValid())
						{
							std::this_thread::yield();
							dataGuard = std::move(entry.Data.Acquire());
						}

						if (compare(dataGuard->Key, key) == 0 && state != Dropped)
						{
							//The same with target key, conflicting
							return false;
						}
					}

					//Reprobe
					index++;

					//Limit exceeded
					if (reprobeLimit-- == 0)
					{
						//for all the writer that want
						//to add new pair(s), they need to help complete the
						//migration from old to new table
						Migrate();
					}
				}
			}

			template<class Fn>
			TValue GetOrUpdate(TKey const& key, Fn&& factory)
			{

			}

			bool TryGet(TKey const& key, TValue& outValue) 
			{
				Int32 hashValue = HasherT()(key);
				Int32 index = hashValue;
				
				ComparatorT compare{};

				auto table = mCurrent.Acquire();
				while (table.IsValid())
				{
					Int32 reprobeLimit = (table->Capacity - 1) >> 1;
					while (true)
					{
						index &= (table->Capacity - 1);
						auto&& entry = table->Entries[index];

						Int64 stateValue = entry.State.load(std::memory_order_acquire);
						Int32 hash = stateValue & 0xFFFFFFFF;

						if (hash == hashValue)
						{
							auto dataGuard = entry.Data.Acquire();
							if (compare(dataGuard->Key, key) == 0)
							{
								outValue = dataGuard->Value;
								return true;
							}
						}

						//Reprobe
						index++;

						//Limit exceeded, go for elder table
						if (reprobeLimit-- == 0)
							break;
					}
					table = std::move(table.OldVersion.Acquire());
				}

				return false;
			}

			void Remove(TKey const& key)
			{

			}
		};
	}
}