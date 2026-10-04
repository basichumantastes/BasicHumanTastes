#pragma once
// Cœur sonore de Lucienne, sans dépendance à Rack : six oscillateurs à intégrateur et comparateur noués en anneau,
// un rail d'alimentation qui s'affaisse, un registre qui ouvre les portes basse-bas, et un halo.
// Tout tourne en interne à 4× la fréquence d'échantillonnage pour les oscillateurs.
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace lucienne {

static const int VOICES = 6;
static const int CELLS = 8;
static const int OVERSAMPLE = 4;
static const float PI = 3.14159265358979f;

inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }
inline float smoothstep(float a, float b, float x) {
	float t = clampf((x - a) / (b - a), 0.f, 1.f);
	return t * t * (3.f - 2.f * t);
}

struct Rng {
	uint64_t s = 0x9E3779B97F4A7C15ull;
	void seed(uint64_t v) { s = v * 0x9E3779B97F4A7C15ull + 1; }
	uint32_t next() {
		s ^= s << 13; s ^= s >> 7; s ^= s << 17;
		return (uint32_t) (s >> 32);
	}
	float uniform() { return (next() >> 8) * (1.f / 16777216.f); }
	float normal() {
		// Somme de quatre uniformes : assez gaussien pour du bruit de composants
		return (uniform() + uniform() + uniform() + uniform() - 2.f) * 1.732f;
	}
};

struct Biquad {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	void lowpass(float fc, float fs, float q) {
		float w = 2.f * PI * fc / fs, c = std::cos(w), s = std::sin(w), al = s / (2.f * q), a0 = 1.f + al;
		b0 = (1.f - c) * 0.5f / a0; b1 = (1.f - c) / a0; b2 = b0; a1 = -2.f * c / a0; a2 = (1.f - al) / a0;
	}
	float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
};

// Filtre d'état variable (topologie TPT), passe-bas seulement
struct Svf {
	float ic1 = 0, ic2 = 0;
	float lowpass(float v0, float g, float k) {
		float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
		float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
		return v2;
	}
};

struct Delay {
	std::vector<float> buf;
	int w = 0;
	void init(int n) { buf.assign(n, 0.f); w = 0; }
	void write(float x) { buf[w] = x; if (++w >= (int) buf.size()) w = 0; }
	// d en échantillons, ≥ 1
	float read(float d) const {
		int n = (int) buf.size();
		float p = (float) w - d;
		while (p < 0.f) p += n;
		int i = (int) p; float f = p - i;
		float a = buf[i % n], b = buf[(i + 1) % n];
		return a + (b - a) * f;
	}
};

struct Allpass {
	Delay d; int len = 1; float g = 0.7f;
	void init(int n, float gain) { d.init(n + 1); len = n; g = gain; }
	float process(float x) {
		float z = d.read((float) len);
		float v = x + g * z;
		d.write(v);
		return z - g * v;
	}
};

// Halo : diffusion puis réseau de quatre retards modulés (FDN), la modulation désaccorde légèrement chaque réinjection
struct Halo {
	Allpass diff[4];
	Delay line[4];
	float base[4], lp[4] = {0, 0, 0, 0}, ph[4] = {0, 0.25f, 0.5f, 0.75f}, rate[4] = {0.11f, 0.17f, 0.13f, 0.07f};
	float sr = 48000;
	void init(float sampleRate) {
		sr = sampleRate;
		float k = sr / 48000.f;
		static const int AP[4] = {142, 107, 379, 277};
		for (int i = 0; i < 4; i++) diff[i].init((int) (AP[i] * k), 0.68f);
		static const float LEN[4] = {1687, 2053, 2459, 2903};
		for (int i = 0; i < 4; i++) {
			base[i] = LEN[i] * 1.7f * k;
			line[i].init((int) base[i] + 64);
		}
	}
	void process(float inL, float inR, float amount, float& outL, float& outR) {
		float fb = 0.55f + 0.42f * amount;
		float mid = (inL + inR) * 0.5f, side = (inL - inR) * 0.5f;
		float x = mid;
		for (int i = 0; i < 4; i++) x = diff[i].process(x);
		float d[4];
		for (int i = 0; i < 4; i++) {
			ph[i] += rate[i] / sr;
			if (ph[i] >= 1.f) ph[i] -= 1.f;
			d[i] = line[i].read(base[i] + 9.f * std::sin(2.f * PI * ph[i]));
		}
		// Matrice de Hadamard 4×4 normalisée
		float h0 = 0.5f * (d[0] + d[1] + d[2] + d[3]);
		float h1 = 0.5f * (d[0] - d[1] + d[2] - d[3]);
		float h2 = 0.5f * (d[0] + d[1] - d[2] - d[3]);
		float h3 = 0.5f * (d[0] - d[1] - d[2] + d[3]);
		float h[4] = {h0, h1, h2, h3};
		float in[4] = {x + side * 0.5f, x - side * 0.5f, -x + side * 0.3f, x};
		for (int i = 0; i < 4; i++) {
			lp[i] += (h[i] - lp[i]) * 0.42f;
			line[i].write(std::tanh(in[i] + fb * lp[i]));
		}
		outL = d[0] + 0.5f * d[2];
		outR = d[1] + 0.5f * d[3];
	}
};

