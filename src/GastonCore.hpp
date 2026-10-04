#pragma once
// Moteur de Gaston, sans dépendance à Rack : horloge maître (interne ou externe, swing, timing vintage), douze voies
// (huit de trigs avec probabilité par pas, quatre de CV) qui avancent chacune à leur longueur, leur rapport et leur sens,
// transport façon CDJ, pads et enregistrement en direct. Le temps se compte en pas maîtres (doubles croches).
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <string>
#include <cstdio>

namespace gaston {

static const int TRIG_LANES = 8;
static const int CV_LANES = 4;
static const int LANES = TRIG_LANES + CV_LANES;
static const int STEPS = 64;
static const int PAGE = 16;
static const int PAGES = STEPS / PAGE;

// Rapports d'horloge des voies, du plus lent au plus rapide. Les rapports binaires suivent le swing ; les autres
// (triolets, quintolets...) restent droits.
static const int RATIOS = 12;
static const int RATIO_X1 = 5;
static const char* const RATIO_NAMES[RATIOS] = {"÷4", "÷3", "÷2", "x2/3", "x3/4", "x1", "x5/4", "x4/3", "x3/2", "x2", "x3", "x4"};
static const int RATIO_NUM[RATIOS] = {1, 1, 1, 2, 3, 1, 5, 4, 3, 2, 3, 4};
static const int RATIO_DEN[RATIOS] = {4, 3, 2, 3, 4, 1, 4, 3, 2, 1, 1, 1};
static const bool RATIO_BINARY[RATIOS] = {true, false, true, false, false, true, false, false, false, true, false, true};

enum Dir { FORWARD, BACKWARD, PINGPONG, RANDOM, DIRS };
static const char* const DIR_NAMES[DIRS] = {"FWD", "REV", "PING-PONG", "RANDOM"};

// Résolutions d'horloge (impulsions par pas maître) : 4, 8, 24 et 48 PPQN
static const int CLOCK_RES = 4;
static const int CLOCK_PPS[CLOCK_RES] = {1, 2, 6, 12};
static const char* const CLOCK_RES_NAMES[CLOCK_RES] = {"1 per step (4 PPQN)", "2 per step (8 PPQN)", "24 PPQN", "48 PPQN"};

// Réalignement global : toutes les voies reviennent au pas 1 tous les N pas maîtres
static const int REALIGNS = 4;
static const int REALIGN_STEPS[REALIGNS] = {0, 16, 32, 64};

static const float TRIG_TIME = 0.010f;
static const float VINTAGE_TICKS = 24.f;    // 96 PPQN : 24 ticks par double croche
static const float VINTAGE_JITTER = 0.0008f; // un délai de 0 à 0,8 ms par événement, soit ±0,4 ms autour de sa moyenne

inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }

struct Rng {
	uint64_t s = 0x9E3779B97F4A7C15ull;
	void seed(uint64_t v) { s = v * 0x9E3779B97F4A7C15ull + 1; }
	uint32_t next() {
		s ^= s << 13; s ^= s >> 7; s ^= s << 17;
		return (uint32_t) (s >> 32);
	}
	float uniform() { return (next() >> 8) * (1.f / 16777216.f); }
	int below(int n) { return (int) (uniform() * n) % n; }
};

// Swing à la Linn : dans chaque paire de pas, le second commence à la fraction `s` de la paire (0,5 droit, 0,75 maximum).
// Transforme une position droite en position musicale.
inline double swingWarp(double p, double s) {
	double k = std::floor(p / 2.0);
	double u = p - 2.0 * k, a = 2.0 * s;
	double f = u < a ? u / a : 1.0 + (u - a) / (2.0 - a);
	return 2.0 * k + f;
}

