#include "plugin.hpp"
#include "LouisCore.hpp"
#include "LouisLayout.hpp"
#include <cmath>
#include <atomic>

// Louis : un générateur Krell à quatre voix, sans son. Chaque voix est un cycle attaque/decay qui se relance lui-même ;
// des dérives lentes déplacent les durées et les hauteurs, une boucle de mémoire peut rejouer les tirages, les voix
// s'influencent en anneau (CHAIN, HUSH, ECHO), et une horloge branchée cale les notes sur sa grille sans latence.
// Sorties par voix : TRIG, GATE, PITCH, ENV. Le moteur est dans LouisCore.hpp ; cahier des charges : docs/Louis-spec.md.

static const NVGcolor L_INK = nvgRGB(0x1d, 0x1b, 0x17);
static const NVGcolor L_IVORY = nvgRGB(0xf1, 0xea, 0xd9);
static const NVGcolor L_CREAM = nvgRGB(0xf3, 0xec, 0xdb);
static const NVGcolor L_GLASS = nvgRGB(0x06, 0x12, 0x0b);
static const NVGcolor L_PHOS = nvgRGB(0x8f, 0xf5, 0xb0);
static const NVGcolor L_PHOS_LIGHT = nvgRGB(0xd9, 0xff, 0xe6);
static const NVGcolor L_PHOS_DIM = nvgRGB(0x4f, 0xa0, 0x70);
static const NVGcolor L_PHOS_DARK = nvgRGB(0x1f, 0x52, 0x35);
static const NVGcolor L_LED_OFF = nvgRGB(0x2b, 0x3a, 0x30);
static const char* const L_NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char* const L_MODE_NAMES[3] = {"CHAIN", "HUSH", "ECHO"};
static const char* const L_MODE_VERBS[3] = {"next voice plays", "next voice rests", "next voice repeats"};

static const int NOTE_RING = 48;     // notes gardées par voix pour la frise
static const int MARK_RING = 64;     // liens gardés pour la frise
static const int PULSE_RING = 160;   // coups d'horloge gardés pour les traits de grille
static const double WINDOW = 8.0;    // secondes visibles sur la frise

// Une note (ou un silence imposé par HUSH) telle que la frise la dessine
struct FriseNote {
	double t0 = -1e9;
	float a = 0.f, d = 0.f;
	float y = 0.f;       // hauteur dans la plage de la voix, 0 à 1
	int kind = 0;        // 0 note tirée, 1 note rejouée par la boucle, 2 silence de HUSH
};

// Un lien entre deux voix : CHAIN et HUSH au moment du lien, ECHO de la note source à celle qui la reprend
struct FriseMark {
	double t = -1e9, t1 = -1e9;
	int from = 0, to = 1, mode = 0;
	float y1 = 0.f, y2 = 0.f;
};


struct Louis : Module {
	enum ParamId {
		PACE_PARAM,
		TILT_PARAM,
		DRIFT_PARAM,
		WANDER_PARAM,
		DENSITY_PARAM,
		MEMORY_PARAM,
		CORREL_PARAM,
		GATE_PARAM,
		SCALE_PARAM,
		ROOT_PARAM,
		RANGE_PARAM,
		SKEW_PARAM,
		LINK_PARAM,
		MODE_PARAM,
		GRID_PARAM,
		SNAP_PARAM,
		ENUMS(TIME_PARAMS, louis::VOICES),
		ENUMS(OCT_PARAMS, louis::VOICES),
		ENUMS(HOLD_PARAMS, louis::VOICES),
		PARAMS_LEN
	};
	enum InputId {
		PACE_INPUT,
		TILT_INPUT,
		DRIFT_INPUT,
		WANDER_INPUT,
		DENSITY_INPUT,
		MEMORY_INPUT,
		CORREL_INPUT,
		GATE_INPUT,
		SCALE_INPUT,
		ROOT_INPUT,
		RANGE_INPUT,
		SKEW_INPUT,
		SCALE_IN_INPUT,
		LINK_INPUT,
		GRID_INPUT,
		SNAP_INPUT,
		CLOCK_INPUT,
		RESET_INPUT,
		STRIKE_INPUT,
		FREEZE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(TRIG_OUTPUTS, louis::VOICES),
		ENUMS(GATE_OUTPUTS, louis::VOICES),
		ENUMS(PITCH_OUTPUTS, louis::VOICES),
		ENUMS(ENV_OUTPUTS, louis::VOICES),
		TRIG_POLY_OUTPUT,
		GATE_POLY_OUTPUT,
		PITCH_POLY_OUTPUT,
		ENV_POLY_OUTPUT,
		ANY_OUTPUT,
		DRIFT_A_OUTPUT,
		DRIFT_B_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(VOICE_LIGHTS, louis::VOICES),
		ANY_LIGHT,
		LIGHTS_LEN
	};

	louis::Core core;
	louis::Params p;
	louis::Events ev;
	louis::Outputs out;
	dsp::SchmittTrigger clockIn, resetIn, strikeIn[louis::VOICES];
	float flash[louis::VOICES] = {}, anyFlash = 0.f;

	// Pour la frise et les voyants (lus par les widgets)
	double now = 0.0;
	FriseNote notes[louis::VOICES][NOTE_RING];
	int noteHead[louis::VOICES] = {};
	FriseMark marks[MARK_RING];
	int markHead = 0;
	double pulses[PULSE_RING];
	int pulseHead = 0;
	float lastY[louis::VOICES] = {};
	double lastT0[louis::VOICES];
	int wheelMask = 0;          // classes de hauteur absolues de la gamme en cours (0 en FREE)
	int lastPc = -1;
	double lastPcTime = -1e9;
	// Ce que la ligne d'état affiche, en nombres seulement : le fil audio les écrit, l'interface en fait du texte.
	// (Une std::string partagée entre les deux fils faisait planter Rack, dans Live notamment.)
	std::atomic<int> dispScale {louis::SCALE_DEFAULT}, dispRoot {0}, dispGrid {louis::GRID_DEFAULT}, dispSnap {100};
	std::atomic<bool> dispExt {false}, dispClock {false};

