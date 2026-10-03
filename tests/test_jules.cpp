// Banc d'essai hors de Rack : fait tourner Jules sample par sample et vérifie les modes.
// Compilation : make -C tests (voir tests/Makefile)
#include "../src/Jules.cpp"
#include <cstdio>

Plugin* pluginInstance = NULL;

static int failures = 0;
static void check(bool ok, const char* what) {
	std::printf("%s %s\n", ok ? "  ok " : "  ÉCHEC", what);
	if (!ok)
		failures++;
}

struct Bench {
	Jules m;
	Module::ProcessArgs args;
	Bench() {
		args.sampleRate = 48000.f;
		args.sampleTime = 1.f / 48000.f;
		args.frame = 0;
		for (int i = 0; i < Jules::INPUTS_LEN; i++)
			m.inputs[i].channels = 0;
	}
	void set(int id, float v) { m.params[id].setValue(v); }
	// Dans Rack, c'est le moteur qui branche les câbles : setChannels() ne connecte pas une entrée libre
	void patch(int id, float v) { m.inputs[id].channels = 1; m.inputs[id].setVoltage(v); }
	void unpatch(int id) { m.inputs[id].channels = 0; }
	float out(int i) { return m.outputs[Jules::SLOPE_OUTPUT + i].getVoltage(); }
	void step() { m.process(args); args.frame++; }
	void variant(float seconds) { for (int i = 0; i < (int) (seconds * args.sampleRate); i++) step(); }
};

// Compte les fronts montants d'une sortie qui franchit un seuil
static void countCycles(Bench& b, float seconds, float threshold, int counts[6]) {
	bool above[6] = {};
	for (int i = 0; i < 6; i++) counts[i] = 0;
	for (int k = 0; k < (int) (seconds * b.args.sampleRate); k++) {
		b.step();
		for (int i = 0; i < 6; i++) {
			bool a = b.out(i) > threshold;
			if (a && !above[i]) counts[i]++;
			above[i] = a;
		}
	}
}

