// Banc d'essai hors de Rack pour le moteur de Gaston : horloge, swing, rapports, longueurs, sens, probabilité,
// transport façon CDJ, horloge externe, REC, réalignement, timing vintage, voies CV.
#include "../src/GastonCore.hpp"
#include <cstdio>
#include <vector>
#include <functional>

using namespace gaston;

static int failures = 0;
static void check(bool ok, const char* what) {
	std::printf("%s %s\n", ok ? "  ok " : "  ÉCHEC", what);
	if (!ok) failures++;
}
static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static const float SR = 48000.f;
static const float DT = 1.f / SR;

// Fronts montants relevés pendant un rendu
struct Edges {
	std::vector<double> trig[TRIG_LANES], clock, reset;
	std::vector<float> cv[CV_LANES];
};

struct Rig {
	Core core;
	Settings st;
	Outputs out;
	double t = 0.0;
	float prevTrig[TRIG_LANES] = {}, prevClock = 0.f, prevReset = 0.f;

	Rig() { core.rng.seed(11); }

	void run(double seconds, Edges& e, std::function<void(double)> each = nullptr) {
		int n = (int) std::lround(seconds * SR);
		for (int k = 0; k < n; k++) {
			if (each) each(t);
			core.process(DT, st, out);
			for (int i = 0; i < TRIG_LANES; i++) {
				if (out.trig[i] > 5.f && prevTrig[i] <= 5.f) e.trig[i].push_back(t);
				prevTrig[i] = out.trig[i];
			}
			if (out.clock > 5.f && prevClock <= 5.f) e.clock.push_back(t);
			if (out.reset > 5.f && prevReset <= 5.f) e.reset.push_back(t);
			prevClock = out.clock;
			prevReset = out.reset;
			for (int c = 0; c < CV_LANES; c++) e.cv[c].push_back(out.cv[c]);
			t += DT;
		}
	}
};

static const double STEP120 = 0.125; // une double croche à 120 BPM

static void testClockAndLength() {
	std::printf("Horloge interne, longueurs\n");
	Rig r;
	Lane& a = r.core.lanes[0];
	a.on[0] = a.on[4] = a.on[8] = a.on[12] = true;
	Lane& b = r.core.lanes[1];
	b.length = 5; b.on[0] = true;
	r.core.rewind(r.st); r.core.play();
	Edges e; r.run(2.0 + 1e-3, e);
	check(e.trig[0].size() == 5, "4 temps par mesure : 5 frappes en 2 s (0 ; 0,5 ; 1 ; 1,5 ; 2)");
	check(near(e.trig[0][0], 0.0, 1e-4) && near(e.trig[0][1], 0.5, 2e-4), "la première frappe part tout de suite, puis toutes les 0,5 s");
	check(e.trig[1].size() == 4 && near(e.trig[1][1], 5 * STEP120, 2e-4), "longueur 5 : un pas 1 toutes les 5 doubles croches");
	check(r.core.lanes[1].pos == 16 % 5, "position de la voie de 5 pas après 16 pas");
}

static void testRatios() {
	std::printf("Rapports d'horloge\n");
	Rig r;
	Lane& a = r.core.lanes[0];
	a.ratio = 8; // ×3/2
	for (int s = 0; s < STEPS; s++) a.on[s] = true;
	Lane& b = r.core.lanes[1];
	b.ratio = 2; // ÷2
	for (int s = 0; s < STEPS; s++) b.on[s] = true;
	r.core.rewind(r.st); r.core.play();
	Edges e; r.run(1.0 - 1e-4, e); // 8 pas maîtres
	check(e.trig[0].size() == 12, "×3/2 : 12 pas pendant 8 pas maîtres");
	check(near(e.trig[0][1] - e.trig[0][0], STEP120 * 2 / 3, 2e-4), "×3/2 : écart de deux tiers de pas");
	check(e.trig[1].size() == 4, "÷2 : 4 pas pendant 8 pas maîtres");
}

