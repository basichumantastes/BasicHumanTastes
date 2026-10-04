// Banc d'essai hors de Rack pour le cœur de Lucienne, et extraits audio (WAV) pour écouter sans Rack
#include "../src/LucienneCore.hpp"
#include <cstdio>
#include <string>
#include <chrono>
#include <functional>

using namespace lucienne;

static int failures = 0;
static void check(bool ok, const char* what) {
	std::printf("%s %s\n", ok ? "  ok " : "  ÉCHEC", what);
	if (!ok) failures++;
}

static void writeWav(const std::string& path, const std::vector<float>& samples, int sr) {
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) return;
	int n = (int) samples.size();
	auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); u32(36 + n * 2); std::fwrite("WAVEfmt ", 1, 8, f);
	u32(16); u16(1); u16(2); u32(sr); u32(sr * 4); u16(4); u16(16);
	std::fwrite("data", 1, 4, f); u32(n * 2);
	for (float s : samples) { int16_t v = (int16_t) std::lround(clampf(s, -1.f, 1.f) * 32000.f); std::fwrite(&v, 2, 1, f); }
	std::fclose(f);
}

static const float SR = 48000.f;

struct Take {
	float peak = 0, rms = 0; bool finite = true; double realtime = 0;
};

// Fait tourner le cœur pendant `seconds`, avec des réglages qui peuvent bouger dans le temps
static Take render(float seconds, std::function<void(float, Params&)> automate, std::vector<float>* wav, uint64_t seed = 7) {
	Core core; core.init(SR, seed);
	Params p;
	Take t; double e = 0; int n = (int) (seconds * SR);
	auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < n; i++) {
		automate(i / SR, p);
		float l, r; core.process(p, l, r);
		if (!std::isfinite(l) || !std::isfinite(r)) t.finite = false;
		t.peak = std::max(t.peak, std::max(std::fabs(l), std::fabs(r)));
		e += l * l + r * r;
		if (wav) { wav->push_back(l / 5.f); wav->push_back(r / 5.f); }
	}
	double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	t.rms = (float) std::sqrt(e / (2.0 * n));
	t.realtime = seconds / el;
	return t;
}

// Rapport de fréquence entre la voix 1 et la voix 0, compté sur les basculements des comparateurs
static float ratio(float serrage, float tune1) {
	Core core; core.init(SR, 3);
	Params p;
	p.serrage = serrage; p.derive = 0.f; p.courant = 1.f; p.halo = 0.f;
	p.tune[1] = tune1;
	for (int i = 0; i < (int) (1.f * SR); i++) { float l, r; core.process(p, l, r); }
	int flips0 = 0, flips1 = 0; float d0 = core.dir[0], d1 = core.dir[1];
	for (int i = 0; i < (int) (4.f * SR); i++) {
		float l, r; core.process(p, l, r);
		if (core.dir[0] != d0) { flips0++; d0 = core.dir[0]; }
		if (core.dir[1] != d1) { flips1++; d1 = core.dir[1]; }
	}
	return flips0 > 0 ? (float) flips1 / flips0 : 0.f;
}

