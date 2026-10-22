#pragma once
// Moteur de Louis, sans dépendance à Rack : quatre Krell (cycles attaque/decay qui se relancent eux-mêmes), des
// dérives lentes, une boucle de mémoire par voix, des liens en anneau et le calage sur une horloge externe.
// Le cahier des charges est docs/Louis-spec.md.
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <initializer_list>

namespace louis {

static const int VOICES = 4;
static const int MAX_LOOP = 16;
static const float MIN_ATTACK = 0.001f, MIN_DECAY = 0.005f, MIN_CYCLE = 0.015f, MAX_CYCLE = 60.f;
static const float TRIG_LEN = 0.01f;      // durée d'un trig
static const float GATE_GAP = 0.002f;     // le gate retombe au moins ça avant la note suivante
static const float STRIKE_SLACK = 0.005f; // un STRIKE si proche du dernier instant de grille part aussitôt
static const float CHAIN_REFRACTORY = 0.05f;
static const float TEMPO_JUMP = 0.05f;      // écart d'intervalle au-delà duquel le tempo est remesuré

inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }
inline int bits(std::initializer_list<int> l) {
	int m = 0;
	for (int s : l) m |= 1 << s;
	return m;
}
inline int64_t floorDiv(int64_t a, int64_t b) {
	int64_t q = a / b;
	return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}
inline int64_t posMod(int64_t a, int64_t b) { return a - floorDiv(a, b) * b; }

// Gammes : un masque de 12 bits, bit n = n demi-tons au-dessus de la tonique. FREE n'en a aucun : pas de quantification.
struct Scale {
	const char* name;
	int mask;
};
static const int SCALE_COUNT = 13;
static const int SCALE_FREE = 0;
static const int SCALE_DEFAULT = 11;
static const Scale SCALES[SCALE_COUNT] = {
	{"FREE", 0},
	{"CHROMATIC", 0xFFF},
	{"MAJOR", bits({0, 2, 4, 5, 7, 9, 11})},
	{"MINOR", bits({0, 2, 3, 5, 7, 8, 10})},
	{"DORIAN", bits({0, 2, 3, 5, 7, 9, 10})},
	{"PHRYGIAN", bits({0, 1, 3, 5, 7, 8, 10})},
	{"LYDIAN", bits({0, 2, 4, 6, 7, 9, 11})},
	{"MIXOLYDIAN", bits({0, 2, 4, 5, 7, 9, 10})},
	{"LOCRIAN", bits({0, 1, 3, 5, 6, 8, 10})},
	{"HARM MINOR", bits({0, 2, 3, 5, 7, 8, 11})},
	{"MAJ PENTA", bits({0, 2, 4, 7, 9})},
	{"MIN PENTA", bits({0, 3, 5, 7, 10})},
	{"WHOLE TONE", bits({0, 2, 4, 6, 8, 10})},
};

// Grille : comptée en unités, un coup d'horloge en vaut UNIT_SUB (144 : tous les produits GRID × TIME y tombent juste)
static const int UNIT_SUB = 144;
static const int GRID_COUNT = 9;
static const int GRID_DEFAULT = 3;
static const char* const GRID_NAMES[GRID_COUNT] = {"1/4", "1/3", "1/2", "1", "2", "3", "4", "8", "16"};
static const int GRID_UNITS[GRID_COUNT] = {36, 48, 72, 144, 288, 432, 576, 1152, 2304};

// TIME : le rapport propre à chaque voix. Horloge branchée, il multiplie la grille de la voix ; toujours, il multiplie
// PACE. La voix joue le même Krell à une autre vitesse.
static const int TIME_COUNT = 9;
static const int TIME_DEFAULT = 4;
static const char* const TIME_NAMES[TIME_COUNT] = {"1/4", "1/3", "1/2", "2/3", "1", "3/2", "2", "3", "4"};
static const int TIME_NUM[TIME_COUNT] = {1, 1, 1, 2, 1, 3, 2, 3, 4};
static const int TIME_DEN[TIME_COUNT] = {4, 3, 2, 3, 1, 2, 1, 1, 1};
inline int timeIndex(int k) { return std::min(std::max(k, 0), TIME_COUNT - 1); }
inline float timeRatio(int k) { k = timeIndex(k); return (float) TIME_NUM[k] / TIME_DEN[k]; }
// Pas de grille d'une voix, en unités
inline int voiceStep(int grid, int time) {
	int t = timeIndex(time);
	return GRID_UNITS[std::min(std::max(grid, 0), GRID_COUNT - 1)] * TIME_NUM[t] / TIME_DEN[t];
}