static void testSwing() {
	std::printf("Swing\n");
	Rig r;
	r.st.swing = 0.66f;
	Lane& a = r.core.lanes[0];
	for (int s = 0; s < STEPS; s++) a.on[s] = true;
	Lane& b = r.core.lanes[1];
	b.ratio = 8; // ×3/2 : reste droit
	for (int s = 0; s < STEPS; s++) b.on[s] = true;
	r.st.clockOutRes = 2; // 24 PPQN
	r.core.rewind(r.st); r.core.play();
	Edges e; r.run(0.5 - 1e-4, e);
	check(near(e.trig[0][1], 2 * 0.66 * STEP120, 2e-4), "le second pas de la paire arrive à 66 % de la paire");
	check(near(e.trig[0][2], 2 * STEP120, 2e-4), "le troisième pas reste sur le temps");
	check(near(e.trig[1][1], STEP120 * 2 / 3, 2e-4), "un rapport ternaire n'est pas swingué");
	check(e.clock.size() == 24, "CLOCK OUT à 24 PPQN : 24 impulsions par noire");
	// La 7e impulsion (début du second pas) est retardée comme le pas
	check(near(e.clock[6], e.trig[0][1], 2e-4), "CLOCK OUT swinguée avec le séquenceur");
}

static void testProbability() {
	std::printf("Probabilité, mutes\n");
	Rig r;
	Lane& a = r.core.lanes[0];
	for (int s = 0; s < STEPS; s++) { a.on[s] = true; a.prob[s] = 0.5f; }
	Lane& b = r.core.lanes[1];
	for (int s = 0; s < STEPS; s++) b.on[s] = true;
	r.st.muted[1] = true;
	r.core.rewind(r.st); r.core.play();
	Edges e; r.run(50.0, e); // 400 pas
	double rate = e.trig[0].size() / 400.0;
	std::printf("     taux à 50 %% : %.3f\n", rate);
	check(rate > 0.43 && rate < 0.57, "un pas à 50 % joue environ une fois sur deux");
	check(e.trig[1].empty(), "une voie mutée ne joue rien");
}

static void testDirections() {
	std::printf("Sens de lecture\n");
	Lane l; l.length = 4;
	l.dir = BACKWARD;
	check(l.positionAt(0) == 3 && l.positionAt(1) == 2 && l.positionAt(4) == 3, "arrière : 3 2 1 0 3");
	l.dir = PINGPONG;
	int seq[8]; for (int k = 0; k < 8; k++) seq[k] = l.positionAt(k);
	check(seq[0] == 0 && seq[3] == 3 && seq[4] == 2 && seq[5] == 1 && seq[6] == 0 && seq[7] == 1, "aller-retour : 0 1 2 3 2 1 0 1");
	l.length = 1;
	check(l.positionAt(5) == 0, "aller-retour sur un seul pas");
}

static void testTransport() {
	std::printf("Transport façon CDJ\n");
	Rig r;
	Lane& a = r.core.lanes[0];
	for (int s = 0; s < STEPS; s++) a.on[s] = true;
	r.core.rewind(r.st);
	Edges e;
	r.run(0.1, e);
	check(e.trig[0].empty(), "au chargement, Gaston est en pause");
	// CUE maintenu à l'arrêt : joue depuis le début
	r.core.cuePress(r.st);
	r.run(0.3, e);
	check(e.trig[0].size() == 3, "CUE maintenu : ça joue (pas 1, 2, 3)");
	r.core.cueRelease(r.st);
	check(!r.core.playing && r.core.lanes[0].count == -1, "CUE relâché : retour au début et arrêt");
	r.run(0.2, e);
	check(e.trig[0].size() == 3, "plus rien après le relâché");
	// CUE + PLAY : la lecture continue
	r.core.cuePress(r.st);
	r.core.togglePlay();
	r.core.cueRelease(r.st);
	check(r.core.playing, "CUE puis PLAY : la lecture continue au relâché");
	r.run(0.3, e);
	// PAUSE puis reprise : on repart où l'on était
	r.core.togglePlay();
	long countAtPause = r.core.lanes[0].count;
	r.run(0.5, e);
	check(r.core.lanes[0].count == countAtPause, "en pause, rien n'avance");
	r.core.togglePlay();
	r.run(0.2, e);
	check(r.core.lanes[0].count > countAtPause, "PLAY reprend où l'on était");
	// CUE pendant la lecture : retour au début et arrêt
	size_t resets = e.reset.size();
	r.core.cuePress(r.st);
	check(!r.core.playing && r.core.pos == 0.0, "CUE en lecture : début et arrêt");
	r.run(0.01, e);
	check(e.reset.size() == resets + 1, "RESET OUT à chaque retour au début");
	// RESET IN : retour au début sans arrêter
	r.core.togglePlay();
	r.run(0.4, e);
	size_t before = e.trig[0].size();
	r.core.resetKeepPlaying(r.st);
	r.run(0.001, e);
	check(r.core.playing && e.trig[0].size() == before + 1 && r.core.lanes[0].pos == 0, "RESET IN : repart du pas 1 sans s'arrêter");
	check(r.out.run > 5.f, "RUN OUT haut pendant la lecture");
}

