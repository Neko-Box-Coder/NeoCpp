#ifndef NSTD_LINKED_LIST_N_HPP
#define NSTD_LINKED_LIST_N_HPP

#include "ncpp.n.hpp"

#include "./Allocator.n.hpp"
#include "./External/MacroPowerToys/ArgsCount.h"
#include <stdarg.h>

namespace Nstd
{
    template<typename T>
    struct ListNode
    {
        ListNode<T>* Next;
        ListNode<T>* Prev;
        T Value;
        bool Batched;
    };
    
    template<typename T, n_enable_if(n_is_simple(T))>
    struct LinkedList
    {
        Allocator* Alloc;
        ListNode<T>* Head;
        ListNode<T>* Tail;
        uint64 Len;
        
        
        inline LinkedList Init(n_ref Allocator& alloc)
        {
            LinkedList retList;
            retList.Alloc = &alloc;
            retList.Head = NULL;
            retList.Tail = NULL;
            retList.Len = 0;
            return retList;
        }
        
        template<typename... Ts>
        inline n_result<ListNode<T>*> AppendValues(n_ref ListNode<T>* node, Ts... values)
        {
            n_check_true(Alloc);
            
            T* arr[] = { &values... };
            ListNode<T>* newNodes = Alloc->Malloc<ListNode<T>>(n_array_cap(arr));
            n_check_true(newNodes);
            for(int i = 0; i < n_array_cap(arr); ++i)
            {
                ListNode<T>* Next;
                ListNode<T>* Prev;
                T Value;
                bool Batched;
                
                if(i == 0 && i == n_array_cap(arr) - 1)
                    newNodes[i] = { node->Next, node, *arr[i], false };
                else if(i == 0)
                    newNodes[i] = { &newNodes[i + 1], node, *arr[i], false };
                else if(i == n_array_cap(arr) - 1)
                    newNodes[i] = { node->Next, &newNodes[i - 1], *arr[i], true };
                else
                    newNodes[i] = { &newNodes[i + 1], &newNodes[i - 1], *arr[i], true };
            }
            
            if(node->Next)
                node->Next->Prev = &newNodes[n_array_cap(arr) - 1];
            node->Next = &newNodes[0];
            Len += n_array_cap(arr);
            return &newNodes[n_array_cap(arr) - 1];
        }
        
        template<typename... Ts>
        inline n_result<ListNode<T>*> PrependValues(n_ref ListNode<T>* node, Ts... values)
        {
            n_check_true(Alloc);
            
            T* arr[] = { &values... };
            ListNode<T>* newNodes = Alloc->Malloc<ListNode<T>>(n_array_cap(arr));
            n_check_true(newNodes);
            for(int i = 0; i < n_array_cap(arr); ++i)
            {
                ListNode<T>* Next;
                ListNode<T>* Prev;
                T Value;
                bool Batched;
                
                if(i == 0 && i == n_array_cap(arr) - 1)
                    newNodes[i] = { node, node->Prev, *arr[i], false };
                else if(i == 0)
                    newNodes[i] = { &newNodes[i + 1], node->Prev, *arr[i], false };
                else if(i == n_array_cap(arr) - 1)
                    newNodes[i] = { node, &newNodes[i - 1], *arr[i], true };
                else
                    newNodes[i] = { &newNodes[i + 1], &newNodes[i - 1], *arr[i], true };
            }
            
            if(node->Prev)
                node->Prev->Next = &newNodes[0];
            node->Prev = &newNodes[n_array_cap(arr) - 1];
            Len += n_array_cap(arr);
            return &newNodes[0];
        }
        
        template<typename... Ts>
        inline LinkedList InitValues(n_ref Allocator& alloc, Ts... values)
        {
            LinkedList l = Init(alloc);
            l.AppendValues(l.Tail, values...);
            return l;
        }
        
        inline n_result<ListNode<T>*> Append(n_ref ListNode<T>* node, T val)
        {
            n_use_error_defer();
            
            n_check_true(Alloc);
            ListNode<T>* newNode = Alloc->Malloc<ListNode<T>>(1);
            n_check_true(newNode);
            n_error_defer { Alloc->Free(newNode); };
            
            *newNode = { node, NULL, val, false };
            
            if(!node) //Empty list
            {
                n_check_false(Head);
                n_check_false(Tail);
                n_check_eq(Len, 0);
                
                Head = newNode;
                Tail = newNode;
                ++Len;
                return newNode;
            }
            
            if(node->Next) //Not tail
            {
                n_check_eq(node->Next->Prev, node);
                ListNode<T> origNext = node->Next;
                newNode->Next = origNext;
                origNext->Prev = newNode;
            }
            else //Tail
                n_check_eq(node, Tail);
            
            node->Next = newNode;
            ++Len;
            return newNode;
        }
        
        inline n_result<ListNode<T>*> Prepend(n_ref ListNode<T>* node, T val)
        {
            n_use_error_defer();
            
            n_check_true(Alloc);
            ListNode<T>* newNode = Alloc->Malloc<ListNode<T>>(1);
            n_check_true(newNode);
            n_error_defer { Alloc->Free(newNode); };
            
            *newNode = { NULL, node, val, false };
            
            if(!node) //Empty list
            {
                n_check_false(Head);
                n_check_false(Tail);
                n_check_eq(Len, 0);
                
                Head = newNode;
                Tail = newNode;
                ++Len;
                return newNode;
            }
            
            if(node->Prev) //Not Head
            {
                n_check_eq(node->Prev->Next, node);
                ListNode<T> origPrev = node->Prev;
                newNode->Prev = origPrev;
                origPrev->Next = newNode;
            }
            else //Head
                n_check_eq(node, Head);
            
            node->Prev = newNode;
            ++Len;
            return newNode;
        }
        