enum LinkMode { LINK_CHAIN, LINK_HUSH, LINK_ECHO };
enum Cause { C_EOC, C_CHAIN, C_STRIKE, C_RESET };

// Note de la gamme la plus proche ; `ref` est la tension de la tonique (0 V pour une gamme absolue)
inline float quantize(float volts, float ref, int mask) {
	if (!(mask & 0xFFF)) return volts;
	float s = (volts - ref) * 12.f;
	int base = (int) std::floor(s);
	int best = base;
	float bestD = 1e9f;
	for (int d = -12; d <= 13; d++) {
		int n = base + d;
		if (!((mask >> (int) posMod(n, 12)) & 1)) continue;
		float dd = std::fabs(s - (float) n);
		if (dd < bestD) {
			bestD = dd;
			best = n;
		}
	}
	return ref + best / 12.f;
}

// Monte de `degrees` notes dans la gamme à partir d'une note qui en fait partie
inline float moveDegrees(float volts, float ref, int mask, int degrees) {
	if (!(mask & 0xFFF)) return volts;
	int n = (int) std::lround((volts - ref) * 12.f);
	for (int k = 0; k < degrees; k++) {
		do n++;
		while (!((mask >> (int) posMod(n, 12)) & 1));
	}
	return ref + n / 12.f;
}

struct Rng {
	uint64_t s = 0x9E3779B97F4A7C15ull;
	void seed(uint64_t v) { s = v * 0x9E3779B97F4A7C15ull + 1; }
	uint32_t next() {
		s ^= s << 13; s ^= s >> 7; s ^= s << 17;
		return (uint32_t) (s >> 32);
	}
	float uniform() { return (next() >> 8) * (1.f / 16777216.f); }
};

// Tension aléatoire lente et continue (la FRV du 266) : glisse entre des cibles tirées au hasard (Catmull-Rom)
struct Drift {
	float p[4] = {0, 0, 0, 0};
	float phase = 0, speed = 1;
	void init(Rng& r) {
		for (int i = 0; i < 4; i++) p[i] = r.uniform() * 2.f - 1.f;
		phase = r.uniform();
		speed = 0.6f + r.uniform();   // chaque dérive a son allure : elles ne marchent jamais au pas
	}
	void step(float dt, float hz, Rng& r) {
		phase += dt * hz * speed;
		while (phase >= 1.f) {
			phase -= 1.f;
			p[0] = p[1]; p[1] = p[2]; p[2] = p[3];
			p[3] = r.uniform() * 2.f - 1.f;
		}
	}
	float value() const {
		float t = phase, t2 = t * t, t3 = t2 * t;
		float v = 0.5f * (2.f * p[1] + (p[2] - p[0]) * t + (2.f * p[0] - 5.f * p[1] + 4.f * p[2] - p[3]) * t2
			+ (3.f * p[1] - p[0] - 3.f * p[2] + p[3]) * t3);
		return clampf(v, -1.f, 1.f);
	}
};

// Réglages effectifs, CV comprises : le module additionne et borne, le moteur ne lit que ça
struct Params {
	float pace = 0.6f;       // s, durée moyenne d'un cycle
	float tilt = 0.15f;      // part de l'attaque
	float drift = 0.08f;     // Hz, vitesse des dérives
	float wander = 0.4f;     // amplitude des dérives
	float density = 0.85f;
	float memory = 0.f;
	float correl = 0.f;      // −1 à 1
	float gate = 0.5f;       // part du decay couverte par le gate
	int scale = SCALE_DEFAULT;
	int extMask = 0;         // SCALE IN : classes de hauteur absolues ; non nul, remplace la gamme
	float root = 0.f;        // V, tonique (potard + CV ROOT)
	float range = 2.f;       // octaves
	float skew = 0.f;        // −1 à 1
	float link = 0.f;
	int linkMode = LINK_CHAIN;
	int grid = GRID_DEFAULT;
	float snap = 1.f;
	int time[VOICES] = {TIME_DEFAULT, TIME_DEFAULT, TIME_DEFAULT, TIME_DEFAULT};   // indices dans TIME_NAMES
	float oct[VOICES] = {};
	bool hold[VOICES] = {};
	bool freeze = false;
	bool clockConnected = false;
	int loopLength = 8;
	bool linearEnv = false;
	bool clockStopFree = false;   // horloge arrêtée : false = les voix attendent, true = timing libre
};

