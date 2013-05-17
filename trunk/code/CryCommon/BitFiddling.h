/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: various integer bit fiddling hacks
  
 -------------------------------------------------------------------------
  History:
  - 29:03:2006   12:44 : Created by Craig Tiller

*************************************************************************/
#ifndef __BITFIDDLING_H__
#define __BITFIDDLING_H__

#pragma once

#if defined(LINUX)
#define countLeadingZeros32(x)              __builtin_clz(x)
#elif defined(XENON)
#define countLeadingZeros32(x)              _CountLeadingZeros(x)
#elif defined(CAFE)
ILINE uint8 countLeadingZeros32(uint32 x)
{
	uint8 res=0;
	while (!(x&(1<<31)))
	{
		res++;
		x<<=1;
		x|=1;
	}
	return res;
}
#elif defined(PS3)
#if defined(__SPU__)
#define countLeadingZeros32(x)              (spu_extract( spu_cntlz( spu_promote((x),0) ),0) )
#else // #if defined(__SPU__)
// SDK3.2 linker errors when using __cntlzw
ILINE uint8 countLeadingZeros32( uint32 x)
{
	long result;
#if defined(__SNC__)
	result = __cntlzw(x);
#else
	__asm__ ("cntlzw %0, %1" 
			/* outputs:  */ : "=r" (result) 
			/* inputs:   */ : "r" (x));
#endif
	return result;
}

#endif // #if defined(__SPU__)
#else		// Windows implementation
ILINE uint32 countLeadingZeros32(uint32 x)
{
  DWORD result = 32^31;	// assumes result is unmodified if _BitScanReverse returns 0
	_BitScanReverse(&result,x);
	result^=31; // needed because the index is from LSB (whereas all other implementations are from MSB)
  return result;
}
#endif

class CBitIter
{
public:
	ILINE CBitIter( uint8 x ) : m_val(x) {}
	ILINE CBitIter( uint16 x ) : m_val(x) {}
	ILINE CBitIter( uint32 x ) : m_val(x) {}

	template <class T>
	ILINE bool Next( T& rIndex )
	{
		bool r = (m_val != 0);
		uint32 maskLSB;

		// Rewritten to use clz technique. Note the iterator now works from lsb to msb but that shouldn't matter - this way we avoid microcoded instructions
		maskLSB = m_val & (~(m_val-1));
		rIndex = 31-countLeadingZeros32(maskLSB);
		m_val &= ~maskLSB;
		return r;
	}

private:
	uint32 m_val;
};

// this function returns the integer logarithm of various numbers without branching
#define IL2VAL(mask,shift) \
	c |= ((x&mask)!=0) * shift; \
	x >>= ((x&mask)!=0) * shift

template <typename TInteger>
inline bool IsPowerOfTwo( TInteger x )
{
	return (x & (x-1)) == 0;
}

// compile time version of IsPowerOfTwo, useful for STATIC_CHECK
template <int nValue>
struct IsPowerOfTwoCompileTime
{
	enum { IsPowerOfTwo = ((nValue & (nValue-1)) == 0) };	
};

inline uint8 IntegerLog2( uint8 x )
{
	uint8 c = 0;
	IL2VAL(0xf0,4);
	IL2VAL(0xc,2);
	IL2VAL(0x2,1);
	return c;
}
inline uint16 IntegerLog2( uint16 x )
{
	uint16 c = 0;
	IL2VAL(0xff00,8);
	IL2VAL(0xf0,4);
	IL2VAL(0xc,2);
	IL2VAL(0x2,1);
	return c;
}

inline uint32 IntegerLog2( uint32 x )
{
	return 31 - countLeadingZeros32(x);
}

inline uint64 IntegerLog2( uint64 x )
{
	uint64 c = 0;
	IL2VAL(0xffffffff00000000ull,32);
	IL2VAL(0xffff0000u,16);
	IL2VAL(0xff00,8);
	IL2VAL(0xf0,4);
	IL2VAL(0xc,2);
	IL2VAL(0x2,1);
	return c;
}
#undef IL2VAL

template <typename TInteger>
inline TInteger IntegerLog2_RoundUp( TInteger x )
{
	return 1 + IntegerLog2(x-1);
}

