#pragma once
// Généré par tools/gastounet_panel.py : ne pas modifier à la main. Coordonnées en mm.

namespace gnl {

static const float SCREEN[4] = {4.000f, 12.600f, 78.000f, 42.600f};
static const float ROW_Y0 = 24.400f;
static const float ROW_PITCH = 4.100f;
static const int ROWS = 4;
static const float EDIT_X = 88.800f;
static const float EDIT_Y[3] = {22.000f, 29.400f, 36.800f};
static const float EDIT_W = 15.000f;
static const float EDIT_H = 5.400f;
static const float MODE_Y = 47.600f;
static const float MODE_W = 21.600f;
static const float MODE_H = 5.600f;
static const float MODE_X[4] = {15.800f, 39.000f, 62.200f, 85.400f};
static const float MEM_BOX[4] = {4.000f, 52.400f, 97.600f, 95.000f};
static const float BANK_X = 9.800f;
static const float BANK_W = 6.800f;
static const float BANK_H = 7.600f;
static const float ROW_Y[4] = {62.000f, 70.800f, 79.600f, 88.400f};
static const float SLOT_X[4] = {21.000f, 30.200f, 39.400f, 48.600f};
static const float SLOT_W = 8.200f;
static const float SLOT_H = 7.600f;
static const float ACT_X[2] = {67.000f, 87.000f};
static const float ACT_Y[2] = {64.600f, 81.800f};
static const float ACT_R = 4.400f;
static const float SEG_Y = 104.000f;
static const float SEG_H = 4.000f;
static const float QUANT_X0 = 6.400f;
static const float QUANT_W = 10.600f;
static const float CHAIN_X0 = 71.400f;
static const float CHAIN_W = 12.000f;
static const float IN_X[4] = {13.400f, 32.000f, 50.600f, 69.200f};
static const float JACK_Y = 118.000f;
static const float END_X = 89.100f;

static const gl::Label LABELS[] = {
	{11.000f, 7.200f, "GASTOUNET", 15.00f, nvgRGB(0xd4, 0xae, 0x66), NVG_ALIGN_LEFT, 1.00f},
	{90.600f, 7.200f, "PATTERNS · SONG", 5.80f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_RIGHT, 0.60f},
	{50.800f, 125.700f, "BASIC HUMAN TASTES", 4.60f, nvgRGB(0xa3, 0x97, 0x80), NVG_ALIGN_CENTER, 1.20f},
	{7.000f, 99.400f, "LAUNCH", 6.40f, nvgRGB(0xb3, 0xa6, 0x87), NVG_ALIGN_LEFT, 0.90f},
	{82.400f, 15.600f, "ROWS", 5.60f, nvgRGB(0xb3, 0xa6, 0x87), NVG_ALIGN_LEFT, 0.90f},
	{67.000f, 71.400f, "WRITE", 6.20f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.50f},
	{87.000f, 71.400f, "COPY", 6.20f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.50f},
	{67.000f, 88.600f, "NEXT", 6.20f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.50f},
	{87.000f, 88.600f, "RANDOM", 6.20f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.50f},
	{13.400f, 112.600f, "PATTERN", 6.20f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.00f},
	{32.000f, 112.600f, "NEXT", 6.20f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.00f},
	{50.600f, 112.600f, "RANDOM", 6.20f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.00f},
	{69.200f, 112.600f, "RESET", 6.20f, nvgRGB(0xd6, 0xc9, 0xaa), NVG_ALIGN_CENTER, 0.00f},
	{89.100f, 112.600f, "END", 6.40f, nvgRGB(0x14, 0x1a, 0x2a), NVG_ALIGN_CENTER, 0.00f},
};

} // namespace gnl