struct Lane {
	bool cv = false;
	int length = 16;
	int ratio = RATIO_X1;
	int dir = FORWARD;
	// Voie de trigs : pas allumé ; voie CV : pas actif (un pas inactif garde la tension du dernier pas actif)
	bool on[STEPS];
	float prob[STEPS];
	// Voie CV : -1 à 1. Tension : 5 × valeur (±5 V), ou 5 × (valeur + 1) en 0–10 V.
	float value[STEPS];
	bool unipolar = false;
	bool quantize = false;
	float slew = 0.f;

	// Lecture : `count` pas avancés depuis le départ (-1 : pas encore parti), `pos` le pas courant
	long count = -1;
	int pos = 0;
	// Un pas écrit en avance par REC ne rejoue pas à son passage
	int skipPos = -1;

	Lane() { clear(); }

	void clear() {
		for (int s = 0; s < STEPS; s++) {
			on[s] = cv;
			prob[s] = 1.f;
			value[s] = 0.f;
		}
	}

	float volts(int step) const {
		float v = unipolar ? 5.f * (value[step] + 1.f) : 5.f * value[step];
		if (quantize)
			v = std::round(v * 12.f) / 12.f;
		return v;
	}

	// Le pas joué au `k`-ième avancement (sauf en hasard, tiré à part)
	int positionAt(long k) const {
		int n = std::max(1, length);
		if (k < 0)
			k = 0;
		switch (dir) {
			case BACKWARD: return n - 1 - (int) (k % n);
			case PINGPONG: {
				if (n == 1)
					return 0;
				int m = (int) (k % (2 * n - 2));
				return m < n ? m : 2 * n - 2 - m;
			}
			default: return (int) (k % n);
		}
	}

	// Sens du déplacement en cours : vers la droite ?
	bool movingRight() const {
		if (dir == BACKWARD)
			return false;
		if (dir == PINGPONG) {
			int n = std::max(1, length);
			return n == 1 || (count < 0 ? 0 : (int) (count % (2 * n - 2))) <= n - 1;
		}
		return true;
	}
};

// --- Mémoires (pilotées par Gastounet, l'expander de gauche)
// Les motifs n'ont pas de longueur : c'est le moment du changement qui est quantifié, sur une grille comptée depuis
// le départ du transport (ou à chaque tour de la voie 1). Un morceau enchaîne des lignes « motif + durée en mesures ».

static const int SLOTS = 64;
static const int BANKS = 4;
static const int BANK_SIZE = 16;
static const int SONG_ROWS = 64;
static const int BAR_STEPS = 16;
enum Quant { Q_NOW, Q_BEAT, Q_BAR, Q_2BARS, Q_4BARS, Q_TRACK1, QUANTS };
static const int QUANT_STEPS[QUANTS] = {1, 4, 16, 32, 64, 0};
static const char* const QUANT_NAMES[QUANTS] = {"NOW", "1 BEAT", "1 BAR", "2 BARS", "4 BARS", "TRACK 1"};
enum Armed { ARM_NONE, ARM_WRITE, ARM_COPY };

// Le contenu d'une voie, sans son état de lecture
inline void copyContent(Lane& d, const Lane& s) {
	d.length = s.length;
	d.ratio = s.ratio;
	d.dir = s.dir;
	d.unipolar = s.unipolar;
	d.quantize = s.quantize;
	d.slew = s.slew;
	for (int k = 0; k < STEPS; k++) {
		d.on[k] = s.on[k];
		d.prob[k] = s.prob[k];
		d.value[k] = s.value[k];
	}
}

inline bool sameContent(const Lane& a, const Lane& b) {
	if (a.length != b.length || a.ratio != b.ratio || a.dir != b.dir)
		return false;
	if (a.cv && (a.unipolar != b.unipolar || a.quantize != b.quantize || a.slew != b.slew))
		return false;
	for (int k = 0; k < STEPS; k++) {
		if (a.on[k] != b.on[k])
			return false;
		if (a.cv ? a.value[k] != b.value[k] : (a.on[k] && a.prob[k] != b.prob[k]))
			return false;
	}
	return true;
}

