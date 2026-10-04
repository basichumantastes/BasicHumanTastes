#pragma once
// Généré par tools/gaston_panel.py : ne pas modifier à la main. Coordonnées en mm.

namespace gl {

static const float TEMPO_X = 15.600f;
static const float TEMPO_Y = 33.600f;
static const float TEMPO_R = 6.200f;
static const float SWING_X = 32.600f;
static const float SWING_Y = 35.200f;
static const float SWING_R = 3.600f;
static const float SWING_READ_Y = 28.600f;
static const float BPM_SCREEN[4] = {7.600f, 16.800f, 39.200f, 23.600f};
static const float TRANSPORT_X[3] = {12.600f, 23.800f, 34.600f};
static const float TRANSPORT_R[3] = {4.000f, 4.400f, 4.400f};
static const float TRANSPORT_Y = 58.200f;
static const float IN_X[4] = {14.600f, 32.200f, 14.600f, 32.200f};
static const float IN_Y[4] = {80.900f, 80.900f, 94.400f, 94.400f};
static const float OUT_L_X[3] = {11.530f, 23.400f, 35.270f};
static const float OUT_JACK_Y = 116.800f;
static const float SCREEN[4] = {43.600f, 12.600f, 177.280f, 20.200f};
static const float SEQ_BOX[4] = {43.600f, 22.200f, 177.280f, 86.800f};
static const float MAP_BOX[4] = {45.440f, 38.000f, 175.440f, 68.600f};
static const float STEP_X0 = 46.640f;
static const float STEP_W = 7.000f;
static const float STEP_PITCH = 7.800f;
static const float GROUP_EXTRA = 1.200f;
static const float NUM_Y = 25.000f;
static const float STEP_Y0 = 26.600f;
static const float STEP_H = 7.000f;
static const float GAUGE_Y0 = 34.600f;
static const float GAUGE_H = 1.600f;
static const float CV_BAR_Y0 = 26.600f;
static const float CV_BAR_H = 9.600f;
static const float MAP_ROW_Y0 = 39.200f;
static const float MAP_ROW_PITCH = 2.400f;
static const float MAP_ROW_H = 2.200f;
static const float MAP_SQ = 1.800f;
static const float PAGE_Y = 72.200f;
static const float PAGE_W = 7.600f;
static const float PAGE_H = 4.000f;
static const float PAGE_PITCH = 9.200f;
static const float MAIN_CX = 110.440f;
static const float RULE_Y = 75.800f;
static const float CTRL_Y = 80.600f;
static const float CTRL_KNOB_R = 3.200f;
static const float CTRL_X[4] = {49.840f, 78.340f, 106.840f, 135.340f};
static const float SEG_X0 = 152.640f;
static const float SEG_W = 10.800f;
static const float SEG_H = 3.000f;
static const float SEG_Y[2] = {78.700f, 82.500f};
static const float TRIG_COL_X[8] = {60.840f, 69.640f, 78.440f, 87.240f, 96.040f, 104.840f, 113.640f, 122.440f};
static const float POLY_X = 131.440f;
static const float CV_COL_X[4] = {143.240f, 152.040f, 160.840f, 169.640f};
static const float LED_Y = 90.900f;
static const float SEL_Y = 95.200f;
static const float SEL_SIZE = 6.000f;
static const float PAD_Y = 102.000f;
static const float PAD_W = 6.800f;
static const float PAD_H = 4.800f;
static const float MUTE_Y = 106.700f;
static const float MUTE_W = 5.600f;
static const float MUTE_H = 2.200f;
static const float CV_READ[4] = {138.840f, 99.600f, 174.040f, 107.800f};

struct Label { float x, y; const char* text; float size; NVGcolor color; int align; float spacing; };
static const Label LABELS[] = {
	{13.000f, 7.200f, "GASTON", 15.00f, nvgRGB(0xd4, 0xae, 0x66), NVG_ALIGN_LEFT, 1.20f},
	{170.000f, 7.000f, "SEQUENCER · 8 TRIG · 4 CV", 5.00f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_RIGHT, 0.60f},
	{91.440f, 125.700f, "BASIC HUMAN TASTES", 4.60f, nvgRGB(0xa3, 0x97, 0x80), NVG_ALIGN_CENTER, 1.20f},
	{8.000f, 15.000f, "CLOCK", 6.40f, nvgRGB(0xb3, 0xa6, 0x87), NVG_ALIGN_LEFT, 0.90f},
	{15.600f, 43.200f, "TEMPO", 6.60f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.60f},
	{32.600f, 43.200f, "SWING", 6.60f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.60f},
	{8.000f, 50.400f, "TRANSPORT", 6.40f, nvgRGB(0xb3, 0xa6, 0x87), NVG_ALIGN_LEFT, 0.90f},
	{8.000f, 69.800f, "INPUTS", 6.40f, nvgRGB(0xb3, 0xa6, 0x87), NVG_ALIGN_LEFT, 0.90f},
	{14.600f, 75.500f, "CLOCK", 6.60f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.00f},
	{32.200f, 75.500f, "PLAY", 6.60f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.00f},
	{14.600f, 89.000f, "RESET", 6.60f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.00f},
	{32.200f, 89.000f, "PADS", 6.60f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.00f},
	{23.400f, 107.400f, "OUTPUTS", 5.80f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.90f},
	{11.530f, 111.400f, "CLOCK", 5.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{23.400f, 111.400f, "RUN", 5.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{35.270f, 111.400f, "RESET", 5.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{47.440f, 38.000f, "MAP", 5.20f, nvgRGB(0xa3, 0x97, 0x80), NVG_ALIGN_LEFT, 0.90f},
	{86.840f, 72.200f, "PAGE", 6.00f, nvgRGB(0xb3, 0xa6, 0x87), NVG_ALIGN_CENTER, 0.90f},
	{47.440f, 95.200f, "SELECT", 4.40f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_LEFT, 0.50f},
	{47.440f, 102.000f, "PADS", 4.60f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_LEFT, 0.50f},
	{47.440f, 106.700f, "MUTE", 4.60f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_LEFT, 0.50f},
	{51.640f, 116.800f, "OUT", 6.00f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.90f},
	{60.840f, 111.400f, "1", 6.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{69.640f, 111.400f, "2", 6.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{78.440f, 111.400f, "3", 6.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{87.240f, 111.400f, "4", 6.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{96.040f, 111.400f, "5", 6.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{104.840f, 111.400f, "6", 6.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{113.640f, 111.400f, "7", 6.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{122.440f, 111.400f, "8", 6.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{131.440f, 111.400f, "POLY", 6.00f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{143.240f, 111.400f, "A", 6.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{152.040f, 111.400f, "B", 6.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{160.840f, 111.400f, "C", 6.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
	{169.640f, 111.400f, "D", 6.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
};

} // namespace gl
