#ifndef NSTD_LINKED_LIST_N_HPP
#define NSTD_LINKED_LIST_N_HPP

#include "ncpp.n.hpp"

#include "./Allocator.n.hpp"
#include "./External/MacroPowerToys/ArgsCount.h"
#include <stdint.h>
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
    
    template<typename T, nenable_if(nis_simple(T))>
    struct LinkedList
    {
        Allocator* Alloc;
        ListNode<T>* Head;
        ListNode<T>* Tail;
        uint64_t Len;
        
        
        inline LinkedList Init(nref Allocator& alloc)
        {
            LinkedList retList;
            retList.Alloc = &alloc;
            retList.Head = NULL;
            retList.Tail = NULL;
            retList.Len = 0;
            return retList;
        }
        
        template<typename... Ts>
        inline nresult<ListNode<T>*> AppendValues(nref ListNode<T>* node, Ts... values)
        {
            ncheck_true(Alloc);
            
            T* arr[] = { &values... };
            ListNode<T>* newNodes = Alloc->Malloc<ListNode<T>>(narray_cap(arr));
            ncheck_true(newNodes);
            for(int i = 0; i < narray_cap(arr); ++i)
            {
                ListNode<T>* Next;
                ListNode<T>* Prev;
                T Value;
                bool Batched;
                
                if(i == 0 && i == narray_cap(arr) - 1)
                    newNodes[i] = { node->Next, node, *arr[i], false };
                else if(i == 0)
                    newNodes[i] = { &newNodes[i + 1], node, *arr[i], false };
                else if(i == narray_cap(arr) - 1)
                    newNodes[i] = { node->Next, &newNodes[i - 1], *arr[i], true };
                else
                    newNodes[i] = { &newNodes[i + 1], &newNodes[i - 1], *arr[i], true };
            }
            
            if(node->Next)
                node->Next->Prev = &newNodes[narray_cap(arr) - 1];
            node->Next = &newNodes[0];
            Len += narray_cap(arr);
            return &newNodes[narray_cap(arr) - 1];
        }
        
        template<typename... Ts>
        inline nresult<ListNode<T>*> PrependValues(nref ListNode<T>* node, Ts... values)
        {
            ncheck_true(Alloc);
            
            T* arr[] = { &values... };
            ListNode<T>* newNodes = Alloc->Malloc<ListNode<T>>(narray_cap(arr));
            ncheck_true(newNodes);
            for(int i = 0; i < narray_cap(arr); ++i)
            {
                ListNode<T>* Next;
                ListNode<T>* Prev;
                T Value;
                bool Batched;
                
                if(i == 0 && i == narray_cap(arr) - 1)
                    newNodes[i] = { node, node->Prev, *arr[i], false };
                else if(i == 0)
                    newNodes[i] = { &newNodes[i + 1], node->Prev, *arr[i], false };
                else if(i == narray_cap(arr) - 1)
                    newNodes[i] = { node, &newNodes[i - 1], *arr[i], true };
                else
                    newNodes[i] = { &newNodes[i + 1], &newNodes[i - 1], *arr[i], true };
            }
            
            if(node->Prev)
                node->Prev->Next = &newNodes[0];
            node->Prev = &newNodes[narray_cap(arr) - 1];
            Len += narray_cap(arr);
            return &newNodes[0];
        }
        
        template<typename... Ts>
        inline LinkedList InitValues(nref Allocator& alloc, Ts... values)
        {
            LinkedList l = Init(alloc);
            l.AppendValues(l.Tail, values...);
            return l;
        }
        
        inline nresult<ListNode<T>*> Append(nref ListNode<T>* node, T val)
        {
            nuse_error_defer();
            
            ncheck_true(Alloc);
            ListNode<T>* newNode = Alloc->Malloc<ListNode<T>>(1);
            ncheck_true(newNode);
            nerror_defer { Alloc->Free(newNode); };
            
            *newNode = { node, NULL, val, false };
            
            if(!node) //Empty list
            {
                ncheck_false(Head);
                ncheck_false(Tail);
                ncheck_eq(Len, 0);
                
                Head = newNode;
                Tail = newNode;
                ++Len;
                return newNode;
            }
            
            if(node->Next) //Not tail
            {
                ncheck_eq(node->Next->Prev, node);
                ListNode<T> origNext = node->Next;
                newNode->Next = origNext;
                origNext->Prev = newNode;
            }
            else //Tail
                ncheck_eq(node, Tail);
            
            node->Next = newNode;
            ++Len;
            return newNode;
        }
        
        inline nresult<ListNode<T>*> Prepend(nref ListNode<T>* node, T val)
        {
            nuse_error_defer();
            
            ncheck_true(Alloc);
            ListNode<T>* newNode = Alloc->Malloc<ListNode<T>>(1);
            ncheck_true(newNode);
            nerror_defer { Alloc->Free(newNode); };
            
            *newNode = { NULL, node, val, false };
            
            if(!node) //Empty list
            {
                ncheck_false(Head);
                ncheck_false(Tail);
                ncheck_eq(Len, 0);
                
                Head = newNode;
                Tail = newNode;
                ++Len;
                return newNode;
            }
            
            if(node->Prev) //Not Head
            {
                ncheck_eq(node->Prev->Next, node);
                ListNode<T> origPrev = node->Prev;
                newNode->Prev = origPrev;
                origPrev->Next = newNode;
            }
            else //Head
                ncheck_eq(node, Head);
            
            node->Prev = newNode;
            ++Len;
            return newNode;
        }
        
        inline nresult<void> Remove(nref ListNode<T>* node)
        {
            if(!node)
                return {};
            
            ncheck_true(Alloc);
            ncheck_true(Head && Tail);
            
            ListNode<T>* next = node->Next;
            ListNode<T>* prev = node->Prev;
            if(!prev) //Head
            {
                ncheck_eq(node, Head);
                if(!node->Batched)
                    Alloc->Free(node);
                Head = next;
                next->Prev = NULL;
                --Len;
                return {};
            }
            else if(!next) //Tail
            {
                ncheck_eq(node, Tail);
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
        
        inline nresult<void> Reserve(uint64_t size)
        {
            ncheck_true(Alloc);
            Alloc->Reserve<LinkedList<T>>(size);
            return {};
        }
        
        inline nresult<ListNode<T>*> AppendRange(nref ListNode<T>* node, nview<const T> v)
        {
            ncheck_true(Alloc);
            if(!v)
                return {};
            
            ListNode<T>* newNodes = Alloc->Malloc<ListNode<T>>(v.len);
            ncheck_true(newNodes);
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
        
        inline nresult<ListNode<T>*> PrependRange(nref ListNode<T>* node, nview<const T> v)
        {
            ncheck_true(Alloc);
            if(!v)
                return {};
            
            ListNode<T>* newNodes = Alloc->Malloc<ListNode<T>>(v.len);
            ncheck_true(newNodes);
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
        
        inline nresult<void> Clone(nref ListNode<T>* nodeToInsertAfter, nref LinkedList<T>& other)
        {
            ncheck_true(Alloc);
            
            if(!other.Len)
                return {};
            
            T* vals = Alloc->Malloc<T>(other.Len);
            ncheck_true(vals);
            ndefer { free(vals); };
            
            uint64_t curIdx = 0;
            ListNode<T>* curNode = other.Head;
            while(curNode)
            {
                ncheck_lt(curIdx, other.Len);
                vals[curIdx++] = curNode->Value;
            }
            
            (void) AppendRange(nodeToInsertAfter, nview<const T> { vals, other.Len }).ntry();
            return {};
        }
        
        //NOTE: `otherNodeEnd` is EXCLUSIVE
        inline nresult<void> CloneRange(nref ListNode<T>* nodeToInsertAfter, 
                                        nref ListNode<T>* otherNodeBegin,
                                        nref ListNode<T>* otherNodeEnd)
        {
            ncheck_true(Alloc);
            ncheck_true(otherNodeBegin);
            ncheck_true(otherNodeEnd);
            
            uint64_t cap = 64;
            T* vals = Alloc->Malloc<T>(cap);
            ncheck_true(vals);
            ndefer { free(vals); };
            
            uint64_t curIdx = 0;
            ListNode<T>* curNode = otherNodeBegin;
            while(curNode != otherNodeEnd)
            {
                if(curIdx >= cap)
                {
                    uint64_t newCap = cap * 2;
                    if(newCap < cap)
                        newCap = UINT64_MAX;
                    
                    T* t = Alloc->Realloc<T>(vals, newCap);
                    ncheck_true(t);
                    vals = t;
                    cap = newCap;
                }
                
                vals[curIdx++] = curNode->Value;
                curNode = curNode->Next;
            }
            
            (void) AppendRange(nodeToInsertAfter, nview<const T> { vals, curIdx }).ntry();
            return {};
        }
        
        inline nresult<void> Free()
        {
            ncheck_true(Alloc);
            
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
