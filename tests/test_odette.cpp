// Banc d'essai hors de Rack pour Odette
#include "../src/Odette.cpp"
#include <cstdio>

Plugin* pluginInstance = NULL;

static int failures = 0;
static void check(bool ok, const char* what) {
	std::printf("%s %s\n", ok ? "  ok " : "  ÉCHEC", what);
	if (!ok) failures++;
}

struct Bench {
	Odette m;
	Module::ProcessArgs args;
	float sr = 48000.f;
	Bench() {
		args.sampleRate = sr;
		args.sampleTime = 1.f / sr;
		args.frame = 0;
		for (int i = 0; i < Odette::INPUTS_LEN; i++) m.inputs[i].channels = 0;
		set(Odette::MIX_PARAM, 1.f);       // seulement les répétitions
		set(Odette::FEEDBACK_PARAM, 0.f);   // une seule répétition
	}
	void set(int id, float v) { m.params[id].setValue(v); }
	// Dans Rack, c'est le moteur qui branche les câbles
	void patch(int id, float v) { m.inputs[id].channels = 1; m.inputs[id].setVoltage(v); }
	float outL() { return m.outputs[Odette::LEFT_OUTPUT].getVoltage(); }
	float outR() { return m.outputs[Odette::RIGHT_OUTPUT].getVoltage(); }
	void step() { m.process(args); args.frame++; }
	void run(float seconds) { for (int i = 0; i < (int) (seconds * sr); i++) step(); }
	// Envoie une impulsion de 1 ms dans l'entrée gauche, puis renvoie la sortie gauche sur `seconds`
	std::vector<float> impulse(float seconds, int output = Odette::LEFT_OUTPUT, int length = 48) {
		patch(Odette::LEFT_INPUT, 0.f);
		std::vector<float> out;
		for (int i = 0; i < (int) (seconds * sr); i++) {
			m.inputs[Odette::LEFT_INPUT].setVoltage(i < length ? 5.f : 0.f);
			step();
			out.push_back(m.outputs[output].getVoltage());
		}
		return out;
	}
};

// Instant (s) du maximum absolu entre deux instants
static float peakTime(const std::vector<float>& x, float sr, float from, float to, float* value = NULL) {
	int best = (int) (from * sr);
	for (int i = (int) (from * sr); i < (int) (to * sr) && i < (int) x.size(); i++)
		if (std::fabs(x[i]) > std::fabs(x[best])) best = i;
	if (value) *value = std::fabs(x[best]);
	return best / sr;
}

