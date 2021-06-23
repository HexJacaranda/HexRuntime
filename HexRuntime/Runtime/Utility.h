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

#define UNDERLYING_TYPE(TYPE) using EnumType = TYPE

#define VALUE(NAME) static constexpr EnumType NAME
}