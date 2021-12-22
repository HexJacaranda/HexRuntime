#pragma once
#include "RuntimeAlias.h"
#include <set>

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

		template<class NodeT>
		static void AppendRangeTwoWay(NodeT*& head, NodeT*& previous, NodeT* targetHead, NodeT* targetTail)
		{
			if (head == nullptr)
			{
				head = targetHead;
				previous = targetTail;
			}
			else
			{
				if (previous == nullptr)
				{
					targetTail->Next = head;
					head->Prev = targetTail;
					head = targetHead;
				}
				else
				{
					NodeT* next = previous->Next;
					previous->Next = targetHead;
					targetHead->Prev = previous;
					if (next != nullptr)
					{
						targetTail->Next = next;
						next->Prev = targetTail;
					}
				}

			}
		}

		template<class NodeT>
		static void InsertBefore(NodeT*& head, NodeT*& previous, NodeT* toInsert)
		{
			if (previous == nullptr)
			{
				toInsert->Next = head;
				head->Prev = toInsert;
				head = toInsert;
			}
			else
			{
				toInsert->Next = previous->Next;
				toInsert->Prev = previous;
				NodeT* current = previous->Next;
				previous->Next = toInsert;
				if (current != nullptr)
					current->Prev = toInsert;
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
	concept ContainerT = requires(T value, Int32 index) 
	{ 
		value.size(); 
		value[index]; 
	};

	class Enumerable
	{
	public:
		template<ContainerT Container, class BodyFn, class ConnectionFn>
		static void ContractIterate(Container&& container, BodyFn&& bodyFn, ConnectionFn&& connectionFn)
		{
			Int32 size = container.size();
			Int32 i = 0;
			for (; i < size - 1; ++i)
			{
				std::forward<BodyFn>(bodyFn)(container[i]);
				std::forward<ConnectionFn>(connectionFn)(container[i]);
			}
			std::forward<BodyFn>(bodyFn)(container[i]);
		}
	};

	template<class T, class Fn>
	static void ForeachInlined(T**& inlineArray, Int32 count, Fn&& action) {
		if (count == 1)
			std::forward<Fn>(action)(*(T**)&inlineArray);
		else if (count > 1)
		{
			for (Int32 i = 0; i < count; ++i)
				std::forward<Fn>(action)(inlineArray[i]);
		}
	}

#define ETY using EnumType
#define VAL static constexpr EnumType
}