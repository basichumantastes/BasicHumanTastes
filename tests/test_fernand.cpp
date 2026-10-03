// Banc d'essai hors de Rack pour Fernand, et un voyage complet joué par Colette (WAV)
#include "../src/Fernand.cpp"
#include "../src/Colette.cpp"
#include <cstdio>

Plugin* pluginInstance = NULL;

static int failures = 0;
static void check(bool ok, const char* what) {
	std::printf("%s %s\n", ok ? "  ok " : "  ÉCHEC", what);
	if (!ok) failures++;
}

template <typename M>
struct Bench {
	M m;
	Module::ProcessArgs args;
	float sr = 48000.f;
	Bench() {
		args.sampleRate = sr;
		args.sampleTime = 1.f / sr;
		args.frame = 0;
		for (int i = 0; i < (int) m.inputs.size(); i++) m.inputs[i].channels = 0;
	}
	void set(int id, float v) { m.params[id].setValue(v); }
	void patch(int id, float v) { m.inputs[id].channels = 1; m.inputs[id].setVoltage(v); }
	float out(int id) { return m.outputs[id].getVoltage(); }
	void step() { m.process(args); args.frame++; }
};

// STAY et TRAVEL : valeurs de bouton qui donnent des durées courtes, pour tester vite
static float stayKnob(float seconds) { return std::log(seconds) / std::log(600.f); }
static float travelKnob(float secondsForFullMap) { return std::log(secondsForFullMap / 2.f) / std::log(300.f); }

struct Events { std::vector<float> gusts, arrivals; std::vector<int> visits; std::vector<float> harmonyChanges; };

