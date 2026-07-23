#ifndef NSTD_BIT_VIEW_N_HPP
#define NSTD_BIT_VIEW_N_HPP

#include "ncpp.n.hpp"

namespace Nstd
{
    struct BitView
    {
        n_view<uint8> ByteViews;
        
        inline static BitView Init(n_view<uint8> bytes)
        {
            BitView bv = {};
            bv.ByteViews = bytes;
            return bv;
        }
        
        inline usize Len() const
        {
            return ByteViews.len * 8;
        }
        
        inline n_result<bool> GetBitAt(usize index)
        {
            const usize i = index / 8;
            n_check_lt(i, ByteViews.len);
            return (ByteViews.data[i] >> (index % 8)) & 0x01;
        }
        
        inline bool GetBit(usize index)
        {
            return (ByteViews.data[index / 8] >> (index % 8)) & 0x01;
        }
        
        inline n_result<usize> GetBitsUntilFlipped(usize startIndex)
        {
            usize i = startIndex / 8;
            n_check_lt(i, ByteViews.len);
            usize b = startIndex % 8;
            
            uint8 s = GetBit(startIndex);
            for(uint8 j = b; j < 8; ++j)
            {
                if(((ByteViews.data[i] >> j) & 0x01) != s)
                    return i * 8 + j;
            }
            
            ++i;
            if(s)
            {
                for(; i < ByteViews.len; ++i)
                {
                    if(ByteViews.data[i] != UINT8_MAX)
                    {
                        for(uint8 j = 0; j < 8; ++j)
                        {
                            if(((ByteViews.data[i] >> j) & 0x01) != s)
                                return i * 8 + j;
                        }
                    }
                }
            }
            else
            {
                for(; i < ByteViews.len; ++i)
                {
                    if(ByteViews.data[i] != 0)
                    {
                        for(uint8 j = 0; j < 8; ++j)
                        {
                            if(((ByteViews.data[i] >> j) & 0x01) != s)
                                return i * 8 + j;
                        }
                    }
                }
            }
            
            return Len();
        }
        
        template<bool B>
        inline n_result<void> SetBitAt(usize index)
        {
            const usize i = index / 8;
            n_check_lt(i, ByteViews.len);
            
            if(B)
                ByteViews.data[i] |= 1 << (index % 8);
            else
                ByteViews.data[i] &= ~(1 << (index % 8));
            
            return {};
        }
        
        template<bool B>
        inline void SetBit(usize index)
        {
            if(B)
                ByteViews.data[index / 8] |= 1 << (index % 8);
            else
                ByteViews.data[index / 8] &= ~(1 << (index % 8));
        }
        
        template<bool B>
        inline void Intern_SetBits(usize index, usize range, usize s, usize e)
        {
            if(B)
            {
                for(usize i = s; i < e; ++i)
                {
                    if(i > s && i < e - 1)
                    {
                        ByteViews.data[i] = UINT8_MAX;
                        continue;
                    }
                    else
                    {
                        uint8 bitStartPos = 0;
                        uint8 bitEndPos = 8;
                        if(i == s)
                            bitStartPos = index % 8;
                        if(i == e - 1)
                            bitEndPos = (index + range) % 8;
                        for(int j = bitStartPos; j < bitEndPos; ++j)
                            ByteViews.data[i] |= 1 << j;
                    }
                }
            }
            else
            {
                for(usize i = s; i < e; ++i)
                {
                    if(i > s && i < e - 1)
                    {
                        ByteViews.data[i] = 0;
                        continue;
                    }
                    else
                    {
                        uint8 bitStartPos = 0;
                        uint8 bitEndPos = 8;
                        if(i == s)
                            bitStartPos = index % 8;
                        if(i == e - 1)
                            bitEndPos = (index + range) % 8;
                        for(int j = bitStartPos; j < bitEndPos; ++j)
                            ByteViews.data[i] &= ~(1 << j);
                    }
                }
            }
        }
        
        template<bool B>
        inline n_result<void> SetBitsAt(usize index, usize range)
        {
            const usize s = index / 8;
            const usize e = (index + range + 7) / 8;
            n_check_lte(e, ByteViews.len);
            Intern_SetBits<B>(index, range, s, e);
            return {};
        }
        
        template<bool B>
        inline void SetBits(usize index, usize range)
        {
            const usize s = index / 8;
            const usize e = (index + range + 7) / 8;
            n_assert(e <= ByteViews.len);
            Intern_SetBits<B>(index, range, s, e);
            return;
        }
    };
    
    static_assert(n_is_simple(BitView), "");
}

#endif
