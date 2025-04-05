#include <windows.h>
#include <stdio.h>
#include <string.h>
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

static void init_backbuffer(int width, int height) {
    if (backbuffer.pixels) free(backbuffer.pixels);
    backbuffer.pixels = (uint32_t*)calloc(width * height, sizeof(uint32_t));
    backbuffer.width = width;
    backbuffer.height = height;
}

static void clear_backbuffer(uint32_t color) {
    for (int i = 0; i < backbuffer.width * backbuffer.height; i++) {
        backbuffer.pixels[i] = color;
    }
}

static int draw_glyph(sft_t* sft, unsigned long cp, int x, int y, int* advance) {
    sft_glyph_t gid;
    if (sft_lookup(sft, cp, &gid) < 0) return 0;

    sft_gmetrics_t mtx;
    if (sft_gmetrics(sft, gid, &mtx) < 0) return 0;

    sft_image_t img = {
        .width  = (mtx.min_width + 3) & ~3,
        .height = mtx.min_height,
    };
    uint8_t* pixels = (uint8_t*)calloc(img.width * img.height, 1);
    img.pixels = pixels;
    
    if (sft_render(sft, gid, img) < 0) {
        free(pixels);
        return 0;
    }

    // Match X11 positioning exactly
    int dest_x = x - mtx.left_side_bearing;  // Same as XGlyphInfo.x
    int dest_y = y - mtx.y_offset;          // Same as XGlyphInfo.y
    uint32_t fg = 0xFF000000;               // Black foreground
    uint32_t bg = 0xFFFFFFFF;               // White background
    
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
    *advance = mtx.advance_width;
    return 1;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    static sft_t sft = {
        .x_scale = 16 * SCALE_FACTOR,
        .y_scale = 16 * SCALE_FACTOR,
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
                sft_lmetrics_t lmtx;
                sft_lmetrics(&sft, &lmtx);
                int y = 20 + lmtx.ascender + lmtx.line_gap;  // Match X11 initial Y

                char text[256];
                while (fgets(text, sizeof(text), file)) {
                    int n = strlen(text) - 1;
                    if (n > 0) {
                        text[n] = 0;
                        unsigned codepoints[sizeof(text)];
                        n = utf8_to_utf32((unsigned char*)text, codepoints, sizeof(text));

                        int line_x = 20;
                        for (int i = 0; i < n; i++) {
                            int advance = 0;
                            draw_glyph(&sft, codepoints[i], line_x, y, &advance);
                            line_x += advance;
                        }
                        y += 2 * (lmtx.ascender + lmtx.descender + lmtx.line_gap);  // Match X11 spacing
                    }
                }
                fclose(file);
            }

            StretchDIBits(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
                         0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
                         backbuffer.pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY: {
            if (sft.font) sft_freefont(sft.font);
            if (backbuffer.pixels) free(backbuffer.pixels);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, w_param, l_param);
}

int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, LPSTR lp_cmd_line, int n_cmd_show) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = h_instance;
    wc.lpszClassName = "glyph_window_class";

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindow(
        "glyph_window_class",
        "Glyph Window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH + GetSystemMetrics(SM_CXFRAME) * 2,
        WINDOW_HEIGHT + GetSystemMetrics(SM_CYFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION),
        NULL, NULL, h_instance, NULL
    );

    if (!hwnd) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, n_cmd_show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}