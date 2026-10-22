#pragma once
// Généré par tools/louis_panel.py : ne pas modifier à la main. Coordonnées en mm.

namespace ll {

static const float SCREEN[4] = {4.600f, 11.400f, 168.120f, 37.000f};
static const float KNOB_Y = 48.400f;
static const float LABEL_Y = 55.400f;
static const float JACK_Y = 60.700f;
static const float PACE_R = 4.600f;
static const float KNOB_R = 3.800f;
static const float CLIM_X[8] = {11.500f, 22.100f, 32.700f, 43.300f, 53.900f, 64.500f, 75.100f, 85.700f};
static const float DRIFT_X[2] = {97.550f, 107.450f};
static const float METER_W = 1.800f;
static const float METER_H = 8.000f;
static const float PITCH_X[5] = {122.492f, 132.276f, 142.060f, 151.844f, 161.628f};
static const float WHEEL_R = 3.900f;
static const float LINK_KNOB[3] = {11.000f, 77.600f, 4.000f};
static const float LINK_JACK_Y = 89.600f;
static const float MODE_X0 = 17.400f;
static const float MODE_X1 = 49.000f;
static const float MODE_Y0 = 72.000f;
static const float MODE_H = 6.600f;
static const float MODE_GAP = 0.500f;
static const float GRID_KNOB[3] = {10.400f, 106.000f, 3.900f};
static const float SNAP_KNOB[3] = {22.000f, 106.000f, 3.900f};
static const float CLOCK_JACK_Y = 117.900f;
static const float CLOCK_IN[3] = {39.400f, 101.900f, 107.200f};
static const float RESET_IN[3] = {39.400f, 112.600f, 117.900f};
static const float OUT_X[4] = {120.050f, 133.350f, 146.650f, 159.950f};
static const float ROW_Y[4] = {80.600f, 89.200f, 97.800f, 106.400f};
static const float ALL_JACK_Y = 118.000f;
static const float TIME_X = 66.400f;
static const float OCT_X = 78.600f;
static const float HOLD_X = 90.800f;
static const float LED_X = 103.000f;
static const float SMALL_R = 3.200f;
static const float HOLD_W = 6.600f;
static const float HOLD_H = 4.400f;
static const float LED_R = 1.000f;
static const float STRIKE_X = 72.500f;

enum LabelFont { FONT_REG, FONT_BOLD, FONT_MIC };
static const char* const FONT_FILES[3] = {"res/fonts/IBMPlexSans-Regular.ttf", "res/fonts/IBMPlexSans-Bold.ttf", "res/fonts/Michroma-Regular.ttf"};
// Taille NanoVG = em / FONT_EM : NanoVG prend la taille pour la hauteur de ligne, pas pour le em
static const float FONT_EM[3] = {0.7692f, 0.7692f, 0.7033f};

struct Label { float x, y; const char* text; float size; NVGcolor color; int align; float spacing; int font; };
static const Label LABELS[] = {
	{13.000f, 5.500f, "LOUIS", 14.00f, nvgRGB(0xf1, 0xea, 0xd9), NVG_ALIGN_LEFT, 1.40f, FONT_MIC},
	{159.720f, 5.600f, "KRELL GENERATOR · FOUR VOICES · TRIG GATE PITCH ENV", 5.00f, nvgRGB(0xe6, 0xdc, 0xc5), NVG_ALIGN_RIGHT, 0.80f, FONT_REG},
	{86.360f, 125.750f, "BASIC HUMAN TASTES", 4.60f, nvgRGB(0x8f, 0x88, 0x7a), NVG_ALIGN_CENTER, 1.20f, FONT_MIC},
	{6.400f, 40.800f, "CLIMATE", 5.40f, nvgRGB(0xf1, 0xea, 0xd9), NVG_ALIGN_LEFT, 1.20f, FONT_BOLD},
	{117.800f, 40.800f, "PITCH", 5.40f, nvgRGB(0xf1, 0xea, 0xd9), NVG_ALIGN_LEFT, 1.20f, FONT_BOLD},
	{6.400f, 69.000f, "LINK", 5.40f, nvgRGB(0xf1, 0xea, 0xd9), NVG_ALIGN_LEFT, 1.20f, FONT_BOLD},
	{6.400f, 98.000f, "CLOCK", 5.40f, nvgRGB(0xf1, 0xea, 0xd9), NVG_ALIGN_LEFT, 1.20f, FONT_BOLD},
	{54.400f, 69.000f, "VOICES", 5.40f, nvgRGB(0xf1, 0xea, 0xd9), NVG_ALIGN_LEFT, 1.20f, FONT_BOLD},
	{11.500f, 55.400f, "PACE", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{22.100f, 55.400f, "TILT", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{32.700f, 55.400f, "DRIFT", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{43.300f, 55.400f, "WANDER", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{53.900f, 55.400f, "DENSITY", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{64.500f, 55.400f, "MEMORY", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{75.100f, 55.400f, "CORREL", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{85.700f, 55.400f, "GATE", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{97.550f, 55.400f, "DRIFT A", 5.20f, nvgRGB(0xf1, 0xea, 0xd9), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{107.450f, 55.400f, "DRIFT B", 5.20f, nvgRGB(0xf1, 0xea, 0xd9), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{122.492f, 55.400f, "SCALE", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{132.276f, 55.400f, "ROOT", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{142.060f, 55.400f, "RANGE", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{151.844f, 55.400f, "SKEW", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{161.628f, 55.400f, "SCALE IN", 5.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{11.000f, 84.600f, "LINK", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{10.400f, 112.800f, "GRID", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{22.000f, 112.800f, "SNAP", 6.20f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{39.400f, 101.900f, "CLOCK", 5.60f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{39.400f, 112.600f, "RESET", 5.60f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{66.400f, 74.400f, "TIME", 5.60f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{78.600f, 74.400f, "OCT", 5.60f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{90.800f, 74.400f, "HOLD", 5.60f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{57.000f, 80.600f, "1", 7.40f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_BOLD},
	{57.000f, 89.200f, "2", 7.40f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_BOLD},
	{57.000f, 97.800f, "3", 7.40f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_BOLD},
	{57.000f, 106.400f, "4", 7.40f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_BOLD},
	{120.050f, 74.400f, "TRIG", 5.60f, nvgRGB(0xf1, 0xea, 0xd9), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{120.050f, 113.000f, "POLY", 4.80f, nvgRGB(0xcf, 0xc6, 0xb2), NVG_ALIGN_CENTER, 0.60f, FONT_REG},
	{133.350f, 74.400f, "GATE", 5.60f, nvgRGB(0xf1, 0xea, 0xd9), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{133.350f, 113.000f, "POLY", 4.80f, nvgRGB(0xcf, 0xc6, 0xb2), NVG_ALIGN_CENTER, 0.60f, FONT_REG},
	{146.650f, 74.400f, "PITCH", 5.60f, nvgRGB(0xf1, 0xea, 0xd9), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{146.650f, 113.000f, "POLY", 4.80f, nvgRGB(0xcf, 0xc6, 0xb2), NVG_ALIGN_CENTER, 0.60f, FONT_REG},
	{159.950f, 74.400f, "ENV", 5.60f, nvgRGB(0xf1, 0xea, 0xd9), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{159.950f, 113.000f, "POLY", 4.80f, nvgRGB(0xcf, 0xc6, 0xb2), NVG_ALIGN_CENTER, 0.60f, FONT_REG},
	{57.000f, 118.000f, "ALL", 4.80f, nvgRGB(0x4f, 0x48, 0x3c), NVG_ALIGN_CENTER, 0.40f, FONT_REG},
	{72.500f, 113.000f, "STRIKE", 5.60f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{90.800f, 113.000f, "FREEZE", 5.60f, nvgRGB(0x1d, 0x1b, 0x17), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
	{103.000f, 113.000f, "ANY", 5.60f, nvgRGB(0xf1, 0xea, 0xd9), NVG_ALIGN_CENTER, 0.00f, FONT_REG},
};

} // namespace ll
