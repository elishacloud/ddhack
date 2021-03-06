#include "StdAfx.h"
#include <varargs.h>
#include <hash_map>

std::hash_map<HDC, myIDDrawSurface_Generic*> open_dcs;
//std::unordered_set<myIDDrawSurface_Generic*> full_surfaces;
std::hash_map<unsigned int, unsigned char> color_map;

unsigned char color2palette(unsigned int c)
{
	if (!gPrimarySurface || !gPrimarySurface->getCurrentPalette())
	{
		return 0;
	}

	if (color_map.find(c) != color_map.end())
		return color_map[c];

	// calculate closest palette entry for the color
	
	const unsigned int *palette = (unsigned int *) gPrimarySurface->getCurrentPalette()->mPal;
	double d = 999999999999.0;
	unsigned char index = 0;

	for (unsigned int i = 0; i < 256; i++)
	{
		double r = (((c & 0x00FF0000) >> 16) - ((palette[i] & 0x00FF0000) >> 16));
		double g = (((c & 0x0000FF00) >> 8) - ((palette[i] & 0x0000FF00) >> 8));
		double b = ((c & 0x000000FF) - (palette[i] & 0x000000FF));

		double dist = r * r + g * g + b * b;

		if (dist < d)
		{
			d = dist;
			index = (unsigned char) i;
		}
		// exact match!
		if (dist == 0)
			break;
	}
	
	color_map[c] = index;
	return index;
}

