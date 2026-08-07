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
            return (ByteViews.at<false>(i) >> (index % 8)) & 0x01;
        }
        
        template<bool CHECK = true>
        inline bool GetBit(usize index)
        {
            if(CHECK)
                n_assert(index / 8 < ByteViews.len);
            return (ByteViews.data[index / 8] >> (index % 8)) & 0x01;
        }
        
        template<bool REVERSE = false>
        inline n_result<ssize> GetBitsUntilFlipped(usize startIndex)
        {
            ssize i = (ssize)startIndex / 8;
            n_check_lt(i, ByteViews.len);
            usize b = startIndex % 8;
            
            uint8 s = GetBit(startIndex);
            if(!REVERSE)
            {
                for(uint8 j = b; j < 8; ++j)
                {
                    if(((ByteViews.at<false>(i) >> j) & 0x01) != s)
                        return i * 8 + j;
                }
            }
            else
            {
                for(int8 j = b; j >= 0; --j)
                {
                    if(((ByteViews.at<false>(i) >> j) & 0x01) != s)
                        return i * 8 + j;
                }
            }
            
            if(!REVERSE)
                ++i;
            else
                --i;
            
            uint8 checkValue = s ? UINT8_MAX : 0;
            if(!REVERSE)
            {
                for(; i < ByteViews.len; ++i)
                {
                    if(ByteViews.at<false>(i) != checkValue)
                    {
                        for(uint8 j = 0; j < 8; ++j)
                        {
                            if(((ByteViews.at<false>(i) >> j) & 0x01) != s)
                                return i * 8 + j;
                        }
                    }
                }
            }
            else
            {
                for(; i >= 0; --i)
                {
                    if(ByteViews.at<false>(i) != checkValue)
                    {
                        for(int8 j = 7; j >= 0; --j)
                        {
                            if(((ByteViews.at<false>(i) >> j) & 0x01) != s)
                                return i * 8 + j;
                        }
                    }
                }
            }
            
            return REVERSE ? -1 : (ssize)Len();
        }
        
        template<bool B>
        inline n_result<void> SetBitAt(usize index)
        {
            const usize i = index / 8;
            n_check_lt(i, ByteViews.len);
            
            if(B)
                ByteViews.at<false>(i) |= 1 << (index % 8);
            else
                ByteViews.at<false>(i) &= ~(1 << (index % 8));
            
            return {};
        }
        
        template<bool B, bool CHECK = true>
        inline void SetBit(usize index)
        {
            if(B)
                ByteViews.at<CHECK>(index / 8) |= 1 << (index % 8);
            else
                ByteViews.at<CHECK>(index / 8) &= ~(1 << (index % 8));
        }
        
        template<bool B, bool CHECK = false>
        inline void Intern_SetBits(usize index, usize range, usize s, usize e)
        {
            if(B)
            {
                for(usize i = s; i < e; ++i)
                {
                    if(i > s && i < e - 1)
                    {
                        ByteViews.at<CHECK>(i) = UINT8_MAX;
                        continue;
                    }
                    else
                    {
                        uint8 bitStartPos = 0;
                        uint8 bitEndPos = 8;
                        if(i == s)
                            bitStartPos = index % 8;
                        if(i == e - 1)
                        {
                            bitEndPos = (index + range) % 8;
                            bitEndPos = bitEndPos == 0 ? 8 : bitEndPos;
                        }
                        for(int j = bitStartPos; j < bitEndPos; ++j)
                            ByteViews.at<CHECK>(i) |= 1 << j;
                    }
                }
            }
            else
            {
                for(usize i = s; i < e; ++i)
                {
                    if(i > s && i < e - 1)
                    {
                        ByteViews.at<CHECK>(i) = 0;
                        continue;
                    }
                    else
                    {
                        uint8 bitStartPos = 0;
                        uint8 bitEndPos = 8;
                        if(i == s)
                            bitStartPos = index % 8;
                        if(i == e - 1)
                        {
                            bitEndPos = (index + range) % 8;
                            bitEndPos = bitEndPos == 0 ? 8 : bitEndPos;
                        }
                        for(int j = bitStartPos; j < bitEndPos; ++j)
                            ByteViews.at<CHECK>(i) &= ~(1 << j);
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
        
        template<bool B, bool CHECK = true>
        inline void SetBits(usize index, usize range)
        {
            const usize s = index / 8;
            const usize e = (index + range + 7) / 8;
            if(CHECK)
                n_assert(e <= ByteViews.len);
            Intern_SetBits<B, CHECK>(index, range, s, e);
            return;
        }
    };
    
    static_assert(n_is_simple(BitView), "");
}

#endif
