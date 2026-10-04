#pragma once
// Le module Gaston, partagé avec Gastounet (son expander de gauche) : la structure du module, les messages entre les
// deux modules, les couleurs et les petits outils de dessin.
#include "plugin.hpp"
#include "widgets.hpp"
#include "GastonCore.hpp"

using namespace gaston;

static const NVGcolor G_BRASS = nvgRGB(0xc8, 0xa2, 0x5c);
static const NVGcolor G_BRASS_LIGHT = nvgRGB(0xec, 0xd2, 0x9a);
static const NVGcolor G_BRASS_EDGE = nvgRGB(0x8a, 0x73, 0x47);
static const NVGcolor G_BRASS_DIM = nvgRGB(0x6e, 0x5d, 0x3c);
static const NVGcolor G_GLOW = nvgRGB(0xf6, 0xdf, 0xa8);
static const NVGcolor G_IVORY = nvgRGB(0xec, 0xe3, 0xcf);
static const NVGcolor G_LABEL = nvgRGB(0xd6, 0xc9, 0xaa);
static const NVGcolor G_LABEL_DIM = nvgRGB(0xb3, 0xa6, 0x87);
static const NVGcolor G_NUM = nvgRGB(0x8f, 0x84, 0x68);
static const NVGcolor G_VERD = nvgRGB(0x74, 0xc2, 0xb2);
static const NVGcolor G_VERD_LIGHT = nvgRGB(0x9f, 0xe0, 0xd2);
static const NVGcolor G_VERD_GLOW = nvgRGB(0xa8, 0xea, 0xdb);
static const NVGcolor G_VERD_DIM = nvgRGB(0x2e, 0x56, 0x50);
static const NVGcolor G_VERD_EDGE = nvgRGB(0x3f, 0x7a, 0x70);
static const NVGcolor G_INK = nvgRGB(0x14, 0x1a, 0x2a);
static const NVGcolor G_NAVY = nvgRGB(0x0e, 0x16, 0x28);
static const NVGcolor G_CELL = nvgRGB(0x0a, 0x10, 0x1c);
static const NVGcolor G_CELL_EDGE = nvgRGB(0x3d, 0x35, 0x26);
static const NVGcolor G_REC = nvgRGB(0xe0, 0x60, 0x3f);

// Bémols, comme sur l'afficheur d'Ernest
static const char* const NOTE_NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};


// Presse-papiers partagé entre tous les Gaston du patch
struct LaneClip {
	bool valid = false;
	Lane lane;
};
struct PageClip {
	bool valid = false;
	bool cv = false;
	bool on[PAGE];
	float prob[PAGE], value[PAGE];
};
static LaneClip laneClip;
static PageClip pageClip;


// --- Messages entre Gastounet (collé à gauche) et Gaston. Gastounet envoie l'état de ses boutons et de ses prises à
// chaque échantillon ; Gaston, qui porte les mémoires et le morceau, renvoie la sortie END.

enum GnButton {
	GN_SLOT0,
	GN_BANK0 = GN_SLOT0 + BANK_SIZE,
	GN_WRITE = GN_BANK0 + BANKS,
	GN_COPY,
	GN_NEXT,
	GN_RANDOM,
	GN_PATTERN,
	GN_SONG,
	GN_SONGREC,
	GN_END,
	GN_QUANT0,
	GN_RESTART = GN_QUANT0 + QUANTS,
	GN_LEGATO,
	GN_ROW_ADD,
	GN_ROW_DUP,
	GN_ROW_DEL,
	GN_BUTTONS
};

struct GastounetControls {
	bool valid = false;
	float buttons[GN_BUTTONS] = {};
	bool cvConnected = false;
	float cv = 0.f, next = 0.f, random = 0.f, reset = 0.f;
};

struct GastonFeedback {
	float end = 0.f;
};