static void testExternalClock() {
	std::printf("Horloge externe\n");
	Rig r;
	r.st.extClock = true;
	Lane& a = r.core.lanes[0];
	for (int s = 0; s < STEPS; s++) a.on[s] = true;
	Lane& b = r.core.lanes[1];
	b.ratio = 9; // ×2
	for (int s = 0; s < STEPS; s++) b.on[s] = true;
	r.core.rewind(r.st); r.core.play();
	Edges e;
	const double P = 0.1; // une impulsion par pas, toutes les 100 ms (150 BPM)
	double next = 0.05;
	r.run(1.0, e, [&](double t) { if (t >= next) { r.core.clockPulse(r.st); next += P; } });
	check(!e.trig[0].empty() && near(e.trig[0][0], 0.05, 1e-3), "rien avant la première impulsion, puis le pas 1 dessus");
	check(e.trig[0].size() == 10, "un pas par impulsion");
	check(near(e.trig[1][3] - e.trig[1][2], 0.05, 2e-3), "×2 : les pas intermédiaires tombent entre les impulsions");
	check(near(r.core.displayBpm(r.st), 150.f, 1.f), "tempo mesuré : 150 BPM");
}

static void testRec() {
	std::printf("Pads et REC\n");
	Rig r;
	r.st.rec = true;
	r.core.rewind(r.st); r.core.play();
	Edges e;
	// En retard : 20 ms après le pas 3 (indice 2)
	r.run(2 * STEP120 + 0.02, e);
	r.core.pad(0, r.st);
	check(r.core.lanes[0].on[2], "frappe en retard : écrite sur le pas courant");
	// En avance : 20 ms avant le pas 6 (indice 5)
	r.run(5 * STEP120 - 0.02 - r.t, e);
	r.core.pad(0, r.st);
	check(r.core.lanes[0].on[5], "frappe en avance : écrite sur le pas suivant");
	r.run(0.005, e); // le trig du pad lui-même
	size_t n = e.trig[0].size();
	r.run(0.05, e);
	check(e.trig[0].size() == n, "le pas écrit en avance ne rejoue pas à son passage");
	r.run(2.0, e);
	check(e.trig[0].size() >= n + 2, "au tour suivant, les deux pas jouent");
	// Pad sur une voie mutée : il joue quand même
	r.st.muted[3] = true;
	size_t m3 = e.trig[3].size();
	r.core.pad(3, r.st);
	r.run(0.02, e);
	check(e.trig[3].size() == m3 + 1, "un pad joue même sur une voie mutée");
}

