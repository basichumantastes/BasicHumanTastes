// Banc d'essai hors de Rack pour Colette, et extraits audio (WAV) pour écouter sans Rack
#include "../src/Colette.cpp"
#include <cstdio>
#include <chrono>

Plugin* pluginInstance = NULL;

static int failures = 0;
static void check(bool ok, const char* what) {
	std::printf("%s %s\n", ok ? "  ok " : "  ÉCHEC", what);
	if (!ok) failures++;
}

struct Bench {
	Colette m;
	Module::ProcessArgs args;
	float sr = 48000.f;
	Bench() {
		args.sampleRate = sr;
		args.sampleTime = 1.f / sr;
		args.frame = 0;
		for (int i = 0; i < Colette::INPUTS_LEN; i++) m.inputs[i].channels = 0;
	}
	void set(int id, float v) { m.params[id].setValue(v); }
	void patch(int id, float v) { m.inputs[id].channels = 1; m.inputs[id].setVoltage(v); }
	float l() { return m.outputs[Colette::LEFT_OUTPUT].getVoltage(); }
	float r() { return m.outputs[Colette::RIGHT_OUTPUT].getVoltage(); }
	void step() { m.process(args); args.frame++; }
	int landed() { int n = 0; for (int i = 0; i < m.activeBirds; i++) n += m.birds[i].landed; return n; }
	// Fait tourner et renvoie crête, RMS, nombre de LAND, et si tout est resté fini
	void run(float seconds, float* peak = NULL, float* rms = NULL, int* lands = NULL, bool* finite = NULL, std::vector<float>* wav = NULL) {
		float p = 0.f; double e = 0.0; int n = (int) (seconds * sr), land = 0; bool fin = true, high = false;
		for (int i = 0; i < n; i++) {
			step();
			float a = l(), b = r();
			if (!std::isfinite(a) || !std::isfinite(b)) fin = false;
			p = std::max(p, std::max(std::fabs(a), std::fabs(b)));
			e += a * a + b * b;
			bool h = m.outputs[Colette::LAND_OUTPUT].getVoltage() > 5.f;
			if (h && !high) land++;
			high = h;
			if (wav) { wav->push_back(a / 5.f); wav->push_back(b / 5.f); }
		}
		if (peak) *peak = p;
		if (rms) *rms = std::sqrt(e / (2.0 * n));
		if (lands) *lands = land;
		if (finite) *finite = fin;
	}
};

static void writeWav(const char* path, const std::vector<float>& samples, int sr) {
	FILE* f = std::fopen(path, "wb");
	if (!f) return;
	int n = (int) samples.size();
	auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); u32(36 + n * 2); std::fwrite("WAVEfmt ", 1, 8, f);
	u32(16); u16(1); u16(2); u32(sr); u32(sr * 4); u16(4); u16(16);
	std::fwrite("data", 1, 4, f); u32(n * 2);
	for (float s : samples) { int16_t v = (int16_t) std::lround(clamp(s, -1.f, 1.f) * 32000.f); std::fwrite(&v, 2, 1, f); }
	std::fclose(f);
}