// Une voie en JSON compact, pour les 64 mémoires : probabilités sur 8 bits, tensions sur 16 bits, en hexadécimal
static json_t* laneToJsonCompact(const Lane& l) {
	json_t* j = json_object();
	json_object_set_new(j, "length", json_integer(l.length));
	json_object_set_new(j, "ratio", json_integer(l.ratio));
	json_object_set_new(j, "dir", json_integer(l.dir));
	std::string on(STEPS, '0'), data;
	for (int k = 0; k < STEPS; k++) {
		on[k] = l.on[k] ? '1' : '0';
		if (l.cv)
			data += string::f("%04x", (int) std::round((clamp(l.value[k], -1.f, 1.f) + 1.f) / 2.f * 65535.f));
		else
			data += string::f("%02x", (int) std::round(clamp(l.prob[k], 0.f, 1.f) * 255.f));
	}
	json_object_set_new(j, "on", json_string(on.c_str()));
	json_object_set_new(j, l.cv ? "value" : "prob", json_string(data.c_str()));
	if (l.cv) {
		json_object_set_new(j, "unipolar", json_boolean(l.unipolar));
		json_object_set_new(j, "quantize", json_boolean(l.quantize));
		json_object_set_new(j, "slew", json_real(l.slew));
	}
	return j;
}

static void laneFromJsonCompact(Lane& l, json_t* j) {
	l.clear();
	if (json_t* v = json_object_get(j, "length"))
		l.length = clamp((int) json_integer_value(v), 1, STEPS);
	if (json_t* v = json_object_get(j, "ratio"))
		l.ratio = clamp((int) json_integer_value(v), 0, RATIOS - 1);
	if (json_t* v = json_object_get(j, "dir"))
		l.dir = clamp((int) json_integer_value(v), 0, DIRS - 1);
	if (json_t* v = json_object_get(j, "on")) {
		std::string s = json_string_value(v) ? json_string_value(v) : "";
		for (int k = 0; k < STEPS && k < (int) s.size(); k++)
			l.on[k] = s[k] == '1';
	}
	json_t* d = json_object_get(j, l.cv ? "value" : "prob");
	std::string s = d && json_string_value(d) ? json_string_value(d) : "";
	int w = l.cv ? 4 : 2;
	for (int k = 0; k < STEPS && (k + 1) * w <= (int) s.size(); k++) {
		long x = std::strtol(s.substr(k * w, w).c_str(), NULL, 16);
		if (l.cv)
			l.value[k] = clamp(x / 65535.f * 2.f - 1.f, -1.f, 1.f);
		else
			l.prob[k] = clamp(x / 255.f, 0.01f, 1.f);
	}
	if (l.cv) {
		if (json_t* v = json_object_get(j, "unipolar"))
			l.unipolar = json_boolean_value(v);
		if (json_t* v = json_object_get(j, "quantize"))
			l.quantize = json_boolean_value(v);
		if (json_t* v = json_object_get(j, "slew"))
			l.slew = clamp((float) json_number_value(v), 0.f, 1.f);
	}
}


struct Gaston : Module {
	enum ParamId {
		TEMPO_PARAM,
		SWING_PARAM,
		PLAY_PARAM,
		CUE_PARAM,
		REC_PARAM,
		ENUMS(SELECT_PARAMS, LANES),
		ENUMS(PAD_PARAMS, TRIG_LANES),
		ENUMS(MUTE_PARAMS, TRIG_LANES),
		LENGTH_PARAM,
		DIVISION_PARAM,
		SLEW_PARAM,
		DIR_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		PLAY_INPUT,
		RESET_INPUT,
		PADS_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(TRIG_OUTPUTS, TRIG_LANES),
		POLY_OUTPUT,
		ENUMS(CV_OUTPUTS, CV_LANES),
		CLOCK_OUTPUT,
		RUN_OUTPUT,
		RESET_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Core core;
	Settings st;
	int selected = 0;
	int page = 0;

	// Les potards de voie suivent la voie choisie : ils sautent à sa valeur quand on en change
	int syncedLane = -1, lastLen = -1, lastRatio = -1;
	float lastSlew = -1.f;
	int syncCounter = 0;

	dsp::BooleanTrigger playButton, dirButton, selectButtons[LANES], padButtons[TRIG_LANES];
	bool cueWasDown = false;
	dsp::SchmittTrigger clockIn, playIn, resetIn, padsIn[TRIG_LANES];