	Louis() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(PACE_PARAM, 0.f, 1.f, 0.4918f, "Pace (mean cycle length)", " s", 1000.f, 0.02f);
		configParam(TILT_PARAM, 0.02f, 0.98f, 0.15f, "Tilt (attack share of the cycle)", "%", 0.f, 100.f);
		configParam(DRIFT_PARAM, 0.f, 1.f, 0.3925f, "Drift (speed of the slow random)", " Hz", 200.f, 0.01f);
		configParam(WANDER_PARAM, 0.f, 1.f, 0.4f, "Wander (depth of the slow random)", "%", 0.f, 100.f);
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.85f, "Density (chance that a cycle plays a note)", "%", 0.f, 100.f);
		configParam(MEMORY_PARAM, 0.f, 1.f, 0.f, "Memory (chance to replay the loop)", "%", 0.f, 100.f);
		configParam(CORREL_PARAM, -1.f, 1.f, 0.f, "Correlation (+: high notes last longer, −: high notes are short)", "%", 0.f, 100.f);
		configParam(GATE_PARAM, 0.f, 1.f, 0.5f, "Gate length (share of the decay)", "%", 0.f, 100.f);
		std::vector<std::string> scales;
		for (int s = 0; s < louis::SCALE_COUNT; s++)
			scales.push_back(louis::SCALES[s].name);
		configSwitch(SCALE_PARAM, 0.f, louis::SCALE_COUNT - 1, louis::SCALE_DEFAULT, "Scale (FREE: not quantized)", scales);
		configSwitch(ROOT_PARAM, 0.f, 11.f, 0.f, "Root", std::vector<std::string>(L_NOTE_NAMES, L_NOTE_NAMES + 12));
		configParam(RANGE_PARAM, 0.f, 5.f, 2.f, "Range", " oct");
		configParam(SKEW_PARAM, -1.f, 1.f, 0.f, "Skew (towards low or high notes)", "%", 0.f, 100.f);
		configParam(LINK_PARAM, 0.f, 1.f, 0.f, "Link (chance that a voice acts on the next one)", "%", 0.f, 100.f);
		configSwitch(MODE_PARAM, 0.f, 2.f, 0.f, "Link mode", {"Chain: next voice plays", "Hush: next voice rests", "Echo: next voice repeats"});
		std::vector<std::string> grids;
		for (int g = 0; g < louis::GRID_COUNT; g++)
			grids.push_back(std::string(louis::GRID_NAMES[g]) + (g < 4 ? " clock pulse" : " clock pulses"));
		configSwitch(GRID_PARAM, 0.f, louis::GRID_COUNT - 1, louis::GRID_DEFAULT, "Grid", grids);
		configParam(SNAP_PARAM, 0.f, 1.f, 1.f, "Snap to the grid", "%", 0.f, 100.f);
		std::vector<std::string> times;
		for (int k = 0; k < louis::TIME_COUNT; k++)
			times.push_back(std::string("×") + louis::TIME_NAMES[k]);
		for (int i = 0; i < louis::VOICES; i++) {
			configSwitch(TIME_PARAMS + i, 0.f, louis::TIME_COUNT - 1, louis::TIME_DEFAULT,
				string::f("Voice %d time (ratio of its grid and of the pace)", i + 1), times);
			ParamQuantity* q = configParam(OCT_PARAMS + i, -3.f, 3.f, 0.f, string::f("Voice %d octave", i + 1), " oct");
			q->snapEnabled = true;
			configSwitch(HOLD_PARAMS + i, 0.f, 1.f, 0.f, string::f("Hold voice %d (replay its loop)", i + 1), {"Off", "On"});
		}

		static const char* const CV_NAMES[8] = {"Pace", "Tilt", "Drift", "Wander", "Density", "Memory", "Correlation", "Gate"};
		for (int k = 0; k < 8; k++)
			configInput(PACE_INPUT + k, string::f("%s CV (added to the knob, 10 V = full travel)", CV_NAMES[k]));
		configInput(SCALE_INPUT, "Scale CV (1 V per scale; replaces the knob when patched)");
		configInput(ROOT_INPUT, "Root (1 V/oct, transposes everything)");
		configInput(RANGE_INPUT, "Range CV");
		configInput(SKEW_INPUT, "Skew CV");
		configInput(SCALE_IN_INPUT, "Scale in (poly: each channel is a note; replaces the scale when patched)");
		configInput(LINK_INPUT, "Link CV");
		configInput(GRID_INPUT, "Grid CV (1 V per step)");
		configInput(SNAP_INPUT, "Snap CV");
		configInput(CLOCK_INPUT, "Clock (notes snap to its grid when patched)");
		configInput(RESET_INPUT, "Reset (all voices start over)");
		configInput(STRIKE_INPUT, "Strike (poly: channel N plays voice N; mono: all voices)");
		configInput(FREEZE_INPUT, "Freeze (gate: every voice replays its loop)");
		for (int i = 0; i < louis::VOICES; i++) {
			configOutput(TRIG_OUTPUTS + i, string::f("Voice %d trigger", i + 1));
			configOutput(GATE_OUTPUTS + i, string::f("Voice %d gate", i + 1));
			configOutput(PITCH_OUTPUTS + i, string::f("Voice %d pitch (V/oct)", i + 1));
			configOutput(ENV_OUTPUTS + i, string::f("Voice %d envelope", i + 1));
		}
		configOutput(TRIG_POLY_OUTPUT, "Triggers (poly, 4 channels)");
		configOutput(GATE_POLY_OUTPUT, "Gates (poly, 4 channels)");
		configOutput(PITCH_POLY_OUTPUT, "Pitches (poly, 4 channels)");
		configOutput(ENV_POLY_OUTPUT, "Envelopes (poly, 4 channels)");
		configOutput(ANY_OUTPUT, "Any (a trigger for every note)");
		configOutput(DRIFT_A_OUTPUT, "Drift A (slow random that tilts the pitches, ±5 V)");
		configOutput(DRIFT_B_OUTPUT, "Drift B (slow random that sways the density, ±5 V)");

		core.init(random::u64());
		clearFrise();
	}

	void clearFrise() {
		for (int i = 0; i < louis::VOICES; i++) {
			for (int k = 0; k < NOTE_RING; k++)
				notes[i][k] = FriseNote();
			noteHead[i] = 0;
			lastT0[i] = -1e9;
		}
		for (int k = 0; k < MARK_RING; k++)
			marks[k] = FriseMark();
		for (int k = 0; k < PULSE_RING; k++)
			pulses[k] = -1e9;
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		p = louis::Params();
		core.init(random::u64());
		clearFrise();
	}

	float cv(int input, float scale) {
		return inputs[input].getVoltage() * scale;
	}

	float unit(int param, int input) {
		return clamp(params[param].getValue() + cv(input, 0.1f), 0.f, 1.f);
	}

	// Position de la hauteur dans la plage de la voix, pour la frise
	float pitchY(int i, float volts) {
		return clamp((volts - p.root - p.oct[i]) / std::max(p.range, 0.25f), 0.f, 1.f);
	}

	void readParams() {
		p.pace = 0.02f * std::pow(1000.f, unit(PACE_PARAM, PACE_INPUT));
		p.tilt = clamp(params[TILT_PARAM].getValue() + cv(TILT_INPUT, 0.096f), 0.02f, 0.98f);
		p.drift = 0.01f * std::pow(200.f, unit(DRIFT_PARAM, DRIFT_INPUT));
		p.wander = unit(WANDER_PARAM, WANDER_INPUT);
		p.density = unit(DENSITY_PARAM, DENSITY_INPUT);
		p.memory = unit(MEMORY_PARAM, MEMORY_INPUT);
		p.correl = clamp(params[CORREL_PARAM].getValue() + cv(CORREL_INPUT, 0.2f), -1.f, 1.f);
		p.gate = unit(GATE_PARAM, GATE_INPUT);
		if (inputs[SCALE_INPUT].isConnected())
			p.scale = clamp((int) std::round(inputs[SCALE_INPUT].getVoltage()), 0, louis::SCALE_COUNT - 1);
		else
			p.scale = (int) std::round(params[SCALE_PARAM].getValue());
		p.root = std::round(params[ROOT_PARAM].getValue()) / 12.f + inputs[ROOT_INPUT].getVoltage();
		p.range = clamp(params[RANGE_PARAM].getValue() + cv(RANGE_INPUT, 0.5f), 0.f, 5.f);
		p.skew = clamp(params[SKEW_PARAM].getValue() + cv(SKEW_INPUT, 0.2f), -1.f, 1.f);
		p.link = unit(LINK_PARAM, LINK_INPUT);
		p.linkMode = clamp((int) std::round(params[MODE_PARAM].getValue()), 0, 2);
		p.grid = clamp((int) std::round(params[GRID_PARAM].getValue() + inputs[GRID_INPUT].getVoltage()), 0, louis::GRID_COUNT - 1);
		p.snap = unit(SNAP_PARAM, SNAP_INPUT);
		for (int i = 0; i < louis::VOICES; i++) {
			p.time[i] = (int) std::round(params[TIME_PARAMS + i].getValue());
			p.oct[i] = std::round(params[OCT_PARAMS + i].getValue());
			p.hold[i] = params[HOLD_PARAMS + i].getValue() > 0.5f;
		}
		p.freeze = inputs[FREEZE_INPUT].getVoltage() > 1.f;
		p.clockConnected = inputs[CLOCK_INPUT].isConnected();
		p.extMask = 0;
		if (inputs[SCALE_IN_INPUT].isConnected()) {
			int n = inputs[SCALE_IN_INPUT].getChannels();
			for (int c = 0; c < n; c++) {
				int pc = (int) std::lround(inputs[SCALE_IN_INPUT].getVoltage(c) * 12.f);
				p.extMask |= 1 << (int) louis::posMod(pc, 12);
			}
		}
	}

	// Ce que la frise, la roue des notes et la ligne d'état montrent (une fois par bloc de 32 échantillons)
	void readDisplay() {
		int rootPc = (int) louis::posMod(std::lround(p.root * 12.f), 12);
		if (p.extMask) {
			wheelMask = p.extMask;
		}
		else {
			int m = louis::SCALES[p.scale].mask, w = 0;
			for (int k = 0; k < 12; k++)
				if ((m >> k) & 1)
					w |= 1 << ((k + rootPc) % 12);
			wheelMask = w;
		}
		dispExt = p.extMask != 0;
		dispScale = p.scale;
		dispRoot = rootPc;
		dispClock = p.clockConnected;
		dispGrid = p.grid;
		dispSnap = (int) std::round(p.snap * 100.f);
	}

	int sinceDisplay = 0;

	void process(const ProcessArgs& args) override {
		readParams();
		if (++sinceDisplay >= 32) {
			sinceDisplay = 0;
			readDisplay();
		}

		ev = louis::Events();
		ev.clock = p.clockConnected && clockIn.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		ev.reset = resetIn.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f);
		int sc = inputs[STRIKE_INPUT].getChannels();
		for (int i = 0; i < louis::VOICES; i++) {
			float v = sc == 1 ? inputs[STRIKE_INPUT].getVoltage(0) : (i < sc ? inputs[STRIKE_INPUT].getVoltage(i) : 0.f);
			ev.strike[i] = strikeIn[i].process(v, 0.1f, 1.f);
		}

		core.process(args.sampleTime, p, ev, out);
		record();

		for (int i = 0; i < louis::VOICES; i++) {
			outputs[TRIG_OUTPUTS + i].setVoltage(out.trig[i]);
			outputs[GATE_OUTPUTS + i].setVoltage(out.gate[i]);
			outputs[PITCH_OUTPUTS + i].setVoltage(out.pitch[i]);
			outputs[ENV_OUTPUTS + i].setVoltage(out.env[i]);
			outputs[TRIG_POLY_OUTPUT].setVoltage(out.trig[i], i);
			outputs[GATE_POLY_OUTPUT].setVoltage(out.gate[i], i);
			outputs[PITCH_POLY_OUTPUT].setVoltage(out.pitch[i], i);
			outputs[ENV_POLY_OUTPUT].setVoltage(out.env[i], i);
			if (out.onset[i])
				flash[i] = 1.f;
			flash[i] = std::max(0.f, flash[i] - args.sampleTime * 8.f);
			lights[VOICE_LIGHTS + i].setBrightness(flash[i]);
		}
		for (int o = TRIG_POLY_OUTPUT; o <= ENV_POLY_OUTPUT; o++)
			outputs[o].setChannels(louis::VOICES);
		outputs[ANY_OUTPUT].setVoltage(out.any);
		outputs[DRIFT_A_OUTPUT].setVoltage(out.driftA);
		outputs[DRIFT_B_OUTPUT].setVoltage(out.driftB);
		if (out.any > 5.f)
			anyFlash = 1.f;
		anyFlash = std::max(0.f, anyFlash - args.sampleTime * 8.f);
		lights[ANY_LIGHT].setBrightness(anyFlash);
		now += args.sampleTime;
	}

	void pushMark(const FriseMark& m) {
		marks[markHead] = m;
		markHead = (markHead + 1) % MARK_RING;
	}

	// Garde ce que la frise doit dessiner : notes, silences de HUSH, liens, coups d'horloge
	void record() {
		if (ev.clock) {
			pulses[pulseHead] = now;
			pulseHead = (pulseHead + 1) % PULSE_RING;
		}
		for (int i = 0; i < louis::VOICES; i++) {
			if (!out.cycle[i])
				continue;
			const louis::Voice& w = core.v[i];
			FriseNote n;
			n.t0 = now;
			n.a = w.attack;
			n.d = w.decay;
			if (w.note) {
				n.y = pitchY(i, w.pitch);
				n.kind = w.fromLoop ? 1 : 0;
				if (w.echoed && w.echoFrom >= 0) {
					FriseMark m;
					m.mode = louis::LINK_ECHO;
					m.from = w.echoFrom;
					m.to = i;
					m.t1 = lastT0[w.echoFrom];
					m.y1 = lastY[w.echoFrom];
					m.t = now;
					m.y2 = n.y;
					pushMark(m);
				}
				lastT0[i] = now;
				lastY[i] = n.y;
				lastPc = (int) louis::posMod(std::lround(w.pitch * 12.f), 12);
				lastPcTime = now;
			}
			else if (w.hushed) {
				n.kind = 2;
			}
			else {
				continue;
			}
			notes[i][noteHead[i]] = n;
			noteHead[i] = (noteHead[i] + 1) % NOTE_RING;
		}
		for (int i = 0; i < louis::VOICES; i++) {
			if (!out.link[i])
				continue;
			int j = (i + 1) % louis::VOICES;
			// CHAIN ne compte que s'il a vraiment relancé la suivante ; ECHO se dessine quand la note reprise part
			if (p.linkMode == louis::LINK_ECHO || (p.linkMode == louis::LINK_CHAIN && !out.onset[j]))
				continue;
			FriseMark m;
			m.mode = p.linkMode;
			m.from = i;
			m.to = j;
			m.t = now;
			pushMark(m);
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "linearEnv", json_boolean(p.linearEnv));
		json_object_set_new(root, "loopLength", json_integer(p.loopLength));
		json_object_set_new(root, "clockStopFree", json_boolean(p.clockStopFree));
		json_t* loops = json_array();
		for (int i = 0; i < louis::VOICES; i++) {
			const louis::Voice& w = core.v[i];
			json_t* l = json_object();
			json_t* u = json_array();
			json_t* jit = json_array();
			json_t* note = json_array();
			for (int k = 0; k < louis::MAX_LOOP; k++) {
				json_array_append_new(u, json_real(w.loopFilled[k] ? w.loopU[k] : -1.0));
				json_array_append_new(jit, json_real(w.loopJit[k]));
				json_array_append_new(note, json_boolean(w.loopNote[k]));
			}
			json_object_set_new(l, "u", u);
			json_object_set_new(l, "jit", jit);
			json_object_set_new(l, "note", note);
			json_object_set_new(l, "pos", json_integer(w.loopPos));
			json_array_append_new(loops, l);
		}
		json_object_set_new(root, "loops", loops);
		return root;
	}

	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "linearEnv"))
			p.linearEnv = json_boolean_value(j);
		if (json_t* j = json_object_get(root, "loopLength"))
			p.loopLength = clamp((int) json_integer_value(j), 4, louis::MAX_LOOP);
		if (json_t* j = json_object_get(root, "clockStopFree"))
			p.clockStopFree = json_boolean_value(j);
		json_t* loops = json_object_get(root, "loops");
		for (int i = 0; loops && i < louis::VOICES && i < (int) json_array_size(loops); i++) {
			json_t* l = json_array_get(loops, i);
			louis::Voice& w = core.v[i];
			json_t* u = json_object_get(l, "u");
			json_t* jit = json_object_get(l, "jit");
			json_t* note = json_object_get(l, "note");
			for (int k = 0; k < louis::MAX_LOOP; k++) {
				float uv = u ? (float) json_number_value(json_array_get(u, k)) : -1.f;
				w.loopFilled[k] = uv >= 0.f;
				w.loopU[k] = std::max(uv, 0.f);
				w.loopJit[k] = jit ? (float) json_number_value(json_array_get(jit, k)) : 0.f;
				w.loopNote[k] = note ? json_boolean_value(json_array_get(note, k)) : false;
			}
			if (json_t* pos = json_object_get(l, "pos"))
				w.loopPos = clamp((int) json_integer_value(pos), 0, louis::MAX_LOOP - 1);
		}
	}
};