inline std::string slotName(int slot) {
	char buf[8];
	std::snprintf(buf, sizeof(buf), "%c%02d", 'A' + slot / BANK_SIZE, slot % BANK_SIZE + 1);
	return buf;
}

struct Pattern {
	bool filled = false;
	Lane lanes[LANES];
	Pattern() {
		for (int i = 0; i < LANES; i++) {
			lanes[i].cv = i >= TRIG_LANES;
			lanes[i].clear();
		}
	}
};

struct SongRow {
	int pattern = 0;
	int bars = 4;
	SongRow() {}
	SongRow(int pattern, int bars) : pattern(pattern), bars(bars) {}
};

struct Memory {
	std::vector<Pattern> slots = std::vector<Pattern>(SLOTS);
	SongRow song[SONG_ROWS];
	int songLen = 0;
	int active = 0;          // le motif chargé dans Gaston
	int queued = -1;         // le motif qui attend son tour
	int bank = 0;            // la banque affichée
	bool songMode = false;
	bool songLoop = true;
	bool songRec = false;    // en mode morceau, chaque mémoire ajoute une ligne
	bool songEnded = false;
	bool songPending = false; // le morceau démarre à la prochaine mesure
	int songRow = 0;
	int rowBar = 0;
	int songSel = 0;         // la ligne choisie dans l'éditeur du morceau
	int quant = Q_BAR;
	bool legato = false;     // ENCHAÎNER : les voies gardent leur position au changement
	int armed = ARM_NONE;
	int copySrc = -1;
	int cvSlot = -1;
};

struct Settings {
	float bpm = 120.f;
	float swing = 0.5f;              // 0,5 à 0,75
	bool extClock = false;           // CLOCK IN branchée
	int clockInRes = 0;              // indice dans CLOCK_PPS
	int clockOutRes = 0;
	bool vintage = false;
	int realign = 0;                 // indice dans REALIGN_STEPS
	bool muted[TRIG_LANES] = {};
	bool rec = false;
};

struct Outputs {
	float trig[TRIG_LANES] = {};
	float cv[CV_LANES] = {};
	float clock = 0.f, run = 0.f, reset = 0.f;
	float end = 0.f;
};

struct Core {
	Lane lanes[LANES];
	Rng rng;

	// Transport
	bool playing = false;
	bool cueHeld = false;

	// Horloge : `pos` en pas maîtres depuis le départ (temps droit), `origin` le dernier réalignement
	double pos = 0.0;
	double origin = 0.0;
	double rate = 8.0;               // pas maîtres par seconde (120 BPM)
	bool waitPulse = false;          // horloge externe : on attend la première impulsion
	double lastPulsePos = 0.0;
	double sincePulse = 0.0;
	bool extActive = false;
	bool prevExt = false;
	long clockTick = -1;
	double lastD[LANES] = {};        // position (droite ou musicale) vue par chaque voie au dernier échantillon

	// Mémoires
	Memory mem;
	bool memOn = false;              // Gastounet est branché
	long lastStep = -1;              // dernier pas maître vu
	double master = 0.0;             // position musicale courante, pour l'affichage
	float endTimer = 0.f;

	// Sorties en cours
	float trigTimer[TRIG_LANES] = {};
	float trigDelay[TRIG_LANES];
	float clockTimer = 0.f, clockDelay = -1.f, clockWidth = 0.005f;
	float resetTimer = 0.f;
	float cvTarget[CV_LANES] = {};
	float cvDelayValue[CV_LANES] = {};
	float cvDelay[CV_LANES];
	float cvOut[CV_LANES] = {};

	// Pour l'affichage : compteurs qui avancent à chaque trig réellement parti
	uint32_t seqFires[LANES] = {};   // par la séquence (le pas `firePos`)
	int firePos[LANES] = {};
	uint32_t padFires[TRIG_LANES] = {};
	uint32_t stepCount[LANES] = {};  // chaque avancement (voies CV comprises)