int main() {
	random::init();

	std::printf("Taille 4, TIME au centre : écho à 326,6 ms\n");
	{
		Bench b;
		b.set(Odette::SIZE_PARAM, 3.f);
		std::vector<float> out = b.impulse(0.6f);
		float t = peakTime(out, b.sr, 0.01f, 0.6f);
		std::printf("    écho à %.1f ms\n", t * 1000.f);
		check(std::fabs(t - 0.3266f) < 0.003f, "durée de la taille 4 au centre");
	}

	std::printf("FEEDBACK à 50 %% : chaque écho vaut environ 0,54 du précédent\n");
	{
		Bench b;
		b.set(Odette::SIZE_PARAM, 3.f);
		b.set(Odette::FEEDBACK_PARAM, 0.5f);
		std::vector<float> out = b.impulse(1.2f);
		float a1, a2, a3;
		peakTime(out, b.sr, 0.30f, 0.36f, &a1);
		peakTime(out, b.sr, 0.63f, 0.69f, &a2);
		peakTime(out, b.sr, 0.95f, 1.02f, &a3);
		std::printf("    échos : %.3f, %.3f, %.3f V (rapports %.2f, %.2f)\n", a1, a2, a3, a2 / a1, a3 / a2);
		check(std::fabs(a2 / a1 - 0.54f) < 0.06f && std::fabs(a3 / a2 - 0.54f) < 0.06f, "décroissance régulière");
	}

	std::printf("Taille 1 : corde, et BEND à +1 V double la fréquence\n");
	{
		Bench a, b;
		for (Bench* x : {&a, &b}) {
			x->set(Odette::SIZE_PARAM, 0.f);
			x->set(Odette::TIME_PARAM, 0.f);  // TIME à fond à gauche : le plus court
			x->set(Odette::FEEDBACK_PARAM, 0.85f);
		}
		b.patch(Odette::BEND_INPUT, 1.f);
		// Impulsion d'un seul échantillon : plus courte que l'écho de 1,3 ms
		std::vector<float> oa = a.impulse(0.05f, Odette::LEFT_OUTPUT, 1), ob = b.impulse(0.05f, Odette::LEFT_OUTPUT, 1);
		// Période = écart entre les deux premiers pics
		float ta1 = peakTime(oa, a.sr, 0.0005f, 0.0019f), ta2 = peakTime(oa, a.sr, ta1 + 0.0005f, ta1 + 0.0019f);
		float tb1 = peakTime(ob, b.sr, 0.0003f, 0.0010f), tb2 = peakTime(ob, b.sr, tb1 + 0.0003f, tb1 + 0.0010f);
		std::printf("    période : %.2f ms, avec BEND +1 V : %.2f ms\n", (ta2 - ta1) * 1000.f, (tb2 - tb1) * 1000.f);
		check(std::fabs((ta2 - ta1) - 0.0013f) < 0.0002f, "1,3 ms en taille 1, TIME à fond à gauche");
		check(std::fabs((tb2 - tb1) - 0.00065f) < 0.00015f, "moitié avec +1 V");
	}

	std::printf("CLOCK à 2 Hz, taille 5, TIME au centre (1/1) : écho à 500 ms\n");
	{
		Bench b;
		b.set(Odette::SIZE_PARAM, 4.f);
		b.patch(Odette::CLOCK_INPUT, 0.f);
		for (int k = 0; k < 3 * 48000; k++) {
			b.m.inputs[Odette::CLOCK_INPUT].setVoltage((k % 24000) < 480 ? 10.f : 0.f);
			b.step();
		}
		std::vector<float> out = b.impulse(0.8f);
		float t = peakTime(out, b.sr, 0.01f, 0.8f);
		std::printf("    écho à %.1f ms (%s)\n", t * 1000.f, CLOCK_NAMES[b.m.displayClockStep >= 0 ? b.m.displayClockStep : 6]);
		check(std::fabs(t - 0.5f) < 0.004f, "calé sur l'horloge");
	}

	std::printf("Sortie PULSE : une impulsion par répétition\n");
	{
		Bench b;
		b.set(Odette::SIZE_PARAM, 3.f);
		int pulses = 0;
		bool high = false;
		for (int k = 0; k < 48000 * 2; k++) {
			b.step();
			bool h = b.m.outputs[Odette::PULSE_OUTPUT].getVoltage() > 5.f;
			if (h && !high) pulses++;
			high = h;
		}
		std::printf("    %d impulsions en 2 s (attendu 6,1)\n", pulses);
		check(pulses >= 6 && pulses <= 7, "au rythme de la taille");
	}

	std::printf("FREEZE : la boucle continue sans entrée\n");
	{
		Bench b;
		b.set(Odette::SIZE_PARAM, 3.f);
		b.patch(Odette::LEFT_INPUT, 0.f);
		for (int k = 0; k < 48000; k++) {
			b.m.inputs[Odette::LEFT_INPUT].setVoltage(4.f * std::sin(2.f * M_PI * 440.f * k / 48000.f));
			b.step();
		}
		b.set(Odette::FREEZE_PARAM, 1.f);
		b.m.inputs[Odette::LEFT_INPUT].setVoltage(0.f);
		b.run(2.f);
		float rms = 0.f;
		for (int k = 0; k < 48000; k++) { b.step(); rms += b.outL() * b.outL(); }
		rms = std::sqrt(rms / 48000.f);
		std::printf("    niveau après 3 s sans entrée : %.2f V RMS\n", rms);
		check(rms > 2.f, "toujours là");
		b.set(Odette::FREEZE_PARAM, 0.f);
		b.run(1.f);
		rms = 0.f;
		for (int k = 0; k < 48000; k++) { b.step(); rms += b.outL() * b.outL(); }
		check(std::sqrt(rms / 48000.f) < 0.05f, "s'éteint quand on relâche (FEEDBACK à 0)");
	}

	std::printf("REVERSE : une rampe montante revient descendante\n");
	{
		Bench b;
		b.set(Odette::SIZE_PARAM, 4.f);
		b.set(Odette::REVERSE_PARAM, 1.f);
		b.patch(Odette::LEFT_INPUT, 0.f);
		int down = 0, up = 0;
		float last = 0.f;
		for (int k = 0; k < 48000 * 4; k++) {
			// Dent de scie montante à 2 Hz
			float ph = std::fmod(k / 48000.f * 2.f, 1.f);
			b.m.inputs[Odette::LEFT_INPUT].setVoltage(-4.f + 8.f * ph);
			b.step();
			float v = b.outL();
			if (k > 48000) { if (v < last - 1e-4f) down++; else if (v > last + 1e-4f) up++; }
			last = v;
		}
		std::printf("    pentes descendantes : %d %%\n", 100 * down / std::max(1, down + up));
		check(down > 1.5f * up, "majoritairement descendante");
	}

	std::printf("BOUNCE : les répétitions passent à droite\n");
	{
		Bench a, b;
		for (Bench* x : {&a, &b}) {
			x->set(Odette::SIZE_PARAM, 3.f);
			x->set(Odette::FEEDBACK_PARAM, 0.6f);
			x->patch(Odette::RIGHT_INPUT, 0.f);
		}
		b.set(Odette::BOUNCE_PARAM, 1.f);
		float ra, rb;
		std::vector<float> oa = a.impulse(0.8f, Odette::RIGHT_OUTPUT), ob = b.impulse(0.8f, Odette::RIGHT_OUTPUT);
		peakTime(oa, a.sr, 0.5f, 0.8f, &ra);
		peakTime(ob, b.sr, 0.5f, 0.8f, &rb);
		std::printf("    droite : %.3f V sans ping pong, %.3f V avec\n", ra, rb);
		check(ra < 0.01f && rb > 0.3f, "le 2e écho sort à droite");
	}

	std::printf("DUCK : les répétitions baissent pendant que le son entre\n");
	{
		Bench a, b;
		b.set(Odette::DUCK_PARAM, 1.f);
		for (Bench* x : {&a, &b}) {
			x->set(Odette::SIZE_PARAM, 3.f);
			x->set(Odette::FEEDBACK_PARAM, 0.6f);
			x->patch(Odette::LEFT_INPUT, 0.f);
		}
		float ea = 0.f, eb = 0.f;
		for (int k = 0; k < 48000 * 2; k++) {
			float v = 4.f * std::sin(2.f * M_PI * 220.f * k / 48000.f);
			a.m.inputs[Odette::LEFT_INPUT].setVoltage(v);
			b.m.inputs[Odette::LEFT_INPUT].setVoltage(v);
			a.step(); b.step();
			if (k > 48000) { ea += a.outL() * a.outL(); eb += b.outL() * b.outL(); }
		}
		std::printf("    niveau : %.2f V sans duck, %.2f V avec\n", std::sqrt(ea / 48000.f), std::sqrt(eb / 48000.f));
		check(eb < 0.1f * ea, "répétitions écrasées");
	}

	std::printf("MIX à 0 : le son sec passe tel quel\n");
	{
		Bench b;
		b.set(Odette::MIX_PARAM, 0.f);
		b.patch(Odette::LEFT_INPUT, 2.5f);
		b.run(0.1f);
		check(std::fabs(b.outL() - 2.5f) < 1e-4f && std::fabs(b.outR() - 2.5f) < 1e-4f, "gauche et droite = entrée");
	}

	std::printf("\n%s (%d échec%s)\n", failures ? "ÉCHECS" : "Tout passe", failures, failures > 1 ? "s" : "");
	return failures ? 1 : 0;
}