// --- Dessin

static void lText(NVGcontext* vg, float x, float y, const std::string& s, float em, NVGcolor color, int align = NVG_ALIGN_CENTER,
	float spacing = 0.f, int font = ll::FONT_REG) {
	std::shared_ptr<window::Font> f = APP->window->loadFont(asset::plugin(pluginInstance, ll::FONT_FILES[font]));
	if (!f)
		return;
	nvgFontFaceId(vg, f->handle);
	nvgFontSize(vg, em / ll::FONT_EM[font]);
	nvgTextLetterSpacing(vg, spacing);
	nvgFillColor(vg, color);
	nvgTextAlign(vg, align | NVG_ALIGN_MIDDLE);
	nvgText(vg, x, y, s.c_str(), NULL);
	// IBM Plex Regular est un peu maigre à cette taille : un second tracé décalé d'un tiers de pixel l'épaissit
	if (font == ll::FONT_REG)
		nvgText(vg, x + 0.3f, y, s.c_str(), NULL);
}

static void lFill(NVGcontext* vg, NVGcolor c) {
	nvgFillColor(vg, c);
	nvgFill(vg);
}

static void lStroke(NVGcontext* vg, NVGcolor c, float width) {
	nvgStrokeColor(vg, c);
	nvgStrokeWidth(vg, width);
	nvgStroke(vg);
}