int main(int argc, char** argv) {
	const char* wavDir = argc > 1 ? argv[1] : NULL;

	std::printf("Le couplage colle une voix accordée près de la quinte\n");
	{
		float lockedSpan = 0, freeSpan = 0;
		for (float dlt = -0.6f; dlt <= 0.601f; dlt += 0.1f) {
			float rf = ratio(0.f, 7.02f + dlt), rl = ratio(0.4f, 7.02f + dlt);
			std::printf("    écart %+.1f dt : libre %.4f, serré %.4f\n", dlt, rf, rl);
			if (std::fabs(rl - 1.5f) < 0.002f) lockedSpan += 0.1f;
			if (std::fabs(rf - 1.5f) < 0.002f) freeSpan += 0.1f;
		}
		std::printf("    plage collée : libre %.1f dt, serré %.1f dt\n", freeSpan, lockedSpan);
		check(lockedSpan > freeSpan + 0.25f, "le serrage élargit la zone où la quinte tient");
	}

	std::printf("Anneau des LFO : fondu à puissance constante entre deux crans\n");
	{
		float a = slotWeight(7.5f, SLOT_FILTRE_B, 1.f), b = slotWeight(7.5f, SLOT_FILTRE_A, 1.f), c = slotWeight(7.5f, SLOT_HALO, 1.f);
		float on = slotWeight(3.f, SLOT_COURANT, 1.f), wrap = slotWeight(11.5f, SLOT_HAUTEUR, 1.f);
		std::printf("    entre B et A : %.3f / %.3f, voisin %.3f ; pile sur un cran %.3f ; entre le dernier et le premier %.3f\n", a, b, c, on, wrap);
		check(std::fabs(a * a + b * b - 1.f) < 0.01f && c == 0.f, "deux crans voisins se partagent la puissance");
		check(on == 1.f && std::fabs(wrap - 0.707f) < 0.01f, "un cran exact pèse 1 et l'anneau se referme");
	}

	std::printf("Filtres à fond de résonance, rail affamé : rien ne s'emballe\n");
	{
		Take t = render(20.f, [](float, Params& p) {
			p.resoA = 1.f; p.resoB = 1.f; p.routage = 0.5f; p.freqA = 0.5f; p.freqB = 0.7f; p.courant = 0.f; p.serrage = 1.f;
		}, NULL);
		std::printf("    crête %.2f V, RMS %.2f V\n", t.peak, t.rms);
		check(t.finite && t.peak < 5.f, "borné");
	}

	struct Scene { const char* name; float seconds; std::function<void(float, Params&)> f; };
	std::vector<Scene> scenes = {
		{"1-nappe", 40.f, [](float, Params& p) {
			p.serrage = 0.12f; p.souffle = 0.9f; p.halo = 0.6f; p.derive = 0.35f; p.timbre = 0.15f;
		}},
		{"2-serrage-0-a-1", 60.f, [](float t, Params& p) {
			p.serrage = t / 60.f; p.souffle = 0.9f; p.halo = 0.45f; p.timbre = 0.25f;
		}},
		{"3-ecart-resserre-puis-etire", 60.f, [](float t, Params& p) {
			float u = t / 60.f;
			p.ecart = u < 0.33f ? 0.5f - 1.5f * u : 0.5f * (u - 0.33f) / 0.67f * 3.f - 0.f;
			p.ecart = clampf(u < 0.33f ? 0.5f - 1.5f * u : (u - 0.33f) * 1.5f, 0.f, 1.f);
			p.serrage = 0.3f; p.souffle = 0.9f; p.halo = 0.5f;
		}},
		{"4-courant-1-a-0", 55.f, [](float t, Params& p) {
			p.courant = clampf(1.f - t / 45.f, 0.f, 1.f); p.serrage = 0.25f; p.souffle = 0.85f; p.halo = 0.4f; p.timbre = 0.35f;
		}},
		{"5-noeud-pince", 40.f, [](float, Params& p) {
			p.souffle = 0.08f; p.cadence = 0.62f; p.densite = 0.5f; p.boucle = 0.15f; p.memoire = 0.4f;
			p.serrage = 0.6f; p.timbre = 0.55f; p.halo = 0.35f; p.hauteur = 1.2f;
		}},
		{"6-respiration", 60.f, [](float, Params& p) {
			p.densite = 0.55f; p.cadence = 0.25f; p.souffle = 0.85f; p.boucle = 0.3f; p.serrage = 0.2f; p.halo = 0.65f;
		}},
		{"8-filtre-A-balaye", 50.f, [](float t, Params& p) {
			// Échelle très résonante balayée lentement de bas en haut puis retour
			float u = t / 50.f;
			p.freqA = 0.25f + 0.65f * (u < 0.5f ? 2.f * u : 2.f - 2.f * u);
			p.resoA = 0.85f; p.freqB = 1.f; p.resoB = 0.f; p.serrage = 0.3f; p.souffle = 0.9f; p.halo = 0.5f; p.timbre = 0.4f;
		}},
		{"9-voyelle", 50.f, [](float t, Params& p) {
			// Deux pics en parallèle qui se croisent lentement
			p.routage = 0.5f;
			p.freqA = 0.5f + 0.1f * std::sin(2.f * PI * t / 23.f); p.resoA = 0.7f;
			p.freqB = 0.68f + 0.12f * std::sin(2.f * PI * t / 37.f); p.resoB = 0.8f;
			p.serrage = 0.35f; p.souffle = 0.9f; p.halo = 0.55f; p.timbre = 0.5f;
		}},
		{"10-creux-passe-bande", 50.f, [](float t, Params& p) {
			p.routage = 1.f; p.freqA = 0.35f; p.resoA = 0.3f;
			p.freqB = 0.6f + 0.2f * std::sin(2.f * PI * t / 31.f); p.resoB = 0.85f;
			p.densite = 0.6f; p.cadence = 0.3f; p.souffle = 0.8f; p.halo = 0.7f; p.timbre = 0.6f; p.serrage = 0.25f;
		}},
		{"11-lfo-entre-les-filtres", 60.f, [](float, Params& p) {
			p.routage = 0.5f; p.freqA = 0.5f; p.resoA = 0.65f; p.freqB = 0.65f; p.resoB = 0.7f; p.timbre = 0.45f; p.halo = 0.55f;
			p.lfo[0].dest = 7.5f / 11.f; p.lfo[0].depth = 0.8f; p.lfo[0].rate = 0.35f; p.lfo[0].spread = 0.5f;
			p.lfo[1].dest = 0.f; p.lfo[1].depth = 0.3f; p.lfo[1].rate = 0.45f; p.lfo[1].spread = 0.7f; p.lfo[1].shape = 0.75f;
		}},
		{"12-lfo-noues", 60.f, [](float, Params& p) {
			// Chacun module la vitesse de l'autre, et déborde sur le timbre et la destination de l'autre
			p.timbre = 0.4f; p.resoA = 0.5f; p.freqA = 0.6f; p.halo = 0.5f; p.serrage = 0.3f;
			for (int l = 0; l < 2; l++) {
				p.lfo[l].dest = 10.f / 11.f; p.lfo[l].width = 0.35f; p.lfo[l].depth = 0.7f; p.lfo[l].spread = 0.4f;
			}
			p.lfo[0].rate = 0.55f; p.lfo[1].rate = 0.5f; p.lfo[1].shape = 0.3f;
		}},
		{"13-frappes", 48.f, [](float t, Params& p) {
			// Un trig toutes les 1,5 s : pincé les 24 premières secondes, puis vagues qui gonflent et s'effacent
			p.trigMode = true;
			p.strike = std::fmod(t, 1.5f) < 1.f / 48000.f;
			p.souffle = t < 24.f ? 0.1f : 0.5f;
			p.serrage = 0.35f; p.resoA = 0.5f; p.freqA = 0.62f; p.halo = 0.55f; p.timbre = 0.45f;
		}},
		{"7-accorder-a-l-oreille", 40.f, [](float t, Params& p) {
			// La voix 2 balaie de la sixte mineure à la septième ; libre les 20 premières secondes, serrée ensuite
			float u = std::fmod(t, 20.f) / 20.f;
			p.tune[1] = 6.2f + 1.6f * u;
			p.serrage = t < 20.f ? 0.f : 0.4f;
			p.derive = 0.f; p.souffle = 0.9f; p.halo = 0.3f; p.densite = 1.f;
			for (int i = 2; i < VOICES; i++) p.tune[i] = 0.f;
		}},
	};

	for (auto& s : scenes) {
		std::printf("Scène %s\n", s.name);
		std::vector<float> wav;
		Take t = render(s.seconds, s.f, wavDir ? &wav : NULL);
		std::printf("    crête %.2f V, RMS %.2f V, %.0f× le temps réel\n", t.peak, t.rms, t.realtime);
		check(t.finite, "aucune valeur folle");
		check(t.rms > 0.15f && t.peak < 4.95f, "niveau audible sans saturer");
		if (wavDir) writeWav(std::string(wavDir) + "/lucienne-" + s.name + ".wav", wav, (int) SR);
	}

	std::printf(failures ? "\n%d échec(s)\n" : "\nTout est bon\n", failures);
	return failures ? 1 : 0;
}