static void testRealign() {
	std::printf("Réalignement\n");
	Rig r;
	r.st.realign = 1; // tous les 16 pas
	Lane& a = r.core.lanes[0];
	a.length = 5; a.on[0] = true;
	r.core.rewind(r.st); r.core.play();
	Edges e; r.run(16 * STEP120 + 1e-3, e);
	// Pas 1 aux pas maîtres 0, 5, 10, 15, puis réalignement à 16
	check(e.trig[0].size() == 5 && near(e.trig[0][4], 16 * STEP120, 2e-4), "la voie de 5 pas repart au pas 1 au 16e pas maître");
}

static void testVintage() {
	std::printf("Timing vintage (96 PPQN)\n");
	Rig r;
	r.st.vintage = true;
	r.st.swing = 0.6f;
	Lane& a = r.core.lanes[0];
	for (int s = 0; s < STEPS; s++) a.on[s] = true;
	Lane& b = r.core.lanes[1];
	b.ratio = 6; // ×5/4
	for (int s = 0; s < STEPS; s++) b.on[s] = true;
	r.core.rewind(r.st); r.core.play();
	Edges e; r.run(2.0, e);
	const double tick = STEP120 / 24.0;
	bool onGrid = true, jitterOk = true, someJitter = false;
	for (auto* v : {&e.trig[0], &e.trig[1]})
		for (double t : *v) {
			double off = t - std::floor(t / tick + 1e-6) * tick;
			if (off > 0.0008 + 1.5 / SR) onGrid = false;
			if (off > 1.0 / SR) someJitter = true;
			if (off < 0) jitterOk = false;
		}
	check(onGrid && jitterOk, "chaque événement tombe sur un tick de 96 PPQN, à moins de 0,8 ms près");
	check(someJitter, "un léger aléa est présent");
	// Swing 60 % arrondi au tick : 2 × 0,6 × 24 = 28,8 ticks → 29 ticks
	double second = e.trig[0][1];
	check(second >= 29 * tick - 1e-4 && second <= 29 * tick + 0.0009, "swing cranté : le second pas tombe au 29e tick");
}

static void testCv() {
	std::printf("Voies CV\n");
	Rig r;
	Lane& a = r.core.cvLane(0);
	a.value[0] = 0.5f; a.value[1] = -0.3f;
	a.length = 2;
	Lane& b = r.core.cvLane(1);
	b.unipolar = true; b.quantize = true; b.value[0] = 0.0f; b.value[1] = 0.0123f;
	b.length = 2;
	Lane& c = r.core.cvLane(2);
	c.value[0] = 1.f; c.slew = 0.5f; c.length = 1;
	r.core.rewind(r.st); r.core.play();
	Edges e; r.run(0.06, e);
	check(near(e.cv[0].back(), 2.5f, 1e-4), "±5 V : valeur 0,5 → 2,5 V");
	check(near(e.cv[1].back(), 5.f, 1e-4), "0–10 V : valeur 0 → 5 V");
	check(e.cv[2].back() > 0.05f && e.cv[2].back() < 4.9f, "SLEW : la tension glisse vers sa cible");
	r.run(0.1, e);
	check(near(e.cv[0].back(), -1.5f, 1e-4), "pas suivant : -0,3 → -1,5 V");
	float q = e.cv[1].back();
	check(near(q * 12.f, std::round(q * 12.f), 1e-3), "quantifié au demi-ton");
	r.run(2.0, e);
	check(near(e.cv[2].back(), 5.f, 0.01f), "SLEW : la cible finit par être atteinte");
}

static void testCvHold() {
	std::printf("Voies CV : pas inactifs (sample & hold)\n");
	Rig r;
	Lane& a = r.core.cvLane(0);
	a.length = 4;
	a.value[0] = 0.2f; a.value[1] = 0.6f; a.value[2] = -0.4f; a.value[3] = 0.8f;
	a.on[1] = false; a.on[2] = false;
	check(r.core.cvLane(1).on[0], "par défaut, les pas CV sont actifs");
	r.core.rewind(r.st); r.core.play();
	Edges e; r.run(STEP120 * 1.5, e);
	check(near(e.cv[0].back(), 1.f, 1e-4), "pas 2 inactif : la tension du pas 1 est gardée");
	r.run(STEP120, e);
	check(near(e.cv[0].back(), 1.f, 1e-4), "pas 3 inactif : toujours la tension du pas 1");
	r.run(STEP120, e);
	check(near(e.cv[0].back(), 4.f, 1e-4), "pas 4 actif : nouvelle tension");
	r.run(STEP120, e);
	check(near(e.cv[0].back(), 1.f, 1e-4), "au tour suivant, le pas 1 repose la sienne");
}