	Core() {
		for (int i = 0; i < LANES; i++) {
			lanes[i].cv = i >= TRIG_LANES;
			lanes[i].clear();
		}
		for (int i = 0; i < TRIG_LANES; i++)
			trigDelay[i] = -1.f;
		for (int i = 0; i < CV_LANES; i++)
			cvDelay[i] = -1.f;
	}

	Lane& cvLane(int c) { return lanes[TRIG_LANES + c]; }

	// --- Transport

	// Retour au début : toutes les voies au pas 1, les compteurs de rapport remis à zéro
	void rewind(const Settings& st) {
		pos = 0.0;
		origin = 0.0;
		clockTick = -1;
		lastPulsePos = 0.0;
		waitPulse = st.extClock;
		for (int i = 0; i < LANES; i++) {
			lanes[i].count = -1;
			lanes[i].pos = lanes[i].positionAt(0);
			lanes[i].skipPos = -1;
			lastD[i] = 0.0;
		}
		resetTimer = 0.002f;
		lastStep = -1;
		master = 0.0;
		// En mode morceau, le retour au début ramène à la première ligne
		if (memOn && mem.songMode) {
			mem.songRow = 0;
			mem.rowBar = 0;
			mem.songEnded = false;
			mem.songPending = false;
			if (mem.songLen > 0)
				loadSlot(mem.song[0].pattern, true, 0.0, false);
		}
	}

	void play() { playing = true; }
	void pause() { playing = false; }

	void togglePlay() {
		if (cueHeld) {
			// CUE maintenu puis PLAY : la lecture continue au relâché
			cueHeld = false;
			playing = true;
			return;
		}
		playing = !playing;
	}

	void cuePress(const Settings& st) {
		if (playing && !cueHeld) {
			playing = false;
			rewind(st);
			return;
		}
		rewind(st);
		cueHeld = true;
		playing = true;
	}

	void cueRelease(const Settings& st) {
		if (!cueHeld)
			return;
		cueHeld = false;
		playing = false;
		rewind(st);
	}

	// RESET IN : retour au début sans arrêter
	void resetKeepPlaying(const Settings& st) {
		rewind(st);
	}

	// Impulsion sur CLOCK IN (front montant)
	void clockPulse(const Settings& st) {
		if (!playing)
			return;
		double step = 1.0 / CLOCK_PPS[st.clockInRes];
		if (sincePulse > 1e-4 && sincePulse < 4.0 && !waitPulse)
			rate = step / sincePulse;
		sincePulse = 0.0;
		if (waitPulse) {
			waitPulse = false;
			lastPulsePos = 0.0;
			pos = 0.0;
		}
		else {
			lastPulsePos += step;
			pos = std::max(pos, lastPulsePos);
		}
	}

	// --- Pads et REC

	// Le pad d'une voie de trigs : il joue toujours ; armé en lecture, il écrit sur le pas le plus proche
	void pad(int i, const Settings& st) {
		fireTrig(i, st, 0.f);
		padFires[i]++;
		if (!(st.rec && playing))
			return;
		Lane& l = lanes[i];
		int target;
		if (l.count < 0) {
			target = l.positionAt(0);
		}
		else {
			double ph = (lastD[i] - origin) * RATIO_NUM[l.ratio] / RATIO_DEN[l.ratio];
			double frac = ph - std::floor(ph);
			if (frac < 0.5 || l.dir == RANDOM) {
				target = l.pos;
			}
			else {
				target = l.positionAt(l.count + 1);
				l.skipPos = target;
			}
		}
		l.on[target] = true;
		l.prob[target] = 1.f;
	}

	// --- Mémoires

	// Charge un motif dans Gaston. RELANCER : toutes les voies repartent du pas 1 à la position `at`.
	void loadSlot(int slot, bool restart, double at, bool pulse = true) {
		slot = clampi(slot, 0, SLOTS - 1);
		const Pattern& pt = mem.slots[slot];
		for (int i = 0; i < LANES; i++)
			copyContent(lanes[i], pt.lanes[i]);
		if (restart) {
			origin = at;
			for (int i = 0; i < LANES; i++) {
				lanes[i].count = -1;
				lanes[i].skipPos = -1;
			}
		}
		mem.active = slot;
		if (pulse)
			endTimer = TRIG_TIME;
	}

