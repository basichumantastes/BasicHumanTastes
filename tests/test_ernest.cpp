// Banc d'essai hors de Rack pour Ernest : la hauteur jouée, avec et sans V/OCT, et les CV de forme et de decay
#include "../src/Ernest.cpp"
#include <cstdio>

Plugin* pluginInstance = NULL;

static int failures = 0;
static void check(bool ok, const char* what) {
	std::printf("%s %s\n", ok ? "  ok " : "  ÉCHEC", what);
	if (!ok) failures++;
}

struct Bench {
	Ernest m;
	Module::ProcessArgs args;
	float sr = 48000.f;
	Bench() {
		args.sampleRate = sr;
		args.sampleTime = 1.f / sr;
		args.frame = 0;
		for (int i = 0; i < Ernest::INPUTS_LEN; i++) m.inputs[i].channels = 0;
		// Sans modulation de hauteur, pour mesurer la note seule
		m.params[Ernest::MOD_DEPTH_PARAM].setValue(0.f);
	}
	void patch(int id, float v) { m.inputs[id].channels = 1; m.inputs[id].setVoltage(v); }
	// Fréquence mesurée sur les passages par zéro montants, pendant une seconde
	float frequency() {
		for (int i = 0; i < 4800; i++) { m.process(args); args.frame++; }
		int n = (int) sr, ups = 0, first = -1, last = -1;
		float prev = m.outputs[Ernest::OUT_OUTPUT].getVoltage();
		for (int i = 0; i < n; i++) {
			m.process(args); args.frame++;
			float v = m.outputs[Ernest::OUT_OUTPUT].getVoltage();
			if (prev <= 0.f && v > 0.f) {
				if (first < 0) first = i;
				last = i;
				ups++;
			}
			prev = v;
		}
		return ups > 1 ? (ups - 1) * sr / (last - first) : 0.f;
	}
};

static bool near(float a, float b) { return std::fabs(a - b) / b < 0.005f; }

int main() {
	std::printf("Hauteur d'Ernest\n");
	{
		Bench b;
		float f = b.frequency();
		std::printf("     sans V/OCT : %.1f Hz\n", f);
		check(near(f, 55.f), "sans V/OCT, PITCH donne la hauteur (55 Hz au départ)");
	}
	{
		Bench b;
		b.patch(Ernest::VOCT_INPUT, 0.f);
		float f = b.frequency();
		std::printf("     V/OCT 0 V : %.1f Hz\n", f);
		check(near(f, dsp::FREQ_C4), "V/OCT à 0 V : do 4 (261,6 Hz)");
		check(noteName(b.m.basePitch) == "C4", "l'afficheur annonce C4");
	}
	{
		Bench b;
		b.patch(Ernest::VOCT_INPUT, -2.f + 9.f / 12.f); // la 2
		float f = b.frequency();
		check(near(f, 110.f), "V/OCT -2 V + 9 demi-tons : la 2 (110 Hz)");
		check(noteName(b.m.basePitch) == "A2", "l'afficheur annonce A2");
	}
	{
		Bench b;
		b.patch(Ernest::VOCT_INPUT, 0.f);
		b.m.params[Ernest::PITCH_PARAM].setValue(PITCH_DEFAULT + 7.2f / 12.f); // un peu plus de 7 demi-tons
		float f = b.frequency();
		check(near(f, dsp::FREQ_C4 * std::pow(2.f, 7.f / 12.f)), "PITCH transpose par demi-tons entiers (sol 4)");
		check(noteName(b.m.basePitch) == "G4", "l'afficheur annonce G4");
	}
	check(noteName(1.f / 12.f) == "Db4" && noteName(-1.f) == "C3", "noms des notes en bémols");

	std::printf("CV de forme et de decay\n");
	{
		Bench b;
		b.m.process(b.args);
		check(b.m.modType == Ernest::MOD_ENVELOPE, "sans CV, la forme est celle du sélecteur");
		b.patch(Ernest::TYPE_INPUT, -2.f);
		b.m.process(b.args);
		check(b.m.modType == Ernest::MOD_SAMPLE_HOLD, "TYPE −2 V : deux formes plus bas (random)");
		b.patch(Ernest::TYPE_INPUT, -2.6f);
		b.m.process(b.args);
		check(b.m.modType == Ernest::MOD_TRIANGLE, "TYPE −2,6 V : arrondi à la forme la plus proche (triangle)");
		b.patch(Ernest::TYPE_INPUT, 10.f);
		b.m.process(b.args);
		check(b.m.modType == Ernest::MOD_ENVELOPE, "TYPE 10 V : bloqué sur la dernière forme");
		b.patch(Ernest::TYPE_INPUT, -10.f);
		b.m.process(b.args);
		check(b.m.modType == Ernest::MOD_SAW_DOWN, "TYPE −10 V : bloqué sur la première forme");
	}
	{
		// Niveau de l'enveloppe 0,1 s après un coup, selon la tension sur DECAY
		auto level = [](float decayCv, bool patched) {
			Bench b;
			b.patch(Ernest::TRIG_INPUT, 0.f);
			if (patched) b.patch(Ernest::DECAY_INPUT, decayCv);
			b.m.process(b.args);
			b.m.inputs[Ernest::TRIG_INPUT].setVoltage(10.f);
			for (int i = 0; i < 4800; i++) b.m.process(b.args);
			return b.m.ampEnv;
		};
		float knob = level(0.f, false), longer = level(3.f, true), shorter = level(-3.f, true);
		std::printf("     enveloppe à 0,1 s : %.3f sans CV, %.3f à +3 V, %.4f à −3 V\n", knob, longer, shorter);
		check(std::fabs(level(0.f, true) - knob) < 1e-6f, "DECAY 0 V : comme sans câble");
		check(longer > knob && shorter < knob, "DECAY : + allonge, − raccourcit");
		// Bouton à 0,65 + 3 V / 10 = 0,95 : 0,003 × 1000^0,95 ≈ 1,49 s
		check(std::fabs(longer - std::exp(-0.1f / (0.003f * std::pow(1000.f, 0.95f)))) < 0.01f, "DECAY +3 V : un dixième de course par volt");
	}
	std::printf(failures ? "%d échec(s)\n" : "tout passe\n", failures);
	return failures ? 1 : 0;
}