static void testEdit() {
	std::printf("Outils d'édition\n");
	Core core;
	Lane& l = core.lanes[0];
	l.length = 4; l.on[0] = true; l.prob[0] = 0.4f;
	core.shiftLane(0, 1);
	check(!l.on[0] && l.on[1] && near(l.prob[1], 0.4f, 1e-6), "décaler à droite, probabilité comprise");
	core.shiftLane(0, -2);
	check(l.on[3], "décaler à gauche reboucle sur la longueur");
}

static void testMemory() {
	std::printf("Mémoires (Gastounet)\n");
	Rig r;
	r.core.memOn = true;
	Core& c = r.core;
	// Motif A01 : la voie 1 en noires ; motif A02 : la voie 1 sur chaque pas
	for (int k = 0; k < STEPS; k++) c.lanes[0].on[k] = k % 4 == 0;
	c.writeSlot(0);
	check(c.mem.slots[0].filled && !c.edited(), "WRITE : la mémoire est pleine, plus rien de modifié");
	for (int k = 0; k < STEPS; k++) c.lanes[0].on[k] = true;
	check(c.edited(), "une modification se voit");
	c.writeSlot(1);
	check(c.mem.active == 1, "WRITE sur un autre slot : il devient la mémoire active");
	// À l'arrêt, une mémoire se charge tout de suite
	c.pressSlot(0);
	check(c.mem.active == 0 && !c.lanes[0].on[1], "à l'arrêt, le motif se charge tout de suite");
	// En lecture, le changement attend la mesure
	c.rewind(r.st); c.play();
	Edges e; r.run(STEP120 * 3.5, e);
	c.pressSlot(1);
	check(c.mem.queued == 1 && c.mem.active == 0, "en lecture, le motif attend");
	r.run(STEP120 * 12.0, e); // jusqu'au pas 15,5
	check(c.mem.active == 0, "pas de changement avant la fin de la mesure");
	size_t ends = e.trig[0].size();
	r.run(STEP120 * 1.0, e);  // passe le pas 16
	check(c.mem.active == 1 && c.mem.queued == -1, "le changement tombe sur la mesure");
	check(c.lanes[0].pos == 0, "RESTART : les voies repartent du pas 1");
	(void) ends;
}

static void testLaunchModes() {
	std::printf("Changement : NOW, LEGATO, TRACK 1\n");
	{
		Rig r; Core& c = r.core; c.memOn = true;
		c.writeSlot(0); c.writeSlot(1);
		c.mem.quant = Q_NOW;
		c.rewind(r.st); c.play();
		Edges e; r.run(STEP120 * 2.5, e);
		c.pressSlot(0);
		r.run(STEP120 * 0.6, e);
		check(c.mem.active == 0, "NOW : le changement tombe au pas suivant");
	}
	{
		Rig r; Core& c = r.core; c.memOn = true;
		c.lanes[1].length = 5;
		c.writeSlot(0); c.writeSlot(1);
		c.mem.legato = true;
		c.rewind(r.st); c.play();
		Edges e; r.run(STEP120 * 3.5, e);
		c.pressSlot(0);
		r.run(STEP120 * 13.0, e); // passe le pas 16
		check(c.mem.active == 0 && c.lanes[1].pos == 16 % 5, "LEGATO : la voie de 5 pas garde sa position");
	}
	{
		Rig r; Core& c = r.core; c.memOn = true;
		c.lanes[0].length = 12;
		c.writeSlot(0); c.writeSlot(1);
		c.mem.quant = Q_TRACK1;
		c.rewind(r.st); c.play();
		Edges e; r.run(STEP120 * 3.5, e);
		c.pressSlot(0);
		r.run(STEP120 * 8.0, e); // pas 11,5
		check(c.mem.active == 1, "TRACK 1 : rien avant que la voie 1 reboucle");
		r.run(STEP120 * 1.0, e); // passe le pas 12
		check(c.mem.active == 0, "TRACK 1 : le changement tombe quand la voie 1 (12 pas) reboucle");
	}
}