static void lGlowDot(NVGcontext* vg, float x, float y, float r, NVGcolor c, float strength) {
	if (strength > 0.01f) {
		NVGpaint g = nvgRadialGradient(vg, x, y, r * 0.5f, r * 3.5f, nvgTransRGBAf(c, 0.5f * strength), nvgTransRGBA(c, 0));
		nvgBeginPath(vg);
		nvgCircle(vg, x, y, r * 3.5f);
		nvgFillPaint(vg, g);
		nvgFill(vg);
	}
	nvgBeginPath(vg);
	nvgCircle(vg, x, y, r);
	lFill(vg, strength > 0.01f ? c : L_LED_OFF);
}

// Trait en pointillés le long d'une suite de points, coupé à la bande [xmin, xmax]. Les tirets sont comptés, pas
// avancés pas à pas : un pas trop petit pour un float bloquait le dessin (et Rack) sur les longues lignes d'ECHO.
static void lDashed(NVGcontext* vg, const std::vector<Vec>& pts, float on, float off, NVGcolor c, float width, float xmin, float xmax) {
	nvgBeginPath(vg);
	float period = on + off;
	int budget = 800;
	for (size_t k = 1; k < pts.size() && budget > 0; k++) {
		Vec a = pts[k - 1], b = pts[k];
		if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(b.x) || !std::isfinite(b.y))
			continue;
		float dx = b.x - a.x;
		float lo = 0.f, hi = 1.f;
		if (std::fabs(dx) > 1e-6f) {
			float t0 = (xmin - a.x) / dx, t1 = (xmax - a.x) / dx;
			lo = std::max(lo, std::min(t0, t1));
			hi = std::min(hi, std::max(t0, t1));
		}
		else if (a.x < xmin || a.x > xmax) {
			continue;
		}
		if (lo >= hi)
			continue;
		Vec p = a.plus(b.minus(a).mult(lo)), q = a.plus(b.minus(a).mult(hi));
		float len = q.minus(p).norm();
		if (len < 1e-3f)
			continue;
		Vec dir = q.minus(p).div(len);
		int dashes = std::min((int) std::ceil(len / period), budget);
		for (int d = 0; d < dashes; d++) {
			float s0 = d * period, s1 = std::min(s0 + on, len);
			nvgMoveTo(vg, p.x + dir.x * s0, p.y + dir.y * s0);
			nvgLineTo(vg, p.x + dir.x * s1, p.y + dir.y * s1);
		}
		budget -= dashes;
	}
	nvgLineCap(vg, NVG_ROUND);
	lStroke(vg, c, width);
}