// Filtre A : échelle passe-bas 24 dB, saturée à chaque étage, à 2× en interne.
// k va de 0 à environ 4,2 ; vers 4 elle auto-oscille.
struct Ladder {
	float st[4] = {0, 0, 0, 0};
	float process(float in, float g, float k, float drive) {
		float y = 0.f;
		for (int o = 0; o < 2; o++) {
			// Chaque étage reçoit la sortie saturée du précédent (modèle de Huovilainen)
			float u = std::tanh(drive * (in - k * st[3]));
			for (int n = 0; n < 4; n++) {
				float t = std::tanh(st[n]);
				st[n] += g * (u - t);
				u = std::tanh(st[n]);
			}
			y = st[3];
		}
		// Compense la perte de graves quand la résonance monte, et en partie le niveau perdu à la saturation
		return y * (1.f + 0.35f * k) / std::sqrt(drive * 1.2f);
	}
};

// Filtre B : variables d'état 12 dB, sorties passe-bas et passe-bande, état passe-bande saturé
// pour que l'auto-oscillation reste ronde et bornée.
struct StateVariable {
	float ic1 = 0, ic2 = 0;
	void process(float v0, float g, float k, float& lp, float& bp) {
		float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
		float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = std::tanh(2.f * v1 - ic1);
		ic2 = 2.f * v2 - ic2;
		lp = v2; bp = v1;
	}
};

// Intervalles justes vers lesquels SERRAGE attire les voix (en demi-tons dans l'octave), avec leur force :
// plus le rapport est simple, plus la zone d'accrochage est large, comme les langues d'Arnold des oscillateurs couplés.
struct Just { float semi, weight; };
static const Just JUST[] = {
	{0.f, 1.f}, {12.f, 1.f}, {7.02f, 0.9f}, {4.98f, 0.8f}, {3.86f, 0.7f}, {8.84f, 0.65f}, {3.16f, 0.6f},
	{8.14f, 0.55f}, {9.69f, 0.5f}, {2.04f, 0.45f}, {10.88f, 0.4f}, {10.18f, 0.4f}, {5.83f, 0.4f}, {1.12f, 0.3f},
};
static const int JUST_COUNT = sizeof(JUST) / sizeof(JUST[0]);

// Attire un intervalle (en demi-tons, de signe quelconque) vers l'intervalle juste le plus proche :
// accroché net dans la moitié centrale de la zone, relâché en douceur jusqu'à son bord.
inline float attract(float semi, float width) {
	if (width <= 0.f) return semi;
	float oct = std::floor(semi / 12.f) * 12.f, r = semi - oct;
	float best = 0.f, bestScore = 1e9f, bestW = 0.f;
	for (int k = 0; k < JUST_COUNT; k++) {
		float w = width * JUST[k].weight, d = r - JUST[k].semi;
		float score = std::fabs(d) / w;
		if (score < bestScore) { bestScore = score; best = d; bestW = w; }
	}
	if (bestScore >= 1.f) return semi;
	return semi - best + best * smoothstep(0.5f * bestW, bestW, std::fabs(best));
}

// Les douze destinations de l'anneau des LFO, dans l'ordre du cadran (sens horaire)
enum Slot {
	SLOT_HAUTEUR, SLOT_ECART, SLOT_SERRAGE, SLOT_COURANT, SLOT_SOUFFLE, SLOT_DENSITE,
	SLOT_HALO, SLOT_FILTRE_B, SLOT_FILTRE_A, SLOT_TIMBRE, SLOT_VITESSE, SLOT_DESTINATION, SLOTS
};
// Dose de chaque destination quand le LFO est à fond, la profondeur à fond et le poids à 1
static const float SLOT_SCALE[SLOTS] = {
	1.5f,   // demi-tons
	0.2f, 0.35f, 0.45f, 0.4f, 0.45f, 0.35f, 0.3f, 0.3f, 0.35f,
	3.f,    // octaves de vitesse de l'autre LFO
	3.f,    // crans de destination de l'autre LFO
};