static ILINE uint8 BitIndex( uint8 v )
{
	uint32 vv=v;
	return 31-countLeadingZeros32(vv);
}

static ILINE uint8 BitIndex( uint16 v )
{
	uint32 vv=v;
	return 31-countLeadingZeros32(vv);
}

static ILINE uint8 BitIndex( uint32 v )
{
	return 31-countLeadingZeros32(v);
}

static ILINE uint8 CountBits( uint8 v )
{
	uint8 c = v;
	c = ((c>>1) & 0x55) + (c&0x55);
	c = ((c>>2) & 0x33) + (c&0x33);
	c = ((c>>4) & 0x0f) + (c&0x0f);
	return c;
}

static ILINE uint8 CountBits( uint16 v )
{
	return	CountBits((uint8)(v	 &0xff)) + 
		CountBits((uint8)((v>> 8)&0xff));
}

static ILINE uint8 CountBits( uint32 v )
{
	return	CountBits((uint8)(v			 &0xff)) + 
					CountBits((uint8)((v>> 8)&0xff)) + 
					CountBits((uint8)((v>>16)&0xff)) + 
					CountBits((uint8)((v>>24)&0xff));
}

// Branchless version of return v < 0 ? alt : v;
ILINE int32 Isel32(int32 v, int32 alt)
{
	return ((static_cast<int32>(v) >> 31) & alt) | ((static_cast<int32>(~v) >> 31) & v);
}

template <uint32 ILOG>
struct CompileTimeIntegerLog2
{
	static const uint32 result = 1 + CompileTimeIntegerLog2<(ILOG>>1)>::result;
};
template <>
struct CompileTimeIntegerLog2<0>
{
	static const uint32 result = 0;
};

template <uint32 ILOG>
struct CompileTimeIntegerLog2_RoundUp
{
	static const uint32 result = CompileTimeIntegerLog2<ILOG>::result + ((ILOG & (ILOG-1))>0);
};

// Character-to-bitfield mapping

inline uint32 AlphaBit(char c)
{
	return c >= 'a' && c <= 'z' ? 1 << (c-'z'+31) : 0;
}

inline uint32 AlphaBits(uint32 wc)
{
	// Handle wide multi-char constants, can be evaluated at compile-time.
	return AlphaBit((char)wc) 
		| AlphaBit((char)(wc>>8))
		| AlphaBit((char)(wc>>16))
		| AlphaBit((char)(wc>>24));
}

inline uint32 AlphaBits(const char* s)
{
	// Handle string of any length.
	uint32 n = 0;
	while (*s)
		n |= AlphaBit(*s++);
	return n;
}