// Soupir (silence d'une noire), centré en (x, y), haut d'environ 16·k px
static void lRest(NVGcontext* vg, float x, float y, float k, NVGcolor c) {
	nvgBeginPath(vg);
	nvgMoveTo(vg, x - 1.5f * k, y - 9.f * k);
	nvgLineTo(vg, x + 2.5f * k, y - 4.5f * k);
	nvgLineTo(vg, x - 1.5f * k, y - 1.f * k);
	nvgLineTo(vg, x + 2.5f * k, y + 3.f * k);
	nvgQuadTo(vg, x - 2.5f * k, y + 1.5f * k, x, y + 7.5f * k);
	nvgLineCap(vg, NVG_ROUND);
	nvgLineJoin(vg, NVG_ROUND);
	lStroke(vg, c, 1.f);
}


// --- Potards en bakélite noire, index crème

struct LouisKnob : app::Knob {
	LouisKnob() {
		minAngle = -0.75f * M_PI;
		maxAngle = 0.75f * M_PI;
	}

	void draw(const DrawArgs& args) override {
		float v = 0.5f;
		if (ParamQuantity* pq = getParamQuantity())
			v = pq->getScaledValue();
		NVGcontext* vg = args.vg;
		Vec c = box.size.div(2.f);
		float r = c.x - 0.6f;
		nvgBeginPath(vg);
		nvgCircle(vg, c.x, c.y + r * 0.14f, r);
		lFill(vg, nvgRGBA(0, 0, 0, 90));
		NVGpaint face = nvgRadialGradient(vg, c.x - r * 0.3f, c.y - r * 0.4f, r * 0.05f, r * 1.25f, nvgRGB(0x57, 0x52, 0x4c), nvgRGB(0x0b, 0x0a, 0x09));
		nvgBeginPath(vg);
		nvgCircle(vg, c.x, c.y, r);
		nvgFillPaint(vg, face);
		nvgFill(vg);
		lStroke(vg, nvgRGB(0x0b, 0x0a, 0x09), 0.8f);
		float a = math::rescale(v, 0.f, 1.f, minAngle, maxAngle);
		Vec d = Vec(std::sin(a), -std::cos(a));
		nvgBeginPath(vg);
		nvgMoveTo(vg, c.x + d.x * r * 0.25f, c.y + d.y * r * 0.25f);
		nvgLineTo(vg, c.x + d.x * r * 0.84f, c.y + d.y * r * 0.84f);
		nvgLineCap(vg, NVG_ROUND);
		lStroke(vg, L_CREAM, std::max(1.4f, r * 0.13f));
		Knob::draw(args);
	}
};


// --- HOLD : bouton à verrou, voyant vert phosphore

struct LouisHold : app::Switch {
	LouisHold() {
		momentary = false;
	}

	void draw(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		bool on = getParamQuantity() && getParamQuantity()->getValue() > 0.5f;
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0.6f, 0.6f, box.size.x - 1.2f, box.size.y - 1.2f, 2.f);
		lFill(vg, on ? L_INK : nvgRGBA(0, 0, 0, 0));
		lStroke(vg, L_INK, 1.f);
		Switch::draw(args);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			bool on = getParamQuantity() && getParamQuantity()->getValue() > 0.5f;
			lGlowDot(args.vg, box.size.x / 2, box.size.y / 2, 2.f, L_PHOS, on ? 1.f : 0.f);
		}
		Switch::drawLayer(args, layer);
	}
};


// --- Les trois modes de LINK : le nom, et l'effet écrit dessous

struct LouisModeButton : OpaqueWidget {
	Louis* module = NULL;
	int mode = 0;

	int current() {
		return module ? (int) std::round(module->params[Louis::MODE_PARAM].getValue()) : 0;
	}

	void draw(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		bool sel = current() == mode;
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 2.f);
		lFill(vg, sel ? L_INK : nvgRGBA(0, 0, 0, 0));
		lStroke(vg, L_INK, 1.f);
		NVGcolor ink = sel ? L_IVORY : L_INK;
		float x = mm2px(4.2f);
		lText(vg, x, box.size.y * 0.31f, L_MODE_NAMES[mode], 6.4f, ink, NVG_ALIGN_LEFT, 1.0f, ll::FONT_BOLD);
		lText(vg, x, box.size.y * 0.72f, L_MODE_VERBS[mode], 5.6f, ink, NVG_ALIGN_LEFT, 0.1f, ll::FONT_REG);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			lGlowDot(args.vg, mm2px(2.2f), box.size.y / 2, 1.8f, L_PHOS, current() == mode ? 1.f : 0.f);
		OpaqueWidget::drawLayer(args, layer);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			ParamQuantity* pq = module->paramQuantities[Louis::MODE_PARAM];
			float old = pq->getValue();
			pq->setValue(mode);
			if (old != (float) mode) {
				history::ParamChange* h = new history::ParamChange;
				h->name = "change link mode";
				h->moduleId = module->id;
				h->paramId = Louis::MODE_PARAM;
				h->oldValue = old;
				h->newValue = mode;
				APP->history->push(h);
			}
			e.consume(this);
		}
		OpaqueWidget::onButton(e);
	}
};


