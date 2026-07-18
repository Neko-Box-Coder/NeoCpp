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
        
        inline LinkedList InitValues(nref Allocator& alloc, uint8_t count,  ...)
        {
            LinkedList<T> l = Init(alloc, count);
            
            va_list args;
            va_start(args, count);
            for (int i = 0; i < count; ++i)
                l.Append(l.Tail, va_arg(args, T));
            va_end(args);
            return l;
        }
        
        #ifndef NSTD_INIT_VALUES
            #define NSTD_INIT_VALUES(alloc, ...) InitValues(alloc, MPT_ARGS_COUNT(__VA_ARGS__), __VA_ARGS__ )
        #endif
        
        
        inline nresult<void> Reserve(uint64_t size)
        {
            Alloc->Reserve<LinkedList<T>>(size);
            return {};
        }
        
        inline nresult<ListNode<T>*> AppendRange(nref ListNode<T>* node, View<T> view)
        {
            nuse_error_defer();
            ncheck_true(Alloc);
            if(!view.Len)
                return {};
            
            ListNode<T>* newNodes = Alloc->Malloc<ListNode<T>>(view.Len);
            ncheck_true(newNodes);
            nerror_defer { Alloc->Free(newNodes); };
            
            for(int i = 0; i < view.Len; ++i)
            {
                ListNode<T>* Next;
                ListNode<T>* Prev;
                T Value;
                bool Batched;
                
                if(i == 0 && i == view.Len)
                    newNodes[i] = { node->Next, node, view.Data[i], false };
                else if(i == 0)
                    newNodes[i] = { &newNodes[i + 1], node, view.Data[i], false };
                else if(i == view.Len)
                    newNodes[i] = { node->Next, &newNodes[i - 1], view.Data[i], true };
                else
                    newNodes[i] = { &newNodes[i + 1], &newNodes[i - 1], view.Data[i], true };
            }
            
            if(node->Next)
                node->Next->Prev = &newNodes[view.Len - 1];
            node->Next = &newNodes[0];
            Len += view.Len;
            return &newNodes[view.Len - 1];
        }
        
        inline nresult<ListNode<T>*> PrependRange(nref ListNode<T>* node, View<T> view)
        {
            nuse_error_defer();
            ncheck_true(Alloc);
            if(!view.Len)
                return {};
            
            ListNode<T>* newNodes = Alloc->Malloc<ListNode<T>>(view.Len);
            ncheck_true(newNodes);
            nerror_defer { Alloc->Free(newNodes); };
            
            for(int i = 0; i < view.Len; ++i)
            {
                ListNode<T>* Next;
                ListNode<T>* Prev;
                T Value;
                bool Batched;
                
                if(i == 0 && i == view.Len)
                    newNodes[i] = { node, node->Prev, view.Data[i], false };
                else if(i == 0)
                    newNodes[i] = { &newNodes[i + 1], node->Prev, view.Data[i], false };
                else if(i == view.Len)
                    newNodes[i] = { node, &newNodes[i - 1], view.Data[i], true };
                else
                    newNodes[i] = { &newNodes[i + 1], &newNodes[i - 1], view.Data[i], true };
            }
            
            if(node->Prev)
                node->Prev->Next = &newNodes[0];
            node->Prev = &newNodes[view.Len - 1];
            Len += view.Len;
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
            
            (void) AppendRange(nodeToInsertAfter, View<T> { vals, other.Len }).ntry();
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
            
            (void) AppendRange(nodeToInsertAfter, View<T> { vals, curIdx }).ntry();
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