// if hardware doesn't support 3Dc we can convert to DXT5 (different channels are used)
// with almost the same quality but the same memory requirements
inline void ConvertBlock3DcToDXT5( uint8 pDstBlock[16], const uint8 pSrcBlock[16] )
{
	assert(pDstBlock!=pSrcBlock);		// does not work in place

	// 4x4 block requires 8 bytes in DXT5 or 3DC

	// DXT5:  8 bit alpha0, 8 bit alpha1, 16*3 bit alpha lerp
	//        16bit col0, 16 bit col1 (R5G6B5 low byte then high byte), 16*2 bit color lerp

	//  3DC:  8 bit x0, 8 bit x1, 16*3 bit x lerp
	//        8 bit y0, 8 bit y1, 16*3 bit y lerp

	for(uint32 dwK=0;dwK<8;++dwK)
		pDstBlock[dwK]=pSrcBlock[dwK];
	for(uint32 dwK=8;dwK<16;++dwK)
		pDstBlock[dwK]=0;

	// 6 bit green channel (highest bits)
	// by using all 3 channels with a slight offset we can get more precision but then a dot product would be needed in PS
	// because of bilinear filter we cannot just distribute bits to get perfect result
	uint16 colDst0 = (((uint16)pSrcBlock[8]+2)>>2)<<5;
	uint16 colDst1 = (((uint16)pSrcBlock[9]+2)>>2)<<5;

	bool bFlip = colDst0<=colDst1;

	if(bFlip)
	{
		uint16 help = colDst0;
		colDst0=colDst1;
		colDst1=help;
	}

	bool bEqual = colDst0==colDst1;

	// distribute bytes by hand to not have problems with endianess
	pDstBlock[8+0] = (uint8)colDst0;
	pDstBlock[8+1] = (uint8)(colDst0>>8);
	pDstBlock[8+2] = (uint8)colDst1;
	pDstBlock[8+3] = (uint8)(colDst1>>8);

	uint16 *pSrcBlock16 = (uint16 *)(pSrcBlock+10);
	uint16 *pDstBlock16 = (uint16 *)(pDstBlock+12);

	// distribute 16 3 bit values to 16 2 bit values (loosing LSB)
	for(uint32 dwK=0;dwK<16;++dwK)
	{
		uint32 dwBit0 = dwK*3+0;
		uint32 dwBit1 = dwK*3+1;
		uint32 dwBit2 = dwK*3+2;

		uint8 hexDataIn = (((pSrcBlock16[(dwBit2>>4)]>>(dwBit2&0xf))&1)<<2)			// get HSB
			| (((pSrcBlock16[(dwBit1>>4)]>>(dwBit1&0xf))&1)<<1)
			| ((pSrcBlock16[(dwBit0>>4)]>>(dwBit0&0xf))&1);					// get LSB

		uint8 hexDataOut;

		switch(hexDataIn)
		{
			case 0: hexDataOut=0;break;		// color 0
			case 1: hexDataOut=1;break;		// color 1

			case 2: hexDataOut=0;break;		// mostly color 0
			case 3: hexDataOut=2;break;
			case 4: hexDataOut=2;break;
			case 5: hexDataOut=3;break;
			case 6: hexDataOut=3;break;
			case 7: hexDataOut=1;break;		// mostly color 1

			default:
				assert(0);
		}

		if(bFlip)
		{
			if(hexDataOut<2)
				hexDataOut=1-hexDataOut;		// 0<->1
			else
				hexDataOut = 5-hexDataOut;	// 2<->3
		}

		if(bEqual)
			if(hexDataOut==3)
				hexDataOut=1;

		pDstBlock16[(dwK>>3)] |= (hexDataOut<<((dwK&0x7)<<1));
	}
}




// is a bit on in a new bit field, but off in an old bit field
static ILINE bool TurnedOnBit( unsigned bit, unsigned oldBits, unsigned newBits )
{
	return (newBits & bit) != 0 && (oldBits & bit) == 0;
}







inline uint32 cellUtilCountLeadingZero(uint32 x)
{
	uint32 y;
	uint32 n = 32;

	y = x >> 16; if (y != 0) { n = n - 16; x = y; }
	y = x >>  8; if (y != 0) { n = n -  8; x = y; }
	y = x >>  4; if (y != 0) { n = n -  4; x = y; }
	y = x >>  2; if (y != 0) { n = n -  2; x = y; }
	y = x >>  1; if (y != 0) { return n - 2; }
	return n - x;
}

inline uint32 cellUtilLog2(uint32 x)
{
	return 31 - cellUtilCountLeadingZero(x);
}




