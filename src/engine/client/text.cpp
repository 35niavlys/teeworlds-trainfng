/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <base/math.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

// ft2 texture
#include <ft2build.h>
#include FT_FREETYPE_H

// TODO: Refactor: clean this up
enum
{
	MAX_CHARACTERS = 64,
};

#include <vector>
#include <map>

static int aFontSizes[] = {8,9,10,11,12,13,14,15,16,17,18,19,20,36,50,64};
#define NUM_FONT_SIZES (sizeof(aFontSizes)/sizeof(int))

struct SFontSizeChar
{
	int m_ID;

	// these values are scaled to the pFont size
	// width * font_size == real_size
	float m_Width;
	float m_Height;
	float m_OffsetX;
	float m_OffsetY;
	float m_AdvanceX;

	float m_aUVs[4];
	int64 m_TouchTime;
};

struct STextCharQuadVertexColor
{
	unsigned char m_R, m_G, m_B, m_A;
};

struct STextCharQuadVertex
{
	STextCharQuadVertex()
	{
		m_Color.m_R = m_Color.m_G = m_Color.m_B = m_Color.m_A = 255;
	}
	float m_X, m_Y;
	// do not use normalized floats as coordinates, since the texture might grow
	float m_U, m_V;
	STextCharQuadVertexColor m_Color;
};

struct STextCharQuad
{
	STextCharQuadVertex m_Vertices[4];
};

struct STextureSkyline
{
	// the height of each column
	std::vector<int> m_CurHeightOfPixelColumn;
};

struct CFontSizeData
{
	int m_FontSize;
	FT_Face *m_pFace;
	
	std::map<int, SFontSizeChar> m_Chars;
	
};

class CFont
{
public:
	char m_aFilename[512];
	FT_Face m_FtFace;
	CFontSizeData m_aSizes[NUM_FONT_SIZES];
	
	int m_aTextures[2];
	// keep the full texture, because opengl doesn't provide texture copying
	unsigned char* m_TextureData[2];

	// width and height are the same
	int m_CurTextureDimensions[2];

	STextureSkyline m_TextureSkyline[2];
};

struct STextString
{
	int m_QuadBufferObjectIndex;
	int m_QuadBufferContainerIndex;
	size_t m_QuadNum;
	int m_SelectionQuadContainerIndex;

	std::vector<STextCharQuad> m_CharacterQuads;
};

struct STextContainer
{
	STextContainer() { Reset(); }

	CFont* m_pFont;
	int m_FontSize;
	STextString m_StringInfo;

	// keep these values to calculate offsets
	float m_X;
	float m_Y;

	int m_Flags;
	int m_LineCount;
	int m_CharCount;
	int m_MaxLines;

	float m_StartX;
	float m_StartY;
	float m_LineWidth;
	float m_UnscaledFontSize;

	void Reset()
	{
		m_StringInfo.m_QuadBufferObjectIndex = m_StringInfo.m_QuadBufferContainerIndex = m_StringInfo.m_SelectionQuadContainerIndex = -1;
		m_StringInfo.m_QuadNum = 0;
		m_StringInfo.m_CharacterQuads.clear();

		m_X = m_Y = 0.f;
		m_Flags = m_LineCount = m_CharCount = 0;
		m_MaxLines = -1;
		m_StartX = m_StartY = 0.f;
		m_LineWidth = -1.f;
		m_UnscaledFontSize = 0.f;
	}
};


class CTextRender : public IEngineTextRender
{
	IGraphics *m_pGraphics;
	IGraphics *Graphics() { return m_pGraphics; }

	unsigned int m_RenderFlags;

	std::vector<STextContainer> m_TextContainers;
	std::vector<int> m_TextContainerIndices;
	int m_FirstFreeTextContainerIndex;

	SBufferContainerInfo m_DefaultTextContainerInfo;
	
	std::vector<CFont*> m_Fonts;
	CFont *m_pCurFont;

	int GetFreeTextContainerIndex()
	{
		if(m_FirstFreeTextContainerIndex == -1)
		{
			int Index = (int)m_TextContainerIndices.size();
			m_TextContainerIndices.push_back(Index);
			return Index;
		}
		else
		{
			int Index = m_FirstFreeTextContainerIndex;
			m_FirstFreeTextContainerIndex = m_TextContainerIndices[Index];
			m_TextContainerIndices[Index] = Index;
			return Index;
		}
	}

	void FreeTextContainerIndex(int Index)
	{
		m_TextContainerIndices[Index] = m_FirstFreeTextContainerIndex;
		m_FirstFreeTextContainerIndex = Index;
	}

	STextContainer& GetTextContainer(int Index)
	{
		if(Index >= (int)m_TextContainers.size()) 
		{
			int Size = (int)m_TextContainers.size();
			for(int i = 0; i < (Index + 1) - Size; ++i)
				m_TextContainers.push_back(STextContainer());
		}

		return m_TextContainers[Index];
	}

	void FreeTextContainer(int Index)
	{
		m_TextContainers[Index].Reset();
		FreeTextContainerIndex(Index);
	}

	int WordLength(const char *pText)
	{
		int s = 1;
		while(1)
		{
			if(*pText == 0)
				return s-1;
			if(*pText == '\n' || *pText == '\t' || *pText == ' ')
				return s;
			pText++;
			s++;
		}
	}

	float m_TextR;
	float m_TextG;
	float m_TextB;
	float m_TextA;

	float m_TextOutlineR;
	float m_TextOutlineG;
	float m_TextOutlineB;
	float m_TextOutlineA;
	
	CFont *m_pDefaultFont;

	FT_Library m_FTLibrary;
	
	virtual void SetRenderFlags(unsigned int Flags)
	{
		m_RenderFlags = Flags;
	}

	int GetFontSizeIndex(int Pixelsize)
	{
		for(unsigned i = 0; i < NUM_FONT_SIZES; i++)
		{
			if(aFontSizes[i] >= Pixelsize)
				return i;
		}

		return NUM_FONT_SIZES-1;
	}

	void Grow(unsigned char *pIn, unsigned char *pOut, int w, int h)
	{
		for(int y = 0; y < h; y++)
			for(int x = 0; x < w; x++)
			{
				int c = pIn[y*w+x];

				for(int sy = -1; sy <= 1; sy++)
					for(int sx = -1; sx <= 1; sx++)
					{
						int GetX = x+sx;
						int GetY = y+sy;
						if(GetX >= 0 && GetY >= 0 && GetX < w && GetY < h)
						{
							int Index = GetY*w+GetX;
							if(pIn[Index] > c)
								c = pIn[Index];
						}
					}

				pOut[y*w+x] = c;
			}
	}

	int InitTexture(int Width, int Height, void* pUploadData = NULL)
	{	
		void *pMem = NULL;
		if(pUploadData)
			pMem = pUploadData;
		else
		{
			pMem = mem_alloc(Width*Height, 1);
			mem_zero(pMem, Width*Height);
		}

		int TextureID = Graphics()->LoadTextureRaw(Width, Height, CImageInfo::FORMAT_ALPHA, pMem, CImageInfo::FORMAT_ALPHA, IGraphics::TEXLOAD_NOMIPMAPS);

		if(!pUploadData)
			mem_free(pMem);

		return TextureID;
	}

	void UnloadTexture(int TextureIndex)
	{
		Graphics()->UnloadTexture(TextureIndex);
	}

	void IncreaseFontTexture(CFont *pFont, int TextureIndex)
	{
		int NewDimensions = pFont->m_CurTextureDimensions[TextureIndex] * 2;

		unsigned char* pTmpTexBuffer = new unsigned char[NewDimensions*NewDimensions];
		mem_zero(pTmpTexBuffer, NewDimensions * NewDimensions * sizeof(unsigned char));

		for(int y = 0; y < pFont->m_CurTextureDimensions[TextureIndex]; ++y)
		{
			for(int x = 0; x < pFont->m_CurTextureDimensions[TextureIndex]; ++x)
			{
				pTmpTexBuffer[x + y * NewDimensions] = pFont->m_TextureData[TextureIndex][x + y * pFont->m_CurTextureDimensions[TextureIndex]];
			}
		}
		UnloadTexture(pFont->m_aTextures[TextureIndex]);
		pFont->m_aTextures[TextureIndex] = InitTexture(NewDimensions, NewDimensions, pTmpTexBuffer);
		
		delete[] pFont->m_TextureData[TextureIndex];
		pFont->m_TextureData[TextureIndex] = pTmpTexBuffer;
		pFont->m_CurTextureDimensions[TextureIndex] = NewDimensions;
		pFont->m_TextureSkyline[TextureIndex].m_CurHeightOfPixelColumn.resize(NewDimensions, 0);
	}