	// Gastounet
	GastounetControls gnMsg[2];
	dsp::BooleanTrigger gnButtons[GN_BUTTONS];
	dsp::SchmittTrigger gnNext, gnRandom, gnReset;
	bool gnConnected = false;

	Gaston() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		ParamQuantity* q = configParam(TEMPO_PARAM, 30.f, 300.f, 120.f, "Tempo", " BPM");
		q->randomizeEnabled = false;
		q = configParam(SWING_PARAM, 50.f, 75.f, 50.f, "Swing (delays the second 16th of each pair)", " %");
		q->randomizeEnabled = false;
		configButton(PLAY_PARAM, "Play / pause");
		configButton(CUE_PARAM, "Cue (stopped: hold to play from the start; playing: back to the start and stop)");
		q = configSwitch(REC_PARAM, 0.f, 1.f, 0.f, "Pad recording", {"Off", "Armed"});
		q->randomizeEnabled = false;
		for (int i = 0; i < LANES; i++)
			configButton(SELECT_PARAMS + i, i < TRIG_LANES ? string::f("Select track %d", i + 1) : string::f("Select CV track %c", 'A' + i - TRIG_LANES));
		for (int i = 0; i < TRIG_LANES; i++) {
			configButton(PAD_PARAMS + i, string::f("Track %d pad", i + 1));
			q = configSwitch(MUTE_PARAMS + i, 0.f, 1.f, 0.f, string::f("Track %d mute", i + 1), {"Playing", "Muted"});
			q->randomizeEnabled = false;
		}
		q = configParam(LENGTH_PARAM, 1.f, (float) STEPS, 16.f, "Selected track length (last step)", " steps");
		q->snapEnabled = true;
		q->randomizeEnabled = false;
		std::vector<std::string> ratios;
		for (int r = 0; r < RATIOS; r++)
			ratios.push_back(RATIO_NAMES[r]);
		q = configSwitch(DIVISION_PARAM, 0.f, RATIOS - 1, RATIO_X1, "Selected track clock ratio", ratios);
		q->randomizeEnabled = false;
		q = configParam(SLEW_PARAM, 0.f, 1.f, 0.f, "Selected CV track slew", "%", 0.f, 100.f);
		q->randomizeEnabled = false;
		configButton(DIR_PARAM, "Selected track direction (forward, reverse, ping-pong, random)");

		configInput(CLOCK_INPUT, "External clock (replaces the tempo; resolution in the menu)");
		configInput(PLAY_INPUT, "Play / pause (a trigger toggles)");
		configInput(RESET_INPUT, "Back to the start without stopping");
		configInput(PADS_INPUT, "Pads (poly: channel N plays and records like pad N)");
		for (int i = 0; i < TRIG_LANES; i++)
			configOutput(TRIG_OUTPUTS + i, string::f("Track %d", i + 1));
		configOutput(POLY_OUTPUT, "All eight trigger tracks (poly)");
		for (int c = 0; c < CV_LANES; c++)
			configOutput(CV_OUTPUTS + c, string::f("CV track %c", 'A' + c));
		configOutput(CLOCK_OUTPUT, "Clock (swung; resolution in the menu)");
		configOutput(RUN_OUTPUT, "Run (gate)");
		configOutput(RESET_OUTPUT, "Reset (back to the start)");

		core.rng.seed(random::u64());
		core.rewind(st);
		leftExpander.producerMessage = &gnMsg[0];
		leftExpander.consumerMessage = &gnMsg[1];
	}

