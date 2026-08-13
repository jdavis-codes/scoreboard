#include <algorithm>
#include <Arduino.h>

#ifndef SEGMENT_PIXELS
 #define SEGMENT_PIXELS 4
#endif

#ifndef SEGMENTS_PER_DIGIT
 #define SEGMENTS_PER_DIGIT 7
#endif

#if !defined(PIXELS_PER_DIGIT)
 #define PIXELS_PER_DIGIT (SEGMENTS_PER_DIGIT * SEGMENT_PIXELS)
#endif


// map of pixel order to segments (using 4 pixels per segment as an example):
/*  
         
       8  7  6  5
       -  -  -  -
    9 |           | 4
    10|           | 3
    11|           | 2
    12|13 14 15 16| 1
       -  -  -  - 
    28|           | 17
    27|           | 18
    26|           | 19
    25|           | 20
       -  -  -  -
       24 23 22 21
*/
int segment_order[7] = {
    1, // A
    0, // B
    4, // C
    5, // D
    6, // E
    2, // F
    3  // G
};

struct Segment{
    char text = ' ';
    uint8_t chr = 0;
    bool segments[7] = {0}; // A, B, C, D, E, F, G
    bool pixels[SEGMENT_PIXELS * 7] = {0};
};

bool segmentPixelAt(const Segment& seg, int segmentIndex, int pixelIndex) {
    if (segmentIndex < 0 || segmentIndex >= 7 || pixelIndex < 0 || pixelIndex >= SEGMENT_PIXELS) {
        return false;
    }

    int mappedSegment = segment_order[segmentIndex];
    if (mappedSegment < 0 || mappedSegment >= 7) {
        return false;
    }

    return seg.pixels[mappedSegment * SEGMENT_PIXELS + pixelIndex];
}

const char* onPixel = "█";
const char* offPixel = "░";

void printRasterRowForSegment(const Segment& seg, int row) {
    const int middleRow = SEGMENT_PIXELS + 1;
    const int lastRow = (SEGMENT_PIXELS * 2) + 2;

    if (row == 0 || row == middleRow || row == lastRow) {
        int segmentIndex = 0; // A
        if (row == middleRow) segmentIndex = 6; // G
        if (row == lastRow) segmentIndex = 3;   // D

        Serial.print(" ");
        for (int i = 0; i < SEGMENT_PIXELS; ++i) {
            Serial.print(segmentPixelAt(seg, segmentIndex, i) ? onPixel : offPixel);
        }
        Serial.print(" ");
        return;
    }

    if (row < middleRow) {
        int pixelIndex = row - 1;
        Serial.print(segmentPixelAt(seg, 5, pixelIndex) ? onPixel : offPixel); // F
        for (int i = 0; i < SEGMENT_PIXELS; ++i) Serial.print(' ');
        Serial.print(segmentPixelAt(seg, 1, pixelIndex) ? onPixel : offPixel); // B
        return;
    }

    int pixelIndex = row - (SEGMENT_PIXELS + 2);
    Serial.print(segmentPixelAt(seg, 4, pixelIndex) ? onPixel : offPixel); // E
    for (int i = 0; i < SEGMENT_PIXELS; ++i) Serial.print(' ');
    Serial.print(segmentPixelAt(seg, 2, pixelIndex) ? onPixel : offPixel); // C
}

void printMinusRow(int row, bool showMinus) {
    if (!showMinus || row != (SEGMENT_PIXELS + 1)) {
        for (int i = 0; i < SEGMENT_PIXELS + 2; ++i) Serial.print(' ');
        return;
    }

    Serial.print(" ");
    for (int i = 0; i < SEGMENT_PIXELS; ++i) Serial.print(onPixel);
    Serial.print(" ");
}