// --- Voyants des dérives : une barre bipolaire

struct LouisDriftMeter : TransparentWidget {
	Louis* module = NULL;
	int which = 0;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		NVGcontext* vg = args.vg;
		Vec s = box.size;
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 0, s.x, s.y, 1.5f);
		lFill(vg, L_GLASS);
		lStroke(vg, nvgRGB(0x3a, 0x37, 0x33), 0.8f);
		float v = module ? (which ? module->out.driftB : module->out.driftA) / 5.f : (which ? -0.3f : 0.5f);
		float half = s.y / 2 - 1.5f;
		float h = std::max(1.f, std::fabs(v) * half);
		float y = v >= 0.f ? s.y / 2 - h : s.y / 2;
		nvgBeginPath(vg);
		nvgRect(vg, 1.2f, y, s.x - 2.4f, h);
		lFill(vg, L_PHOS);
		nvgBeginPath(vg);
		nvgRect(vg, 0.6f, s.y / 2 - 0.3f, s.x - 1.2f, 0.6f);
		lFill(vg, L_PHOS_DARK);
	}
};


// --- Roue des notes : les douze classes de hauteur, C en haut ; la gamme en cours allumée, la dernière note brille

struct LouisWheel : TransparentWidget {
	Louis* module = NULL;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		NVGcontext* vg = args.vg;
		Vec c = box.size.div(2.f);
		float R = c.x - 0.5f;
		nvgBeginPath(vg);
		nvgCircle(vg, c.x, c.y, R);
		lFill(vg, L_GLASS);
		lStroke(vg, L_INK, 1.2f);
		int mask = module ? module->wheelMask : 0x4a9;   // pentatonique mineure par défaut, pour la bibliothèque
		bool hotOn = module && module->now - module->lastPcTime < 0.12;
		for (int pc = 0; pc < 12; pc++) {
			float a = (pc * 30.f - 90.f) * M_PI / 180.f;
			float x = c.x + R * 0.68f * std::cos(a), y = c.y + R * 0.68f * std::sin(a);
			bool in = mask == 0 || ((mask >> pc) & 1);
			bool hot = hotOn && pc == module->lastPc;
			nvgBeginPath(vg);
			nvgCircle(vg, x, y, hot ? 2.6f : (in ? 1.8f : 1.f));
			lFill(vg, hot ? L_PHOS_LIGHT : (in ? (mask ? L_PHOS : L_PHOS_DIM) : L_PHOS_DARK));
		}
	}
};


// --- L'écran : la ligne d'état et la frise des quatre voix

struct LouisScreen : TransparentWidget {
	Louis* module = NULL;
	std::string touched;
	double touchedAt = -1e9;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		NVGcontext* vg = args.vg;
		Vec s = box.size;
		float top = mm2px(1.4f);
		std::string left = "FREE TIMING";
		if (module && module->dispClock)
			left = string::f("CLOCK · GRID %s · SNAP %d %%", louis::GRID_NAMES[clamp((int) module->dispGrid, 0, louis::GRID_COUNT - 1)], (int) module->dispSnap);
		if (system::getTime() - touchedAt < 1.5)
			left = touched;
		std::string right = "MIN PENTA · C";
		if (module)
			right = module->dispExt ? std::string("SCALE IN")
				: std::string(louis::SCALES[clamp((int) module->dispScale, 0, louis::SCALE_COUNT - 1)].name) + " · " + L_NOTE_NAMES[clamp((int) module->dispRoot, 0, 11)];
		lText(vg, mm2px(2.0f), top, left, 4.2f, L_PHOS, NVG_ALIGN_LEFT, 0.6f, ll::FONT_MIC);
		lText(vg, s.x - mm2px(2.0f), top, right, 4.2f, L_PHOS, NVG_ALIGN_RIGHT, 0.6f, ll::FONT_MIC);

		float fx0 = mm2px(4.4f), fx1 = s.x - mm2px(1.6f);
		float fy0 = mm2px(3.6f), fy1 = s.y - mm2px(0.8f);
		float lane = (fy1 - fy0) / louis::VOICES;
		for (int i = 0; i < louis::VOICES; i++)
			lText(vg, mm2px(2.0f), fy0 + lane * (i + 0.5f), string::f("%d", i + 1), 4.0f, L_PHOS_DIM, NVG_ALIGN_LEFT, 0.f, ll::FONT_MIC);
		for (int i = 1; i < louis::VOICES; i++) {
			nvgBeginPath(vg);
			nvgMoveTo(vg, fx0, fy0 + lane * i);
			nvgLineTo(vg, fx1, fy0 + lane * i);
			lStroke(vg, nvgRGB(0x17, 0x3a, 0x26), 0.7f);
		}
		if (!module)
			return;

		double t = module->now;
		auto X = [&](double tt) { return (float) (fx1 - (t - tt) / WINDOW * (fx1 - fx0)); };
		auto dotY = [&](int i, float y) { return fy0 + lane * i + lane - 1.8f - y * (lane - 4.5f); };
		nvgSave(vg);
		nvgScissor(vg, fx0, 0, fx1 - fx0, s.y);

		// Coups d'horloge
		if (module->p.clockConnected) {
			for (int k = 0; k < PULSE_RING; k++) {
				double pt = module->pulses[k];
				if (pt < t - WINDOW)
					continue;
				nvgBeginPath(vg);
				nvgMoveTo(vg, X(pt), fy0);
				nvgLineTo(vg, X(pt), fy1);
				lStroke(vg, nvgRGB(0x1f, 0x52, 0x35), 0.5f);
			}
		}