	// WRITE : le motif de travail dans une mémoire, qui devient la mémoire active
	void writeSlot(int slot) {
		Pattern& pt = mem.slots[clampi(slot, 0, SLOTS - 1)];
		for (int i = 0; i < LANES; i++)
			copyContent(pt.lanes[i], lanes[i]);
		pt.filled = true;
		mem.active = slot;
	}

	void clearSlot(int slot) { mem.slots[clampi(slot, 0, SLOTS - 1)] = Pattern(); }

	// Le motif de travail diffère-t-il de sa mémoire ?
	bool edited() const {
		const Pattern& pt = mem.slots[mem.active];
		for (int i = 0; i < LANES; i++)
			if (!sameContent(lanes[i], pt.lanes[i]))
				return true;
		return false;
	}

	// Un motif demandé : il attend la grille ; à l'arrêt, il se charge tout de suite
	void queueSlot(int slot) {
		if (playing) {
			mem.queued = slot;
			return;
		}
		mem.queued = -1;
		loadSlot(slot, !mem.legato, std::floor(master));
	}

	void pressSlot(int k) {
		int slot = mem.bank * BANK_SIZE + clampi(k, 0, BANK_SIZE - 1);
		if (mem.armed == ARM_WRITE) {
			writeSlot(slot);
			mem.armed = ARM_NONE;
			return;
		}
		if (mem.armed == ARM_COPY) {
			if (mem.copySrc < 0) {
				mem.copySrc = slot;
				return;
			}
			if (slot != mem.copySrc)
				mem.slots[slot] = mem.slots[mem.copySrc];
			mem.armed = ARM_NONE;
			mem.copySrc = -1;
			return;
		}
		if (mem.songMode) {
			if (mem.songRec && mem.songLen < SONG_ROWS) {
				mem.song[mem.songLen].pattern = slot;
				mem.song[mem.songLen].bars = 4;
				mem.songLen++;
			}
			return;
		}
		queueSlot(slot);
	}

	// NEXT : la mémoire pleine suivante de la banque (ou la suivante tout court)
	void nextSlot() {
		if (mem.songMode)
			return;
		int base = mem.bank * BANK_SIZE, from = mem.queued >= 0 ? mem.queued : mem.active;
		int cur = from - base;
		for (int d = 1; d <= BANK_SIZE; d++) {
			int slot = base + ((cur + d) % BANK_SIZE + BANK_SIZE) % BANK_SIZE;
			if (mem.slots[slot].filled) {
				queueSlot(slot);
				return;
			}
		}
		queueSlot(base + ((cur + 1) % BANK_SIZE + BANK_SIZE) % BANK_SIZE);
	}

	// RANDOM : une mémoire pleine de la banque, au hasard, autre que l'active
	void randomSlot() {
		if (mem.songMode)
			return;
		int base = mem.bank * BANK_SIZE, choices[BANK_SIZE], n = 0;
		for (int k = 0; k < BANK_SIZE; k++)
			if (mem.slots[base + k].filled && base + k != mem.active)
				choices[n++] = base + k;
		if (n > 0)
			queueSlot(choices[rng.below(n)]);
	}

	// CV PATTERN : 0 à 10 V parcourent les 16 mémoires de la banque
	void cvSelect(float volts) {
		int slot = mem.bank * BANK_SIZE + clampi((int) (volts / 10.f * BANK_SIZE), 0, BANK_SIZE - 1);
		if (slot == mem.cvSlot)
			return;
		mem.cvSlot = slot;
		if (!mem.songMode && slot != mem.active)
			queueSlot(slot);
	}

	void setSongMode(bool on) {
		mem.queued = -1;
		if (!on) {
			mem.songMode = false;
			mem.songRec = false;
			return;
		}
		if (mem.songMode)
			return;
		mem.songMode = true;
		songReset();
	}

