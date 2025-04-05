/* This file is part of libschrift.
 *
 * © 2019-2022 Thomas Oltmann and contributors
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#ifndef SCHRIFT_H
#define SCHRIFT_H 1

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint_fast32_t, uint_least32_t */

#ifdef __cplusplus
extern "C" {
#endif

#define SFT_DOWNWARD_Y 0x01

typedef struct SFT          SFT;
typedef struct SFT_Font     SFT_Font;
typedef uint_least32_t      SFT_UChar; /* Guaranteed to be compatible with char32_t. */
typedef uint_fast32_t       SFT_Glyph;
typedef struct SFT_LMetrics SFT_LMetrics;
typedef struct SFT_GMetrics SFT_GMetrics;
typedef struct SFT_Kerning  SFT_Kerning;
typedef struct SFT_Image    SFT_Image;

struct SFT
{
	SFT_Font *font;
	double    xScale;
	double    yScale;
	double    xOffset;
	double    yOffset;
	int       flags;
};

struct SFT_LMetrics
{
	double ascender;
	double descender;
	double lineGap;
};

struct SFT_GMetrics
{
	double advanceWidth;
	double leftSideBearing;
	int    yOffset;
	int    minWidth;
	int    minHeight;
};

struct SFT_Kerning
{
	double xShift;
	double yShift;
};

struct SFT_Image
{
	void *pixels;
	int   width;
	int   height;
};

const char *sft_version(void);

SFT_Font *sft_loadmem (const void *mem, size_t size);
SFT_Font *sft_loadfile(const char *filename);
void      sft_freefont(SFT_Font *font);

int sft_lmetrics(const SFT *sft, SFT_LMetrics *metrics);
int sft_lookup  (const SFT *sft, SFT_UChar codepoint, SFT_Glyph *glyph);
int sft_gmetrics(const SFT *sft, SFT_Glyph glyph, SFT_GMetrics *metrics);
int sft_kerning (const SFT *sft, SFT_Glyph leftGlyph, SFT_Glyph rightGlyph,
                 SFT_Kerning *kerning);
int sft_render  (const SFT *sft, SFT_Glyph glyph, SFT_Image image);

typedef struct {
    uint16_t format;
    uint16_t coverage;
} SFT_GPOS_Subtable;


/* GPOS (Glyph Positioning) Table */
typedef struct {
    uint16_t version;
    uint16_t numSubtables;
    SFT_GPOS_Subtable *subtables;
} SFT_GPOS;

typedef struct {
    uint16_t leftGlyph;
    uint16_t rightGlyph;
    int16_t xOffset;
    int16_t yOffset;
} SFT_GPOS_PairPosEntry;


typedef struct {
    uint16_t format;
    uint16_t coverage;
    uint16_t pairCount;
    SFT_GPOS_PairPosEntry *pairs;
} SFT_GPOS_PairPosSubtable;

typedef struct {
    uint16_t glyphCount;
    uint16_t *glyphs;
    int16_t *xOffsets;
    int16_t *yOffsets;
} SFT_GPOS_ContextualRule;

typedef struct {
    uint16_t format;
    uint16_t coverage;
    uint16_t ruleCount;
    SFT_GPOS_ContextualRule *rules;
} SFT_GPOS_ContextualPosSubtable;

typedef struct {
    uint16_t ligatureGlyph;
    int16_t *xOffsets;
    int16_t *yOffsets;
} SFT_GPOS_LigaturePosEntry;

typedef struct {
    uint16_t format;
    uint16_t coverage;
    uint16_t ligatureCount;
    SFT_GPOS_LigaturePosEntry *ligatures;
} SFT_GPOS_LigaturePosSubtable;


typedef struct {
    uint16_t baseGlyph;
    uint16_t markGlyph;
    int16_t xOffset;
    int16_t yOffset;
} SFT_GPOS_MarkPosEntry;

typedef struct {
    uint16_t format;
    uint16_t coverage;
    uint16_t markCount;
    SFT_GPOS_MarkPosEntry *marks;
} SFT_GPOS_MarkPosSubtable;

typedef struct {
    uint16_t componentGlyph;
    int16_t xOffset;
    int16_t yOffset;
} SFT_GPOS_ComponentPosEntry;

typedef struct {
    uint16_t format;
    uint16_t coverage;
    uint16_t componentCount;
    SFT_GPOS_ComponentPosEntry *components;
} SFT_GPOS_ComponentPosSubtable;

typedef struct {
    uint16_t classCount;
    uint16_t *classes;
    int16_t *xOffsets;
    int16_t *yOffsets;
} SFT_GPOS_ClassContextualRule;

typedef struct {
    uint16_t format;
    uint16_t coverage;
    uint16_t classCount;
    uint16_t ruleCount;
    SFT_GPOS_ClassContextualRule *rules;
} SFT_GPOS_ClassContextualPosSubtable;

/* GDEF (Glyph Definition) Table */
typedef struct {
    uint16_t glyphCount;
    uint16_t *glyphs;
    uint16_t *classes;
} SFT_GDEF_GlyphClassDef;

typedef struct {
    uint16_t version;
    uint16_t glyphClassCount;
    SFT_GDEF_GlyphClassDef *glyphClassDefs;
    uint16_t attachPointCount;
    uint16_t *attachPoints;
} SFT_GDEF;


/* GSUB (Glyph Substitution) Table */
typedef struct {
    uint16_t format;
    uint16_t coverage;
} SFT_GSUB_Subtable;

typedef struct {
    uint16_t version;
    uint16_t numSubtables;
    SFT_GSUB_Subtable *subtables;
} SFT_GSUB;

typedef struct {
    uint16_t format;
    uint16_t glyphCount;
    uint16_t *glyphs;
    uint16_t *replacements;
} SFT_GSUB_LigatureSubstitutionSubtable;

typedef struct {
    uint16_t glyphCount;
    uint16_t *glyphs;
    uint16_t *substitutes;
} SFT_GSUB_LigatureSubstitutionEntry;

typedef struct {
    uint16_t format;
    uint16_t glyphCount;
    uint16_t *glyphs;
    uint16_t *replacements;
} SFT_GSUB_AlternateSubstitutionSubtable;

typedef struct {
    uint16_t leftGlyph;
    uint16_t rightGlyph;
    int16_t xOffset;
} SFT_KERN2_KerningPair;

/* KERN2 (Kerning Table) */
typedef struct {
    uint16_t version;
    uint16_t numKerningPairs;
    SFT_KERN2_KerningPair *pairs;
} SFT_KERN2;


/* OS/2 Table */
typedef struct {
    uint16_t version;
    uint16_t xAvgCharWidth;
    int16_t usWeightClass;
    int16_t usWidthClass;
    uint16_t fsType;
    int16_t ySubscriptXSize;
    int16_t ySubscriptYSize;
    int16_t ySubscriptXOffset;
    int16_t ySubscriptYOffset;
    int16_t ySuperscriptXSize;
    int16_t ySuperscriptYSize;
    int16_t ySuperscriptXOffset;
    int16_t ySuperscriptYOffset;
    int16_t yStrikeoutSize;
    int16_t yStrikeoutPosition;
    uint16_t fsSelection;
    uint16_t usFirstCharIndex;
    uint16_t usLastCharIndex;
    int16_t sTypoAscender;
    int16_t sTypoDescender;
    int16_t sTypoLineGap;
    uint16_t usWinAscent;
    uint16_t usWinDescent;
} SFT_OS2;


/* CFF (Compact Font Format) Table */
typedef struct {
    uint16_t numCharStrings;
    uint16_t *charStrings;
} SFT_CFF_Font;

typedef struct {
    uint16_t version;
    uint16_t numFonts;
    SFT_CFF_Font *fonts;
} SFT_CFF;


/* POST (PostScript) Table */
typedef struct {
    uint16_t version;
    uint16_t italicAngle;
    uint16_t underlinePosition;
    uint16_t underlineThickness;
    uint16_t isFixedPitch;
    uint16_t minMemType42;
    uint16_t maxMemType42;
} SFT_POST;


/* name Table */
typedef struct {
    uint16_t platformID;
    uint16_t encodingID;
    uint16_t languageID;
    uint16_t nameID;
    uint16_t length;
    uint16_t offset;
} SFT_NameRecord;

typedef struct {
    uint16_t format;
    uint16_t numNameRecords;
    SFT_NameRecord *nameRecords;
} SFT_NameTable;


/* hhea Table (Horizontal Header) */
typedef struct {
    uint16_t version;
    int16_t ascent;
    int16_t descent;
    int16_t lineGap;
    uint16_t advanceWidthMax;
    uint16_t minLeftSideBearing;
    uint16_t minRightSideBearing;
    uint16_t xMaxExtent;
    int16_t caretSlopeRise;
    int16_t caretSlopeRun;
    int16_t caretOffset;
    uint16_t metricDataFormat;
    uint16_t numOfLongHorMetrics;
} SFT_hhea;


/* maxp Table (Maximum Profile) */
typedef struct {
    uint16_t version;
    uint16_t numGlyphs;
    uint16_t maxPoints;
    uint16_t maxContours;
    uint16_t maxCompositePoints;
    uint16_t maxCompositeContours;
    uint16_t maxZones;
    uint16_t maxTwilightPoints;
    uint16_t maxStorage;
    uint16_t maxFunctionDefs;
    uint16_t maxInstructionDefs;
    uint16_t maxStackElements;
    uint16_t maxSizeOfInstructions;
    uint16_t maxComponentElements;
    uint16_t maxComponentDepth;
} SFT_maxp;


/* head Table (Font Header) */
typedef struct {
    uint16_t version;
    uint16_t fontRevision;
    uint16_t checkSumAdjustment;
    uint16_t magicNumber;
    uint16_t flags;
    uint16_t unitsPerEm;
    uint16_t created[2];
    uint16_t modified[2];
    uint16_t xMin;
    uint16_t yMin;
    uint16_t xMax;
    uint16_t yMax;
    uint16_t macStyle;
    uint16_t lowestRecPPEM;
    uint16_t fontDirectionHint;
    uint16_t indexToLocFormat;
    uint16_t glyphDataFormat;
} SFT_head;


/* loca Table (Index to Location) */
typedef struct {
    uint16_t numOffsets;
    uint32_t *offsets;
} SFT_loca;


/* vhea Table (Vertical Header) */
typedef struct {
    uint16_t version;
    uint16_t vertTypoAscender;
    uint16_t vertTypoDescender;
    uint16_t vertTypoLineGap;
    uint16_t advanceHeightMax;
    uint16_t minTopSideBearing;
    uint16_t minBottomSideBearing;
    uint16_t yMaxExtent;
    uint16_t caretSlopeRise;
    uint16_t caretSlopeRun;
    uint16_t caretOffset;
    uint16_t metricDataFormat;
    uint16_t numOfLongVertMetrics;
} SFT_vhea;


/* vmtx Table (Vertical Metrics) */
typedef struct {
    uint16_t advanceHeight;
    uint16_t topSideBearing;
} SFT_vmtx;


/* COLR (Color Glyphs) Table */
typedef struct {
    uint16_t version;
    uint16_t numGlyphs;
    uint16_t *glyphs;
    uint16_t *layers;
} SFT_COLR;

typedef struct {
    uint16_t glyphID;
    uint16_t layerCount;
    uint16_t *layerGlyphIDs;
    uint16_t *layerColorIndices;
} SFT_COLR_Layer;

/* CPAL (Color Palette) Table */
typedef struct {
    uint16_t version;
    uint16_t numPalettes;
    uint16_t *paletteCount;
    uint32_t **palettes;  // A list of RGB colors for each palette
} SFT_CPAL;

typedef struct {
    uint16_t colorCount;
    uint32_t *colors;  // Array of colors in ARGB format
} SFT_CPAL_Palette;

/* SVG Table (SVG-Based Glyphs) */
typedef struct {
    uint16_t version;
    uint16_t numGlyphs;
    uint16_t *glyphIDs;
    uint16_t *lengths;
    char **svgData;  // SVG data in string form for each glyph
} SFT_SVG;

/* CBDT (Color Bitmap Data) Table */
typedef struct {
    uint16_t version;
    uint16_t numGlyphs;
    uint16_t *glyphIDs;
    uint16_t *lengths;
    uint8_t **bitmaps;  // Bitmap data for each glyph
} SFT_CBDT;

/* CBLC (Color Bitmap Location) Table */
typedef struct {
    uint16_t version;
    uint16_t numGlyphs;
    uint16_t *glyphOffsets;  // Offsets to the bitmap data in the CBDT table
} SFT_CBLC;


typedef struct {
    uint16_t version;
    uint16_t numRecords;
    uint16_t *glyphOffsets;
} SFT_Sbix;

/* curs - Contextual Substitution Table (glyphs change based on context) */
typedef struct {
    uint16_t format;
    uint16_t coverage;
    uint16_t ruleCount;
    struct {
        uint16_t leftGlyph;
        uint16_t rightGlyph;
        uint16_t *substituteGlyphs;
    } *rules;
} SFT_Curs;

typedef struct {
    uint16_t format;
    uint16_t coverage;
    uint16_t ligatureCount;
    struct {
        uint16_t glyphID;
        uint16_t ligatureGlyph;
    } *ligatures;
} SFT_Liga;

#ifdef __cplusplus
}
#endif

#endif