// Fronts montants de l'échantillon courant (détectés par le module)
struct Events {
	bool clock = false, reset = false;
	bool strike[VOICES] = {};
};

struct Outputs {
	float trig[VOICES] = {}, gate[VOICES] = {}, pitch[VOICES] = {}, env[VOICES] = {};
	float any = 0.f, driftA = 0.f, driftB = 0.f;
	bool onset[VOICES] = {};   // une note commence
	bool cycle[VOICES] = {};   // un cycle commence (note ou silence)
	bool link[VOICES] = {};    // la voix agit sur la suivante
};

struct Voice {
	Drift dTime, dShape;
	bool pending = false;      // attend un instant de grille pour commencer
	int pendingCause = C_EOC;
	double start = 0.0;
	float attack = 0.1f, decay = 0.5f;
	float startLevel = 0.f;    // l'attaque repart du niveau courant si un cycle est coupé
	bool note = false;         // le cycle courant est une note (sinon un silence)
	bool onGrid = false;       // le cycle finit sur l'instant de grille `target`
	int64_t target = 0;
	float trigTo = 0.f, gateFrom = 0.f, gateTo = 0.f;
	float pitch = 0.f, env = 0.f;
	bool gateHigh = false;
	double lastOnset = -1e9;
	bool hushNext = false, echoNext = false;
	float echoPitch = 0.f, echoOct = 0.f;
	bool fromLoop = false;
	bool hushed = false;       // le cycle courant est un silence imposé par HUSH
	bool echoed = false;       // la note courante reprend celle de la voix `echoFrom` (ECHO)
	int echoFrom = -1;
	float loopU[MAX_LOOP] = {}, loopJit[MAX_LOOP] = {};
	bool loopNote[MAX_LOOP] = {}, loopFilled[MAX_LOOP] = {};
	int loopPos = 0;
	float total() const { return attack + decay; }
};

inline float attackShape(float x, bool lin) {
	return lin ? x : (1.f - std::exp(-3.f * x)) * (1.f / (1.f - 0.049787068f));
}
inline float decayShape(float x, bool lin) {
	return lin ? 1.f - x : (std::exp(-4.f * x) - 0.018315639f) * (1.f / (1.f - 0.018315639f));
}

struct Core {
	Voice v[VOICES];
	Drift driftA, driftB;
	Rng rng;
	double now = 0.0;
	bool started = false;
	double anyTo = -1.0;

	// Horloge
	bool prevConnected = false;
	int64_t lastPulse = -1;      // indice du dernier coup reçu
	double lastPulseTime = 0.0, lastActivity = 0.0;
	float intervals[4] = {};
	int ivCount = 0, ivPos = 0;
	float period = 0.5f;
	int64_t originPulse = 0;     // le coup qui compte comme premier (départ, RESET)
	bool resetPending = false;
	int64_t prevReached = 0;
	int prevStep[VOICES] = {-1, -1, -1, -1};
	double lastGridTime[VOICES] = {-1e9, -1e9, -1e9, -1e9};

	// Contexte d'horloge de l'échantillon courant : la grille de chaque voix, et si elle vient d'être franchie
	bool constrained = false, predicted = false, bind = false;
	int stepU[VOICES] = {UNIT_SUB, UNIT_SUB, UNIT_SUB, UNIT_SUB};
	bool gridHit[VOICES] = {};
	int64_t reached = 0, unitOrigin = 0;

	void init(uint64_t seed) {
		rng.seed(seed);
		for (int i = 0; i < VOICES; i++) {
			v[i] = Voice();
			v[i].dTime.init(rng);
			v[i].dShape.init(rng);
		}
		driftA.init(rng);
		driftB.init(rng);
		now = 0.0;
		started = false;
	}

	float density(const Params& p) const {
		// L'ondulation s'efface aux extrêmes : 0 % se tait, 100 % ne se tait jamais
		float d = clampf(p.density, 0.f, 1.f);
		return clampf(d + 0.3f * p.wander * driftB.value() * 4.f * d * (1.f - d), 0.f, 1.f);
	}

	float stopTimeout() const { return std::max(ivCount > 0 ? 4.f * period : 0.f, 1.f); }