	// Retour à la première ligne du morceau : à la prochaine mesure en lecture, tout de suite à l'arrêt
	void songReset() {
		if (!mem.songMode)
			return;
		mem.songRow = 0;
		mem.rowBar = 0;
		mem.songEnded = false;
		if (mem.songLen == 0)
			return;
		if (playing)
			mem.songPending = true;
		else
			loadSlot(mem.song[0].pattern, true, std::floor(master), false);
	}

	void insertRow(int at, SongRow r) {
		if (mem.songLen >= SONG_ROWS)
			return;
		at = clampi(at, 0, mem.songLen);
		for (int k = mem.songLen; k > at; k--)
			mem.song[k] = mem.song[k - 1];
		mem.song[at] = r;
		mem.songLen++;
		if (mem.songRow >= at && mem.songLen > 1)
			mem.songRow = clampi(mem.songRow + 1, 0, mem.songLen - 1);
	}

	// Les boutons d'édition du morceau, sur la ligne choisie
	void addRow() {
		int p = mem.songLen > 0 ? mem.song[clampi(mem.songSel, 0, mem.songLen - 1)].pattern : mem.active;
		int at = mem.songLen > 0 ? mem.songSel + 1 : 0;
		insertRow(at, SongRow(p, 4));
		mem.songSel = clampi(at, 0, mem.songLen - 1);
	}

	void duplicateRow() {
		if (mem.songLen == 0)
			return;
		int sel = clampi(mem.songSel, 0, mem.songLen - 1);
		insertRow(sel + 1, mem.song[sel]);
		mem.songSel = clampi(sel + 1, 0, mem.songLen - 1);
	}

	void removeRow() {
		deleteRow(clampi(mem.songSel, 0, std::max(0, mem.songLen - 1)));
		mem.songSel = clampi(mem.songSel, 0, std::max(0, mem.songLen - 1));
	}

	void deleteRow(int at) {
		if (at < 0 || at >= mem.songLen)
			return;
		for (int k = at; k < mem.songLen - 1; k++)
			mem.song[k] = mem.song[k + 1];
		mem.songLen--;
		if (mem.songRow > at)
			mem.songRow--;
		mem.songRow = clampi(mem.songRow, 0, std::max(0, mem.songLen - 1));
	}

	// À chaque nouveau pas maître : les changements de motif et le morceau. Renvoie vrai si le morceau
	// s'arrête (fin sans boucle).
	bool memoryStep(double m) {
		long s = (long) std::floor(m + 1e-9);
		if (s <= lastStep)
			return false;
		lastStep = s;
		if (mem.songMode) {
			if (mem.songLen == 0 || s % BAR_STEPS != 0)
				return false;
			if (mem.songPending) {
				mem.songPending = false;
				mem.songRow = 0;
				mem.rowBar = 0;
				loadSlot(mem.song[0].pattern, !mem.legato, (double) s);
				return false;
			}
			if (s == 0 || mem.songEnded)
				return false;
			mem.songRow = clampi(mem.songRow, 0, mem.songLen - 1);
			if (++mem.rowBar < std::max(1, mem.song[mem.songRow].bars))
				return false;
			mem.rowBar = 0;
			if (mem.songRow + 1 < mem.songLen) {
				mem.songRow++;
			}
			else if (mem.songLoop) {
				mem.songRow = 0;
			}
			else {
				mem.songEnded = true;
				endTimer = TRIG_TIME;
				return true;
			}
			loadSlot(mem.song[mem.songRow].pattern, !mem.legato, (double) s);
			return false;
		}
		if (mem.queued >= 0 && mem.quant != Q_TRACK1 && s % QUANT_STEPS[clampi(mem.quant, 0, QUANTS - 1)] == 0) {
			int q = mem.queued;
			mem.queued = -1;
			loadSlot(q, !mem.legato, (double) s);
		}
		return false;
	}

