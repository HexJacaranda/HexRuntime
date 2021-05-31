#pragma once
#include "RuntimeAlias.h"

namespace RT
{
	enum class InsertOption
	{
		Before,
		After
	};

	class LinkedList
	{
	public:
		template<class NodeT>
		static void AppendTwoWay(NodeT*& head, NodeT*& previous, NodeT* current) {
			if (head == nullptr)
				head = previous = current;
			else
			{
				previous->Next = current;
				current->Prev = previous;
				previous = current;
			}
		}

		/// <summary>
		/// Append node to one-way linked list, the default insert option
		/// passed to predicator is InsertOption::After
		/// </summary>
		/// <typeparam name="NodeT"></typeparam>
		/// <typeparam name="Fn"></typeparam>
		/// <param name="head"></param>
		/// <param name="current"></param>
		/// <param name="predicator"></param>
		template<class NodeT, class Fn>
		static void AppendOneWayOrdered(NodeT*& head, NodeT* current, Fn&& predicator)
		{
			if (head == nullptr)
				head = current;
			else
			{
				InsertOption insertOption = InsertOption::After;
				NodeT* previous = nullptr;
				NodeT* iterator = head;
				while (iterator != nullptr)
				{
					if (predicator(iterator, insertOption))
					{
						//Found position
						if (insertOption == InsertOption::After)
						{
							current->Next = iterator->Next;
							iterator->Next = current;
						}
						else
						{
							current->Next = iterator;
							if (previous == nullptr)
								head = current;
							else
								previous->Next = current;
						}
						return;
					}
					previous = iterator;
					iterator = iterator->Next;
				}
				//Not found, then insert to the end.
				previous->Next = current;
			}
		}
	};

	template<class T>
	class PointerVectorWithInlineStorage
	{
		union
		{
			T** mStorage;
			T* mSingleStorage;
		};
		Int32 mCapacity = 1;
		Int32 mCount = 0;
	private:
		void Resize() {
			Int32 newSize = mCapacity * 2;
			T** newBlock = new T * [newSize];

			if (mCapacity == 1 && mCount == 1)
				newBlock[0] = mSingleStorage;

			if (mCapacity > 1)
				delete[] mStorage;
			mStorage = newBlock;
			mCapacity = newSize;
		}
	public:
		PointerVectorWithInlineStorage() {}
		PointerVectorWithInlineStorage(Int32 count) :mCount(count), mCapacity(count) {
			if (count > 1)
				mStorage = new T * [mCount];
		}
		~PointerVectorWithInlineStorage() {
			if (mCapacity > 1)
			{
				mCapacity = 0;
				if (mStorage != nullptr)
				{
					delete[] mStorage;
					mStorage = nullptr;
				}
			}
			else
				mSingleStorage = nullptr;
		}

		void Add(T* value) {
			if (mCount == mCapacity)
				Resize();
			if (mCapacity == 1)
				mSingleStorage = value;
			else
				mStorage[mCount] = value;
			mCount++;
		}
		Int32 Count()const {
			return mCount;
		}
		T*& operator[](Int32 index) {
			if (mCount == 1)
				return mSingleStorage;
			return mStorage[index];
		}
		T* const& operator[](Int32 index)const {
			if (mCount == 1)
				return mSingleStorage;
			return mStorage[index];
		}
	};

	template<class T>
	struct Array
	{
		T* Elements;
		Int32 Count;

		T* begin()const {
			return Elements;
		}
		T* end()const {
			return Elements + Count;
		}
		T& operator[](Int32 index) {
			return Elements[index];
		}
		T const& operator[](Int32 index)const {
			return Elements[index];
		}
	};
}