myIDDrawSurface1::myIDDrawSurface1(LPDDSURFACEDESC a)
{
	bool is_main = false;
	mWidth = gScreenWidth;
	mHeight = gScreenHeight;
	mSurfaceDesc = *a;
	mCaps = a->ddsCaps;

	if (a->dwFlags & DDSD_WIDTH) mWidth = a->dwWidth;
	if (a->dwFlags & DDSD_HEIGHT) mHeight = a->dwHeight;
	// we don't need no stinking extra pitch bytes..
	
	if (a->dwFlags & DDSD_PITCH) mPitch = a->lPitch;
	if (a->dwFlags & DDSD_CAPS)
	{
		if (mCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
		{
			gPrimarySurface = this;
			init_gl();
			is_main = true;
		}
	}

	//if (mWidth == gScreenWidth && mHeight == gScreenHeight)
	//	full_surfaces.emplace(this);
	
	mPitch = mWidth * gScreenBits / 8;

	mSurfaceDesc.dwWidth = mWidth;
	mSurfaceDesc.dwHeight = mHeight;
	mSurfaceDesc.lPitch = mPitch;
	mSurfaceDesc.dwFlags |= DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH;

	// Let's pad the framebuffer by a couple of megs, in case
	// the app writes outside bounds..
	// (we have enough trouble being stable as it is)
	mRealSurfaceData = new unsigned char[mHeight * mPitch + 2 * 1024 * 1024];
	mSurfaceData = mRealSurfaceData + 1024 * 1024 * 1;
	
	mRealGdiBuffer = new unsigned int[mHeight * mWidth + 1024 * 16];
	mGdiBuffer = mRealGdiBuffer + 1024 * 8;

	memset(mSurfaceData, 0, mHeight * mPitch);
	memset(mGdiBuffer, 0, mHeight * mWidth * 4);

	mCurrentPalette = NULL;
	mIsTextBuffer = false;

	logf("myIDDrawSurface1 Constructor: %08x%s", this, is_main ? " primary" : "");
}


myIDDrawSurface1::~myIDDrawSurface1(void)
{
	logf("myIDDrawSurface1 Destructor");

	if (this == gPrimarySurface)
	{
		gPrimarySurface = NULL;
		delete gBackBuffer;
		gBackBuffer = NULL;
	}
	if (this == gBackBuffer)
	{
		gBackBuffer = NULL;
	}

	delete[] mRealSurfaceData;
	delete[] mRealGdiBuffer;

	/*if (full_surfaces.find(this) != full_surfaces.end())
	{
		full_surfaces.erase(this);
	}*/
}


HRESULT __stdcall myIDDrawSurface1::QueryInterface (REFIID, LPVOID FAR * b)
{
	logf("myIDDrawSurface1::QueryInterface");
	
	*b = NULL;

	return DDERR_UNSUPPORTED;
}


ULONG   __stdcall myIDDrawSurface1::AddRef(void)
{
	logf("myIDDrawSurface1::AddRef");
	return 1;
}


ULONG   __stdcall myIDDrawSurface1::Release(void)
{
	logf("myIDDrawSurface1::Release");
	delete this;
	
	return 0;
}



HRESULT  __stdcall myIDDrawSurface1::AddAttachedSurface(LPDIRECTDRAWSURFACE a)
{
	logf("myIDDrawSurface1::AddAttachedSurface");
	return DDERR_UNSUPPORTED;
}



HRESULT  __stdcall myIDDrawSurface1::AddOverlayDirtyRect(LPRECT a)
{
	logf("myIDDrawSurface1::AddOverlayDirtyRect");
	return DDERR_UNSUPPORTED;
}



HRESULT  __stdcall myIDDrawSurface1::Blt(LPRECT a,LPDIRECTDRAWSURFACE b, LPRECT c,DWORD d, LPDDBLTFX e)
{
	if (a && c)
		logf("myIDDrawSurface1::Blt([%d,%d,%d,%d],%08x,[%d,%d,%d,%d],%08x,%08x)",
			a->top,a->left,a->bottom,a->right,
			b,
			c->top,c->left,c->bottom,c->right,
			d,
			e ? e->dwDDFX : 0);
	else
	if (a)
		logf("myIDDrawSurface1::Blt([%d,%d,%d,%d],%08x,[null],%08x,%08x)",
			a->top,a->left,a->bottom,a->right,
			b,
			d,
			e ? e->dwDDFX : 0);
	else
	if (c)
		logf("myIDDrawSurface1::Blt([null],%08x,[%d,%d,%d,%d],%08x,%08x)",
			b,
			c->top,c->left,c->bottom,c->right,
			d,
			e ? e->dwDDFX : 0);
	else
		logf("myIDDrawSurface1::Blt([null],%08x,[null],%08x,%08x)",
			b,
			d,
			e ? e->dwDDFX : 0);
	std::hash_map<unsigned int, unsigned char> colors;
	int i, j;
	myIDDrawSurface1 *src = NULL;
	if (b) src = (myIDDrawSurface1*)b;
	if (src && src->isTextBuffer()) this->setTextBuffer();
	int usingColorKey = d & DDBLT_KEYDEST || d & DDBLT_KEYSRC || d & DDBLT_ALPHADEST;
	unsigned char colorKey = 0;
	if (usingColorKey)
		colorKey = (unsigned char) (d & DDBLT_KEYDEST ? mDestColorKey.dwColorSpaceLowValue : src->mSrcColorKey.dwColorSpaceLowValue);
	
	if (b == NULL)
	{
		if (a)
			for (i = a->bottom; i < a->top; i++)
				for (j = a->left; j < a->right; j++)
					mSurfaceData[i*mPitch+j] = (d & DDBLT_COLORFILL ? (unsigned char) e->dwFillColor : 0);
		else
			memset(mSurfaceData, (d & DDBLT_COLORFILL ? e->dwFillColor : 0), mHeight * mPitch);
	}
	else
	{
		// Othewise we're scaling a 320x240 to 640x480.. or we're scaling the
		// video on screen.

		if (a && c && gWc3SmallVid)
		{
			for (i = c->top; i < c->bottom; i++)
				for (j = c->left; j < c->right; j++)
					mSurfaceData[(i + (480 - c->bottom)/2) * mPitch + j + 160] = src->mSurfaceData[i * src->mPitch + j];		
		}
		else
		{
			if (a && c)
			{
				for (i = 0; i < a->bottom - a->top; i++)
					for (j = 0; j < a->right - a->left; j++)
						if (!usingColorKey || src->mSurfaceData[(i + c->top) * src->mPitch + j + c->left] != colorKey)
							mSurfaceData[(i + a->top) * mPitch + j + a->left] = src->mSurfaceData[(i + c->top) * src->mPitch + j + c->left];
			}
			else
			{
				for (i = 0; i < mHeight; i++)
					for (j = 0; j < mWidth; j++)
						if (!usingColorKey || src->mSurfaceData[i * src->mPitch + j] != colorKey)
							mSurfaceData[i * mPitch + j] = src->mSurfaceData[i * src->mPitch + j];
			}
		}
	}

	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::BltBatch(LPDDBLTBATCH a, DWORD b, DWORD c)
{
	logf("myIDDrawSurface1::BltBatch");
	return DDERR_UNSUPPORTED;
}



HRESULT  __stdcall myIDDrawSurface1::BltFast(DWORD a,DWORD b,LPDIRECTDRAWSURFACE c, LPRECT d,DWORD e)
{
	logf("myIDDrawSurface1::BltFast(%d,%d,%08x,[%d,%d,%d,%d],%08x)",a,b,c,d->top,d->left,d->bottom,d->right,e);
	myIDDrawSurface1 *src = (myIDDrawSurface1*)c;
	int usingColorKey = e & DDBLT_KEYDEST || e & DDBLT_KEYSRC || e & DDBLT_ALPHADEST;
	unsigned char colorKey = 0;
	if (usingColorKey)
		colorKey = (unsigned char) (e & DDBLT_KEYDEST ? mDestColorKey.dwColorSpaceLowValue : src->mSrcColorKey.dwColorSpaceLowValue);
	
	for (int i = 0; i < d->bottom - d->top; i++)
		for (int j = 0; j < d->right - d->left; j++)
			if (!usingColorKey || src->mSurfaceData[(i + d->top) * src->mPitch + j + d->left] != colorKey)
				mSurfaceData[(i + b) * mPitch + j + a] = src->mSurfaceData[(i + d->top) * src->mPitch + j + d->left];	
	
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::DeleteAttachedSurface(DWORD a,LPDIRECTDRAWSURFACE b)
{
	logf("myIDDrawSurface1::DeleteAttachedSurface");
	return DDERR_UNSUPPORTED;
}



HRESULT  __stdcall myIDDrawSurface1::EnumAttachedSurfaces(LPVOID a,LPDDENUMSURFACESCALLBACK b)
{
	logf("myIDDrawSurface1::EnumAttachedSurfaces");
	return DDERR_UNSUPPORTED;
}



HRESULT  __stdcall myIDDrawSurface1::EnumOverlayZOrders(DWORD a,LPVOID b,LPDDENUMSURFACESCALLBACK c)
{
	logf("myIDDrawSurface1::EnumOverlayZOrders");
	return DDERR_UNSUPPORTED;
}



HRESULT  __stdcall myIDDrawSurface1::Flip(LPDIRECTDRAWSURFACE a, DWORD b)
{
	logf("myIDDrawSurface1::Flip");
	updatescreen();
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::GetAttachedSurface(LPDDSCAPS a, LPDIRECTDRAWSURFACE FAR * b)
{
	logf("myIDDrawSurface1::GetAttachedSurface([%d], %08x)",
		a->dwCaps, b);

	// wc3 and wc4 call this function to access the back buffer..
	// hack: make a new surface which still uses the primary's
	// surface data.

	// Potential memory leak; should check and return gBackBuffer
	// if already exists... but the games I've checked don't call
	// this several times, so why bother.
	// And yes, I realize the checking code would take less space
	// than this comment that's complaining about it.
	// On the other hand, you wouldn't have so much fun reading 
	// this if I just deleted it and wrote the check, now would you?
	
	DDSURFACEDESC newdesc = mSurfaceDesc;
	
	newdesc.ddsCaps.dwCaps |= a->dwCaps;	
	newdesc.ddsCaps.dwCaps &= ~DDSCAPS_PRIMARYSURFACE;

	myIDDrawSurface1 * n = new myIDDrawSurface1(&newdesc);
	n->mSurfaceData = mSurfaceData;
	*b = n;
	gBackBuffer = n;

	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::GetBltStatus(DWORD a)
{
	logf("myIDDrawSurface1::GetBltStatus");
	// we're always ready for bitblts
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::GetCaps(LPDDSCAPS a)
{
	logf("myIDDrawSurface1::GetCaps");
	*a = mCaps;
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::GetClipper(LPDIRECTDRAWCLIPPER FAR* a)
{
	logf("myIDDrawSurface1::GetClipper");
	a = (LPDIRECTDRAWCLIPPER *) mClipper;
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::GetColorKey(DWORD a, LPDDCOLORKEY b)
{
	logf("myIDDrawSurface1::GetColorKey");
	if (a & DDCKEY_DESTBLT)
		b = &mDestColorKey;
	else if (a & DDCKEY_SRCBLT)
		b = &mSrcColorKey;
	else
		return DDERR_UNSUPPORTED;
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::GetDC(HDC FAR *a)
{
	logf("myIDDrawSurface1::GetDC(%08x)", this);
	*a = GetDC2(gHwnd);
	open_dcs[*a] = this;
	//if(full_surfaces.find(this) == full_surfaces.end()) clearGdi();
	logf(" hdc: %08x", *a);
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::GetFlipStatus(DWORD a)
{
	logf("myIDDrawSurface1::GetFlipStatus");
	return DDERR_UNSUPPORTED;
}



HRESULT  __stdcall myIDDrawSurface1::GetOverlayPosition(LPLONG a, LPLONG b)
{
	logf("myIDDrawSurface1::GetOverlayPosition");
	return DDERR_UNSUPPORTED;
}



HRESULT  __stdcall myIDDrawSurface1::GetPalette(LPDIRECTDRAWPALETTE FAR*a)
{
	logf("myIDDrawSurface1::GetPalette");
	*a = mCurrentPalette;
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::GetPixelFormat(LPDDPIXELFORMAT a)
{
	logf("myIDDrawSurface1::GetPixelFormat");
	// Return codes based on what ddwrapper reported..
	int bits = mPitch / mWidth;
	if (bits == 1)
	{
		a->dwSize = 0x20;
		a->dwFlags = 0x60;
		a->dwFourCC = 0;
		a->dwRGBBitCount = 0x8;
		a->dwRBitMask = 0;
		a->dwGBitMask = 0;	
		a->dwBBitMask = 0;
		a->dwRGBAlphaBitMask = 0;
	}
	else
	if (bits == 2)
	{
		a->dwSize = 0x20;
		a->dwFlags = 0x40;
		a->dwFourCC = 0;
		a->dwRGBBitCount = 0x10;
		a->dwRBitMask = 0xf800;
		a->dwGBitMask = 0x07e0;
		a->dwBBitMask = 0x001f;
		a->dwRGBAlphaBitMask = 0;
	}
	else
	if (bits == 4)
	{
		a->dwSize = 0x20;
		a->dwFlags = 0x40;
		a->dwFourCC = 0;
		a->dwRGBBitCount = 24;
		a->dwRBitMask = 0xff0000;
		a->dwGBitMask = 0x00ff00;
		a->dwBBitMask = 0x0000ff;
		a->dwRGBAlphaBitMask = 0;
	}
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::GetSurfaceDesc(LPDDSURFACEDESC a)
{
	logf("myIDDrawSurface1::GetSurfaceDesc([%d %d %d %d %d %d %d %d])",
		a->dwSize, a->dwFlags, a->dwWidth, a->dwHeight, a->lPitch, a->dwBackBufferCount,
		a->lpSurface, a->ddsCaps.dwCaps);
	*a = mSurfaceDesc;
	int bits = mPitch / mWidth;
	if (bits == 1)
	{
		a->ddpfPixelFormat.dwSize = 0x20;
		a->ddpfPixelFormat.dwFlags = 0x60;
		a->ddpfPixelFormat.dwFourCC = 0;
		a->ddpfPixelFormat.dwRGBBitCount = 0x8;
		a->ddpfPixelFormat.dwRBitMask = 0;
		a->ddpfPixelFormat.dwGBitMask = 0;	
		a->ddpfPixelFormat.dwBBitMask = 0;
		a->ddpfPixelFormat.dwRGBAlphaBitMask = 0;
	}
	else
	if (bits == 2)
	{
		a->ddpfPixelFormat.dwSize = 0x20;
		a->ddpfPixelFormat.dwFlags = 0x40;
		a->ddpfPixelFormat.dwFourCC = 0;
		a->ddpfPixelFormat.dwRGBBitCount = 0x10;
		a->ddpfPixelFormat.dwRBitMask = 0xf800;
		a->ddpfPixelFormat.dwGBitMask = 0x07e0;
		a->ddpfPixelFormat.dwBBitMask = 0x001f;
		a->ddpfPixelFormat.dwRGBAlphaBitMask = 0;
	}
	else
	if (bits == 4)
	{
		a->ddpfPixelFormat.dwSize = 0x20;
		a->ddpfPixelFormat.dwFlags = 0x40;
		a->ddpfPixelFormat.dwFourCC = 0;
		a->ddpfPixelFormat.dwRGBBitCount = 32;
		a->ddpfPixelFormat.dwRBitMask = 0xff0000;
		a->ddpfPixelFormat.dwGBitMask = 0x00ff00;
		a->ddpfPixelFormat.dwBBitMask = 0x0000ff;
		a->ddpfPixelFormat.dwRGBAlphaBitMask = 0;
	}
	logf("pixel format: %d bit, R%08x G%08x B%08x", a->ddpfPixelFormat.dwRGBBitCount, a->ddpfPixelFormat.dwRBitMask, a->ddpfPixelFormat.dwGBitMask, a->ddpfPixelFormat.dwBBitMask);
	return DD_OK;//DDERR_UNSUPPORTED;
}



HRESULT  __stdcall myIDDrawSurface1::Initialize(LPDIRECTDRAW a, LPDDSURFACEDESC b)
{
	logf("myIDDrawSurface1::Initialize");
	return DDERR_UNSUPPORTED;
}



HRESULT  __stdcall myIDDrawSurface1::IsLost()
{
	logf("myIDDrawSurface1::IsLost");
	// We're never lost..
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::Lock(LPRECT a,LPDDSURFACEDESC b,DWORD aFlags,HANDLE d)
{
	char *extra = "";
	if (this == gPrimarySurface)
		extra = " primary";
	if (this == gBackBuffer)
		extra = " backbuffer";	

	if (a)	
		logf("myIDDrawSurface1::Lock([%d,%d,%d,%d],%08x,%d,%d)%s",a->top,a->left,a->bottom,a->right,b,aFlags,d,extra);
	else
		logf("myIDDrawSurface1::Lock([null],%08x,%d,%d)%s",b,aFlags,d,extra);

	gGDI = 0;

	*b = mSurfaceDesc;

	b->dwFlags |= DDSD_LPSURFACE | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH;
	b->lpSurface = mSurfaceData;

	b->dwWidth = mWidth;
	b->dwHeight = mHeight;
	b->lPitch = mPitch;

	return NOERROR;
}



HRESULT  __stdcall myIDDrawSurface1::ReleaseDC(HDC a)
{
	logf("myIDDrawSurface1::ReleaseDC");
	DeleteDC(a);
	open_dcs.erase(a);

	const int bpp = mPitch / mWidth;
	// copy gdi drawing to actual surface data
	for (int i = 0; i < mWidth * mHeight; i++)
	{
		if (mGdiBuffer[i])
			mSurfaceData[i * bpp] = color2palette(mGdiBuffer[i]);
	}
	clearGdi();
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::Restore()
{
	logf("myIDDrawSurface1::Restore");
	// we can't lose surfaces, so..
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::SetClipper(LPDIRECTDRAWCLIPPER a)
{
	logf("myIDDrawSurface1::SetClipper");
	mClipper = (myIDDrawClipper *) a;
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::SetColorKey(DWORD a, LPDDCOLORKEY b)
{
	logf("myIDDrawSurface1::SetColorKey");
	if (a & DDCKEY_DESTBLT)
		mDestColorKey = *b;
	else if (a & DDCKEY_SRCBLT)
		mSrcColorKey = *b;
	else
		return DDERR_UNSUPPORTED;
	return DD_OK;
}



HRESULT  __stdcall myIDDrawSurface1::SetOverlayPosition(LONG a, LONG b)
{
	logf("myIDDrawSurface1::SetOverlayPosition");
	return DDERR_UNSUPPORTED;
}



HRESULT  __stdcall myIDDrawSurface1::SetPalette(LPDIRECTDRAWPALETTE a)
{
	logf("myIDDrawSurface1::SetPalette(%08x)",a);
	mCurrentPalette = (myIDDrawPalette *)a;
	return NOERROR;
}

HRESULT  __stdcall myIDDrawSurface1::Unlock(LPVOID a)
{
	logf("myIDDrawSurface1::Unlock(%08x)",a);

	// if primary has been updated, flush..
	// otherwise wc2 misses some screens
	// (no retrace, flip, or even message pump)
	if (this == gPrimarySurface)
		updatescreen();

	return NOERROR;
}



HRESULT  __stdcall myIDDrawSurface1::UpdateOverlay(LPRECT a, LPDIRECTDRAWSURFACE b,LPRECT c,DWORD d, LPDDOVERLAYFX e)
{
	logf("myIDDrawSurface1::UpdateOverlay");
	return DDERR_UNSUPPORTED;
}



HRESULT  __stdcall myIDDrawSurface1::UpdateOverlayDisplay(DWORD a)
{
	logf("myIDDrawSurface1::UpdateOverlayDisplay");
	return DDERR_UNSUPPORTED;
}



HRESULT  __stdcall myIDDrawSurface1::UpdateOverlayZOrder(DWORD a, LPDIRECTDRAWSURFACE b)
{
	logf("myIDDrawSurface1::UpdateOverlayZOrder");
	return DDERR_UNSUPPORTED;
}

