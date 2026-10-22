// Banc d'essai hors de Rack pour le moteur de Louis : planchers, densité, mémoire, quantification, calage sur
// l'horloge, liens, gates, arrêt d'horloge, RESET et STRIKE.
#include "../src/LouisCore.hpp"
#include <cstdio>
#include <vector>
#include <chrono>
#include <functional>
#include <cstdlib>

using namespace louis;

static int failures = 0;
static void check(bool ok, const char* what) {
	std::printf("%s %s\n", ok ? "  ok " : "  ÉCHEC", what);
	if (!ok) failures++;
}

static const float SR = 48000.f;
static const float DT = 1.f / SR;

struct Log {
	std::vector<double> onsets;
	std::vector<float> pitches;        // hauteur à chaque note
	std::vector<int> cycles;           // 1 note, 0 silence, dans l'ordre des cycles
	std::vector<float> cyclePitch;     // hauteur sortie au début de chaque cycle
	int gateRises = 0, trigRises = 0;
	float minAttack = 1e9f, minDecay = 1e9f, minCycle = 1e9f;
};

struct Rig {
	Core core;
	Params p;
	Events e;
	Outputs o;
	Log log[VOICES];
	std::vector<double> pulses;
	double clockPeriod = 0.0, nextPulse = 0.0, clockStop = 1e18;
	int links = 0;
	bool finite = true, envInRange = true;
	float prevGate[VOICES] = {}, prevTrig[VOICES] = {};

	explicit Rig(uint64_t seed = 5) { core.init(seed); }

	void clock(double period, double from) {
		p.clockConnected = true;
		clockPeriod = period;
		nextPulse = from;
	}

	void run(double seconds, std::function<void(double, Rig&)> each = nullptr) {
		long n = std::lround(seconds * SR);
		for (long k = 0; k < n; k++) {
			double t = core.now;
			e = Events();
			if (clockPeriod > 0.0 && t < clockStop && t >= nextPulse - 0.5 * DT) {
				e.clock = true;
				pulses.push_back(t);
				nextPulse += clockPeriod;
			}
			if (each) each(t, *this);
			core.process(DT, p, e, o);
			for (int i = 0; i < VOICES; i++) {
				Log& l = log[i];
				if (o.cycle[i]) {
					const Voice& w = core.v[i];
					l.cycles.push_back(w.note ? 1 : 0);
					l.cyclePitch.push_back(o.pitch[i]);
					l.minAttack = std::min(l.minAttack, w.attack);
					l.minDecay = std::min(l.minDecay, w.decay);
					l.minCycle = std::min(l.minCycle, w.total());
				}
				if (o.onset[i]) {
					l.onsets.push_back(t);
					l.pitches.push_back(o.pitch[i]);
				}
				if (o.link[i]) links++;
				if (o.gate[i] > 5.f && prevGate[i] <= 5.f) l.gateRises++;
				if (o.trig[i] > 5.f && prevTrig[i] <= 5.f) l.trigRises++;
				prevGate[i] = o.gate[i];
				prevTrig[i] = o.trig[i];
				if (!std::isfinite(o.env[i]) || !std::isfinite(o.pitch[i])) finite = false;
				if (o.env[i] < 0.f || o.env[i] > 10.0001f) envInRange = false;
			}
		}
	}

	int notes() const {
		int s = 0;
		for (int i = 0; i < VOICES; i++) s += (int) log[i].onsets.size();
		return s;
	}
	double minInterOnset() const {
		double m = 1e9;
		for (int i = 0; i < VOICES; i++)
			for (size_t k = 1; k < log[i].onsets.size(); k++) m = std::min(m, log[i].onsets[k] - log[i].onsets[k - 1]);
		return m;
	}
};

// Distance (en échantillons) entre un instant et l'instant de grille le plus proche
static double gridError(double t, const std::vector<double>& pulses, int sub, int step, size_t origin) {
	double best = 1e9;
	for (size_t k = origin; k + 1 < pulses.size(); k++) {
		double a = pulses[k], b = pulses[k + 1];
		for (int j = 0; j < sub; j++) {
			int64_t unit = (int64_t) (k - origin) * sub + j;
			if (unit % step) continue;
			best = std::min(best, std::fabs(t - (a + (b - a) * j / sub)));
		}
	}
	return best * SR;
}

