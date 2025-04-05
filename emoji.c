#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "schrift.h"
#include "schrift.c"

// Configuration
#define EMOJI_VIEWER_WIDTH  3200
#define EMOJI_VIEWER_HEIGHT 1200
#define EMOJI_SIZE          48
#define EMOJIS_PER_ROW      50
#define EMOJI_MARGIN        8
#define SCROLL_STEP         20

typedef struct {
    uint32_t* pixels;        // RGBA pixel data (pre-rendered with colors)
    int width, height;       // Dimensions in pixels
    uint16_t glyphID;        // Original glyph ID
} Emoji;

typedef struct {
    uint32_t* pixels;        // ARGB backbuffer
    int width, height;       // Window dimensions
    int scrollOffset;        // Current scroll position
    int maxScroll;           // Maximum scroll position
    Emoji* emojis;          // Array of loaded emojis
    uint32_t emojiCount;     // Number of loaded emojis
    SFT_Font* font;         // Loaded font
    BITMAPINFO bmi;         // DIB info for rendering
    SFT sft;               // SFT context for rendering
} EmojiViewer;

// Function declarations
static bool emoji_viewer_init(EmojiViewer* viewer);
static void emoji_viewer_cleanup(EmojiViewer* viewer);
static void emoji_viewer_render(EmojiViewer* viewer, HDC hdc);
static void emoji_viewer_handle_scroll(EmojiViewer* viewer, int delta);
static bool load_emoji_font(EmojiViewer* viewer);
static bool render_all_emojis(EmojiViewer* viewer);
static bool render_emoji(EmojiViewer* viewer, Emoji* emoji, uint16_t glyphID, uint16_t* layerGlyphs, uint16_t* colorIndices, uint16_t layerCount, uint32_t* palette);
static void draw_emoji(const Emoji* emoji, EmojiViewer* viewer, int x, int y);

// Initialize the emoji viewer
static bool emoji_viewer_init(EmojiViewer* viewer) {
    if (!viewer) return false;

    memset(viewer, 0, sizeof(EmojiViewer));
    viewer->width = EMOJI_VIEWER_WIDTH;
    viewer->height = EMOJI_VIEWER_HEIGHT;

    // Initialize backbuffer
    viewer->pixels = calloc(viewer->width * viewer->height, sizeof(uint32_t));
    if (!viewer->pixels) return false;

    // Setup bitmap info
    viewer->bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    viewer->bmi.bmiHeader.biWidth = viewer->width;
    viewer->bmi.bmiHeader.biHeight = -viewer->height; // Top-down
    viewer->bmi.bmiHeader.biPlanes = 1;
    viewer->bmi.bmiHeader.biBitCount = 32;
    viewer->bmi.bmiHeader.biCompression = BI_RGB;

    // Setup SFT
    viewer->sft.xScale = EMOJI_SIZE;
    viewer->sft.yScale = EMOJI_SIZE;
    viewer->sft.flags = SFT_DOWNWARD_Y;

    // Load font and emojis
    if (!load_emoji_font(viewer) || !render_all_emojis(viewer)) {
        emoji_viewer_cleanup(viewer);
        return false;
    }

    return true;
}

// Clean up resources
static void emoji_viewer_cleanup(EmojiViewer* viewer) {
    if (!viewer) return;

    if (viewer->pixels) {
        free(viewer->pixels);
        viewer->pixels = NULL;
    }

    if (viewer->emojis) {
        for (uint32_t i = 0; i < viewer->emojiCount; i++) {
            free(viewer->emojis[i].pixels);
        }
        free(viewer->emojis);
        viewer->emojis = NULL;
    }

    if (viewer->font) {
        sft_freefont(viewer->font);
        viewer->font = NULL;
    }
}

// Load emoji font from common locations
static bool load_emoji_font(EmojiViewer* viewer) {
    const char* fontPaths[] = {
        "seguiemj.ttf",
        "C:\\Windows\\Fonts\\seguiemj.ttf",
        "C:\\Windows\\Fonts\\seguisym.ttf",
        NULL
    };

    for (int i = 0; fontPaths[i]; i++) {
        viewer->font = sft_loadfile(fontPaths[i]);
        if (viewer->font) {
            printf("Loaded font from: %s\n", fontPaths[i]);
            viewer->sft.font = viewer->font;
            return true;
        }
    }

    fprintf(stderr, "Failed to load emoji font\n");
    return false;
}

