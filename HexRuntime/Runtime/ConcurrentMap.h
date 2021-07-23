#pragma once
#include "RuntimeAlias.h"
#include <atomic>
#include <thread>
#include <utility>
#include <type_traits>

namespace RT
{
	namespace Concurrent
	{
		struct PointerReleaser
		{
			template<class U>
			void operator()(U&& value) { delete value; }
		};

		struct NopReleaser
		{
			template<class U>
			void operator()(U&& value) {}
		};

		template<class T, class ReleaseT = NopReleaser>
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
						ReleaseDirectly();
				}

				void ReleaseDirectly()
				{
					ReleaseT()(Value);
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

				T* operator->() requires(!std::is_pointer_v<T>) { return &mInternal->Value; };
				T operator->()const requires(std::is_pointer_v<T>)  { return mInternal->Value; };

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
				} while (!mExternal.compare_exchange_weak(
					old, 
					newExternal, 
					std::memory_order_acq_rel, 
					std::memory_order_relaxed));

				return Guard{ newExternal.InternalPtr };
			}

			template<class...Args>
			void Emplace(Args&&...args)
			{
				Internal* newOne = new Internal(std::forward<Args>(args)...);
				External newExternal{ 1, newOne };
				External oldOne = mExternal.load(std::memory_order_relaxed);
				while (!mExternal.compare_exchange_weak(oldOne,
					newExternal,
					std::memory_order_acq_rel,
					std::memory_order_relaxed));
				
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
					!(casSuccessful = mExternal.compare_exchange_weak(oldOne,
						newExternal,
						std::memory_order_acq_rel,
						std::memory_order_relaxed)));

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
				while (!mExternal.compare_exchange_weak(
					old, 
					empty, 
					std::memory_order_acq_rel, 
					std::memory_order_relaxed));
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
			ConcurrentReference<ConcurrentKeyValue<TKey, TValue>*, PointerReleaser> Data;
		};

		static inline void BreakState(Int64 stateValue, Int32& hash, Int32& state)
		{
			hash = stateValue & 0xFFFFFFFF;
			state = (stateValue & 0xFFFFFFFF00000000) >> 32;
		}

		static inline Int64 TieUpState(Int32 hash, Int32 state)
		{
			return (((Int64)state) << 32 ) | (Int64)hash;
		}

		static inline Int32 RoundUpPower2(Int32 value)
		{
			value--;
			value |= value >> 1;
			value |= value >> 2;
			value |= value >> 4;
			value |= value >> 8;
			value |= value >> 16;
			return ++value;
		}

		template<class TKey, class TValue>
		struct ConcurrentTable
		{
			ConcurrentReference<ConcurrentTable<TKey, TValue>*, PointerReleaser> OldVersion;
			Int32 Capacity;
			std::atomic<Int32> Count;
			ConcurrentEntry<TKey, TValue>* Entries;

			~ConcurrentTable()
			{
				if (Entries != nullptr)
				{
					delete Entries;
					Entries = nullptr;
				}				
			}
		};

		struct GenericHasher
		{
			template<class T>
			Int32 operator()(T&& value)
			{
				const UInt8* _First = (const UInt8*)&value;

				const UInt32 _FNV_offset_basis = 2166136261U;
				const UInt32 _FNV_prime = 16777619U;
				UInt32 _Val = _FNV_offset_basis;
				for (UInt32 _Next = 0; _Next < sizeof(T); ++_Next)
				{
					_Val ^= (UInt32)_First[_Next];
					_Val *= _FNV_prime;
				}
				
				Int32 hashValue = static_cast<Int32>(_Val);
				if (hashValue < 0)
					return hashValue + std::numeric_limits<Int32>::max();
				else
					return hashValue;
			}
		};

		struct GenericComparator
		{
			template<class T, class U>
			Int32 operator()(T&& left, U&& right)
			{
				return left == right ? 0 : 1;
			}
		};

		template<class TKey, 
			class TValue, 
			class HasherT = GenericHasher, 
			class ComparatorT = GenericComparator>
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
				ProgressReady
			};

			enum class SpaceState
			{
				Success,
				DuplicateKey,
				ReprobeLimit
			};

		private:
			ConcurrentReference<Table*> mCurrent;
			std::atomic<ResizeState> mResizeState;
			ComparatorT mCompare;
			HasherT mHashser;

			bool EnsureKeyAbsenceInOldTable(Guard<Table*>& table, TKey const& key, Int32 hashValue)
			{
				auto oldGuard = table->OldVersion.Acquire();

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
						if (mCompare(dataGuard->Key, key) == 0)
							return false;
					}

					//Maybe we should be migration-aware

					index++;
					if (reprobeLimit-- == 0)
						break;
				}

				return true;
			}

			template<class Fn, class GeneratorT>
			SpaceState TryFindSpaceInTable(
				Guard<Table*>& table, 
				TKey const& key, 
				Int32 hashValue, 
				Fn&& reprobeLimitAction, 
				GeneratorT&& generator)
			{
				Int32 index = hashValue;
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
						table->Count.fetch_add(1, std::memory_order_acq_rel);
						//Emplace the value now
						entry.Data.Emplace(std::forward<GeneratorT>(generator)());
						return SpaceState::Success;
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

						if (mCompare(dataGuard->Key, key) == 0 
							&& state != Dropped)
						{
							//The same with target key, conflicting
							return SpaceState::DuplicateKey;
						}
					}

					//Reprobe
					index++;

					//Limit exceeded
					if (reprobeLimit-- == 0)
					{
						std::forward<Fn>(reprobeLimitAction)();
						return SpaceState::ReprobeLimit;
					}
				}
			}

			inline bool NeedMigrate() {
				return mResizeState.load(std::memory_order_acquire) !=
					ResizeState::NotStarted;
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
					Table* newTable = new Table;
					{
						newTable->Capacity = capacity;
						newTable->Entries = new Entry[capacity];
						newTable->Count.store(0, std::memory_order_relaxed);
					}

					newTable->OldVersion.Emplace(*table);
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
					auto&& entry = table->Entries[i];
					Int64 stateValue = entry.State.load(std::memory_order_acquire);
					Int32 hash, state;
					BreakState(stateValue, hash, state);

					if (state == Set)
					{
						//Try acquire
						Int64 newState = TieUpState(hash, Migrated);
						if (entry.State.compare_exchange_weak(stateValue, newState))
						{
							//Succeed, find a place for copying
							//Should always succeed
							auto dataGuard = entry.Data.Acquire();
							
							TryFindSpaceInTable(
								table,
								dataGuard->Key,
								hash,
								[]() {},
								[&]() { return *dataGuard; });

							oldTable->Count.fetch_add(-1, std::memory_order_acq_rel);
						}
					}
				}

				auto expectedState = ResizeState::ProgressReady;
				if (mResizeState.compare_exchange_weak(expectedState,
					ResizeState::NotStarted,
					std::memory_order_acq_rel,
					std::memory_order_relaxed))
				{
					//The only one to set resizing done
					//Clean up the table
					table->OldVersion.Drop();
				}
			}
		public:
			ConcurrentMap(Int32 initialCapacity)
			{
				if (initialCapacity < 32)
					initialCapacity = 32;
				else
					initialCapacity = RoundUpPower2(initialCapacity);

				Table* newTable = new Table;

				{
					newTable->Capacity = initialCapacity;
					newTable->Entries = new Entry[initialCapacity];
					newTable->Count.store(0, std::memory_order_relaxed);
				}
				
				mCurrent.Emplace(newTable);
			}

			bool TryAdd(TKey const& key, TValue const& value)
			{
				Int32 hashValue = mHashser(key);
				Int32 index = hashValue;
				
				do 
				{
					auto table = mCurrent.Acquire();
					if (NeedMigrate() || table->Count.load(std::memory_order_acquire) == table->Capacity)
					{
						Migrate();
						continue;
					}

					if (!EnsureKeyAbsenceInOldTable(table, key, hashValue))
						return false;

					auto state = TryFindSpaceInTable(
						table,
						key,
						hashValue,
						[&]() { Migrate(); },
						[&]() { return new ConcurrentKeyValue{ key, value }; });

					switch (state)
					{
					case SpaceState::Success:
						return true;
					case SpaceState::DuplicateKey:
						return false;
					case SpaceState::ReprobeLimit:
						//Retry
						continue;
					}

				} while (false);

				return true;
			}

			bool TryGet(TKey const& key, TValue& outValue) 
			{
				Int32 hashValue = mHashser(key);
				Int32 index = hashValue;

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
							if (mCompare(dataGuard->Key, key) == 0)
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
					table = std::move(table->OldVersion.Acquire());
				}

				return false;
			}

			void AddOrUpdate(TKey const& key, TValue const& value)
			{

			}

			template<class Fn>
			TValue GetOrAdd(TKey const& key, Fn&& factory)
			{
				Int32 hashValue = mHashser(key);
				do
				{
					if (NeedMigrate())
					{
						Migrate();
						continue;
					}

					auto table = mCurrent.Acquire();
					if (!EnsureKeyAbsenceInOldTable(table, key, hashValue))
						return false;

					Int32 index = hashValue;
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
							table->Count.fetch_add(1, std::memory_order_acq_rel);
							auto keyValue = new ConcurrentKeyValue<TKey, TValue>{ key, std::forward<Fn>(factory)() };
							//Emplace the value now
							entry.Data.Emplace(keyValue);
							return keyValue->value;
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

							if (mCompare(dataGuard->Key, key) == 0
								&& state != Dropped)
							{
								//Same key, return value
								return dataGuard->Value;
							}
						}

						//Reprobe
						index++;

						//Limit exceeded
						if (reprobeLimit-- == 0)
						{
							Migrate();
							continue;
						}
					}
				} while (false);
				return true;
			}

			~ConcurrentMap()
			{
				mCurrent.Drop();
			}
		};
	}
}