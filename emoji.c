#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "schrift.h"
#include "schrift.c"
#include "util/utf8_to_utf32.h"

#define WINDOW_WIDTH  1200
#define WINDOW_HEIGHT 880
#define SCALE_FACTOR  2
#define EMOJI_SIZE    32  // Size of each emoji in pixels
#define COLS_PER_ROW  20  // Number of emojis per row

typedef struct {
    uint32_t* pixels;  // ARGB format
    int width;
    int height;
} backbuffer_t;

static backbuffer_t backbuffer = {0};

/* Structure to hold rendered emoji data */
typedef struct {
    uint8_t *pixels;    // Grayscale pixel data
    int width;          // Width of the rendered emoji
    int height;         // Height of the rendered emoji
    uint32_t *colors;   // Array of ARGB colors from CPAL
    uint16_t colorCount;// Number of colors in the palette
} EmojiRender;

// Function declarations for emoji rendering
static int sft_render_all_emojis(const SFT *sft, EmojiRender **emojis, uint16_t *emojiCount);
static void sft_free_emojis(EmojiRender *emojis, uint16_t emojiCount);
static void draw_emoji(EmojiRender *emoji, int x, int y);

// Initialize the backbuffer with the specified dimensions
static void init_backbuffer(int width, int height) {
    if (backbuffer.pixels) free(backbuffer.pixels);
    backbuffer.pixels = (uint32_t*)calloc(width * height, sizeof(uint32_t));
    backbuffer.width = width;
    backbuffer.height = height;
}

// Clear the backbuffer to a specified color
static void clear_backbuffer(uint32_t color) {
    for (int i = 0; i < backbuffer.width * backbuffer.height; i++) {
        backbuffer.pixels[i] = color;
    }
}