	int AdjustOutlineThicknessToFontSize(int OutlineThickness, int FontSize)
	{
		if(FontSize > 36)
			OutlineThickness *= 4;
		else if(FontSize >= 18)
			OutlineThickness *= 2;
		return OutlineThickness;
	}

	void InitIndex(CFont *pFont, int Index)
	{
		CFontSizeData *pSizeData = &pFont->m_aSizes[Index];

		pSizeData->m_FontSize = aFontSizes[Index];
		pSizeData->m_pFace = &pFont->m_FtFace;
	}

	CFontSizeData *GetSize(CFont *pFont, int Pixelsize)
	{
		int Index = GetFontSizeIndex(Pixelsize);
		if(pFont->m_aSizes[Index].m_FontSize != aFontSizes[Index])
			InitIndex(pFont, Index);
		return &pFont->m_aSizes[Index];
	}

	void UploadGlyph(CFont *pFont, int TextureIndex, int PosX, int PosY, int Width, int Height, const unsigned char *pData)
	{
		for(int y = 0; y < Height; ++y)
		{
			for(int x = 0; x < Width; ++x)
			{
				pFont->m_TextureData[TextureIndex][x + PosX + ((y + PosY) * pFont->m_CurTextureDimensions[TextureIndex])] = pData[x + y * Width];
			}
		}
		Graphics()->LoadTextureRawSub(pFont->m_aTextures[TextureIndex], PosX, PosY, Width, Height, CImageInfo::FORMAT_ALPHA, pData);
	}

	// 32k of data used for rendering glyphs
	unsigned char ms_aGlyphData[(1024/8) * (1024/8)];
	unsigned char ms_aGlyphDataOutlined[(1024/8) * (1024/8)];


	bool GetCharacterSpace(CFont *pFont, int TextureIndex, int Width, int Height, int& PosX, int& PosY)
	{
		if(pFont->m_CurTextureDimensions[TextureIndex] < Width)
			return false;
		if(pFont->m_CurTextureDimensions[TextureIndex] < Height)
			return false;

		//skyline bottom left algorithm
		std::vector<int>& SkylineHeights = pFont->m_TextureSkyline[TextureIndex].m_CurHeightOfPixelColumn;

		//search a fitting area with less pixel loss
		int SmallestPixelLossAreaX = 0;
		int SmallestPixelLossAreaY = pFont->m_CurTextureDimensions[TextureIndex] + 1;
		int SmallestPixelLossCurPixelLoss = pFont->m_CurTextureDimensions[TextureIndex] * pFont->m_CurTextureDimensions[TextureIndex];

		bool FoundAnyArea = false;
		for(size_t i = 0; i < SkylineHeights.size(); i++)
		{
			int CurHeight = SkylineHeights[i];
			int CurPixelLoss = 0;
			// find width pixels, and we are happy
			int AreaWidth = 1;
			for(size_t n = i + 1; n < i + Width && n < SkylineHeights.size(); ++n)
			{
				if(SkylineHeights[n] <= CurHeight)
				{
					++AreaWidth;
					CurPixelLoss += CurHeight - SkylineHeights[n];
				}
				// if the height changed, we will use that new height and adjust the pixel loss
				else
				{
					CurPixelLoss = 0;
					CurHeight = SkylineHeights[n];
					for(size_t l = i; l <= n; ++l)
					{
						CurPixelLoss += CurHeight - SkylineHeights[l];
					}
				}
			}

			// if the area is too high, continue
			if(CurHeight + Height > pFont->m_CurTextureDimensions[TextureIndex])
				continue;
			// if the found area fits our needs, check if we can use it
			if(AreaWidth == Width)
			{
				if(SmallestPixelLossCurPixelLoss >= CurPixelLoss)
				{
					if(CurHeight < SmallestPixelLossAreaY)
					{
						SmallestPixelLossCurPixelLoss = CurPixelLoss;
						SmallestPixelLossAreaX = (int)i;
						SmallestPixelLossAreaY = CurHeight;
						FoundAnyArea = true;
						if(CurPixelLoss == 0)
							break;
					}
				}
			}
		}

		if(FoundAnyArea)
		{
			PosX = SmallestPixelLossAreaX;
			PosY = SmallestPixelLossAreaY;
			for(int i = PosX; i < PosX + Width; ++i)
			{
				SkylineHeights[i] = PosY + Height;
			}
			return true;
		}
		else
			return false;
	}

	void RenderGlyph(CFont *pFont, CFontSizeData *pSizeData, int Chr)
	{
		FT_Bitmap *pBitmap;

		int x = 1;
		int y = 1;
		unsigned int px, py;

		FT_Set_Pixel_Sizes(pFont->m_FtFace, 0, pSizeData->m_FontSize);

		if(FT_Load_Char(pFont->m_FtFace, Chr, FT_LOAD_RENDER|FT_LOAD_NO_BITMAP))
		{
			dbg_msg("pFont", "error loading glyph %d", Chr);
			return;
		}

		pBitmap = &pFont->m_FtFace->glyph->bitmap; // ignore_convention
		
		// adjust spacing
		int OutlineThickness = AdjustOutlineThicknessToFontSize(1, pSizeData->m_FontSize);
		x += OutlineThickness;
		y += OutlineThickness;

		unsigned int Width = pBitmap->width + x * 2;
		unsigned int Height = pBitmap->rows + y * 2;


		// prepare glyph data
		mem_zero(ms_aGlyphData, Width * Height);

		for(py = 0; py < pBitmap->rows; py++) // ignore_convention
			for(px = 0; px < pBitmap->width; px++) // ignore_convention
				ms_aGlyphData[(py+y)*Width+px+x] = pBitmap->buffer[py*pBitmap->width+px]; // ignore_convention
		

		// upload the glyph
		int X = 0;
		int Y = 0;
		while(!GetCharacterSpace(pFont, 0, (int)Width, (int)Height, X, Y))
		{
			IncreaseFontTexture(pFont, 0);
		}
		UploadGlyph(pFont, 0, X, Y, (int)Width, (int)Height, ms_aGlyphData);

		if(OutlineThickness == 1)
		{
			Grow(ms_aGlyphData, ms_aGlyphDataOutlined, Width, Height);
			int X = 0;
			int Y = 0;
			while(!GetCharacterSpace(pFont, 1, (int)Width, (int)Height, X, Y))
			{
				IncreaseFontTexture(pFont, 1);
			}
			UploadGlyph(pFont, 1, X, Y, (int)Width, (int)Height, ms_aGlyphDataOutlined);
		}
		else
		{
			for(int i = OutlineThickness; i > 0; i-=2)
			{
				Grow(ms_aGlyphData, ms_aGlyphDataOutlined, Width, Height);
				Grow(ms_aGlyphDataOutlined, ms_aGlyphData, Width, Height);
			}
			int X = 0;
			int Y = 0;
			while(!GetCharacterSpace(pFont, 1, (int)Width, (int)Height, X, Y))
			{
				IncreaseFontTexture(pFont, 1);
			}
			UploadGlyph(pFont, 1, X, Y, (int)Width, (int)Height, ms_aGlyphData);
		}

		// set char info
		{
			SFontSizeChar *pFontchr = &pSizeData->m_Chars[Chr];
			int BMPHeight = pBitmap->rows + y * 2;
			int BMPWidth = pBitmap->width + x * 2;

			pFontchr->m_ID = Chr;
			pFontchr->m_Height = Height;
			pFontchr->m_Width = Width;
			pFontchr->m_OffsetX = (pFont->m_FtFace->glyph->metrics.horiBearingX >> 6); // ignore_convention
			pFontchr->m_OffsetY = -((pFont->m_FtFace->glyph->metrics.height >> 6) - (pFont->m_FtFace->glyph->metrics.horiBearingY >> 6));// (pSizeData->m_FontSize - pFont->m_FtFace->glyph->bitmap_top);// ((m_pCurFont->m_FtFace->size->metrics.height >> 6) - pSizeData->m_FontSize + pSizeData->m_FontSize - (pFont->m_FtFace->glyph->metrics.horiBearingY >> 6)); // ignore_convention
			pFontchr->m_AdvanceX = (pFont->m_FtFace->glyph->advance.x>>6); // ignore_convention

			pFontchr->m_aUVs[0] = X;
			pFontchr->m_aUVs[1] = Y;
			pFontchr->m_aUVs[2] = pFontchr->m_aUVs[0] + BMPWidth;
			pFontchr->m_aUVs[3] = pFontchr->m_aUVs[1] + BMPHeight;
		}
	}