struct LfoParams {
	float rate = 0.35f;     // d'un cycle de 20 min à 30 Hz
	float shape = 0.f;      // sinus, triangle, rampe, aléatoire lissé, aléatoire en escalier
	float dest = 0.f;       // 0 à 1 sur les douze crans du cadran
	float width = 0.f;      // 0 : entre deux crans voisins, 1 : déborde sur tout l'anneau
	float depth = 0.f;      // bipolaire
	float spread = 0.f;     // ÉVENTAIL : décalage de phase d'une voix à l'autre (et entre gauche et droite pour les filtres)
	bool inConnected = false;
	float in = 0.f;         // signal externe qui remplace le LFO (±5 V)
	float rateCv = 0.f;     // 1 V par octave
	float destCv = 0.f;     // 10 V parcourent tout le cadran
};

struct Params {
	// Accord de chaque voix, en demi-tons au-dessus de la fondamentale (réglé à l'oreille)
	float tune[VOICES] = {0.f, 7.02f, 12.f, 15.86f, 19.02f, 26.04f};
	float ecart = 0.5f;     // 0 : tout se resserre vers l'unisson, 0,5 : l'accord tel quel, 1 : intervalles doublés
	float serrage = 0.2f;   // couplage de l'anneau : libre, verrouillage, FM croisée, chaos
	float courant = 1.f;    // 1 : alimentation propre, 0 : rail affamé
	float souffle = 0.8f;   // 0 : pincé, 1 : nappe
	float halo = 0.5f;
	float hauteur = 0.5f;   // octaves au-dessus de do 2 (le V/OCT s'y ajoute)
	float timbre = 0.2f;    // triangle, carré, repli
	float derive = 0.3f;
	float memoire = 0.f;    // influence du registre sur les hauteurs
	float boucle = 0.3f;    // 0 : motif figé, 1 : mutation continue
	float densite = 1.f;    // part des voix ouvertes ; 1 = drone
	float cadence = 0.4f;   // vitesse du registre, de 0,05 Hz à 12,8 Hz
	float freqA = 0.8f, resoA = 0.15f;   // filtre A, échelle 24 dB : 30 Hz à 16 kHz, résonance jusqu'à l'auto-oscillation
	float freqB = 0.9f, resoB = 0.1f;    // filtre B, variables d'état 12 dB
	float routage = 0.f;    // 0 : série A puis B, 0,5 : parallèle, 1 : parallèle avec B en passe-bande
	LfoParams lfo[2];
	bool clockConnected = false, clockHigh = false;   // horloge externe du registre
	// TRIG : quand il est branché, les voix ne tiennent plus, chaque frappe les fait monter puis retomber
	bool trigMode = false;
	bool strike = false;    // une frappe à cet échantillon (trig ou bouton)
};

// Angle de chaque cran sur le cadran, en radians depuis midi (sens horaire), calé sur la course du potard
static const float DIAL_SWEEP = 0.83f * PI;
inline float slotAngle(float slot) { return -DIAL_SWEEP + slot * (2.f * DIAL_SWEEP / (SLOTS - 1)); }

// Poids d'un cran pour une destination D (en crans, sur un anneau de période SLOTS) et une demi-largeur h :
// fondu à puissance constante entre voisins quand h = 1, débordement en cloche au-delà.
inline float slotWeight(float D, int slot, float h) {
	float d = std::fabs(D - slot);
	d = std::fmod(d, (float) SLOTS);
	d = std::min(d, SLOTS - d);
	return d < h ? std::cos(0.5f * PI * d / h) : 0.f;
}

struct Lfo {
	float phase = 0.f, drift = 0.f;
	uint32_t cycle = 0;
	float rnd[4] = {0, 0, 0, 0};
	float value = 0.f;          // valeur à la phase 0, pour la sortie et l'affichage
	float D = 0.f, h = 1.f;     // destination effective (en crans) et demi-largeur, pour l'affichage

	static float lerp(float a, float b, float t) { return a + (b - a) * t; }
	float eval(float off, float shape) const {
		float t = phase + off;
		int idx = (int) std::floor(t);
		float f = t - idx;
		float a = rnd[(cycle + idx) & 3], b = rnd[(cycle + idx + 1) & 3];
		float v[5];
		v[0] = std::sin(2.f * PI * f);
		float g = f - 0.25f; g -= std::floor(g);
		v[1] = 4.f * std::fabs(g - 0.5f) - 1.f;
		v[2] = 2.f * f - 1.f;
		v[3] = lerp(a, b, 0.5f - 0.5f * std::cos(PI * f));
		v[4] = a;
		float sh = clampf(shape, 0.f, 1.f) * 4.f;
		int k = std::min(3, (int) sh);
		return lerp(v[k], v[k + 1], sh - k);
	}
};