	// --- Moteur

	void fireTrig(int i, const Settings& st, float delay) {
		if (st.vintage && delay <= 0.f)
			delay = rng.uniform() * VINTAGE_JITTER;
		if (delay > 0.f)
			trigDelay[i] = delay;
		else
			trigTimer[i] = TRIG_TIME;
	}

	// Le pas suivant d'une voie : la voie avance, tire sa probabilité, ou pose sa tension
	void advance(int i, long k, const Settings& st) {
		Lane& l = lanes[i];
		l.count = k;
		l.pos = l.dir == RANDOM ? rng.below(std::max(1, l.length)) : l.positionAt(k);
		int p = l.pos;
		if (l.cv && !l.on[p])
			return; // Pas inactif : la tension reste celle du dernier pas actif (sample & hold)
		stepCount[i]++;
		if (!l.cv) {
			if (l.skipPos == p) {
				l.skipPos = -1;
				return;
			}
			l.skipPos = -1;
			if (l.on[p] && !st.muted[i] && rng.uniform() < l.prob[p]) {
				fireTrig(i, st, 0.f);
				seqFires[i]++;
				firePos[i] = p;
			}
		}
		else {
			int c = i - TRIG_LANES;
			float v = l.volts(p);
			if (st.vintage) {
				cvDelay[c] = rng.uniform() * VINTAGE_JITTER;
				cvDelayValue[c] = v;
			}
			else {
				cvTarget[c] = v;
			}
		}
	}

