/*

  Copyright [2024] [Leonardo Julca]

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#ifndef AURA_LIST_H_
#define AURA_LIST_H_

template <typename T>
struct DoubleLinkedListNode {
  T data;
  DoubleLinkedListNode* next;
  DoubleLinkedListNode* prev;

  DoubleLinkedListNode()
   : data(T()),
     next(nullptr),
     prev(nullptr)
 {
 }

  ~DoubleLinkedListNode()
  {
  }
};

template <typename T>
struct CircleDoubleLinkedList
{
  DoubleLinkedListNode<T>* head;
  DoubleLinkedListNode<T>* tail;

  CircleDoubleLinkedList()
   : head(nullptr),
     tail(nullptr)
  {}

  ~CircleDoubleLinkedList()
  {
    reset();
  }

  inline bool getEmpty() const
  {
    return head == nullptr;
  }

  void insertBefore(DoubleLinkedListNode<T>* next, DoubleLinkedListNode<T>* node)
  {
    DoubleLinkedListNode<T>* prev = next->prev;
    next->prev = node;
    prev->next = node;
    node->next = next;
    node->prev = prev;
  }

  void insertAfter(DoubleLinkedListNode<T>* prev, DoubleLinkedListNode<T>* node)
  {
    DoubleLinkedListNode<T>* next = prev->next;
    prev->next = node;
    next->prev = node;
    node->prev = prev;
    node->next = next;

    if (prev == tail) {
      tail = node;
    }
  }

  void emplaceBefore(DoubleLinkedListNode<T>* next)
  {
    insertBefore(next, new DoubleLinkedListNode<T>());
  }

  void emplaceAfter(DoubleLinkedListNode<T>* prev)
  {
    insertAfter(prev, new DoubleLinkedListNode<T>());
  }

  void insertBack(DoubleLinkedListNode<T>* node)
  {
    if (getEmpty()) {
      node->next = node;
      node->prev = node;
      head = node;
      tail = node;
      return;
    }
    tail->next = node;
    head->prev = node;
    node->prev = tail;
    node->next = head;
    tail = node;
  }

  void emplaceBack()
  {
    insertBack(new DoubleLinkedListNode<T>());
  }

  void remove(DoubleLinkedListNode<T>* node)
  {
    if (node == tail && node == head) {
      head = nullptr;
      tail = nullptr;
      return;
    }
    DoubleLinkedListNode<T>* prev = node->prev;
    DoubleLinkedListNode<T>* next = node->next;
    prev->next = next;
    next->prev = prev;

   if (node == tail) {
      tail = prev;
    }
    if (node == head) {
      head = next;
    }
  }

  void reset()
  {
    while (head != nullptr) {
      DoubleLinkedListNode<T>* tmpTail = tail;
      remove(tmpTail);
      delete tmpTail;
    }
  }
};

#endif // AURA_LIST_H
