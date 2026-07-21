#ifndef NSTD_HASHMAP_N_HPP
#define NSTD_HASHMAP_N_HPP

#include "ncpp.n.hpp"
#include "./Allocator.n.hpp"
#include "./KeyValue.n.hpp"

#define HASH_NONFATAL_OOM 1
#include "./External/uthash/src/uthash.h"

/* undefine the defaults */
#undef uthash_malloc
#undef uthash_free

/* re-define, specifying alternate functions */
#define uthash_malloc(sz) Alloc->Malloc<char>(sz)
#define uthash_free(ptr, sz) Alloc->Free(ptr)

#include <string.h>

namespace Nstd
{
    template<typename T>
    struct HashNode
    {
        n_view<const char> Key;
        T Value;
        UT_hash_handle hh;
        bool Batched;
    };

    template<typename T>
    struct Hashmap
    {
        Allocator* Alloc;
        HashNode<T>* Nodes;
    
        inline Hashmap<T> Init(n_ref Allocator& alloc) 
        { 
            Hashmap<T> h;
            h.Alloc = &alloc;
            h.Nodes = NULL;
            return h;
        }
        
        template<typename... Ts>
        inline n_result<void> AddValues(KeyValue<T> keyval, Ts... keyvals)
        {
            n_use_error_defer();
            n_check_true(Alloc);
            
            KeyValue<T>* arr[] = { &keyval, &keyvals... };
            
            HashNode<T>* nodes = Alloc->Malloc<HashNode<T>>(n_array_cap(arr));
            n_check_true(nodes);
            n_error_defer { free(nodes); };
            
            for(int i = 0; i < n_array_cap(arr); ++i)
                n_check_true(arr[i]->Key.data && arr[i]->Key.len);
            
            for(int i = 0; i < n_array_cap(arr); ++i)
            {
                nodes[i].Key = arr[i]->Key;
                nodes[i].Value = arr[i]->Value;
                nodes[i].Batched = i != 0;
                
                HASH_ADD_KEYPTR(hh, Nodes, nodes[i].Key.data, nodes[i].Key.len, &nodes[i]);
            }
            
            return {};
        }
        
        template<typename... Ts>
        inline Hashmap InitValues(n_ref Allocator& alloc, Ts... values)
        {
            Hashmap h = Init(alloc);
            h.AddValues(values...);
            return h;
        }
        
        inline n_result<void> Add(n_view<const char> key, T value)
        {
            n_check_true(Alloc);
            n_check_true((bool)key);
            
            HashNode<T>* n = Alloc->Malloc<HashNode<T>>(1);
            n_check_true(n);
            n->Key = key;
            n->Value = value;
            n->Batched = false;
            
            HASH_ADD_KEYPTR(hh, Nodes, n->Key.data, n->Key.len, n);
            return {};
        }

        inline n_result<HashNode<T>*> Find(n_view<const char> key)
        {
            n_check_true(Alloc);
            if(!key)
                return NULL;
            
            HashNode<T>* f = NULL;
            HASH_FIND(hh, Nodes, key.data, key.len, f);
            return f;
        }
        
        inline n_result<void> Remove(n_ref HashNode<T>*& node) 
        {
            n_check_true(Alloc);
            n_check_true(node);
            n_check_true(Nodes);
            
            HASH_DEL(Nodes, node);
            if(!node->Batched)
                Alloc->Free(node);
            
            node = NULL;
            return {};
        }
        
        inline n_result<void> Reserve(uint64 size)
        {
            n_check_true(Alloc);
            Alloc->Reserve<HashNode<T>>(size);
            return {};
        }
        
        inline n_result<void> AddRange(n_view<KeyValue<T>> keyValues)
        {
            n_check_true(Alloc);
            n_use_error_defer();
            if(!keyValues)
                return {};
            
            HashNode<T>* nodes = Alloc->Malloc<HashNode<T>>(keyValues.len);
            n_check_true(nodes);
            n_error_defer { free(nodes); };
            
            
            for(int i = 0; i < keyValues.len; ++i)
                n_check_true(keyValues.data[i].Key.data && keyValues.data[i].Key.len);
            
            for(int i = 0; i < keyValues.len; ++i)
            {
                nodes[i].Key = keyValues.data[i].Key;
                nodes[i].Value = keyValues.data[i].Value;
                nodes[i].Batched = i != 0;
                
                HASH_ADD_KEYPTR(hh, Nodes, nodes[i].Key.data, nodes[i].Key.len, &nodes[i]);
            }
            
            return {};
        }
        
        inline n_result<usize> Len()
        {
            n_check_true(Alloc);
            return HASH_COUNT(Nodes);
        }
        
        inline HashNode<T>* First()
        {
            return Nodes;
        }
        
        inline HashNode<T>* Next(n_in HashNode<T>* node)
        {
            if(!node)
                return NULL;
            return (HashNode<T>*)node->hh.next;
        }
        
        inline n_result<void> Free()
        {
            n_check_true(Alloc);
            
            HashNode<T>* curNode = NULL;
            HashNode<T>* tmp = NULL;
            HASH_ITER(hh, Nodes, curNode, tmp) 
            {
                HASH_DEL(Nodes, curNode);
                if(!curNode->Batched)
                    Alloc->Free(curNode);
            }
            
            Nodes = NULL;
            return {};
        }
    };
}




#endif