		// Notes : la silhouette de l'enveloppe, un point à la hauteur ; les silences de HUSH en fantôme
		for (int i = 0; i < louis::VOICES; i++) {
			float yb = fy0 + lane * (i + 1) - 1.2f, yt = fy0 + lane * i + 2.2f;
			for (int k = 0; k < NOTE_RING; k++) {
				const FriseNote& n = module->notes[i][k];
				if (n.t0 + n.a + n.d < t - WINDOW)
					continue;
				float x0 = X(n.t0), xa = X(n.t0 + n.a), xe = X(n.t0 + n.a + n.d);
				if (n.kind == 2) {
					std::vector<Vec> pts;
					for (int q = 0; q <= 8; q++) {
						float u = q / 8.f, v = 1.f - u;
						Vec c1(x0 + (xa - x0) * 0.15f, yt);
						pts.push_back(Vec(v * v * x0 + 2 * v * u * c1.x + u * u * xa, v * v * yb + 2 * v * u * c1.y + u * u * yt));
					}
					for (int q = 1; q <= 10; q++) {
						float u = q / 10.f, v = 1.f - u;
						Vec c2(xa + (xe - xa) * 0.12f, yb);
						pts.push_back(Vec(v * v * xa + 2 * v * u * c2.x + u * u * xe, v * v * yt + 2 * v * u * c2.y + u * u * yb));
					}
					lDashed(vg, pts, 2.f, 2.f, nvgTransRGBA(L_PHOS, 140), 0.9f, fx0 - 4.f, fx1 + 4.f);
					continue;
				}
				float op = n.kind == 1 ? 0.5f : 1.f;
				nvgBeginPath(vg);
				nvgMoveTo(vg, x0, yb);
				nvgQuadTo(vg, x0 + (xa - x0) * 0.15f, yt, xa, yt);
				nvgQuadTo(vg, xa + (xe - xa) * 0.12f, yb, xe, yb);
				lFill(vg, nvgTransRGBAf(L_PHOS, 0.10f * op));
				lStroke(vg, nvgTransRGBAf(L_PHOS, op), 1.f);
				nvgBeginPath(vg);
				nvgCircle(vg, x0, dotY(i, n.y), 1.6f);
				lFill(vg, nvgTransRGBAf(L_PHOS_LIGHT, op));
			}
		}

		// Liens : CHAIN flèche pleine, HUSH pointillés vers un soupir, ECHO pointillés d'une note à celle qui la reprend
		for (int k = 0; k < MARK_RING; k++) {
			const FriseMark& m = module->marks[k];
			if (std::max(m.t, m.t1) < t - WINDOW || m.t < -1e8)
				continue;
			if (m.mode == louis::LINK_ECHO) {
				if (m.t1 < t - 2 * WINDOW)
					continue;
				Vec a(X(m.t1), dotY(m.from, m.y1)), b(X(m.t), dotY(m.to, m.y2));
				lDashed(vg, {a, b}, 1.2f, 2.f, L_PHOS, 0.9f, fx0 - 4.f, fx1 + 4.f);
				for (Vec q : {a, b}) {
					if (q.x < fx0 - 4.f || q.x > fx1 + 4.f)
						continue;
					nvgBeginPath(vg);
					nvgCircle(vg, q.x, q.y, 3.2f);
					lStroke(vg, L_PHOS, 0.9f);
				}
				continue;
			}
			float x = X(m.t);
			bool down = m.to > m.from;
			float y1 = fy0 + lane * (m.from + 0.5f);
			if (m.mode == louis::LINK_CHAIN) {
				float y2 = down ? fy0 + lane * m.to + 2.f : fy0 + lane * (m.to + 1) - 2.f;
				nvgBeginPath(vg);
				nvgMoveTo(vg, x, y1);
				nvgLineTo(vg, x, y2);
				lStroke(vg, L_PHOS, 0.9f);
				float dy = down ? -3.2f : 3.2f;
				nvgBeginPath(vg);
				nvgMoveTo(vg, x - 2.4f, y2 + dy);
				nvgLineTo(vg, x, y2);
				nvgLineTo(vg, x + 2.4f, y2 + dy);
				nvgClosePath(vg);
				lFill(vg, L_PHOS);
			}
			else {
				float yc = fy0 + lane * (m.to + 0.5f);
				lDashed(vg, {Vec(x, y1), Vec(x, down ? yc - 8.f : yc + 8.f)}, 2.f, 2.f, L_PHOS, 0.9f, fx0 - 4.f, fx1 + 4.f);
				lRest(vg, x + 1.f, yc, 0.9f, L_PHOS);
			}
		}
		nvgRestore(vg);

		// L'instant présent
		nvgBeginPath(vg);
		nvgMoveTo(vg, fx1, fy0);
		nvgLineTo(vg, fx1, fy1);
		lStroke(vg, nvgTransRGBA(L_PHOS, 120), 1.f);
	}
};


// --- Étiquettes fixes, en IBM Plex Sans ou en Michroma

struct LouisLabel : TransparentWidget {
	ll::Label l;

	void draw(const DrawArgs& args) override {
		lText(args.vg, 0.f, 0.f, l.text, l.size, l.color, l.align, l.spacing, l.font);
	}
};


struct PhosphorLight : GrayModuleLightWidget {
	PhosphorLight() {
		addBaseColor(L_PHOS);
		bgColor = L_LED_OFF;
		borderColor = nvgRGBA(0x1d, 0x1b, 0x17, 0xc0);
	}
};


struct LouisWidget : ModuleWidget {
	LouisScreen* screen = NULL;
	float lastValues[Louis::PARAMS_LEN];
	bool seen = false;

	template <class T>
	T* place(T* w, float cxMm, float cyMm, float wMm, float hMm) {
		w->box.size = mm2px(Vec(wMm, hMm));
		w->box.pos = mm2px(Vec(cxMm, cyMm)).minus(w->box.size.div(2.f));
		return w;
	}

	void knob(Louis* module, int id, float x, float y, float r) {
		LouisKnob* k = createParam<LouisKnob>(Vec(), module, id);
		place(k, x, y, 2 * r, 2 * r);
		addParam(k);
	}