struct Core {
	float sr = 48000, dt = 1.f / 48000, osDt = 1.f / 192000;
	Rng rng;

	// Oscillateurs (à la fréquence suréchantillonnée)
	float x[VOICES], dir[VOICES], tri[VOICES], sq[VOICES], lastSq[VOICES];
	Biquad aa1[VOICES], aa2[VOICES];

	// Dérive : processus d'Ornstein-Uhlenbeck par voix, plus une dérive thermique commune
	float drift[VOICES], thermal = 0.f;

	// Rail
	float rail = 1.f, railSmooth = 1.f, load = 0.f, drop = 0.f, humPhase = 0.f;
	bool alive[VOICES];
	float aliveGain[VOICES];

	// Registre
	float cells[CELLS], clockPhase = 0.f, memOffset[VOICES], memTarget[VOICES];
	bool wasOpen[VOICES];
	int ticks = 0;

	// Portes basse-bas (vactrol)
	float env[VOICES];
	int stage[VOICES]; // 0 repos/maintien, 1 attaque
	Svf lpg[VOICES];

	Ladder ladder[2];
	StateVariable svfB[2];
	float sFreqA = 0.8f, sResoA = 0.15f, sFreqB = 0.9f, sResoB = 0.1f, sRoutage = 0.f;

	Halo halo;
	float dcL = 0, dcR = 0, dcInL = 0, dcInR = 0;

	// Paramètres lissés
	float sEcart, sSerrage, sCourant, sHauteur, sTimbre, sTune[VOICES];

	// LFO et modulations qu'ils produisent à chaque échantillon
	Lfo lfo[2];
	float lfoWeight[2][SLOTS];
	float modSemi[VOICES], modEcart[VOICES], modDens[VOICES], modTimbre[VOICES];
	float modSerrage = 0, modCourant = 0, modSouffle = 0, modHalo = 0, modFreqA[2] = {0, 0}, modFreqB[2] = {0, 0};
	bool lastClock = false;

	// Pour le module : chaque voix après sa porte (avant les filtres), le pas du registre, l'état du couplage
	float voiceSignal[VOICES];
	bool tickedNow = false;
	float coupling = 0.f, chaos = 0.f;

	void init(float sampleRate, uint64_t seed = 1) {
		sr = sampleRate; dt = 1.f / sr; osDt = dt / OVERSAMPLE;
		rng.seed(seed);
		for (int i = 0; i < VOICES; i++) {
			x[i] = rng.uniform() * 2.f - 1.f;
			dir[i] = rng.uniform() < 0.5f ? 1.f : -1.f;
			tri[i] = x[i]; sq[i] = dir[i]; lastSq[i] = dir[i];
			aa1[i].lowpass(17000.f, sr * OVERSAMPLE, 0.5412f);
			aa2[i].lowpass(17000.f, sr * OVERSAMPLE, 1.3066f);
			drift[i] = rng.normal() * 0.5f;
			alive[i] = true; aliveGain[i] = 1.f;
			memOffset[i] = memTarget[i] = 0.f;
			wasOpen[i] = false;
			env[i] = 0.f; stage[i] = 0;
		}
		for (int k = 0; k < CELLS; k++) cells[k] = rng.uniform();
		for (int l = 0; l < 2; l++) {
			lfo[l] = Lfo();
			lfo[l].phase = rng.uniform();
			for (int k = 0; k < 4; k++) lfo[l].rnd[k] = rng.uniform() * 2.f - 1.f;
			for (int sl = 0; sl < SLOTS; sl++) lfoWeight[l][sl] = 0.f;
		}
		for (int i = 0; i < VOICES; i++) voiceSignal[i] = 0.f;
		halo.init(sr);
		sEcart = 0.5f; sSerrage = 0.2f; sCourant = 1.f; sHauteur = 0.5f; sTimbre = 0.2f;
		Params p;
		for (int i = 0; i < VOICES; i++) sTune[i] = p.tune[i];
	}

	static float shape(float t, float tr, float s) {
		if (t < 0.45f) {
			float u = t / 0.45f;
			return tr * (1.f - u) + s * 0.8f * u;
		}
		float u = (t - 0.45f) / 0.55f;
		float fold = std::sin(PI * 0.5f * (1.f + 5.f * u) * tr);
		float m = std::min(1.f, u * 3.f);
		return s * 0.8f * (1.f - m) + fold * m;
	}