static int
sft_render_all_emojis(const SFT *sft, EmojiRender **emojis, uint16_t *emojiCount)
{
    uint_fast32_t colrOffset, cpalOffset;
    SFT_COLR colrTable = {0};
    SFT_CPAL cpalTable = {0};
    uint16_t i, j;

    *emojis = NULL;
    *emojiCount = 0;

    printf("Starting sft_render_all_emojis\n");

    if (gettable(sft->font, "COLR", &colrOffset) < 0) {
        printf("No COLR table found\n");
        return -1;
    }

    if (gettable(sft->font, "CPAL", &cpalOffset) < 0) {
        printf("No CPAL table found\n");
        return -1;
    }

    // Parse COLR table header
    if (!is_safe_offset(sft->font, colrOffset, 14)) { // 14 bytes for full header
        printf("Invalid COLR header offset\n");
        return -1;
    }
    colrTable.version = getu16(sft->font, colrOffset);
    colrTable.numGlyphs = getu16(sft->font, colrOffset + 2); // numBaseGlyphRecords
    uint_fast32_t baseGlyphOffset = colrOffset + getu32(sft->font, colrOffset + 4);
    uint_fast32_t layerOffset = colrOffset + getu32(sft->font, colrOffset + 8);
    uint16_t numLayerRecords = getu16(sft->font, colrOffset + 12);
    printf("COLR: version=%u, numBaseGlyphs=%u, numLayerRecords=%u\n", 
           colrTable.version, colrTable.numGlyphs, numLayerRecords);

    if (colrTable.numGlyphs == 0) {
        printf("No glyphs in COLR table\n");
        return 0;
    }

    colrTable.glyphs = malloc(colrTable.numGlyphs * sizeof(uint16_t));
    colrTable.layers = malloc(colrTable.numGlyphs * sizeof(uint16_t));
    if (!colrTable.glyphs || !colrTable.layers) {
        printf("Failed to allocate COLR glyph/layer arrays\n");
        free(colrTable.glyphs);
        free(colrTable.layers);
        return -1;
    }

    // Read BaseGlyphRecords
    uint_fast32_t offset = baseGlyphOffset;
    uint16_t *firstLayerIndices = malloc(colrTable.numGlyphs * sizeof(uint16_t));
    if (!firstLayerIndices) {
        printf("Failed to allocate firstLayerIndices\n");
        free(colrTable.glyphs);
        free(colrTable.layers);
        return -1;
    }
    for (i = 0; i < colrTable.numGlyphs; i++) {
        if (!is_safe_offset(sft->font, offset, 6)) { // 6 bytes per BaseGlyphRecord
            printf("Invalid offset for BaseGlyphRecord %u\n", i);
            free(colrTable.glyphs);
            free(colrTable.layers);
            free(firstLayerIndices);
            return -1;
        }
        colrTable.glyphs[i] = getu16(sft->font, offset);
        firstLayerIndices[i] = getu16(sft->font, offset + 2);
        colrTable.layers[i] = getu16(sft->font, offset + 4);
        printf("Glyph %u: GID=%u, firstLayer=%u, numLayers=%u\n", 
               i, colrTable.glyphs[i], firstLayerIndices[i], colrTable.layers[i]);
        offset += 6;
    }

    // Parse CPAL table
    if (!is_safe_offset(sft->font, cpalOffset, 4)) {
        printf("Invalid CPAL offset\n");
        free(colrTable.glyphs);
        free(colrTable.layers);
        free(firstLayerIndices);
        return -1;
    }
    cpalTable.version = getu16(sft->font, cpalOffset);
    cpalTable.numPalettes = getu16(sft->font, cpalOffset + 2);
    printf("CPAL: version=%u, numPalettes=%u\n", cpalTable.version, cpalTable.numPalettes);

    offset = cpalOffset + 4;
    cpalTable.paletteCount = malloc(cpalTable.numPalettes * sizeof(uint16_t));
    if (!cpalTable.paletteCount) {
        printf("Failed to allocate paletteCount\n");
        free(colrTable.glyphs);
        free(colrTable.layers);
        free(firstLayerIndices);
        return -1;
    }

    for (i = 0; i < cpalTable.numPalettes; i++) {
        if (!is_safe_offset(sft->font, offset, 2)) {
            printf("Invalid offset for palette %u\n", i);
            free(colrTable.glyphs);
            free(colrTable.layers);
            free(firstLayerIndices);
            free(cpalTable.paletteCount);
            return -1;
        }
        cpalTable.paletteCount[i] = getu16(sft->font, offset);
        offset += 2;
    }

    cpalTable.palettes = malloc(cpalTable.numPalettes * sizeof(uint32_t*));
    if (!cpalTable.palettes) {
        printf("Failed to allocate palettes array\n");
        free(colrTable.glyphs);
        free(colrTable.layers);
        free(firstLayerIndices);
        free(cpalTable.paletteCount);
        return -1;
    }

    uint16_t colorCount = cpalTable.paletteCount[0];
    cpalTable.palettes[0] = malloc(colorCount * sizeof(uint32_t));
    if (!cpalTable.palettes[0]) {
        printf("Failed to allocate palette 0\n");
        free(colrTable.glyphs);
        free(colrTable.layers);
        free(firstLayerIndices);
        free(cpalTable.paletteCount);
        free(cpalTable.palettes);
        return -1;
    }

    for (i = 0; i < colorCount; i++) {
        if (!is_safe_offset(sft->font, offset, 4)) {
            printf("Invalid offset for color %u\n", i);
            free(colrTable.glyphs);
            free(colrTable.layers);
            free(firstLayerIndices);
            free(cpalTable.paletteCount);
            free(cpalTable.palettes[0]);
            free(cpalTable.palettes);
            return -1;
        }
        cpalTable.palettes[0][i] = getu32(sft->font, offset);
        offset += 4;
    }

    *emojis = calloc(colrTable.numGlyphs, sizeof(EmojiRender));
    if (!*emojis) {
        printf("Failed to allocate emojis array\n");
        free(colrTable.glyphs);
        free(colrTable.layers);
        free(firstLayerIndices);
        free(cpalTable.paletteCount);
        free(cpalTable.palettes[0]);
        free(cpalTable.palettes);
        return -1;
    }
    *emojiCount = colrTable.numGlyphs;
    printf("Allocated %u emojis\n", *emojiCount);

    // Render each emoji
    offset = layerOffset;
    for (i = 0; i < colrTable.numGlyphs; i++) {
        SFT_Glyph glyph = colrTable.glyphs[i];
        uint16_t layerCount = colrTable.layers[i];
        uint16_t firstLayer = firstLayerIndices[i];
        SFT_GMetrics metrics;
        SFT_Image image = {0};

        if (sft_gmetrics(sft, glyph, &metrics) < 0) {
            printf("Failed to get metrics for glyph %u\n", i);
            continue;
        }

        image.width = metrics.minWidth;
        image.height = metrics.minHeight;
        image.pixels = calloc(image.width * image.height, sizeof(uint8_t));
        if (!image.pixels) {
            printf("Failed to allocate pixels for glyph %u\n", i);
            continue;
        }

        if (sft_render(sft, glyph, image) < 0) {
            printf("Failed to render glyph %u\n", i);
            free(image.pixels);
            continue;
        }

        (*emojis)[i].pixels = image.pixels;
        (*emojis)[i].width = image.width;
        (*emojis)[i].height = image.height;
        (*emojis)[i].colors = cpalTable.palettes[0];
        (*emojis)[i].colorCount = colorCount;

        SFT_COLR_Layer layer = {0};
        layer.glyphID = glyph;
        layer.layerCount = layerCount;
        layer.layerGlyphIDs = malloc(layerCount * sizeof(uint16_t));
        layer.layerColorIndices = malloc(layerCount * sizeof(uint16_t));
        if (!layer.layerGlyphIDs || !layer.layerColorIndices) {
            printf("Failed to allocate layer arrays for glyph %u\n", i);
            free(layer.layerGlyphIDs);
            free(layer.layerColorIndices);
            free(image.pixels);
            (*emojis)[i].pixels = NULL;
            continue;
        }

        uint_fast32_t layerBase = layerOffset + firstLayer * 4; // 4 bytes per LayerRecord
        for (j = 0; j < layerCount; j++) {
            if (!is_safe_offset(sft->font, layerBase + j * 4, 4)) {
                printf("Invalid layer offset for glyph %u, layer %u\n", i, j);
                free(layer.layerGlyphIDs);
                free(layer.layerColorIndices);
                free(image.pixels);
                (*emojis)[i].pixels = NULL;
                goto cleanup;
            }
            layer.layerGlyphIDs[j] = getu16(sft->font, layerBase + j * 4);
            layer.layerColorIndices[j] = getu16(sft->font, layerBase + j * 4 + 2);

            SFT_Image layerImage = {0};
            layerImage.width = image.width;
            layerImage.height = image.height;
            layerImage.pixels = calloc(layerImage.width * layerImage.height, sizeof(uint8_t));
            if (!layerImage.pixels) {
                printf("Failed to allocate layer pixels for glyph %u, layer %u\n", i, j);
                continue;
            }
            if (sft_render(sft, layer.layerGlyphIDs[j], layerImage) == 0) {
                uint8_t *imagePixels = (uint8_t *)image.pixels;
                uint8_t *layerPixels = (uint8_t *)layerImage.pixels;
                for (int k = 0; k < image.width * image.height; k++) {
                    if (layerPixels[k]) {
                        imagePixels[k] = layerPixels[k];
                    }
                }
            }
            free(layerImage.pixels);
        }

        free(layer.layerGlyphIDs);
        free(layer.layerColorIndices);
    }

    free(colrTable.glyphs);
    free(colrTable.layers);
    free(firstLayerIndices);
    free(cpalTable.paletteCount);
    for (i = 1; i < cpalTable.numPalettes; i++) {
        free(cpalTable.palettes[i]);
    }
    free(cpalTable.palettes);
    printf("Completed rendering %u emojis\n", *emojiCount);
    return 0;

cleanup:
    printf("Entering cleanup with emojiCount=%u, emojis=%p\n", *emojiCount, (void*)*emojis);
    if (*emojis) {
        for (i = 0; i < *emojiCount; i++) {
            if ((*emojis)[i].pixels) {
                free((*emojis)[i].pixels);
            }
        }
        free(*emojis);
    }
    free(colrTable.glyphs);
    free(colrTable.layers);
    free(firstLayerIndices);
    free(cpalTable.paletteCount);
    for (i = 0; i < cpalTable.numPalettes; i++) {
        free(cpalTable.palettes[i]);
    }
    free(cpalTable.palettes);
    return -1;
}