	SFontSizeChar *GetChar(CFont *pFont, CFontSizeData *pSizeData, int Chr)
	{
		std::map<int, SFontSizeChar>::iterator it = pSizeData->m_Chars.find(Chr);
		if(it == pSizeData->m_Chars.end())
		{
			// render and add character
			SFontSizeChar& FontSizeChr = pSizeData->m_Chars[Chr];
			
			RenderGlyph(pFont, pSizeData, Chr);
			
			return &FontSizeChr;
		}
		else
		{
			return &it->second;
		}
	}
	
	// must only be called from the rendering function as the pFont must be set to the correct size
	void RenderSetup(CFont *pFont, int size)
	{
		FT_Set_Pixel_Sizes(pFont->m_FtFace, 0, size);
	}

	float Kerning(CFont *pFont, int Left, int Right)
	{
		FT_Vector Kerning = {0,0};
		FT_Get_Kerning(pFont->m_FtFace, Left, Right, FT_KERNING_DEFAULT, &Kerning);
		return (Kerning.x>>6);
	}


public:
	CTextRender()
	{
		m_pGraphics = 0;

		m_TextR = 1.0f;
		m_TextG = 1.0f;
		m_TextB = 1.0f;
		m_TextA = 1.0f;
		m_TextOutlineR = 0.0f;
		m_TextOutlineG = 0.0f;
		m_TextOutlineB = 0.0f;
		m_TextOutlineA = 0.3f;

		m_pCurFont = 0;
		m_pDefaultFont = 0;
		m_FTLibrary = 0;

		// GL_LUMINANCE can be good for debugging
		//m_FontTextureFormat = GL_ALPHA;
	}

	virtual ~CTextRender()
	{
		for(size_t i = 0; i < m_Fonts.size(); ++i)
		{
			DestroyFont(m_Fonts[i]);
		}

		if(m_FTLibrary != 0)
			FT_Done_FreeType(m_FTLibrary);
	}

	virtual void Init()
	{
		m_pGraphics = Kernel()->RequestInterface<IGraphics>();
		FT_Init_FreeType(&m_FTLibrary);
		m_FirstFreeTextContainerIndex = -1;

		m_DefaultTextContainerInfo.m_Stride = sizeof(STextCharQuadVertex);

		m_DefaultTextContainerInfo.m_Attributes.push_back(SBufferContainerInfo::SAttribute());
		SBufferContainerInfo::SAttribute* pAttr = &m_DefaultTextContainerInfo.m_Attributes.back();
		pAttr->m_DataTypeCount = 2;
		pAttr->m_FuncType = 0;
		pAttr->m_Normalized = false;
		pAttr->m_pOffset = 0;
		pAttr->m_Type = GRAPHICS_TYPE_FLOAT;
		pAttr->m_VertBufferBindingIndex = -1;
		m_DefaultTextContainerInfo.m_Attributes.push_back(SBufferContainerInfo::SAttribute());
		pAttr = &m_DefaultTextContainerInfo.m_Attributes.back();
		pAttr->m_DataTypeCount = 2;
		pAttr->m_FuncType = 0;
		pAttr->m_Normalized = false;
		pAttr->m_pOffset = (void*)(sizeof(float) * 2);
		pAttr->m_Type = GRAPHICS_TYPE_FLOAT;
		pAttr->m_VertBufferBindingIndex = -1;
		m_DefaultTextContainerInfo.m_Attributes.push_back(SBufferContainerInfo::SAttribute());
		pAttr = &m_DefaultTextContainerInfo.m_Attributes.back();
		pAttr->m_DataTypeCount = 4;
		pAttr->m_FuncType = 0;
		pAttr->m_Normalized = true;
		pAttr->m_pOffset = (void*)(sizeof(float) * 2 + sizeof(float) * 2);
		pAttr->m_Type = GRAPHICS_TYPE_UNSIGNED_BYTE;
		pAttr->m_VertBufferBindingIndex = -1;

		IStorage *pStorage = Kernel()->RequestInterface<IStorage>();
		char aFilename[512];
		const char *pFontFile = "fonts/Icons.ttf";
		IOHANDLE File = pStorage->OpenFile(pFontFile, IOFLAG_READ, IStorage::TYPE_ALL, aFilename, sizeof(aFilename));
		if(File)
		{
			io_close(File);
			LoadFont(aFilename);
		}
	}
	
	virtual CFont *LoadFont(const char *pFilename)
	{
		CFont *pFont = new CFont();

		str_copy(pFont->m_aFilename, pFilename, sizeof(pFont->m_aFilename));

		if(FT_New_Face(m_FTLibrary, pFont->m_aFilename, 0, &pFont->m_FtFace))
		{
			delete pFont;
			return NULL;
		}

		for(unsigned i = 0; i < NUM_FONT_SIZES; i++)
			pFont->m_aSizes[i].m_FontSize = -1;

		dbg_msg("textrender", "loaded pFont from '%s'", pFilename);

		pFont->m_CurTextureDimensions[0] = 256;
		pFont->m_TextureData[0] = new unsigned char[pFont->m_CurTextureDimensions[0] * pFont->m_CurTextureDimensions[0]];
		mem_zero(pFont->m_TextureData[0], pFont->m_CurTextureDimensions[0] * pFont->m_CurTextureDimensions[0] * sizeof(unsigned char));
		pFont->m_CurTextureDimensions[1] = 256;
		pFont->m_TextureData[1] = new unsigned char[pFont->m_CurTextureDimensions[1] * pFont->m_CurTextureDimensions[1]];
		mem_zero(pFont->m_TextureData[1], pFont->m_CurTextureDimensions[1] * pFont->m_CurTextureDimensions[1] * sizeof(unsigned char));

		pFont->m_aTextures[0] = InitTexture(pFont->m_CurTextureDimensions[0], pFont->m_CurTextureDimensions[0]);
		pFont->m_aTextures[1] = InitTexture(pFont->m_CurTextureDimensions[1], pFont->m_CurTextureDimensions[1]);

		pFont->m_TextureSkyline[0].m_CurHeightOfPixelColumn.resize(pFont->m_CurTextureDimensions[0], 0);
		pFont->m_TextureSkyline[1].m_CurHeightOfPixelColumn.resize(pFont->m_CurTextureDimensions[1], 0);
		
		m_Fonts.push_back(pFont);

		return pFont;
	};
	
	virtual CFont *GetFont(int FontIndex)
	{
		if(FontIndex >= 0 && FontIndex < (int)m_Fonts.size())
			return m_Fonts[FontIndex];

		return NULL;
	}

	CFont *GetFont(const char *pFilename)
	{
		for(size_t i = 0; i < m_Fonts.size(); ++i)
		{
			if(str_comp(pFilename, m_Fonts[i]->m_aFilename) == 0)
				return m_Fonts[i];
		}

		return NULL;
	}

	virtual void DestroyFont(CFont *pFont)
	{
		for(size_t i = 0; i < m_Fonts.size(); ++i)
		{
			if(m_Fonts[i] == pFont)
			{
				m_Fonts[i] = m_Fonts[m_Fonts.size() - 1];
				m_Fonts.pop_back();

				FT_Done_Face(pFont->m_FtFace);
				delete pFont;
			}
		}
	}

