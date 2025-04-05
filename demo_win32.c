#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>  // For floor and round
#include "schrift.h"
#include "util/utf8_to_utf32.h"

#define WINDOW_WIDTH  1200
#define WINDOW_HEIGHT 880
#define SCALE_FACTOR  2

typedef struct {
    uint32_t* pixels;  // ARGB format
    int width;
    int height;
} backbuffer_t;

static backbuffer_t backbuffer = {0};

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

// Draw a glyph at the specified position, returning the advance width
static int draw_glyph(SFT* sft, unsigned long cp, double x, int baseline, double* advance) {
    SFT_Glyph gid;
    if (sft_lookup(sft, cp, &gid) < 0) return 0;

    SFT_GMetrics mtx;
    if (sft_gmetrics(sft, gid, &mtx) < 0) return 0;

    SFT_Image img = {
        .width  = (mtx.minWidth + 3) & ~3,
        .height = mtx.minHeight,
    };
    uint8_t* pixels = (uint8_t*)calloc(img.width * img.height, 1);
    img.pixels = pixels;
    
    if (sft_render(sft, gid, img) < 0) {
        free(pixels);
        return 0;
    }

    // Calculate pixel position with precise x-position
    int dest_x = (int)floor(x - mtx.leftSideBearing);
    int dest_y = baseline + mtx.yOffset;  // yOffset is negative due to SFT_DOWNWARD_Y
    uint32_t fg = 0xFF000000;             // Black foreground
    uint32_t bg = 0xFFFFFFFF;             // White background
    
    // Debugging: Print rendering position
    printf("Rendering glyph U+%04lX at x=%d\n", cp, dest_x);
    
    for (int gy = 0; gy < img.height; gy++) {
        for (int gx = 0; gx < img.width; gx++) {
            int buf_x = dest_x + gx;
            int buf_y = dest_y + gy;
            if (buf_x >= 0 && buf_x < backbuffer.width && 
                buf_y >= 0 && buf_y < backbuffer.height) {
                uint8_t alpha = pixels[gy * img.width + gx];
                if (alpha > 0) {
                    uint32_t* dest = &backbuffer.pixels[buf_y * backbuffer.width + buf_x];
                    uint32_t r = ((fg & 0xFF) * alpha + (bg & 0xFF) * (255 - alpha)) >> 8;
                    uint32_t g = (((fg >> 8) & 0xFF) * alpha + ((bg >> 8) & 0xFF) * (255 - alpha)) >> 8;
                    uint32_t b = (((fg >> 16) & 0xFF) * alpha + ((bg >> 16) & 0xFF) * (255 - alpha)) >> 8;
                    *dest = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
        }
    }
    
    free(pixels);
    *advance = mtx.advanceWidth;
    return 1;
}

// Window procedure handling messages
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    static SFT sft = {
        .xScale = 16 * SCALE_FACTOR,
        .yScale = 16 * SCALE_FACTOR,
        .flags = SFT_DOWNWARD_Y
    };
    static BITMAPINFO bmi = {0};

    switch (msg) {
        case WM_CREATE: {
            init_backbuffer(WINDOW_WIDTH, WINDOW_HEIGHT);
            
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = WINDOW_WIDTH;
            bmi.bmiHeader.biHeight = -WINDOW_HEIGHT;  // Top-down
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            sft.font = sft_loadfile("resources/FiraGO-Regular_extended_with_NotoSansEgyptianHieroglyphs-Regular.ttf");
            if (!sft.font) {
                MessageBox(NULL, "TTF load failed", "Error", MB_OK | MB_ICONERROR);
                return -1;
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            clear_backbuffer(0xFFFFFFFF);

            FILE* file = fopen("resources/glass.utf8", "r");
            if (file) {
                SFT_LMetrics lmtx;
                sft_lmetrics(&sft, &lmtx);
                int baseline = 20 + (int)round(lmtx.ascender + lmtx.lineGap);

                char text[256];
                int line_num = 1;
                while (fgets(text, sizeof(text), file)) {
                    int n = strlen(text) - 1;
                    if (n > 0) {
                        text[n] = 0;
                        unsigned codepoints[sizeof(text)];
                        n = utf8_to_utf32((unsigned char*)text, codepoints, sizeof(text));

                        printf("Line %d: baseline=%d\n", line_num++, baseline);
                        double line_x = 20.0;
                        SFT_Glyph prev_gid = 0;
                        for (int i = 0; i < n; i++) {
                            SFT_Glyph gid;
                            if (sft_lookup(&sft, codepoints[i], &gid) < 0) continue;

                            // Debugging: Print glyph ID
                            printf("Glyph for codepoint U+%04lX: gid=%u\n", codepoints[i], gid);

                            // Debugging: Print position before kerning
                            printf("Before kerning: line_x=%.2f\n", line_x);

                            // Apply kerning if there’s a previous glyph
                            if (prev_gid != 0) {
                                SFT_Kerning kern;
                                if (sft_kerning(&sft, prev_gid, gid, &kern) < 0) {
                                    printf("No kerning data found for glyph pair %u and %u\n", prev_gid, gid);
                                    line_x += 5.0; // Fallback: Add a small space if no kerning
                                    printf("Fallback: Added 5.0 units for missing kerning data\n");
                                } else {
                                    printf("Kerning between glyphs %u and %u: xShift=%.2f, yShift=%.2f\n",
                                           prev_gid, gid, kern.xShift, kern.yShift);
                                    line_x += kern.xShift;  // Apply the kerning shift
                                    printf("After kerning: line_x=%.2f (added %.2f)\n", line_x, kern.xShift);
                                }
                            }

                            double advance = 0.0;
                            // Render the glyph and get the advance width
                            draw_glyph(&sft, codepoints[i], line_x, baseline, &advance);
                            printf("After advance: line_x=%.2f (added %.2f)\n", line_x + advance, advance);

                            // Update the line position with the glyph's advance width
                            line_x += advance;  // Move the x-position by the glyph's advance width
                            prev_gid = gid;     // Update the previous glyph ID for kerning
                        }
                        baseline += (int)round(2 * (lmtx.ascender + lmtx.descender + lmtx.lineGap));
                    }
                }
                fclose(file);
            }

            StretchDIBits(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
                          0, 0, backbuffer.width, backbuffer.height,
                          backbuffer.pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            sft_freefont(sft.font);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, msg, w_param, l_param);
}

// Entry point for the Windows application
int main() {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = wnd_proc;
    wc.lpszClassName = "glyph_window_class";
    wc.hInstance = GetModuleHandle(NULL);
    
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window class registration failed", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    HWND hwnd = CreateWindowEx(0, "glyph_window_class", "Glyph Rendering Window",
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