	Lane& lane() { return core.lanes[selected]; }

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		core = Core();
		core.rng.seed(random::u64());
		st = Settings();
		selected = 0;
		page = 0;
		syncedLane = -1;
		core.rewind(st);
	}

	void syncLaneKnobs() {
		Lane& l = lane();
		if (syncedLane != selected) {
			params[LENGTH_PARAM].setValue(l.length);
			params[DIVISION_PARAM].setValue(l.ratio);
			params[SLEW_PARAM].setValue(l.slew);
			lastLen = l.length;
			lastRatio = l.ratio;
			lastSlew = l.slew;
			syncedLane = selected;
			return;
		}
		int len = (int) std::round(params[LENGTH_PARAM].getValue());
		if (len != lastLen) {
			l.length = clamp(len, 1, STEPS);
			lastLen = len;
		}
		else if (l.length != lastLen) {
			params[LENGTH_PARAM].setValue(l.length);
			lastLen = l.length;
		}
		int ratio = (int) std::round(params[DIVISION_PARAM].getValue());
		if (ratio != lastRatio) {
			l.ratio = clamp(ratio, 0, RATIOS - 1);
			lastRatio = ratio;
		}
		else if (l.ratio != lastRatio) {
			params[DIVISION_PARAM].setValue(l.ratio);
			lastRatio = l.ratio;
		}
		float slew = params[SLEW_PARAM].getValue();
		if (slew != lastSlew) {
			l.slew = slew;
			lastSlew = slew;
		}
		else if (l.slew != lastSlew) {
			params[SLEW_PARAM].setValue(l.slew);
			lastSlew = l.slew;
		}
	}

	void process(const ProcessArgs& args) override {
		st.bpm = params[TEMPO_PARAM].getValue();
		st.swing = params[SWING_PARAM].getValue() / 100.f;
		st.extClock = inputs[CLOCK_INPUT].isConnected();
		st.rec = params[REC_PARAM].getValue() > 0.5f;
		for (int i = 0; i < TRIG_LANES; i++)
			st.muted[i] = params[MUTE_PARAMS + i].getValue() > 0.5f;

		for (int i = 0; i < LANES; i++)
			if (selectButtons[i].process(params[SELECT_PARAMS + i].getValue() > 0.5f))
				selected = i;
		if (++syncCounter >= 32) {
			syncCounter = 0;
			syncLaneKnobs();
		}
		if (dirButton.process(params[DIR_PARAM].getValue() > 0.5f))
			lane().dir = (lane().dir + 1) % DIRS;

		// Transport
		if (playButton.process(params[PLAY_PARAM].getValue() > 0.5f))
			core.togglePlay();
		if (playIn.process(inputs[PLAY_INPUT].getVoltage(), 0.1f, 1.f))
			core.togglePlay();
		bool cueDown = params[CUE_PARAM].getValue() > 0.5f;
		if (cueDown && !cueWasDown)
			core.cuePress(st);
		if (!cueDown && cueWasDown)
			core.cueRelease(st);
		cueWasDown = cueDown;
		if (resetIn.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f))
			core.resetKeepPlaying(st);
		if (st.extClock && clockIn.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f))
			core.clockPulse(st);

		// Pads, à la main ou par PADS IN
		int padChannels = inputs[PADS_INPUT].getChannels();
		for (int i = 0; i < TRIG_LANES; i++) {
			bool hit = padButtons[i].process(params[PAD_PARAMS + i].getValue() > 0.5f);
			if (i < padChannels && padsIn[i].process(inputs[PADS_INPUT].getVoltage(i), 0.1f, 1.f))
				hit = true;
			if (hit)
				core.pad(i, st);
		}

		// Gastounet, collé à gauche : ses boutons et ses prises arrivent par message
		Module* gn = leftExpander.module;
		gnConnected = gn && gn->model == modelGastounet;
		core.memOn = gnConnected;
		if (gnConnected)
			handleGastounet(*(GastounetControls*) leftExpander.consumerMessage);

		Outputs o;
		core.process(args.sampleTime, st, o);

		if (gnConnected) {
			GastonFeedback* fb = (GastonFeedback*) gn->rightExpander.producerMessage;
			if (fb) {
				fb->end = o.end;
				gn->rightExpander.requestMessageFlip();
			}
		}

		for (int i = 0; i < TRIG_LANES; i++) {
			outputs[TRIG_OUTPUTS + i].setVoltage(o.trig[i]);
			outputs[POLY_OUTPUT].setVoltage(o.trig[i], i);
		}
		outputs[POLY_OUTPUT].setChannels(TRIG_LANES);
		for (int c = 0; c < CV_LANES; c++)
			outputs[CV_OUTPUTS + c].setVoltage(o.cv[c]);
		outputs[CLOCK_OUTPUT].setVoltage(o.clock);
		outputs[RUN_OUTPUT].setVoltage(o.run);
		outputs[RESET_OUTPUT].setVoltage(o.reset);
	}

	void handleGastounet(const GastounetControls& c) {
		if (!c.valid)
			return;
		Memory& m = core.mem;
		for (int b = 0; b < GN_BUTTONS; b++) {
			if (!gnButtons[b].process(c.buttons[b] > 0.5f))
				continue;
			if (b < GN_BANK0)
				core.pressSlot(b - GN_SLOT0);
			else if (b < GN_WRITE)
				m.bank = b - GN_BANK0;
			else if (b == GN_WRITE) {
				m.armed = m.armed == ARM_WRITE ? ARM_NONE : ARM_WRITE;
				m.copySrc = -1;
			}
			else if (b == GN_COPY) {
				m.armed = m.armed == ARM_COPY ? ARM_NONE : ARM_COPY;
				m.copySrc = -1;
			}
			else if (b == GN_NEXT)
				core.nextSlot();
			else if (b == GN_RANDOM)
				core.randomSlot();
			else if (b == GN_PATTERN)
				core.setSongMode(false);
			else if (b == GN_SONG)
				core.setSongMode(true);
			else if (b == GN_SONGREC) {
				m.songRec = !m.songRec;
				if (m.songRec)
					core.setSongMode(true);
			}
			else if (b == GN_END)
				m.songLoop = !m.songLoop;
			else if (b < GN_RESTART)
				m.quant = b - GN_QUANT0;
			else if (b == GN_RESTART)
				m.legato = false;
			else if (b == GN_LEGATO)
				m.legato = true;
			else if (b == GN_ROW_ADD)
				core.addRow();
			else if (b == GN_ROW_DUP)
				core.duplicateRow();
			else if (b == GN_ROW_DEL)
				core.removeRow();
		}
		if (c.cvConnected)
			core.cvSelect(c.cv);
		else
			m.cvSlot = -1;
		if (gnNext.process(c.next, 0.1f, 1.f))
			core.nextSlot();
		if (gnRandom.process(c.random, 0.1f, 1.f))
			core.randomSlot();
		if (gnReset.process(c.reset, 0.1f, 1.f))
			core.songReset();
	}

	json_t* memoryToJson() {
		const Memory& m = core.mem;
		json_t* mj = json_object();
		json_object_set_new(mj, "active", json_integer(m.active));
		json_object_set_new(mj, "bank", json_integer(m.bank));
		json_object_set_new(mj, "songMode", json_boolean(m.songMode));
		json_object_set_new(mj, "songLoop", json_boolean(m.songLoop));
		json_object_set_new(mj, "quant", json_integer(m.quant));
		json_object_set_new(mj, "legato", json_boolean(m.legato));
		json_t* song = json_array();
		for (int r = 0; r < m.songLen; r++) {
			json_t* row = json_array();
			json_array_append_new(row, json_integer(m.song[r].pattern));
			json_array_append_new(row, json_integer(m.song[r].bars));
			json_array_append_new(song, row);
		}
		json_object_set_new(mj, "song", song);
		json_t* slots = json_array();
		for (int i = 0; i < SLOTS; i++) {
			if (!m.slots[i].filled)
				continue;
			json_t* sj = json_object();
			json_object_set_new(sj, "slot", json_integer(i));
			json_t* lanesJ = json_array();
			for (int l = 0; l < LANES; l++)
				json_array_append_new(lanesJ, laneToJsonCompact(m.slots[i].lanes[l]));
			json_object_set_new(sj, "lanes", lanesJ);
			json_array_append_new(slots, sj);
		}
		json_object_set_new(mj, "slots", slots);
		return mj;
	}

	void memoryFromJson(json_t* mj) {
		Memory& m = core.mem;
		m = Memory();
		m.active = clamp(jsonInt(mj, "active", 0), 0, SLOTS - 1);
		m.bank = clamp(jsonInt(mj, "bank", 0), 0, BANKS - 1);
		if (json_t* j = json_object_get(mj, "songMode"))
			m.songMode = json_boolean_value(j);
		if (json_t* j = json_object_get(mj, "songLoop"))
			m.songLoop = json_boolean_value(j);
		m.quant = clamp(jsonInt(mj, "quant", Q_BAR), 0, QUANTS - 1);
		if (json_t* j = json_object_get(mj, "legato"))
			m.legato = json_boolean_value(j);
		json_t* song = json_object_get(mj, "song");
		for (int r = 0; song && r < (int) json_array_size(song) && r < SONG_ROWS; r++) {
			json_t* row = json_array_get(song, r);
			m.song[r].pattern = clamp((int) json_integer_value(json_array_get(row, 0)), 0, SLOTS - 1);
			m.song[r].bars = clamp((int) json_integer_value(json_array_get(row, 1)), 1, 64);
			m.songLen = r + 1;
		}
		json_t* slots = json_object_get(mj, "slots");
		for (int k = 0; slots && k < (int) json_array_size(slots); k++) {
			json_t* sj = json_array_get(slots, k);
			int i = clamp(jsonInt(sj, "slot", 0), 0, SLOTS - 1);
			Pattern& pt = m.slots[i];
			pt = Pattern();
			pt.filled = true;
			json_t* lanesJ = json_object_get(sj, "lanes");
			for (int l = 0; lanesJ && l < LANES && l < (int) json_array_size(lanesJ); l++)
				laneFromJsonCompact(pt.lanes[l], json_array_get(lanesJ, l));
		}
	}

	// --- Sauvegarde : tout, sauf l'état de lecture (au chargement, Gaston attend calé au début)

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "selected", json_integer(selected));
		json_object_set_new(root, "page", json_integer(page));
		json_object_set_new(root, "clockInRes", json_integer(st.clockInRes));
		json_object_set_new(root, "clockOutRes", json_integer(st.clockOutRes));
		json_object_set_new(root, "vintage", json_boolean(st.vintage));
		json_object_set_new(root, "realign", json_integer(st.realign));
		json_t* lanesJ = json_array();
		for (int i = 0; i < LANES; i++) {
			const Lane& l = core.lanes[i];
			json_t* lj = json_object();
			json_object_set_new(lj, "length", json_integer(l.length));
			json_object_set_new(lj, "ratio", json_integer(l.ratio));
			json_object_set_new(lj, "dir", json_integer(l.dir));
			json_t* values = json_array();
			if (!l.cv) {
				std::string on(STEPS, '0');
				for (int s = 0; s < STEPS; s++) {
					on[s] = l.on[s] ? '1' : '0';
					json_array_append_new(values, json_real(l.prob[s]));
				}
				json_object_set_new(lj, "on", json_string(on.c_str()));
				json_object_set_new(lj, "prob", values);
			}
			else {
				std::string on(STEPS, '0');
				for (int s = 0; s < STEPS; s++) {
					on[s] = l.on[s] ? '1' : '0';
					json_array_append_new(values, json_real(l.value[s]));
				}
				json_object_set_new(lj, "on", json_string(on.c_str()));
				json_object_set_new(lj, "value", values);
				json_object_set_new(lj, "unipolar", json_boolean(l.unipolar));
				json_object_set_new(lj, "quantize", json_boolean(l.quantize));
				json_object_set_new(lj, "slew", json_real(l.slew));
			}
			json_array_append_new(lanesJ, lj);
		}
		json_object_set_new(root, "lanes", lanesJ);
		json_object_set_new(root, "memory", memoryToJson());
		return root;
	}

	static int jsonInt(json_t* o, const char* key, int def) {
		json_t* j = json_object_get(o, key);
		return j ? (int) json_integer_value(j) : def;
	}

	void dataFromJson(json_t* root) override {
		selected = clamp(jsonInt(root, "selected", 0), 0, LANES - 1);
		page = clamp(jsonInt(root, "page", 0), 0, PAGES - 1);
		st.clockInRes = clamp(jsonInt(root, "clockInRes", 0), 0, CLOCK_RES - 1);
		st.clockOutRes = clamp(jsonInt(root, "clockOutRes", 0), 0, CLOCK_RES - 1);
		if (json_t* j = json_object_get(root, "vintage"))
			st.vintage = json_boolean_value(j);
		st.realign = clamp(jsonInt(root, "realign", 0), 0, REALIGNS - 1);
		json_t* lanesJ = json_object_get(root, "lanes");
		for (int i = 0; lanesJ && i < LANES && i < (int) json_array_size(lanesJ); i++) {
			json_t* lj = json_array_get(lanesJ, i);
			Lane& l = core.lanes[i];
			l.clear();
			l.unipolar = false;
			l.quantize = false;
			l.slew = 0.f;
			l.length = clamp(jsonInt(lj, "length", 16), 1, STEPS);
			l.ratio = clamp(jsonInt(lj, "ratio", RATIO_X1), 0, RATIOS - 1);
			l.dir = clamp(jsonInt(lj, "dir", FORWARD), 0, DIRS - 1);
			if (!l.cv) {
				if (json_t* on = json_object_get(lj, "on")) {
					std::string s = json_string_value(on) ? json_string_value(on) : "";
					for (int k = 0; k < STEPS && k < (int) s.size(); k++)
						l.on[k] = s[k] == '1';
				}
				json_t* prob = json_object_get(lj, "prob");
				for (int k = 0; prob && k < STEPS && k < (int) json_array_size(prob); k++)
					l.prob[k] = clamp((float) json_number_value(json_array_get(prob, k)), 0.01f, 1.f);
			}
			else {
				if (json_t* on = json_object_get(lj, "on")) {
					std::string s = json_string_value(on) ? json_string_value(on) : "";
					for (int k = 0; k < STEPS && k < (int) s.size(); k++)
						l.on[k] = s[k] == '1';
				}
				json_t* value = json_object_get(lj, "value");
				for (int k = 0; value && k < STEPS && k < (int) json_array_size(value); k++)
					l.value[k] = clamp((float) json_number_value(json_array_get(value, k)), -1.f, 1.f);
				if (json_t* j = json_object_get(lj, "unipolar"))
					l.unipolar = json_boolean_value(j);
				if (json_t* j = json_object_get(lj, "quantize"))
					l.quantize = json_boolean_value(j);
				if (json_t* j = json_object_get(lj, "slew"))
					l.slew = clamp((float) json_number_value(j), 0.f, 1.f);
			}
		}
		if (json_t* mj = json_object_get(root, "memory"))
			memoryFromJson(mj);
		core.playing = false;
		core.cueHeld = false;
		core.rewind(st);
		syncedLane = -1;
	}

	// --- Outils du menu de voie

	void copyLane() {
		laneClip.lane = lane();
		laneClip.valid = true;
	}

	bool canPasteLane() { return laneClip.valid && laneClip.lane.cv == lane().cv; }

	void pasteLane() {
		if (!canPasteLane())
			return;
		Lane& l = lane();
		const Lane& c = laneClip.lane;
		for (int s = 0; s < STEPS; s++) {
			l.on[s] = c.on[s];
			l.prob[s] = c.prob[s];
			l.value[s] = c.value[s];
		}
		l.length = c.length;
		l.ratio = c.ratio;
		l.dir = c.dir;
		l.unipolar = c.unipolar;
		l.quantize = c.quantize;
		l.slew = c.slew;
	}

	void copyPage() {
		const Lane& l = lane();
		pageClip.cv = l.cv;
		for (int k = 0; k < PAGE; k++) {
			int s = page * PAGE + k;
			pageClip.on[k] = l.on[s];
			pageClip.prob[k] = l.prob[s];
			pageClip.value[k] = l.value[s];
		}
		pageClip.valid = true;
	}

	bool canPastePage() { return pageClip.valid && pageClip.cv == lane().cv; }

	void pastePage() {
		if (!canPastePage())
			return;
		Lane& l = lane();
		for (int k = 0; k < PAGE; k++) {
			int s = page * PAGE + k;
			l.on[s] = pageClip.on[k];
			l.prob[s] = pageClip.prob[k];
			l.value[s] = pageClip.value[k];
		}
	}
};