	void tick(const Params& p) {
		ticks++;
		float last = cells[CELLS - 1];
		for (int k = CELLS - 1; k > 0; k--) cells[k] = cells[k - 1];
		cells[0] = rng.uniform() < p.boucle ? rng.uniform() : last;
		// Mémoire : sauts d'octave d'abord (l'accord reste juste), puis écarts libres au-delà de la moitié
		float m = p.memoire;
		for (int i = 0; i < VOICES; i++) {
			float c = cells[(i + 3) % CELLS] - 0.5f;
			float oct = std::round(c * 2.f * std::min(1.f, m * 2.f)) * 12.f;
			float freeAmt = std::max(0.f, m - 0.5f) * 2.f;
			float c2 = cells[(i + 5) % CELLS] - 0.5f;
			memTarget[i] = oct + c2 * freeAmt * 7.f;
		}
	}

	// Les deux LFO : vitesse (avec ce que l'autre lui envoie), destination effective sur le cadran, poids de chaque cran,
	// puis les modulations, évaluées avec un décalage de phase par voix (ÉVENTAIL) pour les destinations qui en ont un.
	void runLfos(const Params& p) {
		for (int i = 0; i < VOICES; i++) modSemi[i] = modEcart[i] = modDens[i] = modTimbre[i] = 0.f;
		modSerrage = modCourant = modSouffle = modHalo = 0.f;
		modFreqA[0] = modFreqA[1] = modFreqB[0] = modFreqB[1] = 0.f;
		float amt[2], metaRate[2], metaDest[2];
		for (int l = 0; l < 2; l++) {
			float d = clampf(p.lfo[l].depth, -1.f, 1.f);
			amt[l] = d * (0.5f + 0.5f * std::fabs(d));
		}
		// Ce que chaque LFO reçoit de l'autre, d'après les poids de l'échantillon précédent
		for (int l = 0; l < 2; l++) {
			int o = 1 - l;
			metaRate[l] = amt[o] * lfoWeight[o][SLOT_VITESSE] * lfo[o].value * SLOT_SCALE[SLOT_VITESSE];
			metaDest[l] = amt[o] * lfoWeight[o][SLOT_DESTINATION] * lfo[o].value * SLOT_SCALE[SLOT_DESTINATION];
		}
		for (int l = 0; l < 2; l++) {
			const LfoParams& lp = p.lfo[l];
			Lfo& f = lfo[l];
			// Asynchrones : chaque LFO dérive un peu, lentement, de son côté
			f.drift += -f.drift * 0.2f * dt + std::sqrt(0.4f * dt) * rng.normal();
			float oct = clampf(lp.rate, 0.f, 1.f) * 15.14f + lp.rateCv + metaRate[l];
			float hz = clampf(std::pow(2.f, oct) / 1200.f * std::exp(0.03f * f.drift), 1.f / 3600.f, 40.f);
			f.phase += hz * dt;
			if (f.phase >= 1.f) {
				f.phase -= std::floor(f.phase);
				f.cycle++;
				f.rnd[(f.cycle + 2) & 3] = rng.uniform() * 2.f - 1.f;
			}
			float D = clampf(lp.dest, 0.f, 1.f) * (SLOTS - 1) + lp.destCv * (SLOTS - 1) / 10.f + metaDest[l];
			D = std::fmod(D, (float) SLOTS);
			if (D < 0.f) D += SLOTS;
			f.D = D;
			f.h = 1.f + 5.f * std::pow(clampf(lp.width, 0.f, 1.f), 1.5f);
			for (int sl = 0; sl < SLOTS; sl++) lfoWeight[l][sl] = slotWeight(D, sl, f.h);
			f.value = lp.inConnected ? clampf(lp.in / 5.f, -1.5f, 1.5f) : f.eval(0.f, lp.shape);
			if (amt[l] == 0.f) continue;

			auto at = [&](float off) { return lp.inConnected ? f.value : f.eval(off, lp.shape); };
			float* w = lfoWeight[l];
			float g = amt[l];
			float spread = clampf(lp.spread, 0.f, 1.f);
			if (w[SLOT_HAUTEUR] > 0.f || w[SLOT_ECART] > 0.f || w[SLOT_DENSITE] > 0.f || w[SLOT_TIMBRE] > 0.f) {
				for (int i = 0; i < VOICES; i++) {
					float v = at(spread * i / VOICES);
					modSemi[i] += g * w[SLOT_HAUTEUR] * SLOT_SCALE[SLOT_HAUTEUR] * v;
					modEcart[i] += g * w[SLOT_ECART] * SLOT_SCALE[SLOT_ECART] * v;
					modDens[i] += g * w[SLOT_DENSITE] * SLOT_SCALE[SLOT_DENSITE] * v;
					modTimbre[i] += g * w[SLOT_TIMBRE] * SLOT_SCALE[SLOT_TIMBRE] * v;
				}
			}
			if (w[SLOT_FILTRE_A] > 0.f || w[SLOT_FILTRE_B] > 0.f) {
				float vL = f.value, vR = at(spread * 0.5f);
				modFreqA[0] += g * w[SLOT_FILTRE_A] * SLOT_SCALE[SLOT_FILTRE_A] * vL;
				modFreqA[1] += g * w[SLOT_FILTRE_A] * SLOT_SCALE[SLOT_FILTRE_A] * vR;
				modFreqB[0] += g * w[SLOT_FILTRE_B] * SLOT_SCALE[SLOT_FILTRE_B] * vL;
				modFreqB[1] += g * w[SLOT_FILTRE_B] * SLOT_SCALE[SLOT_FILTRE_B] * vR;
			}
			modSerrage += g * w[SLOT_SERRAGE] * SLOT_SCALE[SLOT_SERRAGE] * f.value;
			modCourant += g * w[SLOT_COURANT] * SLOT_SCALE[SLOT_COURANT] * f.value;
			modSouffle += g * w[SLOT_SOUFFLE] * SLOT_SCALE[SLOT_SOUFFLE] * f.value;
			modHalo += g * w[SLOT_HALO] * SLOT_SCALE[SLOT_HALO] * f.value;
		}
	}