int main() {
	random::init();

	std::printf("LOOP / CV, SPREAD à fond : vitesses 1:2:3:4:5:6\n");
	{
		Bench b;
		b.set(Jules::MODE_PARAM, Jules::MODE_LOOP);
		b.set(Jules::SPREAD_PARAM, 1.f);
		b.set(Jules::RATE_PARAM, 0.f);  // 1 Hz
		int c[6];
		countCycles(b, 10.f, 4.f, c);
		std::printf("    cycles en 10 s : %d %d %d %d %d %d\n", c[0], c[1], c[2], c[3], c[4], c[5]);
		bool ok = true;
		for (int i = 0; i < 6; i++) ok &= std::abs(c[i] - 10 * (i + 1)) <= 1;
		check(ok, "la pente 1 à 1 Hz, la pente N à N Hz");
	}

	std::printf("LOOP / CV, SPREAD à fond à gauche : vitesses 1, 1/2 … 1/6\n");
	{
		Bench b;
		b.set(Jules::MODE_PARAM, Jules::MODE_LOOP);
		b.set(Jules::SPREAD_PARAM, -1.f);
		b.set(Jules::RATE_PARAM, 3.f / 7.f);  // 8 Hz
		int c[6];
		countCycles(b, 6.f, 4.f, c);
		std::printf("    cycles en 6 s : %d %d %d %d %d %d\n", c[0], c[1], c[2], c[3], c[4], c[5]);
		bool ok = true;
		for (int i = 0; i < 6; i++) ok &= std::abs(c[i] - 48.f / (i + 1)) <= 1.5f;
		check(ok, "la pente N à 8/N Hz");
	}

	std::printf("LOOP / AUDIO : la pente 1 à C4 au centre\n");
	{
		Bench b;
		b.set(Jules::MODE_PARAM, Jules::MODE_LOOP);
		b.set(Jules::RANGE_PARAM, 1.f);
		int c[6];
		countCycles(b, 1.f, 0.f, c);
		std::printf("    la pente 1 : %d Hz (C4 = 261,6)\n", c[0]);
		check(std::abs(c[0] - 262) <= 2, "la pente 1 à 261,6 Hz");
		float lo = 1e9, hi = -1e9;
		for (int k = 0; k < 2000; k++) { b.step(); lo = std::min(lo, b.out(0)); hi = std::max(hi, b.out(0)); }
		std::printf("    plage : %.2f à %.2f V\n", lo, hi);
		check(lo < -4.9f && hi > 4.9f, "sortie ±5 V en AUDIO");
	}

	std::printf("ONE-SHOT / CV : un trigger dans TRIG 6 déclenche les six\n");
	{
		Bench b;
		b.set(Jules::RATE_PARAM, 3.f / 7.f);  // 8 Hz : enveloppe de 125 ms
		b.patch(Jules::TRIG_INPUT + 5, 0.f);
		b.variant(0.01f);
		b.patch(Jules::TRIG_INPUT + 5, 10.f);
		b.variant(0.002f);
		b.patch(Jules::TRIG_INPUT + 5, 0.f);
		float peak[6] = {};
		for (int k = 0; k < 4800; k++) { b.step(); for (int i = 0; i < 6; i++) peak[i] = std::max(peak[i], b.out(i)); }
		std::printf("    crêtes : %.1f %.1f %.1f %.1f %.1f %.1f\n", peak[0], peak[1], peak[2], peak[3], peak[4], peak[5]);
		bool ok = true;
		for (int i = 0; i < 6; i++) ok &= peak[i] > 7.9f;
		check(ok, "les six enveloppes montent à 8 V");
		b.variant(0.5f);
		ok = true;
		for (int i = 0; i < 6; i++) ok &= b.out(i) == 0.f;
		check(ok, "elles retombent à 0 V");
	}

	std::printf("CHANCE à 0 : seule la pente branchée réagit\n");
	{
		Bench b;
		b.set(Jules::RATE_PARAM, 3.f / 7.f);
		b.set(Jules::CHANCE_PARAM, 0.f);
		b.patch(Jules::TRIG_INPUT + 5, 0.f);
		b.variant(0.01f);
		b.patch(Jules::TRIG_INPUT + 5, 10.f);
		b.variant(0.002f);
		b.patch(Jules::TRIG_INPUT + 5, 0.f);
		float peak[6] = {};
		for (int k = 0; k < 4800; k++) { b.step(); for (int i = 0; i < 6; i++) peak[i] = std::max(peak[i], b.out(i)); }
		check(peak[5] > 7.9f && peak[0] == 0.f && peak[4] == 0.f, "TRIG 6 joue, les autres restent muettes");
	}

	std::printf("CHANCE à 50 %% : environ la moitié des triggers normalisés passent\n");
	{
		Bench b;
		b.set(Jules::RATE_PARAM, 1.f);  // très court
		b.set(Jules::CHANCE_PARAM, 0.5f);
		int fired = 0;
		for (int t = 0; t < 400; t++) {
			b.patch(Jules::TRIG_INPUT + 5, 10.f);
			b.variant(0.001f);
			b.patch(Jules::TRIG_INPUT + 5, 0.f);
			float peak = 0.f;
			for (int k = 0; k < 480; k++) { b.step(); peak = std::max(peak, b.out(0)); }
			if (peak > 1.f) fired++;
		}
		std::printf("    la pente 1 a joué %d fois sur 400\n", fired);
		check(fired > 160 && fired < 240, "proche de 200");
	}

	std::printf("CURVE à fond à gauche : rectangles 0 / 8 V\n");
	{
		Bench b;
		b.set(Jules::MODE_PARAM, Jules::MODE_LOOP);
		b.set(Jules::CURVE_PARAM, -1.f);
		bool ok = true;
		for (int k = 0; k < 48000; k++) { b.step(); float v = b.out(0); ok &= (v == 0.f || v == 8.f); }
		check(ok, "seulement 0 ou 8 V");
	}

	std::printf("GATE / CV : tient à 8 V tant que la gate est haute\n");
	{
		Bench b;
		b.set(Jules::MODE_PARAM, Jules::MODE_GATE);
		b.set(Jules::RATE_PARAM, 3.f / 7.f);
		b.patch(Jules::TRIG_INPUT + 0, 10.f);
		b.variant(1.f);
		check(std::fabs(b.out(0) - 8.f) < 1e-3f, "8 V après 1 s de gate");
		b.patch(Jules::TRIG_INPUT + 0, 0.f);
		b.variant(1.f);
		check(b.out(0) == 0.f, "0 V après relâchement");
	}

	std::printf("SUSTAIN (TWEAK à 0 V) : sustain à mi-hauteur\n");
	{
		Bench b;
		b.set(Jules::MODE_PARAM, Jules::MODE_SUSTAIN);
		b.set(Jules::RATE_PARAM, 3.f / 7.f);
		// TWEAK au centre = 0 V
		b.patch(Jules::TRIG_INPUT + 0, 10.f);
		b.variant(1.f);
		std::printf("    la pente 1 tenu à %.2f V\n", b.out(0));
		check(std::fabs(b.out(0) - 4.f) < 0.05f, "4 V (milieu de 0-8 V, CURVE au centre)");
	}

	std::printf("BURST (TWEAK à 0 V) : 6 cycles par trigger\n");
	{
		Bench b;
		b.set(Jules::MODE_PARAM, Jules::MODE_BURST);
		b.set(Jules::RATE_PARAM, 3.f / 7.f);  // 8 Hz
		// TWEAK au centre = 0 V
		b.patch(Jules::TRIG_INPUT + 0, 0.f);
		b.variant(0.5f);
		check(b.out(0) == 0.f, "au repos sans trigger");
		b.patch(Jules::TRIG_INPUT + 0, 10.f);
		b.variant(0.001f);
		b.patch(Jules::TRIG_INPUT + 0, 0.f);
		int c[6];
		countCycles(b, 2.f, 4.f, c);
		std::printf("    la pente 1 : %d cycles\n", c[0]);
		check(c[0] == 6, "exactement 6");
	}

	std::printf("PLUCK : muet sans trigger, chante avec une gate\n");
	{
		Bench b;
		b.set(Jules::MODE_PARAM, Jules::MODE_SUSTAIN);
		b.set(Jules::RANGE_PARAM, 1.f);
		// TWEAK au centre = 0 V
		b.patch(Jules::TRIG_INPUT + 0, 0.f);
		float peak = 0.f;
		for (int k = 0; k < 24000; k++) { b.step(); peak = std::max(peak, std::fabs(b.out(0))); }
		check(peak < 0.01f, "silence");
		b.patch(Jules::TRIG_INPUT + 0, 10.f);
		peak = 0.f;
		for (int k = 0; k < 24000; k++) { b.step(); peak = std::max(peak, std::fabs(b.out(0))); }
		std::printf("    crête avec gate : %.2f V\n", peak);
		check(peak > 3.f, "le lowpass gate s'ouvre");
	}

	std::printf("FM : la FM change le timbre\n");
	{
		Bench a, b;
		for (Bench* x : {&a, &b}) {
			x->set(Jules::MODE_PARAM, Jules::MODE_BURST);
			x->set(Jules::RANGE_PARAM, 1.f);
			// TWEAK au centre = 0 V
		}
		b.set(Jules::FM_PARAM, 0.8f);
		float diff = 0.f;
		for (int k = 0; k < 4800; k++) { a.step(); b.step(); diff = std::max(diff, std::fabs(a.out(0) - b.out(0))); }
		check(diff > 1.f, "sortie différente avec FM");
	}

	std::printf("SUBHARMONIC : la pente 1 libre, les autres déclenchées par sa fin de cycle\n");
	{
		Bench b;
		b.set(Jules::MODE_PARAM, Jules::MODE_RETRIGGER);
		b.set(Jules::RANGE_PARAM, 1.f);
		b.set(Jules::SPREAD_PARAM, 1.f);
		b.set(Jules::TWEAK_PARAM, 1.f);  // +5 V
		int c[6];
		countCycles(b, 1.f, 0.f, c);
		std::printf("    impulsions en 1 s : %d %d %d %d %d %d\n", c[0], c[1], c[2], c[3], c[4], c[5]);
		check(std::abs(c[0] - 262) <= 2 && std::abs(c[3] - c[0]) <= 2, "les pentes 2 à 6 au rythme de la pente 1");
	}

	std::printf("TWEAK par CV : l'entrée s'ajoute au bouton (SUSTAIN, bouton à -5 V + 5 V de CV = mi-hauteur)\n");
	{
		Bench b;
		b.set(Jules::MODE_PARAM, Jules::MODE_SUSTAIN);
		b.set(Jules::RATE_PARAM, 3.f / 7.f);
		b.set(Jules::TWEAK_PARAM, -1.f);
		b.patch(Jules::TWEAK_INPUT, 5.f);
		b.patch(Jules::TRIG_INPUT + 0, 10.f);
		b.variant(1.f);
		check(std::fabs(b.out(0) - 4.f) < 0.05f, "4 V");
		std::printf("    afficheur : %s\n", b.m.tweakText().c_str());
	}

	std::printf("\n%s (%d échec%s)\n", failures ? "ÉCHECS" : "Tout passe", failures, failures > 1 ? "s" : "");
	return failures ? 1 : 0;
}