// --- Dessin

// NanoVG prend la taille d'une police pour la hauteur de ligne (ascendante + descendante), pas pour le em : chez
// Michroma, un em vaut 0,70 de cette taille. Les tailles de Gaston et Gastounet sont des em (celles des aperçus et des
// mesures de place des outils de panneau) : on les convertit ici.
static const float G_FONT_SCALE = 1.f / 0.7033f;

static void gText(NVGcontext* vg, float x, float y, const std::string& s, float size, NVGcolor color, int align = NVG_ALIGN_CENTER, float spacing = 0.f) {
	size *= G_FONT_SCALE;
	std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Michroma-Regular.ttf"));
	if (!font)
		return;
	nvgFontFaceId(vg, font->handle);
	nvgFontSize(vg, size);
	nvgTextLetterSpacing(vg, spacing);
	nvgFillColor(vg, color);
	nvgTextAlign(vg, align | NVG_ALIGN_MIDDLE);
	nvgText(vg, x, y, s.c_str(), NULL);
	// Michroma est fine : un second tracé décalé d'un tiers de pixel l'épaissit
	nvgText(vg, x + 0.35f, y, s.c_str(), NULL);
}

static void gRoundRect(NVGcontext* vg, float x, float y, float w, float h, float r) {
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x, y, w, h, std::min(r, std::min(w, h) / 2.f));
}