	void process(const Params& p, float& outL, float& outR) {
		runLfos(p);
		const float k = 1.f - std::exp(-dt / 0.02f);
		sEcart += (p.ecart - sEcart) * k;
		sSerrage += (p.serrage - sSerrage) * k;
		sCourant += (p.courant - sCourant) * k;
		sHauteur += (p.hauteur - sHauteur) * k;
		sTimbre += (p.timbre - sTimbre) * k;
		for (int i = 0; i < VOICES; i++) sTune[i] += (p.tune[i] - sTune[i]) * k;

		// --- Registre et portes
		bool ticked = false;
		if (p.clockConnected) {
			if (p.clockHigh && !lastClock) { tick(p); ticked = true; }
		}
		else {
			float hz = 0.05f * std::pow(2.f, p.cadence * 8.f);
			clockPhase += hz * dt * (1.f + 0.03f * rng.normal());
			if (clockPhase >= 1.f) { clockPhase -= 1.f; tick(p); ticked = true; }
		}
		lastClock = p.clockHigh;
		tickedNow = ticked;
		float s = clampf(p.souffle + modSouffle, 0.f, 1.f);
		float attack = 0.003f * std::pow(2.f, s * 11.5f);
		float release = 0.08f * std::pow(2.f, s * 8.f);
		float sustain = p.trigMode ? 0.f : smoothstep(0.15f, 0.55f, s);
		float kA = 1.f - std::exp(-dt / attack), kR = 1.f - std::exp(-dt / release);
		float kMem = 1.f - std::exp(-dt / 0.006f);
		for (int i = 0; i < VOICES; i++) {
			float dens = clampf(p.densite + modDens[i], 0.f, 1.f);
			bool open = dens >= 0.999f || cells[i] < dens;
			// En mode trig, seules les frappes rouvrent les portes ; sinon le registre les rouvre à chaque pas
			if (open && (p.strike || (!p.trigMode && (!wasOpen[i] || ticked)))) stage[i] = 1;
			wasOpen[i] = open;
			if (stage[i] == 1) {
				env[i] += (1.15f - env[i]) * kA;
				if (env[i] >= 1.f) { env[i] = 1.f; stage[i] = 0; }
			}
			else {
				float target = open ? sustain : 0.f;
				// Le vactrol se referme de plus en plus lentement
				env[i] += (target - env[i]) * kR * (0.3f + 0.7f * env[i]);
			}
			memOffset[i] += (memTarget[i] - memOffset[i]) * kMem;
		}

		// --- Rail d'alimentation
		float c = clampf(sCourant + modCourant, 0.f, 1.f), starve = 1.f - c;
		float nominal = 0.55f + 0.45f * c;
		float sagK = 0.04f + 0.6f * starve * starve;
		float target = 0.f;
		for (int i = 0; i < VOICES; i++) target += env[i] * aliveGain[i];
		target /= VOICES;
		load += (target - load) * (target > load ? 1.f - std::exp(-dt / 0.005f) : 1.f - std::exp(-dt / 0.08f));
		humPhase += 50.f * dt; if (humPhase >= 1.f) humPhase -= 1.f;
		if (rng.uniform() < starve * starve * starve * 30.f * dt) drop -= 0.3f * rng.uniform();
		drop *= std::exp(-dt / 0.004f);
		float r = nominal - sagK * load + starve * starve * (0.04f * std::sin(2.f * PI * humPhase) + 0.01f * rng.normal()) + drop;
		rail = clampf(r, 0.05f, 1.05f);
		railSmooth += (rail - railSmooth) * (1.f - std::exp(-dt / 0.001f));
		float railPitch = std::pow(railSmooth, 0.35f);
		float kAlive = 1.f - std::exp(-dt / 0.001f);
		for (int i = 0; i < VOICES; i++) {
			float dropout = 0.33f + 0.12f * i / (VOICES - 1);
			if (alive[i] && railSmooth < dropout) alive[i] = false;
			else if (!alive[i] && railSmooth > dropout + 0.04f) alive[i] = true;
			aliveGain[i] += ((alive[i] ? 1.f : 0.f) - aliveGain[i]) * kAlive;
		}

		// --- Dérive
		float derive = p.derive;
		const float theta = 0.5f, sigma = std::sqrt(2.f * theta) * std::sqrt(dt);
		for (int i = 0; i < VOICES; i++) drift[i] += -drift[i] * theta * dt + sigma * rng.normal();
		thermal += -thermal * 0.05f * dt + std::sqrt(0.1f) * std::sqrt(dt) * rng.normal();

		// --- Fréquences
		float root = 65.41f * std::pow(2.f, sHauteur);
		float freq[VOICES];
		float serrage = clampf(sSerrage + modSerrage, 0.f, 1.f);
		// La voix 1 est la référence ; les autres sont attirées vers un intervalle juste avec elle
		float width = 0.8f * smoothstep(0.f, 0.5f, serrage);
		float semi[VOICES];
		for (int i = 0; i < VOICES; i++) {
			float ecartScale = 2.f * clampf(sEcart + modEcart[i], 0.f, 1.f);
			semi[i] = sTune[i] * ecartScale + memOffset[i] + derive * 12.f * 0.02f * drift[i] + modSemi[i];
		}
		for (int i = 1; i < VOICES; i++) semi[i] = semi[0] + attract(semi[i] - semi[0], width);
		for (int i = 0; i < VOICES; i++) {
			float oct = semi[i] / 12.f + derive * 0.01f * thermal;
			freq[i] = clampf(root * std::pow(2.f, oct) * railPitch, 0.5f, 0.4f * sr);
		}

		// --- Couplage de l'anneau
		// Une fois les hauteurs attirées, un petit couplage par impulsions verrouille la phase : quand une voisine bascule,
		// la voix reçoit un coup qui la décale. FM croisée au-delà, puis chaos.
		float eps = 0.06f * smoothstep(0.f, 0.6f, serrage);
		float b = 2.2f * smoothstep(0.35f, 1.f, serrage);
		coupling = smoothstep(0.f, 0.5f, serrage);
		chaos = clampf(b / 2.2f + 0.8f * starve * starve, 0.f, 1.f);
		float timbre[VOICES];
		for (int i = 0; i < VOICES; i++) timbre[i] = clampf(sTimbre + modTimbre[i], 0.f, 1.f);

		float voiceOut[VOICES];
		for (int i = 0; i < VOICES; i++) voiceOut[i] = 0.f;
		for (int o = 0; o < OVERSAMPLE; o++) {
			float prevTri[VOICES], prevSq[VOICES];
			for (int i = 0; i < VOICES; i++) { prevTri[i] = tri[i]; prevSq[i] = sq[i]; }
			for (int i = 0; i < VOICES; i++) {
				int j = (i + VOICES - 1) % VOICES, jn = (i + 1) % VOICES;
				float fm = clampf(1.f + b * prevTri[j], -0.5f, 3.f);
				x[i] += dir[i] * 4.f * freq[i] * fm * osDt;
				// Coup proportionnel à la position : retard juste après un basculement, avance près du seuil, nul en moyenne
				x[i] += eps * x[i] * (std::fabs(prevSq[j] - lastSq[j]) * 0.5f + std::fabs(prevSq[jn] - lastSq[jn]) * 0.25f);
				if (dir[i] > 0.f && x[i] >= 1.f) { x[i] = 1.f - (x[i] - 1.f); dir[i] = -1.f; }
				else if (dir[i] < 0.f && x[i] <= -1.f) { x[i] = -1.f + (-1.f - x[i]); dir[i] = 1.f; }
				x[i] = clampf(x[i], -1.5f, 1.5f);
				tri[i] = x[i];
				sq[i] = dir[i];
				float w = shape(timbre[i], clampf(x[i], -1.f, 1.f), sq[i]);
				w = aa2[i].process(aa1[i].process(w));
				if (o == OVERSAMPLE - 1) voiceOut[i] = w;
			}
			for (int i = 0; i < VOICES; i++) lastSq[i] = prevSq[i];
		}

		// --- Portes basse-bas, saturation par le rail, panoramique
		static const float PAN[VOICES] = {-0.8f, 0.5f, -0.3f, 0.8f, -0.55f, 0.25f};
		float L = 0.f, R = 0.f;
		for (int i = 0; i < VOICES; i++) {
			float v = railSmooth * std::tanh(voiceOut[i] * 1.2f / railSmooth);
			float e = env[i] * aliveGain[i];
			float fc = std::min(40.f * std::pow(2.f, e * 8.5f) * (0.6f + 0.8f * sTimbre), 0.45f * sr);
			float g = std::tan(PI * fc / sr);
			v = lpg[i].lowpass(v, g, 1.4f) * std::pow(e, 1.3f);
			voiceSignal[i] = v;
			float pp = (PAN[i] + 1.f) * 0.25f * PI;
			L += v * std::cos(pp);
			R += v * std::sin(pp);
		}
		// Un rail affamé écrase le signal ; on en rend une partie pour que la pile mourante reste audible
		float makeup = 0.55f / std::sqrt(railSmooth);
		L *= makeup; R *= makeup;
		for (int i = 0; i < VOICES; i++) voiceSignal[i] = 5.f * std::tanh(2.f * makeup * voiceSignal[i]);

		// --- Filtres résonants
		sFreqA += (p.freqA - sFreqA) * k; sResoA += (p.resoA - sResoA) * k;
		sFreqB += (p.freqB - sFreqB) * k; sResoB += (p.resoB - sResoB) * k;
		sRoutage += (p.routage - sRoutage) * k;
		{
			// Suivi de clavier à 50 % ; un rail affamé sature plus fort et étouffe la résonance
			float track = std::pow(2.f, 0.5f * (sHauteur - 0.5f));
			float railK = 0.55f + 0.45f * railSmooth;
			float drive = 1.2f / railSmooth;
			float resA = 4.2f * sResoA * railK;
			float resB = clampf(2.f - 1.98f * sResoB * railK, 0.02f, 2.f);
			float series = clampf(1.f - 2.f * sRoutage, 0.f, 1.f);     // 1 en série, 0 dès la moitié
			float bandB = clampf(2.f * sRoutage - 1.f, 0.f, 1.f);      // passe-bande au-delà de la moitié
			float in[2] = {L, R}, out[2];
			for (int ch = 0; ch < 2; ch++) {
				// Le canal droit est un peu décalé, pour l'ampleur
				float spread = ch ? 1.03f : 1.f;
				float fa = clampf(30.f * std::pow(2.f, clampf(sFreqA + modFreqA[ch], 0.f, 1.f) * 9.1f) * track, 20.f, 0.4f * sr);
				float fb = clampf(30.f * std::pow(2.f, clampf(sFreqB + modFreqB[ch], 0.f, 1.f) * 9.1f) * track, 20.f, 0.4f * sr);
				float gA = 1.f - std::exp(-2.f * PI * fa * spread / (2.f * sr));
				float outA = ladder[ch].process(in[ch], gA, resA, drive);
				float gB = std::tan(PI * std::min(fb * spread, 0.45f * sr) / sr);
				float bIn = in[ch] + (outA - in[ch]) * series;
				float lp, bp;
				svfB[ch].process(bIn, gB, resB, lp, bp);
				float outB = lp + (bp * 1.4f - lp) * bandB;
				// En série, B reçoit A ; en parallèle, on mélange les deux
				out[ch] = outB * series + 0.6f * (outA + outB) * (1.f - series);
			}
			L = out[0]; R = out[1];
		}

		// --- Halo
		float hL, hR;
		float haloAmt = clampf(p.halo + modHalo, 0.f, 1.f);
		halo.process(L, R, haloAmt, hL, hR);
		float dry = 1.f - 0.45f * haloAmt, wet = 0.9f * haloAmt;
		L = L * dry + hL * wet;
		R = R * dry + hR * wet;

		// Bloqueur de continu, puis ±5 V
		dcL = L - dcInL + 0.9995f * dcL; dcInL = L;
		dcR = R - dcInR + 0.9995f * dcR; dcInR = R;
		outL = 5.f * std::tanh(dcL);
		outR = 5.f * std::tanh(dcR);
	}
};

} // namespace lucienne