void printSegment(const Segment& seg, bool segments = true, bool pixels = false, bool rasterize = false ) {
    Serial.print("Character: ");
    Serial.println(seg.text);

    if (segments) {
        Serial.print("Segments: ");
        for (int i = 0; i < 7; ++i) {
            Serial.print(seg.segments[i]);
            if (i < 6) Serial.print(", ");
        }
    }

    if (pixels) {
        Serial.println();
        Serial.print("Pixels: ");
        for (int i = 0; i < 7; ++i) {
            Serial.printf("segment_order[%d]: %d, pixel: %d, values: ", i, segment_order[i], seg.pixels[segment_order[i] * SEGMENT_PIXELS]);
            Serial.println("");
            for (int j = 0; j < SEGMENT_PIXELS; ++j) {
                Serial.print(seg.pixels[segment_order[i] * SEGMENT_PIXELS + j]);
                if (j < SEGMENT_PIXELS - 1) Serial.print(", ");
            }
            if (i < 6) Serial.print(" | ");
        }
        Serial.println("");
    }
    if (rasterize){
        const int rasterRows = (SEGMENT_PIXELS * 2) + 3;
        Serial.println("Rasterized:");
        for (int row = 0; row < rasterRows; ++row) {
            printRasterRowForSegment(seg, row);
            Serial.println();
        }
    }

    Serial.println();

}