static bool inMask(float volts, float ref, int mask) {
	float s = (volts - ref) * 12.f;
	int n = (int) std::lround(s);
	return std::fabs(s - n) < 1e-3f && ((mask >> (int) posMod(n, 12)) & 1);
}

int main() {
	std::printf("Planchers, quels que soient les réglages et les CV\n");
	{
		Rig r(9);
		Rng chaos;
		chaos.seed(3);
		auto tweak = [&](double t, Rig& g) {
			static double next = 0.0;
			if (t >= next) {
				next = t + 0.25;
				g.p.pace = 0.001f + chaos.uniform() * 0.3f;
				g.p.tilt = chaos.uniform();
				g.p.wander = chaos.uniform();
				g.p.drift = 0.01f + chaos.uniform() * 2.f;
				g.p.correl = chaos.uniform() * 2.f - 1.f;
				g.p.link = chaos.uniform();
				g.p.linkMode = (int) (chaos.uniform() * 3.f) % 3;
				g.p.memory = chaos.uniform();
				g.p.gate = chaos.uniform();
				for (int i = 0; i < VOICES; i++) g.p.time[i] = (int) (chaos.uniform() * TIME_COUNT) % TIME_COUNT;
			}
			for (int i = 0; i < VOICES; i++) g.e.strike[i] = chaos.uniform() < 0.002f;
		};
		r.run(30.0, tweak);
		float ma = 1e9f, md = 1e9f, mc = 1e9f;
		for (int i = 0; i < VOICES; i++) {
			ma = std::min(ma, r.log[i].minAttack);
			md = std::min(md, r.log[i].minDecay);
			mc = std::min(mc, r.log[i].minCycle);
		}
		std::printf("    %d notes ; attaque ≥ %.4f s, decay ≥ %.4f s, cycle ≥ %.4f s, écart entre notes ≥ %.4f s\n",
			r.notes(), ma, md, mc, r.minInterOnset());
		check(ma >= MIN_ATTACK - 1e-6f && md >= MIN_DECAY - 1e-6f && mc >= MIN_CYCLE - 1e-6f, "aucun segment sous les planchers");
		check(r.minInterOnset() >= MIN_CYCLE - 1.5 * DT, "jamais deux notes d'une voix à moins de 15 ms");
		check(r.finite && r.envInRange, "sorties finies, enveloppe entre 0 et 10 V");
	}

	std::printf("Densité\n");
	{
		Rig r;
		r.p.density = 0.f;
		r.p.wander = 1.f;
		r.run(30.0);
		int gates = 0;
		for (int i = 0; i < VOICES; i++) gates += r.log[i].gateRises;
		check(r.notes() == 0 && gates == 0, "DENSITY à 0 : rien ne sort");

		Rig s;
		s.p.density = 1.f;
		s.p.wander = 1.f;
		s.run(30.0);
		int rests = 0, cycles = 0;
		for (int i = 0; i < VOICES; i++)
			for (size_t k = 1; k < s.log[i].cycles.size(); k++) {   // le premier cycle est le silence de départ
				cycles++;
				rests += 1 - s.log[i].cycles[k];
			}
		std::printf("    à 100 %% : %d cycles, %d silences\n", cycles, rests);
		check(cycles > 50 && rests == 0, "DENSITY à 100 % sans liens : aucun silence");

		Rig h;
		h.p.density = 0.5f;
		h.run(60.0);
		int c2 = 0, n2 = 0;
		for (int i = 0; i < VOICES; i++)
			for (size_t k = 1; k < h.log[i].cycles.size(); k++) {
				c2++;
				n2 += h.log[i].cycles[k];
			}
		std::printf("    à 50 %% : %d notes sur %d cycles\n", n2, c2);
		check(n2 > 0.35 * c2 && n2 < 0.65 * c2, "DENSITY à 50 % : à peu près une note sur deux");
	}

	std::printf("Mémoire\n");
	{
		Rig r;
		r.p.memory = 1.f;
		r.p.wander = 0.f;
		r.p.density = 0.6f;
		r.p.scale = 2;
		r.p.loopLength = 8;
		r.run(40.0);
		bool same = true;
		int compared = 0;
		for (int i = 0; i < VOICES; i++) {
			const Log& l = r.log[i];
			// Cycle 0 : silence de départ ; les cycles 1 à 8 remplissent la boucle, puis elle se répète
			for (size_t k = 9; k < l.cycles.size(); k++) {
				compared++;
				if (l.cycles[k] != l.cycles[k - 8]) same = false;
				if (l.cycles[k] && std::fabs(l.cyclePitch[k] - l.cyclePitch[k - 8]) > 1e-6f) same = false;
			}
		}
		std::printf("    %d cycles comparés\n", compared);
		check(compared > 40 && same, "MEMORY à 100 % : la boucle se répète à l'identique");

		Rig f;
		f.p.memory = 0.f;
		f.p.wander = 0.f;
		f.p.density = 1.f;
		f.p.loopLength = 8;
		f.run(40.0, [](double t, Rig& g) { g.p.hold[1] = t > 10.0; });
		const Log& l = f.log[1];
		bool held = true, freeBefore = false;
		size_t firstHeld = 0;
		// Sans HOLD les hauteurs varient ; avec HOLD elles reviennent toutes les 8 notes
		std::vector<float> ps = l.pitches;
		for (size_t k = 8; k < ps.size() / 4; k++) if (ps[k] != ps[k - 8]) freeBefore = true;
		for (size_t k = ps.size() / 2; k < ps.size(); k++) {
			firstHeld++;
			if (k >= 8 && std::fabs(ps[k] - ps[k - 8]) > 1e-6f) held = false;
		}
		check(freeBefore && held && firstHeld > 8, "HOLD fige la boucle d'une voix");
	}

	std::printf("Quantification\n");
	{
		Rig r;
		r.p.scale = 2;   // MAJOR
		r.p.root = 2.f / 12.f;
		r.p.range = 3.f;
		r.run(30.0);
		bool ok = r.notes() > 50;
		for (int i = 0; i < VOICES; i++)
			for (float v : r.log[i].pitches) if (!inMask(v, r.p.root, SCALES[2].mask)) ok = false;
		check(ok, "MAJOR en ré : chaque hauteur est dans la gamme");

		Rig f;
		f.p.scale = SCALE_FREE;
		f.run(30.0);
		int off = 0, all = 0;
		for (int i = 0; i < VOICES; i++)
			for (float v : f.log[i].pitches) {
				all++;
				float s = v * 12.f;
				if (std::fabs(s - std::round(s)) > 0.01f) off++;
			}
		check(all > 50 && off > 0.9 * all, "FREE : des hauteurs continues");

		Rig x;
		x.p.scale = 2;
		x.p.root = 5.f / 12.f;
		x.p.extMask = bits({0, 4, 7});
		x.run(30.0);
		bool ok2 = x.notes() > 50;
		for (int i = 0; i < VOICES; i++)
			for (float v : x.log[i].pitches) if (!inMask(v, 0.f, x.p.extMask)) ok2 = false;
		check(ok2, "SCALE IN remplace le sélecteur (notes absolues)");

		Rig k;
		k.p.scale = 11;
		k.p.range = 2.f;
		k.p.skew = -1.f;
		k.p.wander = 0.f;
		k.run(30.0);
		Rig u;
		u.p.scale = 11;
		u.p.range = 2.f;
		u.p.skew = 1.f;
		u.p.wander = 0.f;
		u.run(30.0);
		auto mean = [](const Rig& g) {
			double s = 0; int n = 0;
			for (int i = 0; i < VOICES; i++)
				for (float v : g.log[i].pitches) { s += v - g.p.oct[i]; n++; }
			return n ? s / n : 0.0;
		};
		std::printf("    hauteur moyenne dans la plage : SKEW −1 %.2f V, SKEW +1 %.2f V\n", mean(k), mean(u));
		check(mean(k) < 0.6 && mean(u) > 1.4, "SKEW penche vers le grave ou l'aigu");
	}

	std::printf("Horloge stable, SNAP 100 %%\n");
	{
		const int grids[3] = {3, 4, 2};   // 1, 2, 1/2
		for (int gi = 0; gi < 3; gi++) {
			int g = grids[gi];
			Rig r(21 + g);
			r.p.grid = g;
			r.p.pace = 0.4f;
			r.p.wander = 0.8f;
			r.clock(0.125, 0.05);
			r.run(30.0);
			double worst = 0;
			int n = 0;
			for (int i = 0; i < VOICES; i++)
				for (double t : r.log[i].onsets) {
					if (t > r.pulses.back() - 0.5) continue;
					worst = std::max(worst, gridError(t, r.pulses, UNIT_SUB, GRID_UNITS[g], 0));
					n++;
				}
			std::printf("    GRID %s : %d notes, écart maximal %.2f échantillon(s)\n", GRID_NAMES[g], n, worst);
			check(n > 50 && worst <= 1.0, "chaque note commence sur la grille, à un échantillon près");
		}

		Rig c;
		c.clock(0.125, 0.05);
		c.p.clockConnected = false;
		c.clockPeriod = 0.125;
		c.run(30.0);
		int offGrid = 0, all = 0;
		for (int i = 0; i < VOICES; i++)
			for (double t : c.log[i].onsets) {
				all++;
				if (gridError(t, c.pulses, 1, 1, 0) > 2.0) offGrid++;
			}
		check(all > 50 && offGrid > 0.8 * all, "CLOCK IN débranchée : timing libre");
	}

	std::printf("Changement de tempo\n");
	{
		Rig r(4);
		r.p.grid = 2;   // 1/2 : instants prédits entre les coups
		r.p.pace = 0.3f;
		r.clock(0.125, 0.05);
		r.run(10.0);
		r.clockPeriod = 0.1;
		double change = r.pulses.back();
		r.run(20.0);
		double worst = 0;
		int n = 0;
		for (int i = 0; i < VOICES; i++)
			for (double t : r.log[i].onsets) {
				if (t < change + 0.125 + 2 * 0.1 || t > r.pulses.back() - 0.5) continue;   // le premier coup après `change` garde l'ancien intervalle
				worst = std::max(worst, gridError(t, r.pulses, 2, 1, 0));
				n++;
			}
		std::printf("    après deux pas : %d notes, écart maximal %.2f échantillon(s)\n", n, worst);
		check(n > 50 && worst <= 1.0, "retour sur la grille en moins de deux pas");
	}

	std::printf("TIME : une grille par voix\n");
	{
		Rig r(31);
		const int T[VOICES] = {4, 6, 2, 5};   // 1, 2, 1/2, 3/2
		for (int i = 0; i < VOICES; i++) r.p.time[i] = T[i];
		r.p.pace = 0.3f;
		r.p.wander = 0.f;   // sans dérive, la vitesse de chaque voix ne dépend que de PACE × TIME
		r.clock(0.125, 0.05);
		r.run(40.0);
		bool ok = true;
		double mean[VOICES];
		for (int i = 0; i < VOICES; i++) {
			double worst = 0;
			int n = 0;
			for (double t : r.log[i].onsets) {
				if (t > r.pulses.back() - 0.5) continue;
				worst = std::max(worst, gridError(t, r.pulses, UNIT_SUB, voiceStep(r.p.grid, T[i]), 0));
				n++;
			}
			const std::vector<double>& on = r.log[i].onsets;
			mean[i] = on.size() > 1 ? (on.back() - on.front()) / (on.size() - 1) : 0.0;
			std::printf("    voix %d, TIME %s : %d notes, écart à sa grille %.2f échantillon(s), une note toutes les %.2f s\n",
				i + 1, TIME_NAMES[T[i]], n, worst, mean[i]);
			if (n < 20 || worst > 1.0) ok = false;
		}
		check(ok, "chaque voix tombe sur sa propre grille (GRID × TIME)");
		check(mean[2] < mean[0] && mean[0] < mean[3] && mean[3] < mean[1], "TIME règle aussi la vitesse : 1/2 < 1 < 3/2 < 2");
	}

	std::printf("SNAP partiel\n");
	{
		Rig r(8);
		r.p.snap = 0.5f;
		r.p.wander = 1.f;
		r.clock(0.125, 0.05);
		r.run(30.0);
		Rig f(8);
		f.p.wander = 1.f;
		f.clockPeriod = 0.125;
		f.nextPulse = 0.05;
		f.run(30.0);
		auto meanErr = [](const Rig& g) {
			double s = 0; int n = 0;
			for (int i = 0; i < VOICES; i++)
				for (double t : g.log[i].onsets) if (t > 1.0 && t < 29.0) { s += gridError(t, g.pulses, 1, 1, 0); n++; }
			return n ? s / n : 0.0;
		};
		std::printf("    écart moyen à la grille : libre %.0f, SNAP 50 %% %.0f échantillons\n", meanErr(f), meanErr(r));
		check(meanErr(r) < 0.8 * meanErr(f), "SNAP à 50 % rapproche les notes de la grille sans les y coller");
	}

	std::printf("Liens\n");
	{
		Rig r(12);
		r.p.link = 1.f;
		r.p.linkMode = LINK_CHAIN;
		r.p.wander = 1.f;
		r.p.pace = 0.2f;
		r.run(30.0);
		int together = 0, all = 0;
		for (double t : r.log[0].onsets) {
			all++;
			for (double u : r.log[1].onsets) if (std::fabs(u - t) < 0.5 * DT) { together++; break; }
		}
		std::printf("    %d liens, plus petit écart entre deux notes d'une voix %.4f s\n", r.links, r.minInterOnset());
		check(r.links > 50 && r.minInterOnset() >= MIN_CYCLE - 1.5 * DT, "CHAIN à 100 % : cascades, période réfractaire tenue");
		check(together > 0 && together < all, "CHAIN fait jouer la suivante avec la voix, sans les figer ensemble");

		Rig h(13);
		h.p.link = 1.f;
		h.p.linkMode = LINK_HUSH;
		h.p.density = 1.f;
		h.run(30.0);
		int rests = 0;
		for (int i = 0; i < VOICES; i++)
			for (size_t k = 1; k < h.log[i].cycles.size(); k++) rests += 1 - h.log[i].cycles[k];
		check(rests > 20, "HUSH impose des silences même à DENSITY 100 %");

		Rig e(14);
		e.p.link = 1.f;
		e.p.linkMode = LINK_ECHO;
		e.p.scale = 2;
		e.run(30.0);
		bool ok = e.notes() > 50;
		for (int i = 0; i < VOICES; i++)
			for (float v : e.log[i].pitches) if (!inMask(v, e.p.root, SCALES[2].mask)) ok = false;
		check(ok && e.links > 20, "ECHO reste dans la gamme");
	}

	std::printf("Gates\n");
	{
		Rig r(15);
		r.p.gate = 1.f;
		r.p.wander = 1.f;
		r.p.link = 0.6f;
		r.p.linkMode = LINK_CHAIN;
		r.run(30.0, [](double, Rig& g) { for (int i = 0; i < VOICES; i++) g.e.strike[i] = (std::rand() % 4000) == 0; });
		bool ok = true;
		for (int i = 0; i < VOICES; i++)
			if (r.log[i].gateRises != (int) r.log[i].onsets.size() || r.log[i].trigRises != (int) r.log[i].onsets.size()) ok = false;
		check(ok, "GATE à 100 % : un front de gate et de trig par note, même coupée");
	}

	std::printf("Arrêt de l'horloge\n");
	{
		Rig r(16);
		r.clock(0.125, 0.05);
		r.clockStop = 10.0;
		r.run(20.0);
		double last = r.pulses.back();
		int after = 0;
		for (int i = 0; i < VOICES; i++)
			for (double t : r.log[i].onsets) if (t > last + DT) after++;
		check(after == 0, "WAIT : plus aucune note après le dernier coup");

		Rig f(16);
		f.p.clockStopFree = true;
		f.clock(0.125, 0.05);
		f.clockStop = 10.0;
		f.run(20.0);
		int later = 0;
		for (int i = 0; i < VOICES; i++)
			for (double t : f.log[i].onsets) if (t > 12.0) later++;
		check(later > 10, "FREE : les voix repartent en timing libre");

		f.clockStop = 1e18;
		f.nextPulse = 20.0;
		size_t resume = f.pulses.size();
		f.run(20.0);
		double worst = 0;
		int n = 0;
		for (int i = 0; i < VOICES; i++)
			for (double t : f.log[i].onsets) if (t > 20.0 + 3 * 0.125 + 2.0 && t < f.pulses.back() - 0.5) {
				worst = std::max(worst, gridError(t, f.pulses, 1, 1, resume));
				n++;
			}
		check(n > 20 && worst <= 1.0, "au retour de l'horloge, les notes reviennent sur la grille");
	}

	std::printf("RESET et STRIKE\n");
	{
		Rig r(17);
		r.p.grid = 4;
		r.clock(0.125, 0.05);
		double resetAt = 5.03;
		r.run(10.0, [&](double t, Rig& g) { g.e.reset = std::fabs(t - resetAt) < 0.5 * DT; });
		double firstPulse = 0;
		for (double t : r.pulses) if (t > resetAt) { firstPulse = t; break; }
		bool all = true;
		for (int i = 0; i < VOICES; i++) {
			bool hit = false;
			for (double t : r.log[i].onsets) if (std::fabs(t - firstPulse) < 0.5 * DT) hit = true;
			for (double t : r.log[i].onsets) if (t > resetAt && t < firstPulse - 0.5 * DT) all = false;
			if (!hit) all = false;
		}
		check(all, "horloge branchée : RESET relance les quatre voix sur le coup suivant");
		// Après RESET, la grille de 2 coups compte depuis ce coup-là
		size_t origin = 0;
		while (origin < r.pulses.size() && r.pulses[origin] < firstPulse - 0.5 * DT) origin++;
		double worst = 0;
		for (int i = 0; i < VOICES; i++)
			for (double t : r.log[i].onsets) if (t > firstPulse && t < r.pulses.back() - 0.5)
				worst = std::max(worst, gridError(t, r.pulses, 1, 2, origin));
		check(worst <= 1.0, "après RESET, la grille part de ce coup");

		Rig s(18);
		s.p.density = 0.f;
		double strikeAt = 3.0;
		s.run(5.0, [&](double t, Rig& g) { g.e.strike[2] = std::fabs(t - strikeAt) < 0.5 * DT; });
		check(s.log[2].onsets.size() == 1 && std::fabs(s.log[2].onsets[0] - strikeAt) < DT && s.notes() == 1,
			"STRIKE joue une note tout de suite, même à DENSITY 0");

		Rig c(19);
		c.clock(0.125, 0.05);
		strikeAt = 3.11;
		c.p.density = 0.f;
		c.run(5.0, [&](double t, Rig& g) { g.e.strike[0] = std::fabs(t - strikeAt) < 0.5 * DT; });
		double next = 0;
		for (double t : c.pulses) if (t > strikeAt) { next = t; break; }
		check(c.log[0].onsets.size() == 1 && std::fabs(c.log[0].onsets[0] - next) < 0.5 * DT,
			"horloge branchée : STRIKE attend le coup suivant");
	}

	std::printf("Coût\n");
	{
		Rig r;
		r.p.wander = 1.f;
		r.p.link = 0.5f;
		auto t0 = std::chrono::steady_clock::now();
		r.run(60.0);
		double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		std::printf("    %.0f× le temps réel (banc compris)\n", 60.0 / el);
		check(60.0 / el > 50.0, "assez léger pour Rack");
	}

	std::printf(failures ? "\n%d échec(s)\n" : "\nTout est bon\n", failures);
	return failures ? 1 : 0;
}