	// Durée d'un sous-coup et position fractionnaire de l'instant présent, en sous-coups
	double unitDur() const { return (double) period / UNIT_SUB; }
	double unitNow() const { return (double) lastPulse * UNIT_SUB + (now - lastPulseTime) / unitDur(); }
	double unitTime(int64_t u) const { return lastPulseTime + (double) (u - lastPulse * UNIT_SUB) * unitDur(); }

	// Ajuste attaque et decay pour que le cycle dure `t` : le decay s'étire ou se comprime, l'attaque aussi si besoin
	void fit(Voice& w, double t) {
		float T = (float) std::max(t, (double) MIN_CYCLE);
		if (T >= w.attack + MIN_DECAY) {
			w.decay = T - w.attack;
		}
		else {
			float r = w.attack / w.total();
			w.attack = std::max(MIN_ATTACK, T * r);
			w.decay = std::max(MIN_DECAY, T - w.attack);
		}
	}

	void startNote(int i, int cause, const Params& p, Outputs& o) {
		Voice& w = v[i];
		bool forced = cause != C_EOC;   // STRIKE, CHAIN et RESET jouent toujours une note
		int L = std::min(std::max(p.loopLength, 4), MAX_LOOP);
		if (w.loopPos >= L) w.loopPos = 0;
		int pos = w.loopPos;

		// Rejouer la boucle ou tirer du neuf
		float u, jit;
		bool isNote;
		bool lock = p.hold[i] || p.freeze;
		bool replay = w.loopFilled[pos] && (lock || rng.uniform() < p.memory);
		if (replay) {
			u = w.loopU[pos];
			jit = w.loopJit[pos];
			isNote = w.loopNote[pos];
		}
		else {
			u = rng.uniform();
			jit = rng.uniform() + rng.uniform() - 1.f;
			isNote = rng.uniform() < density(p);
			w.loopU[pos] = u;
			w.loopJit[pos] = jit;
			w.loopNote[pos] = isNote;
			w.loopFilled[pos] = true;
		}
		w.loopPos = (pos + 1) % L;
		w.fromLoop = replay;
		w.hushed = false;
		w.echoed = false;
		if (w.hushNext && !forced) {
			isNote = false;
			w.hushNext = false;
			w.hushed = true;
		}
		if (forced) isNote = true;

		// Hauteur : distribution penchée par SKEW et la dérive A, puis quantification
		int mask = p.extMask ? p.extMask : SCALES[std::min(std::max(p.scale, 0), SCALE_COUNT - 1)].mask;
		float ref = p.extMask ? 0.f : p.root;
		float skew = clampf(p.skew + 0.6f * p.wander * driftA.value(), -1.f, 1.f);
		float us = std::pow(u, std::pow(4.f, -skew));
		float norm = us;
		if (isNote) {
			float volts;
			if (w.echoNext) {
				// ECHO : la note de la voix précédente, dans le registre de celle-ci, déplacée d'unisson, tierce ou quinte
				static const int DEG[3] = {0, 2, 4};
				static const int FREE_ST[3] = {0, 7, 12};
				int k = (int) (rng.uniform() * 3.f) % 3;
				volts = w.echoPitch + (p.oct[i] - w.echoOct);
				if (mask) volts = moveDegrees(quantize(volts, ref, mask), ref, mask, DEG[k]);
				else volts += FREE_ST[k] / 12.f;
				w.echoNext = false;
				w.echoed = true;
				norm = 0.5f;
			}
			else {
				volts = quantize(p.root + p.oct[i] + us * p.range, ref, mask);
			}
			w.pitch = volts;
		}

		// Durée et forme, en domaine logarithmique
		float lg = std::log2(std::max(p.pace * timeRatio(p.time[i]), 1e-4f)) + 2.f * p.wander * w.dTime.value()
			+ 0.5f * p.wander * jit + p.correl * (norm - 0.5f) * 2.f;
		float T = clampf(std::exp2(lg), MIN_CYCLE, MAX_CYCLE);
		float tl = clampf(p.tilt, 0.02f, 0.98f);
		float lt = std::log(tl / (1.f - tl)) + 3.f * p.wander * w.dShape.value();
		float tilt = clampf(1.f / (1.f + std::exp(-lt)), 0.02f, 0.98f);
		w.attack = std::max(MIN_ATTACK, T * tilt);
		w.decay = std::max(MIN_DECAY, T - w.attack);
		if (w.total() < MIN_CYCLE) w.decay = MIN_CYCLE - w.attack;

		// Calage : la fin prévue va sur l'instant de grille prédit le plus proche (ou s'en rapproche, SNAP < 1)
		w.onGrid = false;
		if (constrained && predicted) {
			double ud = unitDur();
			double uEnd = unitNow() + w.total() / ud;
			int64_t st = stepU[i];
			int64_t kMin = floorDiv(reached - unitOrigin, st) + 1;
			int64_t k = std::max((int64_t) std::llround((uEnd - unitOrigin) / st), kMin);
			double tGrid = unitTime(unitOrigin + k * st);
			while (tGrid - now < MIN_CYCLE) {
				k++;
				tGrid += st * ud;
			}
			double planned = now + w.total();
			fit(w, bind ? tGrid - now : (double) w.total() + p.snap * (tGrid - planned));
			if (bind) {
				w.onGrid = true;
				w.target = unitOrigin + k * st;
			}
		}

		w.pending = false;
		w.start = now;
		w.note = isNote;
		o.cycle[i] = true;
		if (isNote) {
			w.startLevel = w.env;
			w.trigTo = std::min(TRIG_LEN, 0.5f * w.total());
			w.gateFrom = w.gateHigh ? GATE_GAP : 0.f;
			w.gateTo = std::max(w.gateFrom + 0.001f, std::min(w.attack + p.gate * w.decay, w.total() - GATE_GAP));
			w.lastOnset = now;
			o.onset[i] = true;
			anyTo = now + TRIG_LEN;
		}
	}

