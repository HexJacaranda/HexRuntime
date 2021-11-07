#pragma once
#include "RuntimeAlias.h"
#include "Utility.h"

namespace RT
{
	template<class NodeT>
	class DoubleLinkList
	{
		NodeT* mHead = nullptr;
		NodeT* mTail = nullptr;
	public:
		NodeT* GetHead()const {
			return mHead;
		}
		NodeT* GetTail()const {
			return mTail;
		}
		bool IsEmpty()const
		{
			return mHead == nullptr;
		}
		void Append(NodeT* value) {
			LinkedList::AppendTwoWay(mHead, mTail, value);
		}
		void Append(DoubleLinkList<NodeT> const& another)
		{
			if (another.IsEmpty())
				return;

			if (mHead == nullptr)
			{
				mHead = another.mHead;
				mTail = another.mTail;
			}
			else
			{
				mTail->Next = another.mHead;
				another.mHead->Prev = mTail;
				mTail = another.mTail;
			}
		}
	};
}