// Function to free the rendered emoji data
static void
sft_free_emojis(EmojiRender *emojis, uint16_t emojiCount)
{
    if (!emojis) return;
    for (uint16_t i = 0; i < emojiCount; i++) {
        free(emojis[i].pixels);
        // Note: colors array is shared from CPAL table, freed only once
        if (i == 0) free(emojis[i].colors);
    }
    free(emojis);
}

// Draw an emoji at the specified position with color
static void draw_emoji(EmojiRender *emoji, int x, int y) {
    for (int gy = 0; gy < emoji->height; gy++) {
        for (int gx = 0; gx < emoji->width; gx++) {
            int buf_x = x + gx;
            int buf_y = y + gy;
            if (buf_x >= 0 && buf_x < backbuffer.width && 
                buf_y >= 0 && buf_y < backbuffer.height) {
                uint8_t alpha = emoji->pixels[gy * emoji->width + gx];
                if (alpha > 0) {
                    // Use the first color from the palette for simplicity
                    uint32_t color = emoji->colors[0]; // ARGB format
                    uint32_t fg = color;
                    uint32_t bg = 0xFFFFFFFF; // White background
                    uint32_t* dest = &backbuffer.pixels[buf_y * backbuffer.width + buf_x];
                    uint32_t r = ((fg & 0xFF) * alpha + (bg & 0xFF) * (255 - alpha)) >> 8;
                    uint32_t g = (((fg >> 8) & 0xFF) * alpha + ((bg >> 8) & 0xFF) * (255 - alpha)) >> 8;
                    uint32_t b = (((fg >> 16) & 0xFF) * alpha + ((bg >> 16) & 0xFF) * (255 - alpha)) >> 8;
                    *dest = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
        }
    }
}

// Window procedure handling messages
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    static SFT sft = {
        .xScale = EMOJI_SIZE,
        .yScale = EMOJI_SIZE,
        .flags = SFT_DOWNWARD_Y
    };
    static BITMAPINFO bmi = {0};
    static EmojiRender *emojis = NULL;
    static uint16_t emojiCount = 0;

    switch (msg) {
        case WM_CREATE: {
            init_backbuffer(WINDOW_WIDTH, WINDOW_HEIGHT);
            
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = WINDOW_WIDTH;
            bmi.bmiHeader.biHeight = -WINDOW_HEIGHT;  // Top-down
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            sft.font = sft_loadfile("C:\\Windows\\Fonts\\seguiemj.ttf");
            if (!sft.font) {
                MessageBox(NULL, "TTF load failed", "Error", MB_OK | MB_ICONERROR);
                return -1;
            }

            // Render all emojis
            if (sft_render_all_emojis(&sft, &emojis, &emojiCount) < 0) {
                MessageBox(NULL, "Failed to render emojis", "Error", MB_OK | MB_ICONERROR);
                sft_freefont(sft.font);
                return -1;
            }
            printf("Rendered %d emojis\n", emojiCount);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            clear_backbuffer(0xFFFFFFFF);

            // Render emojis in a table-like grid
            int margin = 10;
            int x = margin;
            int y = margin;
            int max_height = 0;

            for (uint16_t i = 0; i < emojiCount; i++) {
                draw_emoji(&emojis[i], x, y);
                x += EMOJI_SIZE + margin;
                max_height = max(max_height, emojis[i].height);

                // Move to next row if needed
                if ((i + 1) % COLS_PER_ROW == 0) {
                    x = margin;
                    y += max_height + margin;
                    max_height = 0;
                }

                // Check if we exceed window height
                if (y + EMOJI_SIZE > WINDOW_HEIGHT) {
                    break; // Stop rendering if we run out of space
                }
            }

            StretchDIBits(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
                          0, 0, backbuffer.width, backbuffer.height,
                          backbuffer.pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            sft_free_emojis(emojis, emojiCount);
            sft_freefont(sft.font);
            if (backbuffer.pixels) free(backbuffer.pixels);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, msg, w_param, l_param);
}

// Entry point for the Windows application
int main() {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = wnd_proc;
    wc.lpszClassName = "emoji_window_class";
    wc.hInstance = GetModuleHandle(NULL);
    
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window class registration failed", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    HWND hwnd = CreateWindowEx(0, "emoji_window_class", "Emoji Table Window",
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL,
                               wc.hInstance, NULL);
    if (!hwnd) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}