        inline n_result<void> Remove(n_ref ListNode<T>* node)
        {
            if(!node)
                return {};
            
            n_check_true(Alloc);
            n_check_true(Head && Tail);
            
            ListNode<T>* next = node->Next;
            ListNode<T>* prev = node->Prev;
            if(!prev) //Head
            {
                n_check_eq(node, Head);
                if(!node->Batched)
                    Alloc->Free(node);
                Head = next;
                next->Prev = NULL;
                --Len;
                return {};
            }
            else if(!next) //Tail
            {
                n_check_eq(node, Tail);
                if(!node->Batched)
                    Alloc->Free(node);
                Tail = prev;
                prev->Next = NULL;
                --Len;
                return {};
            }
            else
            {
                if(!node->Batched)
                    Alloc->Free(node);
                next->Prev = node->Prev;
                prev->Next = node->Next;
                --Len;
                return {};
            }
        }
        
        inline n_result<void> Reserve(uint64 size)
        {
            n_check_true(Alloc);
            Alloc->ReserveAhead<LinkedList<T>>(size);
            return {};
        }
        
        inline n_result<ListNode<T>*> AppendRange(n_ref ListNode<T>* node, n_view<const T> v)
        {
            n_check_true(Alloc);
            if(!v)
                return {};
            
            ListNode<T>* newNodes = Alloc->Malloc<ListNode<T>>(v.len);
            n_check_true(newNodes);
            for(int i = 0; i < v.len; ++i)
            {
                ListNode<T>* Next;
                ListNode<T>* Prev;
                T Value;
                bool Batched;
                
                if(i == 0 && i == v.len - 1)
                    newNodes[i] = { node->Next, node, v.data[i], false };
                else if(i == 0)
                    newNodes[i] = { &newNodes[i + 1], node, v.data[i], false };
                else if(i == v.len - 1)
                    newNodes[i] = { node->Next, &newNodes[i - 1], v.data[i], true };
                else
                    newNodes[i] = { &newNodes[i + 1], &newNodes[i - 1], v.data[i], true };
            }
            
            if(node->Next)
                node->Next->Prev = &newNodes[v.len - 1];
            node->Next = &newNodes[0];
            Len += v.len;
            return &newNodes[v.len - 1];
        }
        
        inline n_result<ListNode<T>*> PrependRange(n_ref ListNode<T>* node, n_view<const T> v)
        {
            n_check_true(Alloc);
            if(!v)
                return {};
            
            ListNode<T>* newNodes = Alloc->Malloc<ListNode<T>>(v.len);
            n_check_true(newNodes);
            for(int i = 0; i < v.len; ++i)
            {
                ListNode<T>* Next;
                ListNode<T>* Prev;
                T Value;
                bool Batched;
                
                if(i == 0 && i == v.len - 1)
                    newNodes[i] = { node, node->Prev, v.data[i], false };
                else if(i == 0)
                    newNodes[i] = { &newNodes[i + 1], node->Prev, v.data[i], false };
                else if(i == v.len - 1)
                    newNodes[i] = { node, &newNodes[i - 1], v.data[i], true };
                else
                    newNodes[i] = { &newNodes[i + 1], &newNodes[i - 1], v.data[i], true };
            }
            
            if(node->Prev)
                node->Prev->Next = &newNodes[0];
            node->Prev = &newNodes[v.len - 1];
            Len += v.len;
            return &newNodes[0];
        }
        
        inline n_result<void> Clone(n_ref ListNode<T>* nodeToInsertAfter, n_ref LinkedList<T>& other)
        {
            n_check_true(Alloc);
            
            if(!other.Len)
                return {};
            
            T* vals = Alloc->Malloc<T>(other.Len);
            n_check_true(vals);
            n_defer { free(vals); };
            
            uint64 curIdx = 0;
            ListNode<T>* curNode = other.Head;
            while(curNode)
            {
                n_check_lt(curIdx, other.Len);
                vals[curIdx++] = curNode->Value;
            }
            
            (void) AppendRange(nodeToInsertAfter, n_view<const T> { vals, other.Len }).ntry();
            return {};
        }
        
        //NOTE: `otherNodeEnd` is EXCLUSIVE
        inline n_result<void> CloneRange(   n_ref ListNode<T>* nodeToInsertAfter, 
                                            n_ref ListNode<T>* otherNodeBegin,
                                            n_ref ListNode<T>* otherNodeEnd)
        {
            n_check_true(Alloc);
            n_check_true(otherNodeBegin);
            n_check_true(otherNodeEnd);
            
            uint64 cap = 64;
            T* vals = Alloc->Malloc<T>(cap);
            n_check_true(vals);
            n_defer { free(vals); };
            
            uint64 curIdx = 0;
            ListNode<T>* curNode = otherNodeBegin;
            while(curNode != otherNodeEnd)
            {
                if(curIdx >= cap)
                {
                    uint64 newCap = cap * 2;
                    if(newCap < cap)
                        newCap = UINT64_MAX;
                    
                    T* t = Alloc->Realloc<T>(vals, newCap);
                    n_check_true(t);
                    vals = t;
                    cap = newCap;
                }
                
                vals[curIdx++] = curNode->Value;
                curNode = curNode->Next;
            }
            
            (void) AppendRange(nodeToInsertAfter, n_view<const T> { vals, curIdx }).ntry();
            return {};
        }
        
        inline n_result<void> Free()
        {
            n_check_true(Alloc);
            
            ListNode<T>* currentNode = Head;
            while(currentNode)
            {
                ListNode<T>* next = currentNode->Next;
                if(!currentNode->Batched)
                    Alloc->Free(currentNode);
                currentNode = next;
            }
            Len = 0;
            Head = NULL;
            Tail = NULL;
            return {};
        }
    };
}


#endif