static void gFill(NVGcontext* vg, NVGcolor c) {
	nvgFillColor(vg, c);
	nvgFill(vg);
}

static void gStroke(NVGcontext* vg, NVGcolor c, float width) {
	nvgStrokeColor(vg, c);
	nvgStrokeWidth(vg, width);
	nvgStroke(vg);
}

// Halo doux autour d'un rectangle (le pas courant, un trig qui part)
static void gGlow(NVGcontext* vg, float x, float y, float w, float h, float r, float spread, NVGcolor c) {
	NVGpaint p = nvgBoxGradient(vg, x, y, w, h, r, spread, c, nvgTransRGBA(c, 0));
	nvgBeginPath(vg);
	nvgRect(vg, x - spread, y - spread, w + 2 * spread, h + 2 * spread);
	nvgFillPaint(vg, p);
	nvgFill(vg);
}

// Flèche du sens de lecture ; « ? » pour le hasard
static void gDirIcon(NVGcontext* vg, float cx, float cy, float w, int dir, NVGcolor c) {
	if (dir == RANDOM) {
		gText(vg, cx, cy, "?", w * 0.9f, c);
		return;
	}
	float x0 = cx - w / 2, x1 = cx + w / 2, h = w * 0.28f;
	nvgBeginPath(vg);
	nvgMoveTo(vg, x0, cy);
	nvgLineTo(vg, x1, cy);
	if (dir == FORWARD || dir == PINGPONG) {
		nvgMoveTo(vg, x1 - h, cy - h);
		nvgLineTo(vg, x1, cy);
		nvgLineTo(vg, x1 - h, cy + h);
	}
	if (dir == BACKWARD || dir == PINGPONG) {
		nvgMoveTo(vg, x0 + h, cy - h);
		nvgLineTo(vg, x0, cy);
		nvgLineTo(vg, x0 + h, cy + h);
	}
	nvgLineCap(vg, NVG_ROUND);
	nvgLineJoin(vg, NVG_ROUND);
	gStroke(vg, c, std::max(1.f, w * 0.1f));
}

static std::string cvText(const Lane& l, float volts) {
	if (l.quantize) {
		int n = (int) std::round(volts * 12.f);
		return string::f("%s%d", NOTE_NAMES[((n % 12) + 12) % 12], 4 + (int) std::floor(n / 12.f));
	}
	return string::f(l.unipolar ? "%.1f" : "%+.1f", volts);
}

// Un pas joue-t-il en ce moment ? (la voie est partie et le transport tourne ou attend sur un CUE)
static bool laneRunning(const Core& c, const Lane& l) {
	return l.count >= 0 && (c.playing || c.pos > 0.0);
}


