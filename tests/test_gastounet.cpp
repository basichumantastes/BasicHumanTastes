// Banc d'essai hors de Rack : Gastounet collé à gauche de Gaston, les messages échangés comme le ferait le moteur
// de Rack (bascule des tampons à la fin de chaque échantillon).
#include "../src/Gaston.cpp"
#include "../src/Gastounet.cpp"
#include <cstdio>

Plugin* pluginInstance = NULL;

static int failures = 0;
static void check(bool ok, const char* what) {
	std::printf("%s %s\n", ok ? "  ok " : "  ÉCHEC", what);
	if (!ok) failures++;
}

static void flip(Module::Expander& e) {
	if (e.messageFlipRequested) {
		std::swap(e.producerMessage, e.consumerMessage);
		e.messageFlipRequested = false;
	}
}

struct Pair {
	Gaston* g = new Gaston;
	Gastounet* n = new Gastounet;
	Module::ProcessArgs args;
	int endPulses = 0;
	bool endHigh = false;

	Pair(bool attach = true) {
		args.sampleRate = 48000.f;
		args.sampleTime = 1.f / 48000.f;
		args.frame = 0;
		g->model = modelGaston;
		n->model = modelGastounet;
		for (int i = 0; i < Gaston::INPUTS_LEN; i++) g->inputs[i].channels = 0;
		for (int i = 0; i < Gastounet::INPUTS_LEN; i++) n->inputs[i].channels = 0;
		if (attach) {
			g->leftExpander.module = n;
			n->rightExpander.module = g;
		}
	}
	~Pair() { delete g; delete n; }

	void step() {
		n->process(args);
		g->process(args);
		flip(g->leftExpander);
		flip(n->rightExpander);
		bool h = n->outputs[Gastounet::END_OUTPUT].getVoltage() > 5.f;
		if (h && !endHigh) endPulses++;
		endHigh = h;
		args.frame++;
	}
	void run(float seconds) { for (int i = 0, k = (int) (seconds * args.sampleRate); i < k; i++) step(); }
	// Un appui sur un bouton de Gastounet, le temps de quelques échantillons
	void press(int button) {
		n->params[button].setValue(1.f);
		run(0.002f);
		n->params[button].setValue(0.f);
		run(0.002f);
	}
};

static const float STEP = 0.125f; // une double croche à 120 BPM

int main() {
	std::printf("Gastounet et Gaston\n");
	{
		Pair p;
		p.run(0.01f);
		check(p.g->gnConnected && p.g->core.memOn, "Gaston voit Gastounet collé à sa gauche");
		// Un motif sur Gaston, écrit dans A02 avec WRITE puis le slot 2
		for (int k = 0; k < STEPS; k += 4) p.g->core.lanes[0].on[k] = true;
		p.press(GN_WRITE);
		check(p.g->core.mem.armed == ARM_WRITE, "WRITE s'arme");
		p.press(GN_SLOT0 + 1);
		check(p.g->core.mem.slots[1].filled && p.g->core.mem.active == 1 && p.g->core.mem.armed == ARM_NONE, "WRITE + slot 02 : A02 est écrit et devient actif");
		// Un second motif dans A03
		for (int k = 0; k < STEPS; k++) p.g->core.lanes[0].on[k] = true;
		p.press(GN_WRITE);
		p.press(GN_SLOT0 + 2);
		// En lecture, on demande A02 : il attend la mesure
		p.g->core.rewind(p.g->st);
		p.g->core.play();
		p.run(STEP * 3.5f);
		int before = p.endPulses;
		p.press(GN_SLOT0 + 1);
		check(p.g->core.mem.queued == 1, "le slot 02 attend la mesure");
		p.run(STEP * 13.f);
		check(p.g->core.mem.active == 1 && !p.g->core.lanes[0].on[1], "à la mesure, A02 est chargé dans Gaston");
		check(p.endPulses == before + 1, "la sortie END de Gastounet envoie un trig au changement");
		// Le mode morceau et ses lignes
		p.press(GN_SONGREC);
		p.press(GN_SLOT0 + 2);
		p.press(GN_SLOT0 + 1);
		check(p.g->core.mem.songMode && p.g->core.mem.songLen == 2, "SONG REC : deux lignes ajoutées");
		p.press(GN_QUANT0 + Q_NOW);
		p.press(GN_LEGATO);
		check(p.g->core.mem.quant == Q_NOW && p.g->core.mem.legato, "LAUNCH : NOW et LEGATO");
		p.press(GN_BANK0 + 2);
		check(p.g->core.mem.bank == 2, "banque C affichée");
		// Sauvegarde : les mémoires et le morceau passent par le JSON de Gaston
		json_t* j = p.g->dataToJson();
		Pair q;
		q.g->dataFromJson(j);
		json_decref(j);
		check(q.g->core.mem.slots[1].filled && q.g->core.mem.slots[2].filled && !q.g->core.mem.slots[0].filled, "les mémoires pleines sont relues");
		check(q.g->core.mem.slots[1].lanes[0].on[4] && !q.g->core.mem.slots[1].lanes[0].on[5], "le contenu des mémoires est relu");
		check(q.g->core.mem.songLen == 2 && q.g->core.mem.song[0].pattern == 2, "le morceau est relu");
	}
	{
		Pair p(false);
		p.run(0.01f);
		check(!p.g->gnConnected && !p.g->core.memOn, "sans Gastounet, Gaston ignore les mémoires");
	}
	{
		// Une probabilité et une tension passent bien par le codage compact
		Lane a, b;
		a.prob[3] = 0.5f;
		a.on[3] = true;
		json_t* j = laneToJsonCompact(a);
		laneFromJsonCompact(b, j);
		json_decref(j);
		check(std::fabs(b.prob[3] - 0.5f) < 0.005f && b.on[3], "probabilité relue à 8 bits près");
		Lane c, d;
		c.cv = d.cv = true;
		c.clear(); c.value[5] = -0.33f; c.on[6] = false;
		j = laneToJsonCompact(c);
		laneFromJsonCompact(d, j);
		json_decref(j);
		check(std::fabs(d.value[5] + 0.33f) < 1e-4f && !d.on[6] && d.on[5], "tension et activation relues");
	}
	std::printf(failures ? "%d échec(s)\n" : "tout passe\n", failures);
	return failures ? 1 : 0;
}