static void testSong() {
	std::printf("Morceau\n");
	Rig r; Core& c = r.core; c.memOn = true;
	c.writeSlot(0); c.writeSlot(1); c.writeSlot(2);
	c.mem.song[0] = {1, 1};
	c.mem.song[1] = {2, 2};
	c.mem.songLen = 2;
	c.setSongMode(true);
	check(c.mem.active == 1, "à l'arrêt, le morceau charge sa première ligne");
	c.rewind(r.st); c.play();
	Edges e;
	r.run(STEP120 * 16.5, e);
	check(c.mem.active == 2 && c.mem.songRow == 1, "après 1 mesure, la ligne 2");
	r.run(STEP120 * 16.0, e);
	check(c.mem.active == 2, "la ligne 2 dure 2 mesures");
	r.run(STEP120 * 16.0, e);
	check(c.mem.active == 1 && c.mem.songRow == 0, "FIN : BOUCLE, retour à la ligne 1");
	c.mem.songLoop = false;
	r.run(STEP120 * 48.0, e);
	check(!c.playing && c.mem.songRow == 0, "FIN : STOP, le transport s'arrête, calé au début");
	check(!e.reset.empty(), "un RESET part au retour au début");
	// SONG REC : chaque mémoire ajoute une ligne
	c.mem.songRec = true;
	c.pressSlot(2);
	check(c.mem.songLen == 3 && c.mem.song[2].pattern == 2, "SONG REC : une ligne de plus");
	c.insertRow(0, SongRow{0, 8});
	check(c.mem.songLen == 4 && c.mem.song[0].bars == 8 && c.mem.song[1].pattern == 1, "insérer une ligne");
	c.deleteRow(0);
	check(c.mem.songLen == 3 && c.mem.song[0].pattern == 1, "supprimer une ligne");
}

static void testCopyNextEnd() {
	std::printf("COPY, NEXT, RANDOM, sortie END\n");
	Rig r; Core& c = r.core; c.memOn = true;
	c.lanes[0].on[3] = true;
	c.writeSlot(0);
	c.mem.armed = ARM_COPY;
	c.pressSlot(0); c.pressSlot(5);
	check(c.mem.slots[5].filled && c.mem.slots[5].lanes[0].on[3] && c.mem.armed == ARM_NONE, "COPY A01 → A06");
	c.nextSlot();
	check(c.mem.active == 5, "NEXT (à l'arrêt) : la mémoire pleine suivante");
	c.randomSlot();
	check(c.mem.active == 0, "RANDOM : une autre mémoire pleine de la banque");
	c.rewind(r.st); c.play();
	c.pressSlot(5);
	Edges e; size_t endPulses = 0; float prev = 0.f;
	r.run(STEP120 * 17.0, e, [&](double) { if (r.out.end > 5.f && prev <= 5.f) endPulses++; prev = r.out.end; });
	check(endPulses == 1, "END : un trig quand le changement tombe");
	c.cvSelect(10.f * 2.5f / 16.f);
	check(c.mem.queued == 2, "CV PATTERN : 0 à 10 V choisissent la mémoire de la banque");
}

int main() {
	testClockAndLength();
	testRatios();
	testSwing();
	testProbability();
	testDirections();
	testTransport();
	testExternalClock();
	testRec();
	testRealign();
	testVintage();
	testCv();
	testCvHold();
	testEdit();
	testMemory();
	testLaunchModes();
	testSong();
	testCopyNextEnd();
	std::printf(failures ? "%d échec(s)\n" : "tout passe\n", failures);
	return failures ? 1 : 0;
}