	void process(float dt, const Settings& st, Outputs& out) {
		// Horloge
		extActive = st.extClock;
		sincePulse += dt;
		if (st.extClock && !prevExt) {
			// On vient de brancher CLOCK IN : la prochaine impulsion part d'ici
			lastPulsePos = pos;
			sincePulse = 1e9;
		}
		prevExt = st.extClock;
		if (!st.extClock) {
			waitPulse = false;
			rate = st.bpm / 15.0;
		}
		if (playing && !waitPulse) {
			if (st.extClock) {
				// Entre deux impulsions, on avance au rythme mesuré sans dépasser la suivante
				double step = 1.0 / CLOCK_PPS[st.clockInRes];
				if (sincePulse < 4.0)
					pos = std::min(pos + rate * dt, lastPulsePos + step - 1e-7);
			}
		}

		if (playing && !waitPulse) {
			double p = pos;
			double s = st.swing;
			if (st.vintage) {
				p = std::floor(p * VINTAGE_TICKS + 1e-9) / VINTAGE_TICKS;
				s = std::round(s * 2.0 * VINTAGE_TICKS) / (2.0 * VINTAGE_TICKS);
			}
			double m = swingWarp(p, s);
			master = m;
			bool songStop = memOn && memoryStep(m);
			if (songStop) {
				// Fin du morceau sans boucle : arrêt, calé au début
				playing = false;
				rewind(st);
			}

			// Réalignement : toutes les voies au pas 1 tous les N pas maîtres
			int every = REALIGN_STEPS[clampi(st.realign, 0, REALIGNS - 1)];
			if (!songStop && every > 0 && m - origin >= every - 1e-9) {
				origin += every * std::floor((m - origin + 1e-9) / every);
				for (int i = 0; i < LANES; i++)
					lanes[i].count = -1;
			}

			for (int i = 0; i < LANES && !songStop; i++) {
				Lane& l = lanes[i];
				double d = RATIO_BINARY[l.ratio] ? m : p;
				lastD[i] = d;
				double ph = (d - origin) * RATIO_NUM[l.ratio] / RATIO_DEN[l.ratio];
				long k = (long) std::floor(ph + 1e-9);
				// Quantification « TRACK 1 » : le motif attendu part quand la voie 1 reboucle
				if (i == 0 && memOn && !mem.songMode && mem.queued >= 0 && mem.quant == Q_TRACK1
					&& k > l.count && k > 0 && k % std::max(1, l.length) == 0) {
					double at = origin + (double) k * RATIO_DEN[l.ratio] / RATIO_NUM[l.ratio];
					int q = mem.queued;
					mem.queued = -1;
					loadSlot(q, !mem.legato, at);
					ph = (d - origin) * RATIO_NUM[l.ratio] / RATIO_DEN[l.ratio];
					k = (long) std::floor(ph + 1e-9);
				}
				if (k > l.count)
					advance(i, k, st);
			}

			// CLOCK OUT, sur la grille musicale (swinguée)
			int pps = CLOCK_PPS[clampi(st.clockOutRes, 0, CLOCK_RES - 1)];
			long t = (long) std::floor(m * pps + 1e-9);
			if (!songStop && t > clockTick) {
				clockTick = t;
				clockWidth = (float) std::min(0.005, 0.5 / (std::max(rate, 0.1) * pps));
				if (st.vintage)
					clockDelay = rng.uniform() * VINTAGE_JITTER;
				else
					clockTimer = clockWidth;
			}

			if (!st.extClock && playing)
				pos += rate * dt;
		}

		// Sorties, avec les délais du timing vintage
		for (int i = 0; i < TRIG_LANES; i++) {
			if (trigDelay[i] >= 0.f) {
				trigDelay[i] -= dt;
				if (trigDelay[i] < 0.f)
					trigTimer[i] = TRIG_TIME;
			}
			out.trig[i] = trigTimer[i] > 0.f ? 10.f : 0.f;
			trigTimer[i] -= dt;
		}
		if (clockDelay >= 0.f) {
			clockDelay -= dt;
			if (clockDelay < 0.f)
				clockTimer = clockWidth;
		}
		out.clock = clockTimer > 0.f ? 10.f : 0.f;
		clockTimer -= dt;
		out.reset = resetTimer > 0.f ? 10.f : 0.f;
		resetTimer -= dt;
		out.run = playing ? 10.f : 0.f;
		out.end = endTimer > 0.f ? 10.f : 0.f;
		endTimer -= dt;

		for (int c = 0; c < CV_LANES; c++) {
			if (cvDelay[c] >= 0.f) {
				cvDelay[c] -= dt;
				if (cvDelay[c] < 0.f)
					cvTarget[c] = cvDelayValue[c];
			}
			float slew = cvLane(c).slew;
			if (slew <= 0.001f) {
				cvOut[c] = cvTarget[c];
			}
			else {
				float tau = 0.002f * std::pow(500.f, slew);
				cvOut[c] += (cvTarget[c] - cvOut[c]) * (1.f - std::exp(-dt / tau));
			}
			out.cv[c] = cvOut[c];
		}
	}

	// Tempo affiché : celui du potard, ou celui mesuré sur CLOCK IN
	float displayBpm(const Settings& st) const {
		return st.extClock ? (float) (rate * 15.0) : st.bpm;
	}

	static int clampi(int x, int a, int b) { return std::min(std::max(x, a), b); }

	// --- Outils d'édition (menu de voie)

	void shiftLane(int i, int by) {
		Lane& l = lanes[i];
		int n = std::max(1, l.length);
		bool on[STEPS]; float prob[STEPS], value[STEPS];
		for (int s = 0; s < n; s++) {
			int d = ((s + by) % n + n) % n;
			on[d] = l.on[s]; prob[d] = l.prob[s]; value[d] = l.value[s];
		}
		for (int s = 0; s < n; s++) {
			l.on[s] = on[s]; l.prob[s] = prob[s]; l.value[s] = value[s];
		}
	}

	void randomizeLane(int i) {
		Lane& l = lanes[i];
		for (int s = 0; s < STEPS; s++) {
			if (l.cv) {
				l.value[s] = rng.uniform() * 2.f - 1.f;
				l.on[s] = true;
			}
			else {
				l.on[s] = rng.uniform() < 0.3f;
				l.prob[s] = 1.f;
			}
		}
	}
};

} // namespace gaston
