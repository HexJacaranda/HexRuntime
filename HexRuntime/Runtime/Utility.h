#pragma once
#include "RuntimeAlias.h"

namespace RT
{
	template<class NodeT>
	inline void AppendToTwoWayLinkedList(NodeT*& head, NodeT*& previous, NodeT* current)
	{
		if (head == nullptr)
			head = previous = current;
		else
		{
			previous->Next = current;
			current->Prev = previous;
			previous = current;
		}
	}

	template<class NodeT, class Fn>
	inline void AppendToOneWayLinkedListOrdered(NodeT*& head, NodeT* current, Fn&& predicator)
	{
		if (head == nullptr)
			head = current;
		else
		{
			NodeT* previous = nullptr;
			NodeT* iterator = head;
			while (iterator != nullptr)
			{
				if (predicator(current))
				{
					//Found position
					current->Next = iterator->Next;
					iterator->Next = current;
					return;
				}
				previous = iterator;
				iterator = iterator->Next;
			}
			//Not found, then insert to the end.
			previous->Next = current;
		}
	}

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
}