	void setPending(int i, int cause) {
		Voice& w = v[i];
		if (!w.pending || cause == C_STRIKE || cause == C_RESET) w.pendingCause = cause;
		w.pending = true;
		w.onGrid = false;
	}

	// Fin d'un cycle joué : peut-être agir sur la voix suivante de l'anneau
	void endCycle(int i, const Params& p, Outputs& o) {
		Voice& w = v[i];
		if (!w.note || p.link <= 0.f || rng.uniform() >= p.link) return;
		int j = (i + 1) % VOICES;
		Voice& n = v[j];
		o.link[i] = true;
		if (p.linkMode == LINK_CHAIN) {
			double refractory = std::max((double) CHAIN_REFRACTORY, 0.25 * n.total());
			if (now - n.lastOnset >= refractory) startNote(j, C_CHAIN, p, o);
		}
		else if (p.linkMode == LINK_HUSH) {
			n.hushNext = true;
		}
		else {
			n.echoNext = true;
			n.echoPitch = w.pitch;
			n.echoOct = p.oct[i];
			n.echoFrom = i;
		}
	}

	void clock(const Params& p, const Events& e) {
		bool connected = p.clockConnected;
		if (connected && !prevConnected) {
			lastActivity = now;
			lastPulse = -1;
			ivCount = 0;
		}
		prevConnected = connected;
		if (!connected) {
			lastPulse = -1;
			ivCount = 0;
		}
		bool force = false;
		if (connected && e.clock) {
			if (lastPulse >= 0) {
				float d = (float) (now - lastPulseTime);
				if (d > stopTimeout()) {
					ivCount = 0;   // reprise après un arrêt : la période est à remesurer
				}
				else {
					// Changement de tempo : on oublie les anciens intervalles (en deçà, la moyenne absorbe le jitter)
					if (ivCount > 0 && std::fabs(d - period) > TEMPO_JUMP * period) ivCount = 0;
					intervals[ivPos] = d;
					ivPos = (ivPos + 1) % 4;
					ivCount = std::min(ivCount + 1, 4);
					float s = 0.f;
					for (int k = 0; k < ivCount; k++) s += intervals[(ivPos - 1 - k + 8) % 4];
					period = s / ivCount;
				}
			}
			lastPulse++;
			lastPulseTime = now;
			lastActivity = now;
			if (resetPending || lastPulse == 0) {
				originPulse = lastPulse;
				resetPending = false;
				force = true;
			}
		}
		bool stopped = connected && now - lastActivity > stopTimeout();
		constrained = connected && !(stopped && p.clockStopFree);
		predicted = constrained && ivCount > 0 && !stopped && lastPulse >= 0;
		bind = p.snap >= 0.999f;
		unitOrigin = originPulse * UNIT_SUB;
		reached = 0;
		if (lastPulse >= 0) {
			int64_t frac = 0;
			if (predicted)
				frac = std::min((int64_t) UNIT_SUB - 1, (int64_t) std::floor((now - lastPulseTime) / unitDur() + 1e-9));
			reached = lastPulse * UNIT_SUB + frac;
		}

		for (int i = 0; i < VOICES; i++) {
			int st = voiceStep(p.grid, p.time[i]);
			stepU[i] = st;
			gridHit[i] = force;
			if (st != prevStep[i]) {
				// Nouvelle grille pour cette voix : le cycle calé sur l'ancienne finit à son heure prévue
				v[i].onGrid = false;
				prevStep[i] = st;
			}
			else if (lastPulse >= 0 && reached > prevReached && reached >= unitOrigin)
				gridHit[i] = gridHit[i] || floorDiv(reached - unitOrigin, st) != floorDiv(prevReached - unitOrigin, st);
			if (gridHit[i]) lastGridTime[i] = now;
		}
		prevReached = reached;
	}

