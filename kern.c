#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "schrift.h"

int main() {
    const char *font_path = "C:\\Windows\\Fonts\\times.ttf"; // Replace with the path to your font file

    // Load the font file
    SFT_Font *font = sft_loadfile(font_path);
    if (!font) {
        printf("Failed to load font file.\n");
        return 1;
    }
    printf("Font loaded successfully.\n");

    // Create an SFT context to hold font-related settings
    SFT sft;
    sft.font = font;
    sft.xScale = 1.0;
    sft.yScale = 1.0;
    sft.xOffset = 0.0;
    sft.yOffset = 0.0;
    sft.flags = 0;

    // Test characters to check for kerning
    const char *test_text = "AVToWA"; // Modify this string to test different pairs

    // Loop through the text and check kerning for each pair of characters
    for (size_t i = 0; i < strlen(test_text) - 1; ++i) {
        char left_char = test_text[i];
        char right_char = test_text[i + 1];

        // Lookup the glyph IDs for the two characters
        SFT_Glyph left_gid, right_gid;
        if (sft_lookup(&sft, left_char, &left_gid) < 0) {
            printf("Glyph for '%c' not found.\n", left_char);
            continue;
        }
        if (sft_lookup(&sft, right_char, &right_gid) < 0) {
            printf("Glyph for '%c' not found.\n", right_char);
            continue;
        }

        printf("Glyph for '%c' found with ID: %u\n", left_char, left_gid);
        printf("Glyph for '%c' found with ID: %u\n", right_char, right_gid);

        // Check for kerning between the two glyphs
        SFT_Kerning kern;
        int result = sft_kerning(&sft, left_gid, right_gid, &kern);
        
        if (result < 0) {
            // If no kerning data is found, print a fallback message
            printf("No kerning data found for glyph pair '%c' and '%c'.\n", left_char, right_char);
            printf("Fallback: Adding default space.\n");
            printf("Kerning between '%c' and '%c': xShift=0.00, yShift=0.00\n", left_char, right_char);
        } else {
            // Print kerning data if found
            printf("Kerning between '%c' and '%c': xShift=%.2f, yShift=%.2f\n", left_char, right_char, kern.xShift, kern.yShift);
        }
    }

    // Free the font when done
    sft_freefont(font);

    return 0;
}