//This is the combined array that contains all the segment configurations for many different characters and symbols
const uint8_t characterArray[] {
//  ABCDEFG  Segments      7-segment map:
  0b1111110, // 0   "0"          AAA
  0b0110000, // 1   "1"         F   B
  0b1101101, // 2   "2"         F   B
  0b1111001, // 3   "3"          GGG
  0b0110011, // 4   "4"         E   C
  0b1011011, // 5   "5"         E   C
  0b1011111, // 6   "6"          DDD
  0b1110000, // 7   "7"
  0b1111111, // 8   "8"
  0b1111011, // 9   "9"
  0b1110111, // 10  "A"
  0b0011111, // 11  "b"
  0b1001110, // 12  "C"
  0b0111101, // 13  "d"
  0b1001111, // 14  "E"
  0b1000111, // 15  "F"
  0b0000000, // 16  NO DISPLAY
  0b0000000, // 17  NO DISPLAY
  0b0000000, // 18  NO DISPLAY
  0b0000000, // 19  NO DISPLAY
  0b0000000, // 20  NO DISPLAY
  0b0000000, // 21  NO DISPLAY
  0b0000000, // 22  NO DISPLAY
  0b0000000, // 23  NO DISPLAY
  0b0000000, // 24  NO DISPLAY
  0b0000000, // 25  NO DISPLAY
  0b0000000, // 26  NO DISPLAY
  0b0000000, // 27  NO DISPLAY
  0b0000000, // 28  NO DISPLAY
  0b0000000, // 29  NO DISPLAY
  0b0000000, // 30  NO DISPLAY
  0b0000000, // 31  NO DISPLAY
  0b0000000, // 32  ' '
  0b0000000, // 33  '!'  NO DISPLAY
  0b0100010, // 34  '"'
  0b0000000, // 35  '#'  NO DISPLAY
  0b0000000, // 36  '$'  NO DISPLAY
  0b0000000, // 37  '%'  NO DISPLAY
  0b0000000, // 38  '&'  NO DISPLAY
  0b0100000, // 39  '''
  0b1001110, // 40  '('
  0b1111000, // 41  ')'
  0b0000000, // 42  '*'  NO DISPLAY
  0b0000000, // 43  '+'  NO DISPLAY
  0b0000100, // 44  ','
  0b0000001, // 45  '-'
  0b0000000, // 46  '.'  NO DISPLAY
  0b0000000, // 47  '/'  NO DISPLAY
  0b1111110, // 48  '0'
  0b0110000, // 49  '1'
  0b1101101, // 50  '2'
  0b1111001, // 51  '3'
  0b0110011, // 52  '4'
  0b1011011, // 53  '5'
  0b1011111, // 54  '6'
  0b1110000, // 55  '7'
  0b1111111, // 56  '8'
  0b1111011, // 57  '9'
  0b0000000, // 58  ':'  NO DISPLAY
  0b0000000, // 59  ';'  NO DISPLAY
  0b0000000, // 60  '<'  NO DISPLAY
  0b0000000, // 61  '='  NO DISPLAY
  0b0000000, // 62  '>'  NO DISPLAY
  0b0000000, // 63  '?'  NO DISPLAY
  0b0000000, // 64  '@'  NO DISPLAY
  0b1110111, // 65  'A'
  0b0011111, // 66  'b'
  0b1001110, // 67  'C'
  0b0111101, // 68  'd'
  0b1001111, // 69  'E'
  0b1000111, // 70  'F'
  0b1011110, // 71  'G'
  0b0110111, // 72  'H'
  0b0110000, // 73  'I'
  0b0111000, // 74  'J'
  0b0000000, // 75  'K'  NO DISPLAY
  0b0001110, // 76  'L'
  0b0000000, // 77  'M'  NO DISPLAY
  0b0010101, // 78  'n'
  0b1111110, // 79  'O'
  0b1100111, // 80  'P'
  0b1110011, // 81  'q'
  0b0000101, // 82  'r'
  0b1011011, // 83  'S'
  0b0001111, // 84  't'
  0b0111110, // 85  'U'
  0b0000000, // 86  'V'  NO DISPLAY
  0b0000000, // 87  'W'  NO DISPLAY
  0b0000000, // 88  'X'  NO DISPLAY
  0b0111011, // 89  'y'
  0b0000000, // 90  'Z'  NO DISPLAY
  0b1001110, // 91  '['
  0b0000000, // 92  '\'  NO DISPLAY
  0b1111000, // 93  ']'
  0b0000000, // 94  '^'  NO DISPLAY
  0b0001000, // 95  '_'
  0b0000010, // 96  '`'
  0b1110111, // 97  'a' SAME AS CAP
  0b0011111, // 98  'b' SAME AS CAP
  0b0001101, // 99  'c'
  0b0111101, // 100 'd' SAME AS CAP
  0b1101111, // 101 'e'
  0b1000111, // 102 'F' SAME AS CAP
  0b1011110, // 103 'G' SAME AS CAP
  0b0010111, // 104 'h'
  0b0010000, // 105 'i'
  0b0111000, // 106 'j' SAME AS CAP
  0b0000000, // 107 'k'  NO DISPLAY
  0b0110000, // 108 'l'
  0b0000000, // 109 'm'  NO DISPLAY
  0b0010101, // 110 'n' SAME AS CAP
  0b0011101, // 111 'o'
  0b1100111, // 112 'p' SAME AS CAP
  0b1110011, // 113 'q' SAME AS CAP
  0b0000101, // 114 'r' SAME AS CAP
  0b1011011, // 115 'S' SAME AS CAP
  0b0001111, // 116 't' SAME AS CAP
  0b0011100, // 117 'u'
  0b0000000, // 118 'b'  NO DISPLAY
  0b0000000, // 119 'w'  NO DISPLAY
  0b0000000, // 120 'x'  NO DISPLAY
  0b0000000, // 121 'y'  NO DISPLAY
  0b0000000, // 122 'z'  NO DISPLAY
  0b0000000, // 123 '0b'  NO DISPLAY
  0b0000000, // 124 '|'  NO DISPLAY
  0b0000000, // 125 ','  NO DISPLAY
  0b0000000, // 126 '~'  NO DISPLAY
  0b0000000, // 127 'DEL'  NO DISPLAY
};

void UpdateSegmentFromChar(Segment& seg, char toDisplay){
    seg.text = toDisplay;
    seg.chr = characterArray[static_cast<uint8_t>(seg.text)];
    // clear the pixels array before updating it
    std::fill_n(seg.pixels, SEGMENT_PIXELS * 7, false);

    int pixel_section = 0;

    for (int i = 0; i < 7; i++) {
        seg.segments[i] = (seg.chr >> (6 - i)) & 0x01;
        pixel_section = segment_order[i];
        if (pixel_section < 0 || pixel_section >= 7) {
            continue;
        }

        for (int p = 0; p < SEGMENT_PIXELS; p++) {
            seg.pixels[pixel_section * SEGMENT_PIXELS + p] = seg.segments[i];
        }
    }
}
