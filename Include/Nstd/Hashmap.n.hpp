#ifndef NSTD_HASHMAP_N_HPP
#define NSTD_HASHMAP_N_HPP

#include "ncpp.n.hpp"
#include "./Allocator.n.hpp"
#include "./KeyValue.n.hpp"

#define HASH_NONFATAL_OOM 1
#include "./External/uthash/include/uthash.h"

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
        nview<const char> Key;
        T Value;
        UT_hash_handle hh;
        bool Batched;
    };

    template<typename T>
    struct Hashmap
    {
        Allocator* Alloc;
        HashNode<T>* Nodes;
    
        inline Hashmap<T> Init(nref Allocator& alloc) 
        { 
            Hashmap<T> h;
            h.Alloc = &alloc;
            h.Nodes = NULL;
            return h;
        }
        
        template<typename... Ts>
        inline nresult<void> AddValues(KeyValue<T> keyval, Ts... keyvals)
        {
            nuse_error_defer();
            ncheck_true(Alloc);
            
            KeyValue<T>* arr[] = { &keyval, &keyvals... };
            
            HashNode<T>* nodes = Alloc->Malloc<HashNode<T>>(narray_cap(arr));
            ncheck_true(nodes);
            nerror_defer { free(nodes); };
            
            for(int i = 0; i < narray_cap(arr); ++i)
                ncheck_true(arr[i]->Key.data && arr[i]->Key.len);
            
            for(int i = 0; i < narray_cap(arr); ++i)
            {
                nodes[i].Key = arr[i]->Key;
                nodes[i].Value = arr[i]->Value;
                nodes[i].Batched = i != 0;
                
                HASH_ADD_KEYPTR(hh, Nodes, nodes[i].Key.data, nodes[i].Key.len, &nodes[i]);
            }
            
            return {};
        }
        
        template<typename... Ts>
        inline Hashmap InitValues(nref Allocator& alloc, Ts... values)
        {
            Hashmap h = Init(alloc);
            h.AddValues(values...);
            return h;
        }
        
        inline nresult<void> Add(nview<const char> key, T value)
        {
            ncheck_true(Alloc);
            ncheck_true((bool)key);
            
            HashNode<T>* n = Alloc->Malloc<HashNode<T>>(1);
            ncheck_true(n);
            n->Key = key;
            n->Value = value;
            n->Batched = false;
            
            HASH_ADD_KEYPTR(hh, Nodes, n->Key.data, n->Key.len, n);
            return {};
        }

        inline nresult<HashNode<T>*> Find(nview<const char> key)
        {
            ncheck_true(Alloc);
            if(!key)
                return NULL;
            
            HashNode<T>* f = NULL;
            HASH_FIND(hh, Nodes, key.data, key.len, f);
            return f;
        }
        
        inline nresult<void> Remove(nref HashNode<T>*& node) 
        {
            ncheck_true(Alloc);
            ncheck_true(node);
            ncheck_true(Nodes);
            
            HASH_DEL(Nodes, node);
            if(!node->Batched)
                Alloc->Free(node);
            
            node = NULL;
            return {};
        }
        
        inline nresult<void> Reserve(uint64_t size)
        {
            ncheck_true(Alloc);
            Alloc->Reserve<HashNode<T>>(size);
            return {};
        }
        
        inline nresult<void> AddRange(nview<KeyValue<T>> keyValues)
        {
            ncheck_true(Alloc);
            nuse_error_defer();
            if(!keyValues)
                return {};
            
            HashNode<T>* nodes = Alloc->Malloc<HashNode<T>>(keyValues.len);
            ncheck_true(nodes);
            nerror_defer { free(nodes); };
            
            
            for(int i = 0; i < keyValues.len; ++i)
                ncheck_true(keyValues.data[i].Key.data && keyValues.data[i].Key.len);
            
            for(int i = 0; i < keyValues.len; ++i)
            {
                nodes[i].Key = keyValues.data[i].Key;
                nodes[i].Value = keyValues.data[i].Value;
                nodes[i].Batched = i != 0;
                
                HASH_ADD_KEYPTR(hh, Nodes, nodes[i].Key.data, nodes[i].Key.len, &nodes[i]);
            }
            
            return {};
        }
        
        inline nresult<size_t> Len()
        {
            ncheck_true(Alloc);
            return HASH_COUNT(Nodes);
        }
        
        inline HashNode<T>* First()
        {
            return Nodes;
        }
        
        inline HashNode<T>* Next(nin_ref HashNode<T>* node)
        {
            if(!node)
                return NULL;
            return (HashNode<T>*)node->hh.next;
        }
        
        inline nresult<void> Free()
        {
            ncheck_true(Alloc);
            
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