// Render all emojis from the font with proper colors
static bool render_all_emojis(EmojiViewer* viewer) {
    if (!viewer || !viewer->font) return false;

    uint_fast32_t colrOffset, cpalOffset;
    if (gettable(viewer->font, "COLR", &colrOffset) < 0 || 
        gettable(viewer->font, "CPAL", &cpalOffset) < 0) {
        fprintf(stderr, "Font doesn't contain color emoji tables\n");
        return false;
    }

    // Parse COLR table header
    if (!is_safe_offset(viewer->font, colrOffset, 14)) {
        fprintf(stderr, "Invalid COLR table\n");
        return false;
    }

    uint16_t numGlyphs = getu16(viewer->font, colrOffset + 2);
    uint_fast32_t baseGlyphOffset = colrOffset + getu32(viewer->font, colrOffset + 4);
    uint_fast32_t layerOffset = colrOffset + getu32(viewer->font, colrOffset + 8);

    // Read BaseGlyphRecords
    uint16_t* glyphs = malloc(numGlyphs * sizeof(uint16_t));
    uint16_t* firstLayers = malloc(numGlyphs * sizeof(uint16_t));
    uint16_t* layerCounts = malloc(numGlyphs * sizeof(uint16_t));
    
    if (!glyphs || !firstLayers || !layerCounts) {
        fprintf(stderr, "Memory allocation failed\n");
        free(glyphs);
        free(firstLayers);
        free(layerCounts);
        return false;
    }

    uint_fast32_t offset = baseGlyphOffset;
    for (uint16_t i = 0; i < numGlyphs; i++) {
        if (!is_safe_offset(viewer->font, offset, 6)) break;
        glyphs[i] = getu16(viewer->font, offset);
        firstLayers[i] = getu16(viewer->font, offset + 2);
        layerCounts[i] = getu16(viewer->font, offset + 4);
        offset += 6;
    }

    // Parse CPAL table
    if (!is_safe_offset(viewer->font, cpalOffset, 12)) {
        fprintf(stderr, "Invalid CPAL table\n");
        free(glyphs);
        free(firstLayers);
        free(layerCounts);
        return false;
    }

    uint16_t numPaletteEntries = getu16(viewer->font, cpalOffset + 4);
    uint_fast32_t colorArrayOffset = cpalOffset + getu32(viewer->font, cpalOffset + 8);

    // Read color palette (first palette only)
    uint32_t* palette = malloc(numPaletteEntries * sizeof(uint32_t));
    if (!palette) {
        fprintf(stderr, "Failed to allocate palette\n");
        free(glyphs);
        free(firstLayers);
        free(layerCounts);
        return false;
    }

    for (uint16_t i = 0; i < numPaletteEntries; i++) {
        if (!is_safe_offset(viewer->font, colorArrayOffset + i * 4, 4)) break;
        uint32_t bgra = getu32(viewer->font, colorArrayOffset + i * 4);
        // Convert BGRA to RGBA
        palette[i] = (bgra & 0xFF00FF00) | ((bgra & 0xFF) << 16) | ((bgra >> 16) & 0xFF);
    }

    // Allocate emojis array
    viewer->emojis = calloc(numGlyphs, sizeof(Emoji));
    if (!viewer->emojis) {
        fprintf(stderr, "Failed to allocate emojis array\n");
        free(glyphs);
        free(firstLayers);
        free(layerCounts);
        free(palette);
        return false;
    }
    viewer->emojiCount = numGlyphs;

    // For each emoji, read layer information and render
    for (uint32_t i = 0; i < numGlyphs; i++) {
        uint16_t layerCount = layerCounts[i];
        uint16_t* layerGlyphs = malloc(layerCount * sizeof(uint16_t));
        uint16_t* colorIndices = malloc(layerCount * sizeof(uint16_t));
        
        if (!layerGlyphs || !colorIndices) {
            free(layerGlyphs);
            free(colorIndices);
            continue;
        }

        // Read layer records
        for (uint16_t j = 0; j < layerCount; j++) {
            uint_fast32_t layerRecordOffset = layerOffset + (firstLayers[i] + j) * 4;
            if (!is_safe_offset(viewer->font, layerRecordOffset, 4)) break;
            layerGlyphs[j] = getu16(viewer->font, layerRecordOffset);
            colorIndices[j] = getu16(viewer->font, layerRecordOffset + 2);
        }

        // Render this emoji with all its layers and colors
        if (!render_emoji(viewer, &viewer->emojis[i], glyphs[i], layerGlyphs, colorIndices, layerCount, palette)) {
            free(layerGlyphs);
            free(colorIndices);
            continue;
        }

        free(layerGlyphs);
        free(colorIndices);
    }

    // Calculate max scroll position
    int rows = (viewer->emojiCount + EMOJIS_PER_ROW - 1) / EMOJIS_PER_ROW;
    int row_height = EMOJI_SIZE + EMOJI_MARGIN;
    viewer->maxScroll = max(0, rows * row_height + EMOJI_MARGIN - viewer->height);

    free(glyphs);
    free(firstLayers);
    free(layerCounts);
    free(palette);

    printf("Successfully rendered %u emojis\n", viewer->emojiCount);
    return true;
}