	void process(float dt, const Params& p, const Events& e, Outputs& o) {
		for (int i = 0; i < VOICES; i++) o.onset[i] = o.cycle[i] = o.link[i] = false;
		clock(p, e);

		if (!started) {
			// Départ : chaque voix commence par un silence de durée tirée au hasard, pour ne pas partir ensemble
			started = true;
			for (int i = 0; i < VOICES; i++) {
				Voice& w = v[i];
				if (constrained) {
					setPending(i, C_EOC);
					continue;
				}
				w.note = false;
				w.start = now;
				w.attack = MIN_ATTACK;
				w.decay = std::max(MIN_DECAY, rng.uniform() * p.pace * timeRatio(p.time[i]));
			}
		}

		// RESET : boucles à leur début, toutes les voix repartent (au prochain coup si l'horloge contraint)
		if (e.reset) {
			for (int i = 0; i < VOICES; i++) v[i].loopPos = 0;
			if (constrained && bind) {
				resetPending = true;
				for (int i = 0; i < VOICES; i++) setPending(i, C_RESET);
			}
			else {
				for (int i = 0; i < VOICES; i++) startNote(i, C_RESET, p, o);
			}
		}

		for (int i = 0; i < VOICES; i++) {
			if (!e.strike[i] || now - v[i].lastOnset < MIN_CYCLE) continue;
			if (constrained && bind && now - lastGridTime[i] >= STRIKE_SLACK) setPending(i, C_STRIKE);
			else startNote(i, C_STRIKE, p, o);
		}

		for (int i = 0; i < VOICES; i++) {
			Voice& w = v[i];
			if (w.pending) {
				if (!constrained || (gridHit[i] && !resetPending)) startNote(i, w.pendingCause, p, o);
				continue;
			}
			if (o.cycle[i]) continue;   // vient de commencer (RESET, STRIKE, CHAIN)
			bool ended = (w.onGrid && constrained) ? reached >= w.target : now >= w.start + w.total();
			if (!ended) continue;
			endCycle(i, p, o);
			if (!constrained || (predicted && (w.onGrid || !bind))) startNote(i, C_EOC, p, o);
			else setPending(i, C_EOC);
		}

		// Sorties
		for (int i = 0; i < VOICES; i++) {
			Voice& w = v[i];
			float x = (float) (now - w.start);
			float env = 0.f;
			bool trig = false, gate = false;
			if (!w.pending && w.note) {
				if (x < w.attack) env = w.startLevel + (1.f - w.startLevel) * attackShape(x / w.attack, p.linearEnv);
				else if (x < w.total()) env = decayShape((x - w.attack) / w.decay, p.linearEnv);
				trig = x < w.trigTo;
				gate = x >= w.gateFrom && x < w.gateTo;
			}
			w.env = env;
			w.gateHigh = gate;
			o.env[i] = 10.f * env;
			o.trig[i] = trig ? 10.f : 0.f;
			o.gate[i] = gate ? 10.f : 0.f;
			o.pitch[i] = w.pitch;
		}
		o.any = now < anyTo ? 10.f : 0.f;
		o.driftA = 5.f * driftA.value();
		o.driftB = 5.f * driftB.value();

		// Dérives
		for (int i = 0; i < VOICES; i++) {
			v[i].dTime.step(dt, p.drift, rng);
			v[i].dShape.step(dt, p.drift, rng);
		}
		driftA.step(dt, p.drift, rng);
		driftB.step(dt, p.drift, rng);
		now += dt;
	}
};

} // namespace louis
