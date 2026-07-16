#ifndef NSTD_LINKED_LIST_HPP
#define NSTD_LINKED_LIST_HPP

#include "ncpp.hpp"

#include "./Allocator.hpp"
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
            ncheck_true(Alloc);
            ListNode<T>* newNode = Alloc->Malloc<ListNode<T>>(1);
            ndefer { Alloc->Free(newNode); };
            
            ListNode<T>* retNode = NULL;
            *newNode = { node, NULL, val };
            
            if(!node) //Empty list
            {
                ncheck_false(Head);
                ncheck_false(Tail);
                ncheck_eq(Len, 0);
                
                Head = newNode;
                Tail = newNode;
                ++Len;
                retNode = nmove(newNode);
                return {retNode};
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
            retNode = nmove(newNode);
            return {retNode};
        }
        
        inline nresult<ListNode<T>*> Prepend(nref ListNode<T>* node, T val)
        {
            ncheck_true(Alloc);
            ListNode<T>* newNode = Alloc->Malloc<ListNode<T>>(1);
            ndefer { Alloc->Free(newNode); };
            
            ListNode<T>* retNode = NULL;
            *newNode = { NULL, node, val };
            
            if(!node) //Empty list
            {
                ncheck_false(Head);
                ncheck_false(Tail);
                ncheck_eq(Len, 0);
                
                Head = newNode;
                Tail = newNode;
                ++Len;
                retNode = nmove(newNode);
                return {retNode};
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
            retNode = nmove(newNode);
            return {retNode};
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
                Alloc->Free(node);
                Head = next;
                --Len;
                return {};
            }
            else //Tail
            {
                ncheck_eq(node, Tail);
                Alloc->Free(node);
                Tail = prev;
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
        
        
        /*
        T Dummy;
        T* Data;
        uint64_t Len;
        uint64_t Cap;
        
        inline List Init(nref Allocator& alloc, uint64_t initSize)
        {
            List retList;
            retList.Alloc = &alloc;
            retList.Dummy = {};
            retList.Data = alloc.Malloc<T>(initSize);
            retList.Len = initSize;
            retList.Cap = initSize;
            return retList;
        }
        
        inline List InternInitValues(nref Allocator& alloc, uint8_t count,  ...)
        {
            List<T> l = Init(alloc, count);
            
            va_list args;
            va_start(args, count);
            for (int i = 0; i < count; ++i)
                l.Data[i] = va_arg(args, T);
            va_end(args);
            return l;
        }
        
        #define NSTD_INIT_LIST_VALUES(alloc, ...) InternInitValues(alloc, MPT_ARGS_COUNT(__VA_ARGS__), __VA_ARGS__ )
        
        inline T& At(uint64_t index)
        {
            if(index >= Len)
                return Dummy;
            else
                return Data[index];
        }
        
        inline const T& At(uint64_t index) const
        {
            if(index >= Len)
                return Dummy;
            else
                return Data[index];
        }
        
        inline nresult<void> Reserve(uint64_t size)
        {
            if(size <= Cap)
                return {};
            
            T* tmp = Alloc->Realloc(Data, size);
            if(!tmp)
                return nerror_msg("%s", "Failed to realloc");
            else
            {
                Data = tmp;
                Cap = size;
            }
            return {};
        }
        
        inline nresult<void> Resize(uint64_t size)
        {
            if(size <= Len)
                Len = size;
            else
            {
                Reserve(size).ntry();
                Len = size;
            }
            return {};
        }
        
        inline nresult<void> Add(T t)
        {
            if(Len >= Cap)
            {
                uint64_t reserveLen;
                if(UINT64_MAX / 2 < Cap)
                    reserveLen = UINT64_MAX;
                else
                    reserveLen = Cap * 2;
                Reserve(reserveLen).ntry();
            }
            
            Data[Len++] = t;
            return {};
        }
        
        inline nresult<void> Insert(T t, uint64_t index)
        {
            ncheck_lte(index, Len);
            Reserve(Len + 1).ntry();
            ++Len;
            
            if(index != Len - 1)
            {
                memmove(&Data[index + 1], &Data[index], sizeof(T) * (Len - index));
                return {};
            }
            else
            {
                Data[index] = t;
                return {};
            }
        }
        
        inline nresult<void> Remove(uint64_t index)
        {
            ncheck_lt(index, Len);
            if(index != Len - 1)
            {
                memmove(&Data[index], &Data[index + 1], sizeof(T) * (Len - index - 1));
                --Len;
                return {};
            }
            else
            {
                --Len;
                return {};
            }
        }
        */
    };
}


#endif