// Render a single emoji with all its layers and proper colors
static bool render_emoji(EmojiViewer* viewer, Emoji* emoji, uint16_t glyphID, 
                        uint16_t* layerGlyphs, uint16_t* colorIndices, 
                        uint16_t layerCount, uint32_t* palette) {
    if (!viewer || !emoji || layerCount == 0) return false;

    // Get metrics for the base glyph
    SFT_GMetrics metrics;
    if (sft_gmetrics(&viewer->sft, glyphID, &metrics) < 0) {
        return false;
    }

    emoji->width = metrics.minWidth;
    emoji->height = metrics.minHeight;
    emoji->glyphID = glyphID;
    emoji->pixels = calloc(emoji->width * emoji->height, sizeof(uint32_t));
    if (!emoji->pixels) return false;

    // Temporary buffer for layer rendering
    SFT_Image layerImage = {
        .width = emoji->width,
        .height = emoji->height,
        .pixels = calloc(emoji->width * emoji->height, sizeof(uint8_t))
    };
    if (!layerImage.pixels) {
        free(emoji->pixels);
        return false;
    }

    // Composite all layers
    for (uint16_t i = 0; i < layerCount; i++) {
        // Clear layer image
        memset(layerImage.pixels, 0, emoji->width * emoji->height * sizeof(uint8_t));

        // Render the layer
        if (sft_render(&viewer->sft, layerGlyphs[i], layerImage) == 0) {
            uint32_t color = palette[colorIndices[i] % layerCount];
            uint8_t a = (color >> 24) & 0xFF;
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;

            // Composite this layer onto the emoji
            for (int y = 0; y < emoji->height; y++) {
                for (int x = 0; x < emoji->width; x++) {
                    uint8_t alpha = ((uint8_t*)layerImage.pixels)[y * emoji->width + x];
                    if (alpha > 0) {
                        uint32_t* pixel = &emoji->pixels[y * emoji->width + x];
                        
                        // Unpack existing pixel
                        uint8_t dst_a = (*pixel >> 24) & 0xFF;
                        uint8_t dst_r = (*pixel >> 16) & 0xFF;
                        uint8_t dst_g = (*pixel >> 8) & 0xFF;
                        uint8_t dst_b = *pixel & 0xFF;

                        // Alpha blend
                        uint8_t out_a = a + dst_a * (255 - a) / 255;
                        uint8_t out_r = (r * a + dst_r * (255 - a)) / 255;
                        uint8_t out_g = (g * a + dst_g * (255 - a)) / 255;
                        uint8_t out_b = (b * a + dst_b * (255 - a)) / 255;

                        *pixel = (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
                    }
                }
            }
        }
    }

    free(layerImage.pixels);
    return true;
}

// Render the emoji viewer to the window
static void emoji_viewer_render(EmojiViewer* viewer, HDC hdc) {
    if (!viewer || !hdc) return;

    // Clear to white background
    for (int i = 0; i < viewer->width * viewer->height; i++) {
        viewer->pixels[i] = 0xFFFFFFFF;
    }

    // Draw emojis in grid with scroll offset
    int x = EMOJI_MARGIN;
    int y = EMOJI_MARGIN - viewer->scrollOffset;
    int max_height = 0;

    for (uint32_t i = 0; i < viewer->emojiCount; i++) {
        if (viewer->emojis[i].pixels) {
            draw_emoji(&viewer->emojis[i], viewer, x, y);
            x += EMOJI_SIZE + EMOJI_MARGIN;
            max_height = max(max_height, viewer->emojis[i].height);

            if ((i + 1) % EMOJIS_PER_ROW == 0) {
                x = EMOJI_MARGIN;
                y += max_height + EMOJI_MARGIN;
                max_height = 0;
            }

            // Stop if we're past the bottom of the window
            if (y > viewer->height) {
                break;
            }
        }
    }

    // Update window
    StretchDIBits(hdc, 0, 0, viewer->width, viewer->height,
                  0, 0, viewer->width, viewer->height,
                  viewer->pixels, &viewer->bmi, DIB_RGB_COLORS, SRCCOPY);
}

// Draw a single emoji (already pre-rendered with colors)
static void draw_emoji(const Emoji* emoji, EmojiViewer* viewer, int x, int y) {
    if (!emoji || !viewer || !emoji->pixels) return;

    // Only draw if at least partially visible
    if (y + emoji->height < 0 || y >= viewer->height) return;

    int start_y = max(0, -y);
    int end_y = min(emoji->height, viewer->height - y);

    for (int ey = start_y; ey < end_y; ey++) {
        int screen_y = y + ey;
        if (screen_y < 0 || screen_y >= viewer->height) continue;

        for (int ex = 0; ex < emoji->width; ex++) {
            int screen_x = x + ex;
            if (screen_x < 0 || screen_x >= viewer->width) continue;

            uint32_t pixel = emoji->pixels[ey * emoji->width + ex];
            uint8_t alpha = (pixel >> 24) & 0xFF;

            if (alpha > 0) {
                // Alpha blend with white background
                uint32_t bg = viewer->pixels[screen_y * viewer->width + screen_x];
                uint8_t bg_r = (bg >> 16) & 0xFF;
                uint8_t bg_g = (bg >> 8) & 0xFF;
                uint8_t bg_b = bg & 0xFF;

                uint8_t r = (pixel >> 16) & 0xFF;
                uint8_t g = (pixel >> 8) & 0xFF;
                uint8_t b = pixel & 0xFF;

                uint8_t out_r = (r * alpha + bg_r * (255 - alpha)) / 255;
                uint8_t out_g = (g * alpha + bg_g * (255 - alpha)) / 255;
                uint8_t out_b = (b * alpha + bg_b * (255 - alpha)) / 255;

                viewer->pixels[screen_y * viewer->width + screen_x] = 
                    0xFF000000 | (out_r << 16) | (out_g << 8) | out_b;
            }
        }
    }
}

// Handle mouse wheel scrolling
static void emoji_viewer_handle_scroll(EmojiViewer* viewer, int delta) {
    if (!viewer) return;

    viewer->scrollOffset -= delta / WHEEL_DELTA * SCROLL_STEP;
    viewer->scrollOffset = max(0, min(viewer->scrollOffset, viewer->maxScroll));
}

// Window procedure
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    static EmojiViewer viewer;

    switch (msg) {
        case WM_CREATE: {
            if (!emoji_viewer_init(&viewer)) {
                MessageBox(hwnd, "Failed to initialize emoji viewer", "Error", MB_OK | MB_ICONERROR);
                return -1;
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            emoji_viewer_render(&viewer, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            emoji_viewer_handle_scroll(&viewer, GET_WHEEL_DELTA_WPARAM(w_param));
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_SIZE: {
            // Handle window resizing (optional)
            return 0;
        }

        case WM_DESTROY: {
            emoji_viewer_cleanup(&viewer);
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, w_param, l_param);
}

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = {
        .lpfnWndProc = wnd_proc,
        .hInstance = hInstance,
        .lpszClassName = "EmojiViewerClass",
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1)
    };

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window registration failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindow(
        wc.lpszClassName, "Emoji Viewer - Scroll to navigate",
        WS_OVERLAPPEDWINDOW | WS_VSCROLL, CW_USEDEFAULT, CW_USEDEFAULT,
        EMOJI_VIEWER_WIDTH, EMOJI_VIEWER_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBox(NULL, "Window creation failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}