int main(int argc, char** argv) {
	random::init();
	const char* wavDir = argc > 1 ? argv[1] : NULL;

	std::printf("Réglages de départ : stable, audible, sans saturer\n");
	{
		Bench b;
		float peak, rms; int lands; bool finite;
		std::vector<float> wav;
		b.run(40.f, &peak, &rms, &lands, &finite, wavDir ? &wav : NULL);
		std::printf("    crête %.2f V, RMS %.2f V, %d oiseaux posés en 40 s, %d/%d posés à la fin\n", peak, rms, lands, b.landed(), b.m.activeBirds);
		check(finite, "aucune valeur folle");
		check(rms > 0.3f && peak < 5.f, "niveau confortable");
		check(lands > 0, "des oiseaux se posent");
		if (wavDir) writeWav((std::string(wavDir) + "/colette-1-depart.wav").c_str(), wav, 48000);
	}

	std::printf("PULL à fond, sans vent : la nuée finit par se poser sur l'accord\n");
	{
		Bench b;
		b.set(Colette::PULL_PARAM, 1.f);
		b.set(Colette::WIND_PARAM, 0.f);
		b.run(30.f);
		int landed = b.landed();
		bool onPerch = true;
		for (int i = 0; i < b.m.activeBirds; i++)
			if (b.m.birds[i].landed) onPerch &= std::fabs(b.m.nearestPerch(b.m.birds[i].y) - b.m.birds[i].y) < 1e-4f;
		std::printf("    %d/%d posés après 30 s\n", landed, b.m.activeBirds);
		check(landed >= b.m.activeBirds * 3 / 4, "au moins les trois quarts posés");
		check(onPerch, "chacun exactement sur une note de l'accord");
		float note = b.m.outputs[Colette::NOTE_OUTPUT].getVoltage() + 2.f;
		check(std::fabs(b.m.nearestPerch(note) - note) < 1e-4f, "NOTE donne la hauteur d'un perchoir (1V/oct)");

		std::printf("GUST : tout le monde s'envole\n");
		b.set(Colette::GUST_PARAM, 1.f);
		b.run(0.01f);
		b.set(Colette::GUST_PARAM, 0.f);
		b.run(0.05f);
		std::printf("    %d posés juste après la rafale\n", b.landed());
		check(b.landed() == 0, "plus aucun oiseau posé");
	}

	std::printf("Nuée posée : baisser PULL fait repartir les oiseaux tout de suite\n");
	{
		Bench b;
		b.set(Colette::PULL_PARAM, 1.f);
		b.set(Colette::WIND_PARAM, 0.f);
		b.run(30.f);
		int before = b.landed();
		b.set(Colette::PULL_PARAM, 0.f);
		b.run(3.f);
		std::printf("    %d posés, puis %d posés 3 s après avoir mis PULL à zéro\n", before, b.landed());
		check(b.landed() <= before / 3, "la plupart repartent en quelques secondes");
	}

	std::printf("Nuée posée : WIND fait trembler les notes avant même de faire décoller\n");
	{
		Bench b;
		b.set(Colette::PULL_PARAM, 1.f);
		b.set(Colette::WIND_PARAM, 0.f);
		b.set(Colette::FREEZE_PARAM, 0.f);
		b.run(30.f);
		b.set(Colette::FREEZE_PARAM, 1.f);  // les positions ne bougent plus : seul le tremblement change
		b.run(0.5f);
		check(b.m.currentWind == 0.f, "vent nul enregistré");
		b.set(Colette::WIND_PARAM, 1.f);
		b.run(0.1f);
		check(b.m.currentWind == 1.f, "le vent agit aussitôt, même nuée figée");
	}

	std::printf("FREE : sans perchoirs, personne ne se pose\n");
	{
		Bench b;
		b.set(Colette::HARMONY_PARAM, HARMONY_FREE);
		int lands;
		b.run(20.f, NULL, NULL, &lands);
		check(lands == 0 && b.landed() == 0, "aucun atterrissage");
	}

	std::printf("Changer d'accord : les oiseaux se reposent sur les nouvelles notes\n");
	{
		Bench b;
		b.set(Colette::PULL_PARAM, 1.f);
		b.set(Colette::WIND_PARAM, 0.f);
		b.set(Colette::HARMONY_PARAM, 1.f);  // MAJOR
		b.run(20.f);
		b.set(Colette::HARMONY_PARAM, 2.f);  // MINOR
		b.run(20.f);
		bool ok = true;
		for (int i = 0; i < b.m.activeBirds; i++)
			if (b.m.birds[i].landed) {
				float st = std::fmod(b.m.birds[i].y * 12.f + 1200.f, 12.f);
				ok &= std::fabs(st - 4.f) > 0.01f;  // plus de tierce majeure
			}
		std::printf("    %d/%d posés en mineur\n", b.landed(), b.m.activeBirds);
		check(ok && b.landed() > 0, "aucun oiseau resté sur la tierce majeure");
	}

	std::printf("HARMONY branchée : la CV choisit seule, quel que soit le bouton\n");
	{
		Bench b;
		b.set(Colette::HARMONY_PARAM, 4.f);
		b.patch(Colette::HARMONY_INPUT, 6.f);
		for (int i = 0; i < 64; i++) b.step();
		check(b.m.harmonyIndex() == 6, "bouton sur 4, CV à 6 V : PENTATONIC, pas FREE");
		b.patch(Colette::HARMONY_INPUT, 2.f);
		for (int i = 0; i < 64; i++) b.step();
		check(b.m.harmonyIndex() == 2, "CV à 2 V : MINOR");
	}

	std::printf("FREEZE : la nuée ne bouge plus\n");
	{
		Bench b;
		b.run(3.f);
		b.patch(Colette::FREEZE_INPUT, 10.f);
		float before[MAX_BIRDS];
		for (int i = 0; i < MAX_BIRDS; i++) before[i] = b.m.birds[i].y;
		float peak, rms;
		b.run(3.f, &peak, &rms);
		bool still = true;
		for (int i = 0; i < b.m.activeBirds; i++) still &= b.m.birds[i].y == before[i];
		check(still, "positions inchangées");
		check(rms > 0.2f, "le son continue");
	}

	std::printf("Réglages extrêmes, 32 oiseaux : rien ne s'emballe\n");
	{
		bool allFinite = true; float worst = 0.f;
		for (int k = 0; k < 4; k++) {
			Bench b;
			float v = k % 2 ? 1.f : 0.f;
			b.set(Colette::BIRDS_PARAM, 32.f);
			b.set(Colette::RANGE_PARAM, k < 2 ? 5.f : 1.f);
			b.set(Colette::ROOT_PARAM, k < 2 ? 12.f : -12.f);
			for (int id : {Colette::PULL_PARAM, Colette::WIND_PARAM, Colette::COHESION_PARAM, Colette::SCATTER_PARAM, Colette::TIMBRE_PARAM, Colette::BLOOM_PARAM, Colette::SPACE_PARAM})
				b.set(id, v);
			float peak; bool finite;
			b.run(15.f, &peak, NULL, NULL, &finite);
			allFinite &= finite;
			worst = std::max(worst, peak);
		}
		std::printf("    pire crête %.2f V\n", worst);
		check(allFinite && worst <= 5.f, "toujours fini et dans ±5 V");
	}

	std::printf("Charge processeur, 32 oiseaux\n");
	{
		Bench b;
		b.set(Colette::BIRDS_PARAM, 32.f);
		b.set(Colette::TIMBRE_PARAM, 1.f);
		b.set(Colette::WIND_PARAM, 1.f);
		auto t0 = std::chrono::steady_clock::now();
		b.run(10.f);
		double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		std::printf("    %.1f %% d'un cœur (10 s de son en %.2f s)\n", s / 10.0 * 100.0, s);
		check(s / 10.0 < 0.15, "moins de 15 % d'un cœur");
	}

	if (wavDir) {
		std::printf("Extraits audio\n");
		struct Scene { const char* name; float pull, wind, cohesion, scatter, harmony, birds, timbre, bloom, space, range, root; };
		Scene scenes[] = {
			{"colette-2-accord-qui-se-pose", 0.85f, 0.12f, 0.4f, 0.35f, 4.f, 16.f, 0.2f, 0.7f, 0.6f, 3.f, 0.f},
			{"colette-3-nuee-libre", 0.f, 0.6f, 0.7f, 0.6f, 9.f, 24.f, 0.6f, 0.4f, 0.5f, 2.f, 0.f},
			{"colette-4-harmoniques", 0.6f, 0.25f, 0.3f, 0.5f, 8.f, 20.f, 0.1f, 0.8f, 0.7f, 4.f, -5.f},
		};
		for (Scene& sc : scenes) {
			Bench b;
			b.set(Colette::PULL_PARAM, sc.pull); b.set(Colette::WIND_PARAM, sc.wind);
			b.set(Colette::COHESION_PARAM, sc.cohesion); b.set(Colette::SCATTER_PARAM, sc.scatter);
			b.set(Colette::HARMONY_PARAM, sc.harmony); b.set(Colette::BIRDS_PARAM, sc.birds);
			b.set(Colette::TIMBRE_PARAM, sc.timbre); b.set(Colette::BLOOM_PARAM, sc.bloom);
			b.set(Colette::SPACE_PARAM, sc.space); b.set(Colette::RANGE_PARAM, sc.range); b.set(Colette::ROOT_PARAM, sc.root);
			std::vector<float> wav;
			b.run(40.f, NULL, NULL, NULL, NULL, &wav);
			writeWav((std::string(wavDir) + "/" + sc.name + ".wav").c_str(), wav, 48000);
			std::printf("    %s.wav\n", sc.name);
		}
	}

	std::printf("\n%s (%d échec%s)\n", failures ? "ÉCHECS" : "Tout passe", failures, failures > 1 ? "s" : "");
	return failures ? 1 : 0;
}