	void in(Louis* module, int id, float x, float y) {
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, y)), module, id));
	}

	void outJack(Louis* module, int id, float x, float y) {
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, y)), module, id));
	}

	LouisWidget(Louis* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Louis.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		for (const ll::Label& l : ll::LABELS) {
			LouisLabel* w = createWidget<LouisLabel>(mm2px(Vec(l.x, l.y)));
			w->l = l;
			addChild(w);
		}

		screen = createWidget<LouisScreen>(mm2px(Vec(ll::SCREEN[0] + 0.4f, ll::SCREEN[1] + 0.4f)));
		screen->box.size = mm2px(Vec(ll::SCREEN[2] - ll::SCREEN[0] - 0.8f, ll::SCREEN[3] - ll::SCREEN[1] - 0.8f));
		screen->module = module;
		addChild(screen);

		// CLIMATE : potard et entrée CV par colonne, puis les deux dérives
		for (int k = 0; k < 8; k++) {
			knob(module, Louis::PACE_PARAM + k, ll::CLIM_X[k], ll::KNOB_Y, k == 0 ? ll::PACE_R : ll::KNOB_R);
			in(module, Louis::PACE_INPUT + k, ll::CLIM_X[k], ll::JACK_Y);
		}
		for (int k = 0; k < 2; k++) {
			LouisDriftMeter* m = place(createWidget<LouisDriftMeter>(Vec()), ll::DRIFT_X[k], ll::KNOB_Y, ll::METER_W, ll::METER_H);
			m->module = module;
			m->which = k;
			addChild(m);
			outJack(module, Louis::DRIFT_A_OUTPUT + k, ll::DRIFT_X[k], ll::JACK_Y);
		}

		// PITCH
		static const int PITCH_PARAMS[4] = {Louis::SCALE_PARAM, Louis::ROOT_PARAM, Louis::RANGE_PARAM, Louis::SKEW_PARAM};
		static const int PITCH_INPUTS[5] = {Louis::SCALE_INPUT, Louis::ROOT_INPUT, Louis::RANGE_INPUT, Louis::SKEW_INPUT, Louis::SCALE_IN_INPUT};
		for (int k = 0; k < 4; k++)
			knob(module, PITCH_PARAMS[k], ll::PITCH_X[k], ll::KNOB_Y, ll::KNOB_R);
		for (int k = 0; k < 5; k++)
			in(module, PITCH_INPUTS[k], ll::PITCH_X[k], ll::JACK_Y);
		LouisWheel* wheel = place(createWidget<LouisWheel>(Vec()), ll::PITCH_X[4], ll::KNOB_Y, 2 * ll::WHEEL_R, 2 * ll::WHEEL_R);
		wheel->module = module;
		addChild(wheel);

		// LINK
		knob(module, Louis::LINK_PARAM, ll::LINK_KNOB[0], ll::LINK_KNOB[1], ll::LINK_KNOB[2]);
		in(module, Louis::LINK_INPUT, ll::LINK_KNOB[0], ll::LINK_JACK_Y);
		for (int m = 0; m < 3; m++) {
			LouisModeButton* b = createWidget<LouisModeButton>(mm2px(Vec(ll::MODE_X0, ll::MODE_Y0 + m * (ll::MODE_H + ll::MODE_GAP))));
			b->box.size = mm2px(Vec(ll::MODE_X1 - ll::MODE_X0, ll::MODE_H));
			b->module = module;
			b->mode = m;
			addChild(b);
		}

		// CLOCK
		knob(module, Louis::GRID_PARAM, ll::GRID_KNOB[0], ll::GRID_KNOB[1], ll::GRID_KNOB[2]);
		knob(module, Louis::SNAP_PARAM, ll::SNAP_KNOB[0], ll::SNAP_KNOB[1], ll::SNAP_KNOB[2]);
		in(module, Louis::GRID_INPUT, ll::GRID_KNOB[0], ll::CLOCK_JACK_Y);
		in(module, Louis::SNAP_INPUT, ll::SNAP_KNOB[0], ll::CLOCK_JACK_Y);
		in(module, Louis::CLOCK_INPUT, ll::CLOCK_IN[0], ll::CLOCK_IN[2]);
		in(module, Louis::RESET_INPUT, ll::RESET_IN[0], ll::RESET_IN[2]);

		// VOICES : réglages, voyant et seize sorties par rangée, puis la rangée ALL
		for (int i = 0; i < louis::VOICES; i++) {
			float y = ll::ROW_Y[i];
			knob(module, Louis::TIME_PARAMS + i, ll::TIME_X, y, ll::SMALL_R);
			knob(module, Louis::OCT_PARAMS + i, ll::OCT_X, y, ll::SMALL_R);
			LouisHold* h = createParam<LouisHold>(Vec(), module, Louis::HOLD_PARAMS + i);
			place(h, ll::HOLD_X, y, ll::HOLD_W, ll::HOLD_H);
			addParam(h);
			addChild(createLightCentered<SmallLight<PhosphorLight>>(mm2px(Vec(ll::LED_X, y)), module, Louis::VOICE_LIGHTS + i));
			static const int OUTS[4] = {Louis::TRIG_OUTPUTS, Louis::GATE_OUTPUTS, Louis::PITCH_OUTPUTS, Louis::ENV_OUTPUTS};
			for (int k = 0; k < 4; k++)
				outJack(module, OUTS[k] + i, ll::OUT_X[k], y);
		}
		for (int k = 0; k < 4; k++)
			outJack(module, Louis::TRIG_POLY_OUTPUT + k, ll::OUT_X[k], ll::ALL_JACK_Y);
		in(module, Louis::STRIKE_INPUT, ll::STRIKE_X, ll::ALL_JACK_Y);
		in(module, Louis::FREEZE_INPUT, ll::HOLD_X, ll::ALL_JACK_Y);
		outJack(module, Louis::ANY_OUTPUT, ll::LED_X, ll::ALL_JACK_Y);
		addChild(createLightCentered<SmallLight<PhosphorLight>>(mm2px(Vec(ll::LED_X + 3.6f, ll::ALL_JACK_Y - 5.0f)), module, Louis::ANY_LIGHT));
	}

	// La ligne d'état montre un instant le potard qu'on vient de tourner
	void step() override {
		ModuleWidget::step();
		Louis* m = getModule<Louis>();
		if (!m)
			return;
		for (int id = 0; id < Louis::PARAMS_LEN; id++) {
			float v = m->params[id].getValue();
			if (seen && v != lastValues[id] && id != Louis::MODE_PARAM && !(id >= Louis::HOLD_PARAMS && id < Louis::HOLD_PARAMS + louis::VOICES)) {
				ParamQuantity* pq = m->paramQuantities[id];
				std::string name = string::uppercase(pq->getLabel());
				size_t paren = name.find(" (");
				if (paren != std::string::npos)
					name = name.substr(0, paren);
				screen->touched = name + " · " + string::uppercase(pq->getDisplayValueString() + pq->getUnit());
				screen->touchedAt = system::getTime();
			}
			lastValues[id] = v;
		}
		seen = true;
	}

	void appendContextMenu(Menu* menu) override {
		Louis* m = getModule<Louis>();
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Envelope shape", {"Analog", "Linear"},
			[=]() { return m->p.linearEnv ? 1 : 0; },
			[=](int k) { m->p.linearEnv = k == 1; }));
		menu->addChild(createIndexSubmenuItem("Loop length", {"4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"},
			[=]() { return m->p.loopLength - 4; },
			[=](int k) { m->p.loopLength = k + 4; }));
		menu->addChild(createIndexSubmenuItem("When the clock stops", {"Wait", "Free timing"},
			[=]() { return m->p.clockStopFree ? 1 : 0; },
			[=](int k) { m->p.clockStopFree = k == 1; }));
	}
};


Model* modelLouis = createModel<Louis, LouisWidget>("Louis");