static Events record(Bench<Fernand>& b, float seconds) {
	Events e;
	bool g = false, a = false;
	b.step();  // première valeur des sorties : ce n'est pas un changement d'accord
	float lastHarmony = b.out(Fernand::HARMONY_OUTPUT);
	for (int i = 0; i < (int) (seconds * b.sr); i++) {
		b.step();
		float t = i / b.sr;
		bool gh = b.out(Fernand::GUST_OUTPUT) > 5.f, ah = b.out(Fernand::ARRIVE_OUTPUT) > 5.f;
		if (gh && !g) e.gusts.push_back(t);
		if (ah && !a) { e.arrivals.push_back(t); e.visits.push_back(b.m.current); }
		g = gh; a = ah;
		float h = b.out(Fernand::HARMONY_OUTPUT);
		if (h != lastHarmony) { e.harmonyChanges.push_back(t); lastHarmony = h; }
	}
	return e;
}

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

	std::printf("Dans l'ordre : 1 → 2 → 3 → 4 → 5 → 1\n");
	{
		Bench<Fernand> b;
		b.set(Fernand::STAY_PARAM, stayKnob(2.f));
		b.set(Fernand::TRAVEL_PARAM, travelKnob(3.f));
		Events e = record(b, 40.f);
		std::printf("    îles visitées :");
		for (int v : e.visits) std::printf(" %d", v + 1);
		std::printf("\n");
		bool order = e.visits.size() >= 5;
		for (size_t k = 0; k < e.visits.size(); k++) order &= e.visits[k] == (int) ((k + 1) % 5);
		check(order, "chaque île à son tour, puis retour à la première");
		check(e.gusts.size() >= e.arrivals.size() && e.gusts.size() <= e.arrivals.size() + 1, "un GUST à chaque départ");
		// L'accord change à mi-chemin : entre le départ et l'arrivée de chaque traversée
		bool midway = !e.harmonyChanges.empty();
		for (float t : e.harmonyChanges) {
			if (e.arrivals.empty() || t > e.arrivals.back())
				continue;  // traversée pas encore terminée quand le test s'arrête
			bool inside = false;
			for (size_t k = 0; k < e.arrivals.size() && k < e.gusts.size(); k++) {
				float mid = 0.5f * (e.gusts[k] + e.arrivals[k]);
				inside |= std::fabs(t - mid) < 0.05f;
			}
			midway &= inside;
		}
		if (!midway) {
			std::printf("    départs :"); for (float t : e.gusts) std::printf(" %.2f", t);
			std::printf("\n    arrivées :"); for (float t : e.arrivals) std::printf(" %.2f", t);
			std::printf("\n    changements d'accord :"); for (float t : e.harmonyChanges) std::printf(" %.2f", t);
			std::printf("\n");
		}
		check(midway, "HARMONY change à mi-traversée");
	}

	std::printf("Vent et attraction pendant la traversée\n");
	{
		Bench<Fernand> b;
		b.set(Fernand::STAY_PARAM, stayKnob(1.f));
		b.set(Fernand::TRAVEL_PARAM, travelKnob(10.f));
		b.set(Fernand::WEATHER_PARAM, 0.8f);
		float restWind = 0.f, maxWind = 0.f, minPull = 10.f, restPull = 0.f;
		for (int i = 0; i < 48000 * 12; i++) {
			b.step();
			if (b.out(Fernand::RESTING_OUTPUT) > 5.f) { restWind = b.out(Fernand::WIND_OUTPUT); restPull = b.out(Fernand::PULL_OUTPUT); }
			else { maxWind = std::max(maxWind, b.out(Fernand::WIND_OUTPUT)); minPull = std::min(minPull, b.out(Fernand::PULL_OUTPUT)); }
		}
		std::printf("    au repos : vent %.1f V, pull %.1f V ; en route : vent jusqu'à %.1f V, pull jusqu'à %.1f V\n", restWind, restPull, maxWind, minPull);
		check(maxWind > restWind + 4.f && minPull < restPull - 4.f, "le vent se lève et les notes lâchent prise en route");
	}

	std::printf("HOLD retient, NEXT fait partir\n");
	{
		Bench<Fernand> b;
		b.set(Fernand::STAY_PARAM, stayKnob(1.f));
		b.patch(Fernand::HOLD_INPUT, 10.f);
		Events e = record(b, 5.f);
		check(e.gusts.empty() && b.out(Fernand::RESTING_OUTPUT) > 5.f, "personne ne part tant que HOLD est haut");
		b.patch(Fernand::NEXT_INPUT, 10.f);
		b.step();
		check(b.m.traveling, "NEXT : départ immédiat");
	}

	std::printf("Horloge : repos et traversées comptés en temps\n");
	{
		Bench<Fernand> b;
		b.set(Fernand::STAY_PARAM, std::sqrt(3.f / 63.f));   // 4 temps
		b.set(Fernand::TRAVEL_PARAM, 0.f);                   // le minimum : 1 temps par traversée courte
		b.patch(Fernand::CLOCK_INPUT, 0.f);
		std::vector<float> departures;
		bool g = false;
		for (int i = 0; i < 48000 * 20; i++) {
			b.m.inputs[Fernand::CLOCK_INPUT].setVoltage((i % 24000) < 240 ? 10.f : 0.f);  // 2 Hz
			b.step();
			bool gh = b.out(Fernand::GUST_OUTPUT) > 5.f;
			if (gh && !g) departures.push_back(i / 48000.f);
			g = gh;
		}
		bool onBeat = !departures.empty();
		for (float t : departures) onBeat &= std::fabs(std::fmod(t, 0.5f)) < 0.002f || std::fabs(std::fmod(t, 0.5f) - 0.5f) < 0.002f;
		std::printf("    %zu départs en 20 s, premiers à %.2f s et %.2f s\n", departures.size(), departures.size() > 0 ? departures[0] : -1.f, departures.size() > 1 ? departures[1] : -1.f);
		check(onBeat, "chaque départ tombe sur un temps");
	}

	std::printf("Édition : les boutons suivent l'île choisie et écrivent dedans\n");
	{
		Bench<Fernand> b;
		for (int i = 0; i < 64; i++) b.step();
		b.set(Fernand::SELECT_PARAM, 3.f);
		for (int i = 0; i < 64; i++) b.step();
		check(b.m.params[Fernand::EDIT_HARMONY_PARAM].getValue() == b.m.islands[2].harmony, "le bouton HARMONY montre l'île 3");
		b.set(Fernand::EDIT_HARMONY_PARAM, 8.f);
		b.set(Fernand::EDIT_ROOT_PARAM, -7.f);
		for (int i = 0; i < 64; i++) b.step();
		check(b.m.islands[2].harmony == 8 && b.m.islands[2].root == -7, "l'île 3 a pris les nouvelles valeurs");
		int before = b.m.islands[0].harmony;
		b.set(Fernand::SELECT_PARAM, 1.f);
		for (int i = 0; i < 64; i++) b.step();
		check(b.m.islands[0].harmony == before && b.m.params[Fernand::EDIT_HARMONY_PARAM].getValue() == before, "changer d'île ne recopie rien par erreur");

		std::printf("Sauvegarde dans le patch\n");
		json_t* j = b.m.dataToJson();
		Bench<Fernand> c;
		c.m.dataFromJson(j);
		json_decref(j);
		bool same = true;
		for (int i = 0; i < MAX_ISLANDS; i++)
			same &= c.m.islands[i].harmony == b.m.islands[i].harmony && c.m.islands[i].root == b.m.islands[i].root && c.m.islands[i].x == b.m.islands[i].x;
		check(same, "la carte revient à l'identique");
	}

	std::printf("Errance et caprice : on finit par voir toutes les îles\n");
	{
		Bench<Fernand> b;
		b.set(Fernand::ROUTE_PARAM, 2.f);
		b.set(Fernand::CAPRICE_PARAM, 0.3f);
		b.set(Fernand::STAY_PARAM, stayKnob(0.5f));
		b.set(Fernand::TRAVEL_PARAM, travelKnob(1.f));
		Events e = record(b, 60.f);
		bool seen[MAX_ISLANDS] = {};
		for (int v : e.visits) seen[v] = true;
		int n = 0; for (int i = 0; i < 5; i++) n += seen[i];
		std::printf("    %zu arrivées, %d îles différentes\n", e.visits.size(), n);
		check(n == 5, "les cinq îles visitées");
	}

	if (wavDir) {
		std::printf("Un voyage joué par Colette (3 min)\n");
		Bench<Fernand> f;
		Bench<Colette> c;
		f.set(Fernand::STAY_PARAM, stayKnob(14.f));
		f.set(Fernand::TRAVEL_PARAM, travelKnob(22.f));
		f.set(Fernand::WEATHER_PARAM, 0.5f);
		// Colette pilotée par Fernand : ses boutons à zéro, les CV font tout
		c.set(Colette::HARMONY_PARAM, 0.f);
		c.set(Colette::PULL_PARAM, 0.f);
		c.set(Colette::WIND_PARAM, 0.f);
		c.set(Colette::BIRDS_PARAM, 16.f);
		c.set(Colette::BLOOM_PARAM, 0.7f);
		c.set(Colette::SPACE_PARAM, 0.65f);
		c.set(Colette::TIMBRE_PARAM, 0.25f);
		for (int id : {Colette::HARMONY_INPUT, Colette::PULL_INPUT, Colette::WIND_INPUT, Colette::VOCT_INPUT, Colette::GUST_INPUT}) c.patch(id, 0.f);
		std::vector<float> wav;
		for (int i = 0; i < 48000 * 180; i++) {
			f.step();
			c.m.inputs[Colette::HARMONY_INPUT].setVoltage(f.out(Fernand::HARMONY_OUTPUT));
			c.m.inputs[Colette::PULL_INPUT].setVoltage(f.out(Fernand::PULL_OUTPUT));
			c.m.inputs[Colette::WIND_INPUT].setVoltage(f.out(Fernand::WIND_OUTPUT));
			c.m.inputs[Colette::VOCT_INPUT].setVoltage(f.out(Fernand::ROOT_OUTPUT));
			c.m.inputs[Colette::GUST_INPUT].setVoltage(f.out(Fernand::GUST_OUTPUT));
			c.step();
			wav.push_back(c.out(Colette::LEFT_OUTPUT) / 5.f);
			wav.push_back(c.out(Colette::RIGHT_OUTPUT) / 5.f);
		}
		writeWav((std::string(wavDir) + "/fernand-colette-voyage.wav").c_str(), wav, 48000);
		std::printf("    fernand-colette-voyage.wav\n");
	}

	std::printf("\n%s (%d échec%s)\n", failures ? "ÉCHECS" : "Tout passe", failures, failures > 1 ? "s" : "");
	return failures ? 1 : 0;
}