	virtual void SetDefaultFont(CFont *pFont)
	{
		dbg_msg("textrender", "default pFont set %p", pFont);
		m_pDefaultFont = pFont;
		m_pCurFont = m_pDefaultFont;
	}

	virtual void SetCurFont(CFont *pFont)
	{
		if(pFont == NULL)
			m_pCurFont = m_pDefaultFont;
		else
			m_pCurFont = pFont;
	}

	virtual void SetCursor(CTextCursor *pCursor, float x, float y, float FontSize, int Flags)
	{
		mem_zero(pCursor, sizeof(*pCursor));
		pCursor->m_FontSize = FontSize;
		pCursor->m_StartX = x;
		pCursor->m_StartY = y;
		pCursor->m_X = x;
		pCursor->m_Y = y;
		pCursor->m_LineCount = 1;
		pCursor->m_LineWidth = -1;
		pCursor->m_Flags = Flags;
		pCursor->m_CharCount = 0;
	}
	
	virtual void Text(void *pFontSetV, float x, float y, float Size, const char *pText, int MaxWidth)
	{
		CTextCursor Cursor;
		SetCursor(&Cursor, x, y, Size, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = MaxWidth;
		TextEx(&Cursor, pText, -1);
	}

	virtual float TextWidth(void *pFontSetV, float Size, const char *pText, int Length)
	{
		CTextCursor Cursor;
		SetCursor(&Cursor, 0, 0, Size, 0);
		TextEx(&Cursor, pText, Length);
		return Cursor.m_X;
	}

	virtual int TextLineCount(void *pFontSetV, float Size, const char *pText, float LineWidth)
	{
		CTextCursor Cursor;
		SetCursor(&Cursor, 0, 0, Size, 0);
		Cursor.m_LineWidth = LineWidth;
		TextEx(&Cursor, pText, -1);
		return Cursor.m_LineCount;
	}

	virtual void TextColor(float r, float g, float b, float a)
	{
		m_TextR = r;
		m_TextG = g;
		m_TextB = b;
		m_TextA = a;
	}

	virtual void TextOutlineColor(float r, float g, float b, float a)
	{
		m_TextOutlineR = r;
		m_TextOutlineG = g;
		m_TextOutlineB = b;
		m_TextOutlineA = a;
	}

	virtual void TextEx(CTextCursor *pCursor, const char *pText, int Length)
	{
		CFont *pFont = pCursor->m_pFont;
		CFontSizeData *pSizeData = NULL;

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		float FakeToScreenY;

		int ActualSize;
		int GotNewLine = 0;
		float DrawX = 0.0f, DrawY = 0.0f;
		int LineCount = 0;

		float Size = pCursor->m_FontSize;

		// calculate the font size of the displayed glyphs
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

		FakeToScreenY = (Graphics()->ScreenHeight()/(ScreenY1-ScreenY0));

		// same with size
		ActualSize = (int)(Size * FakeToScreenY);
		Size = ActualSize / FakeToScreenY;

		// fetch pFont data
		if(!pFont)
			pFont = m_pCurFont;

		if(!pFont)
			return;

		pSizeData = GetSize(pFont, ActualSize);
				
		// set length
		if(Length < 0)
			Length = str_length(pText);
				
		float Scale = 1.0f / pSizeData->m_FontSize;

		//the outlined texture is always the same size as the current
		float UVScale = 1.0f / pFont->m_CurTextureDimensions[0];

		const char *pCurrent = (char *)pText;
		const char *pEnd = pCurrent+Length;

		DrawX = pCursor->m_X;
		DrawY = pCursor->m_Y;
		LineCount = pCursor->m_LineCount;

		if(pCursor->m_Flags&TEXTFLAG_RENDER)
		{
			// make sure there are no vertices
			Graphics()->FlushVertices();
			
			if(Graphics()->IsBufferingEnabled())
			{
				Graphics()->TextureSet(-1);
				Graphics()->TextQuadsBegin();
				Graphics()->SetColor(m_TextR, m_TextG, m_TextB, m_TextA);
			}
			else
			{
				Graphics()->TextureSet(pFont->m_aTextures[1]);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(m_TextOutlineR, m_TextOutlineG, m_TextOutlineB, m_TextOutlineA*m_TextA);
			}
		}

		while(pCurrent < pEnd && (pCursor->m_MaxLines < 1 || LineCount <= pCursor->m_MaxLines))
		{
			int NewLine = 0;
			const char *pBatchEnd = pEnd;
			if(pCursor->m_LineWidth > 0 && !(pCursor->m_Flags&TEXTFLAG_STOP_AT_END))
			{
				int Wlen = min(WordLength((char *)pCurrent), (int)(pEnd-pCurrent));
				CTextCursor Compare = *pCursor;
				Compare.m_X = DrawX;
				Compare.m_Y = DrawY;
				Compare.m_Flags &= ~TEXTFLAG_RENDER;
				Compare.m_LineWidth = -1;
				TextEx(&Compare, pCurrent, Wlen);

				if(Compare.m_X-DrawX > pCursor->m_LineWidth)
				{
					// word can't be fitted in one line, cut it
					CTextCursor Cutter = *pCursor;
					Cutter.m_CharCount = 0;
					Cutter.m_X = DrawX;
					Cutter.m_Y = DrawY;
					Cutter.m_Flags &= ~TEXTFLAG_RENDER;
					Cutter.m_Flags |= TEXTFLAG_STOP_AT_END;

					TextEx(&Cutter, (const char *)pCurrent, Wlen);
					Wlen = Cutter.m_CharCount;
					NewLine = 1;

					if(Wlen <= 3) // if we can't place 3 chars of the word on this line, take the next
						Wlen = 0;
				}
				else if(Compare.m_X-pCursor->m_StartX > pCursor->m_LineWidth)
				{
					NewLine = 1;
					Wlen = 0;
				}

				pBatchEnd = pCurrent + Wlen;
			}

			const char *pTmp = pCurrent;
			int NextCharacter = str_utf8_decode(&pTmp);
			while(pCurrent < pBatchEnd)
			{
				int Character = NextCharacter;
				pCurrent = pTmp;
				NextCharacter = str_utf8_decode(&pTmp);

				if(Character == '\n')
				{
					DrawX = pCursor->m_StartX;
					DrawY += Size;

					++LineCount;
					if(pCursor->m_MaxLines > 0 && LineCount > pCursor->m_MaxLines)
						break;
					continue;
				}

				SFontSizeChar *pChr = GetChar(pFont, pSizeData, Character);
				if(pChr)
				{
					float Advance = ((((m_RenderFlags&TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH) != 0) ? (pChr->m_Width) : (pChr->m_AdvanceX + (((m_RenderFlags&TEXT_RENDER_FLAG_NO_X_BEARING) != 0) ? (-pChr->m_OffsetX) : 0.f))))  * Scale + Kerning(pFont, Character, NextCharacter)*Scale;
					if(pCursor->m_Flags&TEXTFLAG_STOP_AT_END && DrawX+Advance*Size-pCursor->m_StartX > pCursor->m_LineWidth)
					{
						// we hit the end of the line, no more to render or count
						pCurrent = pEnd;
						break;
					}

					if(pCursor->m_Flags&TEXTFLAG_RENDER && m_TextA != 0.f)
					{
						if(Graphics()->IsBufferingEnabled())
							Graphics()->QuadsSetSubset(pChr->m_aUVs[0], pChr->m_aUVs[3], pChr->m_aUVs[2], pChr->m_aUVs[1]);
						else
							Graphics()->QuadsSetSubset(pChr->m_aUVs[0] * UVScale, pChr->m_aUVs[3] * UVScale, pChr->m_aUVs[2] * UVScale, pChr->m_aUVs[1] * UVScale);
						float Y = (DrawY + Size);
						IGraphics::CQuadItem QuadItem(DrawX + ((((m_RenderFlags&TEXT_RENDER_FLAG_NO_X_BEARING) != 0) ? 0.f : pChr->m_OffsetX)*Scale*Size), Y - ((((m_RenderFlags&TEXT_RENDER_FLAG_NO_Y_BEARING) != 0) ? 0.f : pChr->m_OffsetY)*Scale*Size), pChr->m_Width*Scale*Size, -pChr->m_Height*Scale*Size);
						Graphics()->QuadsDrawTL(&QuadItem, 1);
					}

					DrawX += Advance*Size;
					pCursor->m_CharCount++;
				}
			}

			if(NewLine)
			{
				DrawX = pCursor->m_StartX;
				DrawY += Size;
				GotNewLine = 1;
				++LineCount;
			}
		}

		if(pCursor->m_Flags&TEXTFLAG_RENDER)
		{
			if(Graphics()->IsBufferingEnabled())
			{
				float OutlineColor[4] = { m_TextOutlineR, m_TextOutlineG, m_TextOutlineB, m_TextOutlineA*m_TextA };
				Graphics()->TextQuadsEnd(pFont->m_CurTextureDimensions[0], pFont->m_aTextures[0], pFont->m_aTextures[1], OutlineColor);
			}
			else
			{
				Graphics()->QuadsEndKeepVertices();

				Graphics()->TextureSet(pFont->m_aTextures[0]);
				Graphics()->ChangeColorOfCurrentQuadVertices(m_TextR, m_TextG, m_TextB, m_TextA);

				// render non outlined
				Graphics()->QuadsDrawCurrentVertices(false);
			}
		}

		pCursor->m_X = DrawX;
		pCursor->m_LineCount = LineCount;

		if(GotNewLine)
			pCursor->m_Y = DrawY;
	}

	virtual int CreateTextContainer(CTextCursor *pCursor, const char *pText)
	{
		CFont *pFont = pCursor->m_pFont;

		// fetch pFont data
		if(!pFont)
			pFont = m_pCurFont;

		if(!pFont)
			return -1;

		int ContainerIndex = GetFreeTextContainerIndex();
		STextContainer& TextContainer = GetTextContainer(ContainerIndex);
		TextContainer.m_pFont = pFont;

		CFontSizeData *pSizeData = NULL;

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		float FakeToScreenY;

		int ActualSize;

		float Size = pCursor->m_FontSize;

		// calculate the font size of the displayed glyphs
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

		FakeToScreenY = (Graphics()->ScreenHeight() / (ScreenY1 - ScreenY0));

		TextContainer.m_X = pCursor->m_X;
		TextContainer.m_Y = pCursor->m_Y;

		TextContainer.m_Flags = pCursor->m_Flags;

		// same with size
		ActualSize = (int)(Size * FakeToScreenY);

		pSizeData = GetSize(pFont, ActualSize);
		
		TextContainer.m_FontSize = pSizeData->m_FontSize;
			
		AppendTextContainer(pCursor, ContainerIndex, pText);

		if(TextContainer.m_StringInfo.m_CharacterQuads.size() == 0)
		{
			FreeTextContainer(ContainerIndex);
			return -1;
		}
		else
		{
			TextContainer.m_StringInfo.m_QuadNum = TextContainer.m_StringInfo.m_CharacterQuads.size();
			if(Graphics()->IsBufferingEnabled())
			{
				size_t DataSize = TextContainer.m_StringInfo.m_CharacterQuads.size() * sizeof(STextCharQuad);
				void *pUploadData = &TextContainer.m_StringInfo.m_CharacterQuads[0];

				TextContainer.m_StringInfo.m_QuadBufferObjectIndex = Graphics()->CreateBufferObject(DataSize, pUploadData);

				for(size_t i = 0; i < m_DefaultTextContainerInfo.m_Attributes.size(); ++i)
					m_DefaultTextContainerInfo.m_Attributes[i].m_VertBufferBindingIndex = TextContainer.m_StringInfo.m_QuadBufferObjectIndex;

				TextContainer.m_StringInfo.m_QuadBufferContainerIndex = Graphics()->CreateBufferContainer(&m_DefaultTextContainerInfo);
				Graphics()->IndicesNumRequiredNotify(TextContainer.m_StringInfo.m_QuadNum * 6);
			}
			
			TextContainer.m_LineCount = pCursor->m_LineCount;
			TextContainer.m_CharCount = pCursor->m_CharCount;
			TextContainer.m_MaxLines = pCursor->m_MaxLines;
			TextContainer.m_StartX = pCursor->m_StartX;
			TextContainer.m_StartY = pCursor->m_StartY;
			TextContainer.m_LineWidth = pCursor->m_LineWidth;
			TextContainer.m_UnscaledFontSize = pCursor->m_FontSize;
		}

		return ContainerIndex;
	}

	virtual void AppendTextContainer(CTextCursor *pCursor, int TextContainerIndex, const char *pText)
	{
		STextContainer& TextContainer = GetTextContainer(TextContainerIndex);

		CFontSizeData *pSizeData = NULL;

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		float FakeToScreenY;

		int ActualSize;
		int GotNewLine = 0;
		float DrawX = 0.0f, DrawY = 0.0f;
		int LineCount = 0;

		float Size = pCursor->m_FontSize;

		// calculate the font size of the displayed glyphs
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

		FakeToScreenY = (Graphics()->ScreenHeight() / (ScreenY1 - ScreenY0));

		// same with size
		ActualSize = (int)(Size * FakeToScreenY);
		Size = ActualSize / FakeToScreenY;

		pSizeData = GetSize(TextContainer.m_pFont, TextContainer.m_FontSize);

		// string length
		int Length = str_length(pText);

		float Scale = 1.0f / pSizeData->m_FontSize;

		const char *pCurrent = (char *)pText;
		const char *pEnd = pCurrent + Length;

		DrawX = pCursor->m_X;
		DrawY = pCursor->m_Y;
		LineCount = pCursor->m_LineCount;
		
		while(pCurrent < pEnd && (pCursor->m_MaxLines < 1 || LineCount <= pCursor->m_MaxLines))
		{
			int NewLine = 0;
			const char *pBatchEnd = pEnd;
			if(pCursor->m_LineWidth > 0 && !(pCursor->m_Flags&TEXTFLAG_STOP_AT_END))
			{
				int Wlen = min(WordLength((char *)pCurrent), (int)(pEnd - pCurrent));
				CTextCursor Compare = *pCursor;
				Compare.m_X = DrawX;
				Compare.m_Y = DrawY;
				Compare.m_Flags &= ~TEXTFLAG_RENDER;
				Compare.m_LineWidth = -1;
				TextEx(&Compare, pCurrent, Wlen);

				if(Compare.m_X - DrawX > pCursor->m_LineWidth)
				{
					// word can't be fitted in one line, cut it
					CTextCursor Cutter = *pCursor;
					Cutter.m_CharCount = 0;
					Cutter.m_X = DrawX;
					Cutter.m_Y = DrawY;
					Cutter.m_Flags &= ~TEXTFLAG_RENDER;
					Cutter.m_Flags |= TEXTFLAG_STOP_AT_END;

					TextEx(&Cutter, (const char *)pCurrent, Wlen);
					Wlen = Cutter.m_CharCount;
					NewLine = 1;

					if(Wlen <= 3) // if we can't place 3 chars of the word on this line, take the next
						Wlen = 0;
				}
				else if(Compare.m_X - pCursor->m_StartX > pCursor->m_LineWidth)
				{
					NewLine = 1;
					Wlen = 0;
				}

				pBatchEnd = pCurrent + Wlen;
			}

			const char *pTmp = pCurrent;
			int NextCharacter = str_utf8_decode(&pTmp);
			while(pCurrent < pBatchEnd)
			{
				int Character = NextCharacter;
				pCurrent = pTmp;
				NextCharacter = str_utf8_decode(&pTmp);

				if(Character == '\n')
				{
					DrawX = pCursor->m_StartX;
					DrawY += Size;
					++LineCount;
					if(pCursor->m_MaxLines > 0 && LineCount > pCursor->m_MaxLines)
						break;
					continue;
				}

				SFontSizeChar *pChr = GetChar(TextContainer.m_pFont, pSizeData, Character);
				if(pChr)
				{
					float Advance = ((((m_RenderFlags&TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH) != 0) ? (pChr->m_Width) : (pChr->m_AdvanceX + (((m_RenderFlags&TEXT_RENDER_FLAG_NO_X_BEARING) != 0) ? (-pChr->m_OffsetX) : 0.f))))  * Scale + Kerning(TextContainer.m_pFont, Character, NextCharacter)*Scale;

					if(pCursor->m_Flags&TEXTFLAG_STOP_AT_END && DrawX + Advance * Size - pCursor->m_StartX > pCursor->m_LineWidth)
					{
						// we hit the end of the line, no more to render or count
						pCurrent = pEnd;
						break;
					}

					// don't add text that isnt drawn, the color overwrite is used for that
					if(m_TextA != 0.f)
					{
						TextContainer.m_StringInfo.m_CharacterQuads.push_back(STextCharQuad());
						STextCharQuad& TextCharQuad = TextContainer.m_StringInfo.m_CharacterQuads.back();

						float Y = (DrawY + Size);
						TextCharQuad.m_Vertices[0].m_X = DrawX + (((m_RenderFlags&TEXT_RENDER_FLAG_NO_X_BEARING) != 0) ? 0.f : pChr->m_OffsetX)*Scale*Size;
						TextCharQuad.m_Vertices[0].m_Y = Y - (((m_RenderFlags&TEXT_RENDER_FLAG_NO_Y_BEARING) != 0) ? 0.f : pChr->m_OffsetY)*Scale*Size;
						TextCharQuad.m_Vertices[0].m_U = pChr->m_aUVs[0];
						TextCharQuad.m_Vertices[0].m_V = pChr->m_aUVs[3];
						TextCharQuad.m_Vertices[0].m_Color.m_R = (unsigned char)(m_TextR * 255.f);
						TextCharQuad.m_Vertices[0].m_Color.m_G = (unsigned char)(m_TextG * 255.f);
						TextCharQuad.m_Vertices[0].m_Color.m_B = (unsigned char)(m_TextB * 255.f);
						TextCharQuad.m_Vertices[0].m_Color.m_A = (unsigned char)(m_TextA * 255.f);

						TextCharQuad.m_Vertices[1].m_X = DrawX + (((m_RenderFlags&TEXT_RENDER_FLAG_NO_X_BEARING) != 0) ? 0.f : pChr->m_OffsetX)*Scale*Size + pChr->m_Width*Scale*Size;
						TextCharQuad.m_Vertices[1].m_Y = Y - (((m_RenderFlags&TEXT_RENDER_FLAG_NO_Y_BEARING) != 0) ? 0.f : pChr->m_OffsetY)*Scale*Size;
						TextCharQuad.m_Vertices[1].m_U = pChr->m_aUVs[2];
						TextCharQuad.m_Vertices[1].m_V = pChr->m_aUVs[3];
						TextCharQuad.m_Vertices[1].m_Color.m_R = (unsigned char)(m_TextR * 255.f);
						TextCharQuad.m_Vertices[1].m_Color.m_G = (unsigned char)(m_TextG * 255.f);
						TextCharQuad.m_Vertices[1].m_Color.m_B = (unsigned char)(m_TextB * 255.f);
						TextCharQuad.m_Vertices[1].m_Color.m_A = (unsigned char)(m_TextA * 255.f);

						TextCharQuad.m_Vertices[2].m_X = DrawX + (((m_RenderFlags&TEXT_RENDER_FLAG_NO_X_BEARING) != 0) ? 0.f : pChr->m_OffsetX)*Scale*Size + pChr->m_Width*Scale*Size;
						TextCharQuad.m_Vertices[2].m_Y = Y - (((m_RenderFlags&TEXT_RENDER_FLAG_NO_Y_BEARING) != 0) ? 0.f : pChr->m_OffsetY)*Scale*Size - pChr->m_Height*Scale*Size;
						TextCharQuad.m_Vertices[2].m_U = pChr->m_aUVs[2];
						TextCharQuad.m_Vertices[2].m_V = pChr->m_aUVs[1];
						TextCharQuad.m_Vertices[2].m_Color.m_R = (unsigned char)(m_TextR * 255.f);
						TextCharQuad.m_Vertices[2].m_Color.m_G = (unsigned char)(m_TextG * 255.f);
						TextCharQuad.m_Vertices[2].m_Color.m_B = (unsigned char)(m_TextB * 255.f);
						TextCharQuad.m_Vertices[2].m_Color.m_A = (unsigned char)(m_TextA * 255.f);

						TextCharQuad.m_Vertices[3].m_X = DrawX + (((m_RenderFlags&TEXT_RENDER_FLAG_NO_X_BEARING) != 0) ? 0.f : pChr->m_OffsetX)*Scale*Size;
						TextCharQuad.m_Vertices[3].m_Y = Y - (((m_RenderFlags&TEXT_RENDER_FLAG_NO_Y_BEARING) != 0) ? 0.f : pChr->m_OffsetY)*Scale*Size - pChr->m_Height*Scale*Size;
						TextCharQuad.m_Vertices[3].m_U = pChr->m_aUVs[0];
						TextCharQuad.m_Vertices[3].m_V = pChr->m_aUVs[1];
						TextCharQuad.m_Vertices[3].m_Color.m_R = (unsigned char)(m_TextR * 255.f);
						TextCharQuad.m_Vertices[3].m_Color.m_G = (unsigned char)(m_TextG * 255.f);
						TextCharQuad.m_Vertices[3].m_Color.m_B = (unsigned char)(m_TextB * 255.f);
						TextCharQuad.m_Vertices[3].m_Color.m_A = (unsigned char)(m_TextA * 255.f);
					}

					DrawX += Advance * Size;
					pCursor->m_CharCount++;
				}
			}

			if(NewLine)
			{
				DrawX = pCursor->m_StartX;
				DrawY += Size;
				GotNewLine = 1;
				++LineCount;
			}
		}

		if(TextContainer.m_StringInfo.m_CharacterQuads.size() != 0)
		{
			TextContainer.m_StringInfo.m_QuadNum = TextContainer.m_StringInfo.m_CharacterQuads.size();
			// setup the buffers
			if(Graphics()->IsBufferingEnabled())
			{
				size_t DataSize = TextContainer.m_StringInfo.m_CharacterQuads.size() * sizeof(STextCharQuad);
				void *pUploadData = &TextContainer.m_StringInfo.m_CharacterQuads[0];

				if(TextContainer.m_StringInfo.m_QuadBufferObjectIndex != -1)
				{
					Graphics()->RecreateBufferObject(TextContainer.m_StringInfo.m_QuadBufferObjectIndex, DataSize, pUploadData);
					Graphics()->IndicesNumRequiredNotify(TextContainer.m_StringInfo.m_QuadNum * 6);
				}
			}
		}

		// even if no text is drawn the cursor position will be adjusted
		pCursor->m_X = DrawX;
		pCursor->m_LineCount = LineCount;

		if(GotNewLine)
			pCursor->m_Y = DrawY;
	}

	// just deletes and creates text container
	virtual void RecreateTextContainer(CTextCursor *pCursor, int TextContainerIndex, const char *pText)
	{
		DeleteTextContainer(TextContainerIndex);
		CreateTextContainer(pCursor, pText);
	}

	virtual void RecreateTextContainerSoft(CTextCursor *pCursor, int TextContainerIndex, const char *pText)
	{
		STextContainer& TextContainer = GetTextContainer(TextContainerIndex);
		TextContainer.m_StringInfo.m_CharacterQuads.clear();
		TextContainer.m_StringInfo.m_QuadNum = 0;
		// the text buffer gets then recreated by the appended quads
		AppendTextContainer(pCursor, TextContainerIndex, pText);
	}

	virtual void SetTextContainerSelection(int TextContainerIndex, const char *pText, int CursorPos, int SelectionStart, int SelectionEnd)
	{
		STextContainer& TextContainer = GetTextContainer(TextContainerIndex);

		CFontSizeData *pSizeData = NULL;

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		float FakeToScreenY;

		int ActualSize;
		float DrawX = 0.0f, DrawY = 0.0f;
		int LineCount = 0;

		float Size = TextContainer.m_UnscaledFontSize;

		// calculate the font size of the displayed glyphs
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

		FakeToScreenY = (Graphics()->ScreenHeight() / (ScreenY1 - ScreenY0));

		// same with size
		ActualSize = (int)(Size * FakeToScreenY);
		Size = ActualSize / FakeToScreenY;

		pSizeData = GetSize(TextContainer.m_pFont, TextContainer.m_FontSize);

		FT_Set_Pixel_Sizes(TextContainer.m_pFont->m_FtFace, 0, TextContainer.m_FontSize);

		// string length
		int Length = str_length(pText);

		float Scale = 1.0f / pSizeData->m_FontSize;
		float MaxRowHeight = (TextContainer.m_pFont->m_FtFace->size->metrics.height >> 6) * Scale * Size;

		const char *pCurrent = (char *)pText;
		const char *pCurrentLast = (char *)pText;
		const char *pEnd = pCurrent + Length;
		DrawX = TextContainer.m_X;
		DrawY = TextContainer.m_Y;
		LineCount = TextContainer.m_LineCount;

		if(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex == -1)
			TextContainer.m_StringInfo.m_SelectionQuadContainerIndex = Graphics()->CreateQuadContainer();

		Graphics()->QuadContainerReset(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex);

		std::vector<IGraphics::CQuadItem> SelectionQuads;
		IGraphics::CQuadItem CursorQuad;

		while(pCurrent < pEnd && (TextContainer.m_MaxLines < 1 || LineCount <= TextContainer.m_MaxLines))
		{
			int NewLine = 0;
			const char *pBatchEnd = pEnd;
			if(TextContainer.m_LineWidth > 0 && !(TextContainer.m_Flags&TEXTFLAG_STOP_AT_END))
			{
				CTextCursor FakeCursor;
				SetCursor(&FakeCursor, DrawX, DrawY, TextContainer.m_UnscaledFontSize, TextContainer.m_Flags);
				FakeCursor.m_LineCount = TextContainer.m_LineCount;
				FakeCursor.m_CharCount = TextContainer.m_CharCount;
				FakeCursor.m_MaxLines = TextContainer.m_MaxLines;
				FakeCursor.m_StartX = TextContainer.m_StartX;
				FakeCursor.m_StartY = TextContainer.m_StartY;
				FakeCursor.m_LineWidth = TextContainer.m_LineWidth;
				FakeCursor.m_pFont = TextContainer.m_pFont;

				int Wlen = min(WordLength((char *)pCurrent), (int)(pEnd - pCurrent));
				CTextCursor Compare = FakeCursor;
				Compare.m_X = DrawX;
				Compare.m_Y = DrawY;
				Compare.m_Flags &= ~TEXTFLAG_RENDER;
				Compare.m_LineWidth = -1;
				TextEx(&Compare, pCurrent, Wlen);

				if(Compare.m_X - DrawX > TextContainer.m_LineWidth)
				{
					// word can't be fitted in one line, cut it
					CTextCursor Cutter = FakeCursor;
					Cutter.m_CharCount = 0;
					Cutter.m_X = DrawX;
					Cutter.m_Y = DrawY;
					Cutter.m_Flags &= ~TEXTFLAG_RENDER;
					Cutter.m_Flags |= TEXTFLAG_STOP_AT_END;

					TextEx(&Cutter, (const char *)pCurrent, Wlen);
					Wlen = Cutter.m_CharCount;
					NewLine = 1;

					if(Wlen <= 3) // if we can't place 3 chars of the word on this line, take the next
						Wlen = 0;
				}
				else if(Compare.m_X - TextContainer.m_StartX > TextContainer.m_LineWidth)
				{
					NewLine = 1;
					Wlen = 0;
				}

				pBatchEnd = pCurrent + Wlen;
			}
			
			pCurrentLast = pCurrent;
			const char *pTmp = pCurrent;
			int NextCharacter = str_utf8_decode(&pTmp);
			while(pCurrent < pBatchEnd)
			{
				int Character = NextCharacter;
				pCurrent = pTmp;
				NextCharacter = str_utf8_decode(&pTmp);

				if(Character == '\n')
				{
					DrawX = TextContainer.m_StartX;
					DrawY += Size;
					++LineCount;
					if(TextContainer.m_MaxLines > 0 && LineCount > TextContainer.m_MaxLines)
						break;
					continue;
				}

				SFontSizeChar *pChr = GetChar(TextContainer.m_pFont, pSizeData, Character);
				if(pChr)
				{
					float Advance = ((((m_RenderFlags&TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH) != 0) ? (pChr->m_Width) : (pChr->m_AdvanceX + (((m_RenderFlags&TEXT_RENDER_FLAG_NO_X_BEARING) != 0) ? (-pChr->m_OffsetX) : 0.f))))  * Scale + Kerning(TextContainer.m_pFont, Character, NextCharacter)*Scale;

					if(TextContainer.m_Flags&TEXTFLAG_STOP_AT_END && DrawX + Advance * Size - TextContainer.m_StartX > TextContainer.m_LineWidth)
					{
						// we hit the end of the line, no more to render or count
						pCurrent = pEnd;
						break;
					}

					int CharBytePos = (int)((size_t)(pCurrentLast - pText));

					if(CharBytePos == CursorPos)
					{
						CursorQuad.Set(DrawX, DrawY, 2.f * Scale * Size, MaxRowHeight);
					}

					if(CharBytePos >= SelectionStart && CharBytePos < SelectionEnd)
					{
						SelectionQuads.push_back(IGraphics::CQuadItem(DrawX, DrawY, Advance * Size, MaxRowHeight));
					}

					DrawX += Advance * Size;
					TextContainer.m_CharCount++;
				}
				pCurrentLast = pCurrent;
			}

			if(NewLine)
			{
				DrawX = TextContainer.m_StartX;
				DrawY += Size;
				++LineCount;
			}
		}

		int CharBytePos = (int)((size_t)(pCurrentLast - pText));
		if(CharBytePos == CursorPos)
		{
			CursorQuad.Set(DrawX, DrawY, 2.f * Scale * Size, MaxRowHeight);
		}

		Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
		Graphics()->QuadContainerAddQuads(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex, &CursorQuad, 1);
		Graphics()->SetColor(0.f, 0.f, 1.f, 0.8f);
		if(SelectionQuads.size() > 0)
			Graphics()->QuadContainerAddQuads(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex, &SelectionQuads[0], (int)SelectionQuads.size());
		Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	}

	virtual void DeleteTextContainer(int TextContainerIndex)
	{
		STextContainer& TextContainer = GetTextContainer(TextContainerIndex);
		if(Graphics()->IsBufferingEnabled())
		{
			if(TextContainer.m_StringInfo.m_QuadBufferContainerIndex != -1)
				Graphics()->DeleteBufferContainer(TextContainer.m_StringInfo.m_QuadBufferContainerIndex, true);
			if(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex != -1)
				Graphics()->DeleteQuadContainer(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex);
		}
		FreeTextContainer(TextContainerIndex);
	}

	virtual void RenderTextContainer(int TextContainerIndex, STextRenderColor *pTextColor, STextRenderColor *pTextOutlineColor)
	{
		STextContainer& TextContainer = GetTextContainer(TextContainerIndex);
		CFont *pFont = TextContainer.m_pFont;

		if(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex != -1)
		{
			Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
			Graphics()->RenderQuadContainer(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex, 1, -1);

			static int64 s_CursorRenderTime = time_get_microseconds();

			if((time_get_microseconds() - s_CursorRenderTime) > 500000)
				Graphics()->RenderQuadContainer(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex, 0, 1);
			if((time_get_microseconds() - s_CursorRenderTime) > 1000000)
				s_CursorRenderTime = time_get_microseconds();
		}

		if(Graphics()->IsBufferingEnabled())
		{
			Graphics()->TextureSet(-1);
			// render buffered text
			Graphics()->RenderText(TextContainer.m_StringInfo.m_QuadBufferContainerIndex, TextContainer.m_StringInfo.m_QuadNum, pFont->m_CurTextureDimensions[0], pFont->m_aTextures[0], pFont->m_aTextures[1], (float*)pTextColor, (float*)pTextOutlineColor);
		}
		else
		{
			// render tiles
			float UVScale = 1.0f / pFont->m_CurTextureDimensions[0];

			Graphics()->FlushVertices();
			Graphics()->TextureSet(pFont->m_aTextures[1]);
			
			Graphics()->QuadsBegin();

			for(size_t i = 0; i < TextContainer.m_StringInfo.m_QuadNum; ++i)
			{
				STextCharQuad& TextCharQuad = TextContainer.m_StringInfo.m_CharacterQuads[i];

				if(pTextOutlineColor->m_A != 0)
					Graphics()->SetColor(TextCharQuad.m_Vertices[0].m_Color.m_R / 255.f * pTextOutlineColor->m_R, TextCharQuad.m_Vertices[0].m_Color.m_G / 255.f * pTextOutlineColor->m_G, TextCharQuad.m_Vertices[0].m_Color.m_B / 255.f * pTextOutlineColor->m_B, TextCharQuad.m_Vertices[0].m_Color.m_A / 255.f * pTextOutlineColor->m_A);
				else 
					Graphics()->SetColor(TextCharQuad.m_Vertices[0].m_Color.m_R / 255.f, TextCharQuad.m_Vertices[0].m_Color.m_G / 255.f, TextCharQuad.m_Vertices[0].m_Color.m_B / 255.f, TextCharQuad.m_Vertices[0].m_Color.m_A / 255.f);


				Graphics()->QuadsSetSubset(TextCharQuad.m_Vertices[0].m_U * UVScale, TextCharQuad.m_Vertices[0].m_V * UVScale, TextCharQuad.m_Vertices[2].m_U * UVScale, TextCharQuad.m_Vertices[2].m_V * UVScale);
				IGraphics::CQuadItem QuadItem(TextCharQuad.m_Vertices[0].m_X, TextCharQuad.m_Vertices[0].m_Y, TextCharQuad.m_Vertices[1].m_X - TextCharQuad.m_Vertices[0].m_X, TextCharQuad.m_Vertices[2].m_Y - TextCharQuad.m_Vertices[0].m_Y);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
			}

			Graphics()->QuadsEndKeepVertices();

			Graphics()->TextureSet(pFont->m_aTextures[0]);
			if(pTextColor->m_A != 0)
			{
				STextCharQuad& TextCharQuad = TextContainer.m_StringInfo.m_CharacterQuads[0];
				Graphics()->ChangeColorOfCurrentQuadVertices(TextCharQuad.m_Vertices[0].m_Color.m_R / 255.f * pTextColor->m_R, TextCharQuad.m_Vertices[0].m_Color.m_G / 255.f * pTextColor->m_G, TextCharQuad.m_Vertices[0].m_Color.m_B / 255.f * pTextColor->m_B, TextCharQuad.m_Vertices[0].m_Color.m_A / 255.f * pTextColor->m_A);
			}
			else
			{
				STextCharQuad& TextCharQuad = TextContainer.m_StringInfo.m_CharacterQuads[0];
				Graphics()->ChangeColorOfCurrentQuadVertices(TextCharQuad.m_Vertices[0].m_Color.m_R / 255.f, TextCharQuad.m_Vertices[0].m_Color.m_G / 255.f, TextCharQuad.m_Vertices[0].m_Color.m_B / 255.f, TextCharQuad.m_Vertices[0].m_Color.m_A / 255.f);
			}

			// render non outlined
			Graphics()->QuadsDrawCurrentVertices(false);
		}
	}

	virtual void RenderTextContainer(int TextContainerIndex, STextRenderColor *pTextColor, STextRenderColor *pTextOutlineColor, float X, float Y)
	{
		// remap the current screen, after render revert the change again
		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		
		Graphics()->MapScreen(ScreenX0 - X, ScreenY0 - Y, ScreenX1 - X, ScreenY1 - Y);
		RenderTextContainer(TextContainerIndex, pTextColor, pTextOutlineColor);
		Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
	}

	virtual void UploadEntityLayerText(int TextureID, const char *pText, int Length, float x, float y, int Size, int MaxWidth, int MaxSize = -1, int MinSize = -1)
	{
		CFont *pFont = m_pDefaultFont;
		FT_Bitmap *pBitmap;

		if(!pFont)
			return;
		
		// set length
		if(Length < 0)
			Length = str_length(pText);

		const char *pCurrent = (char *)pText;
		const char *pEnd = pCurrent+Length;
		
		int WidthLastChars = 0;
		
		int FontSize = Size;
		
		//adjust font size by the full space
		if(Size == -1)
		{
			int WidthOfText = 0;
			FT_Set_Pixel_Sizes(pFont->m_FtFace, 0, 100);
			while(pCurrent < pEnd)
			{
				const char *pTmp = pCurrent;
				int NextCharacter = str_utf8_decode(&pTmp);

				if(NextCharacter)
				{
					FT_Int32 FTFlags = 0;
#if FREETYPE_MAJOR >= 2 && FREETYPE_MINOR >= 7 && (FREETYPE_MINOR > 7 || FREETYPE_PATCH >= 1)
					FTFlags = FT_LOAD_BITMAP_METRICS_ONLY | FT_LOAD_NO_BITMAP;
#else
					FTFlags = FT_LOAD_RENDER | FT_LOAD_NO_BITMAP;
#endif
					if(FT_Load_Char(pFont->m_FtFace, NextCharacter, FTFlags))
					{
						dbg_msg("pFont", "error loading glyph %d", NextCharacter);
						pCurrent = pTmp;
						continue;
					}

					WidthOfText += (pFont->m_FtFace->glyph->metrics.width >> 6) + 1;
				}
				pCurrent = pTmp;
			}

			FontSize = 100.f / ((float)WidthOfText / (float)MaxWidth);
			if(FontSize > MaxSize)
				FontSize = MaxSize;

			pCurrent = (char *)pText;
			pEnd = pCurrent + Length;
		}
		
	
		while(pCurrent < pEnd)
		{			
			const char *pTmp = pCurrent;
			int NextCharacter = str_utf8_decode(&pTmp);
			
			if(NextCharacter)
			{
				unsigned int px, py;
				
				FT_Set_Pixel_Sizes(pFont->m_FtFace, 0, FontSize-1);

				if(FT_Load_Char(pFont->m_FtFace, NextCharacter, FT_LOAD_RENDER|FT_LOAD_NO_BITMAP))
				{
					dbg_msg("pFont", "error loading glyph %d", NextCharacter);
					pCurrent = pTmp;
					continue;
				}
				
				pBitmap = &pFont->m_FtFace->glyph->bitmap; // ignore_convention
				
				int MaxSize = (MaxWidth - WidthLastChars);
				if(MaxSize > 0)
				{
					int SlotW = ((((unsigned int)MaxSize) < (unsigned int)pBitmap->width) ? MaxSize : pBitmap->width);
					int SlotH = pBitmap->rows;
					int SlotSize = SlotW*SlotH;
					
					// prepare glyph data
					mem_zero(ms_aGlyphData, SlotSize);

					if(pBitmap->pixel_mode == FT_PIXEL_MODE_GRAY) // ignore_convention
					{
						for(py = 0; py < (unsigned)SlotH; py++) // ignore_convention
							for(px = 0; px < (unsigned)SlotW; px++)
							{
								ms_aGlyphData[(py)*SlotW + px] = pBitmap->buffer[py*pBitmap->width + px]; // ignore_convention
							}
					}
					/*else if(pBitmap->pixel_mode == FT_PIXEL_MODE_MONO) // ignore_convention
					{
						for(py = 0; py < (unsigned)pBitmap->rows; py++) // ignore_convention
							for(px = 0; px < (unsigned)pBitmap->width; px++) // ignore_convention
							{
								if(pBitmap->buffer[py*pBitmap->pitch+px/8]&(1<<(7-(px%8)))) // ignore_convention
									ms_aGlyphData[(py)*SlotW+px] = 255;
							}
					}*/
					
					Graphics()->LoadTextureRawSub(TextureID, x + WidthLastChars, y, SlotW, SlotH, CImageInfo::FORMAT_ALPHA, ms_aGlyphData);
					WidthLastChars += (SlotW + 1);
				}
			}
			pCurrent = pTmp;
		}
	}
};

IEngineTextRender *CreateEngineTextRender() { return new CTextRender; }