inline void convertSwizzle(uint8*& dst,const uint8*&	src,
										const uint32 SrcPitch, const uint32 depth,
										const uint32 xpos, const uint32 ypos,
										const uint32 SciX1,const uint32 SciY1,
										const uint32 SciX2,const uint32 SciY2,
										const uint32 level)
{
	if (level == 1)
	{
		switch(depth)
		{
		case 16:
			if(xpos>=SciX1 && xpos<SciX2 && ypos>=SciY1 && ypos<SciY2)
			{
				//					*((uint32*&)dst)++ = ((uint32*)src)[ypos * width + xpos];
				//					*((uint32*&)dst)++ = ((uint32*)src)[ypos * width + xpos+1];
				//					*((uint32*&)dst)++ = ((uint32*)src)[ypos * width + xpos+2];
				//					*((uint32*&)dst)++ = ((uint32*)src)[ypos * width + xpos+3];
				*((uint32*&)dst)++ = *((uint32*)(src+(ypos * SrcPitch + xpos*16)));
				*((uint32*&)dst)++ = *((uint32*)(src+(ypos * SrcPitch + xpos*16+4)));
				*((uint32*&)dst)++ = *((uint32*)(src+(ypos * SrcPitch + xpos*16+8)));
				*((uint32*&)dst)++ = *((uint32*)(src+(ypos * SrcPitch + xpos*16+12)));
			}
			else
				((uint32*&)dst)+=4;
			break;
		case 8:
			if(xpos>=SciX1 && xpos<SciX2 && ypos>=SciY1 && ypos<SciY2)
			{
				*((uint32*&)dst)++ = *((uint32*)(src+(ypos * SrcPitch + xpos*8)));
				*((uint32*&)dst)++ = *((uint32*)(src+(ypos * SrcPitch + xpos*8+4)));
			}
			else
				((uint32*&)dst)+=2;
			break;
		case 4:
			if(xpos>=SciX1 && xpos<SciX2 && ypos>=SciY1 && ypos<SciY2)
				*((uint32*&)dst) = *((uint32*)(src+(ypos * SrcPitch + xpos*4)));
			dst+=4;
			break;
		case 3:
			if(xpos>=SciX1 && xpos<SciX2 && ypos>=SciY1 && ypos<SciY2)
			{
				*dst++ = src[ypos * SrcPitch + xpos * depth];
				*dst++ = src[ypos * SrcPitch + xpos * depth + 1];
				*dst++ = src[ypos * SrcPitch + xpos * depth + 2];
			}
			else
				dst+=3;
			break;
		case 1:
			if(xpos>=SciX1 && xpos<SciX2 && ypos>=SciY1 && ypos<SciY2)
				*dst++ = src[ypos * SrcPitch + xpos * depth];
			else
				dst++;
			break;
		default: assert(0);
		}
		return;
	}
	else
	{
		convertSwizzle(dst, src, SrcPitch, depth, xpos, ypos,SciX1,SciY1,SciX2,SciY2, level - 1);
		convertSwizzle(dst, src, SrcPitch, depth,xpos + (1U << (level - 2)), ypos,SciX1,SciY1,SciX2,SciY2, level - 1);
		convertSwizzle(dst, src, SrcPitch, depth,xpos, ypos + (1U << (level - 2)),SciX1,SciY1,SciX2,SciY2, level - 1);
		convertSwizzle(dst, src, SrcPitch, depth, xpos + (1U << (level - 2)),ypos + (1U << (level - 2)),SciX1,SciY1,SciX2,SciY2, level - 1);
	}
}




inline void Linear2Swizzle(		uint8*	 dst,
										const uint8*	 src,
										const uint32 SrcPitch,
										const uint32 width,
										const uint32 height,
										const uint32 depth,
										const uint32 SciX1,const uint32 SciY1,
										const uint32 SciX2,const uint32 SciY2)
{
/*
#if defined(_DEBUG)
	//PS3HACK 
	if(width==0 || height==0)
	{
		while(true)
		{
			CRY_ASSERT_MESSAGE(width==0 || height==0,"width==0 || height==0");
			int a=0;
		}
	}
#endif
*/

	/*	cellGcmConvertSwizzleFormat(dst,src,
	0,0,0,width,height,0,
	0,0,0,width,height,0,width,height,0,
	depth,depth,CELL_GCM_FALSE,2,NULL);

	return;*/

	src-=SciY1*SrcPitch+SciX1*depth;
	if (width == height)
		convertSwizzle(dst, src, SrcPitch, depth, 0, 0,SciX1,SciY1,SciX2,SciY2, cellUtilLog2(width) + 1);
	else
		if (width > height)
		{
			uint32 baseLevel	= cellUtilLog2(width)-(cellUtilLog2(width)-cellUtilLog2(height));
			for (uint32 i = 0;i < (1UL << (cellUtilLog2(width)-cellUtilLog2(height))); i++)
				convertSwizzle(dst, src, SrcPitch, depth,(1U << baseLevel)*i, 0,SciX1,SciY1,SciX2,SciY2, baseLevel + 1);
		}
		else
			//	if (width < height)//wtf
		{
			uint32 baseLevel	= cellUtilLog2(height)-(cellUtilLog2(height)-cellUtilLog2(width));
			for (uint32 i = 0;i < (1UL << (cellUtilLog2(height)-cellUtilLog2(width))); i++)
				convertSwizzle(dst, src, SrcPitch, depth, 0,(1U << baseLevel)*i,SciX1,SciY1,SciX2,SciY2, baseLevel + 1);
		}
}













#endif
