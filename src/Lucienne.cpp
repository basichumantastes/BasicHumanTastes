#include "plugin.hpp"
#include "widgets.hpp"
#include "LucienneCore.hpp"

// Lucienne : six oscillateurs à intégrateur et comparateur, noués en anneau. On les accorde à l'oreille, voix par voix ;
// SERRAGE les fait s'accrocher aux intervalles justes, puis se moduler l'un l'autre jusqu'au chaos. ÉCART resserre ou étire
// l'accord. COURANT affame leur alimentation : la hauteur s'affaisse, le son s'écrase et bégaie. Un registre ouvre leurs
// portes basse-bas (DENSITÉ, CADENCE, BOUCLE, SOUFFLE) et peut les faire sauter d'octave (MÉMOIRE) ; un trig (ou FRAPPE)
// les frappe, et tant qu'il est branché elles ne tiennent plus. Deux filtres résonants,
// en série ou en parallèle (ROUTAGE), puis un halo. Deux LFO asynchrones se posent sur un cadran de douze destinations,
// en fondu entre les crans voisins ; ils peuvent viser la vitesse ou la destination l'un de l'autre.


// Les noms complets des crans, pour l'infobulle du potard DESTINATION
static const char* const LUC_SLOT_FULL[2][lucienne::SLOTS] = {
	{"Hauteur", "Écart", "Serrage", "Courant", "Souffle", "Densité", "Halo", "Filtre B", "Filtre A", "Timbre", "Vitesse du LFO B", "Destination du LFO B"},
	{"Hauteur", "Écart", "Serrage", "Courant", "Souffle", "Densité", "Halo", "Filtre B", "Filtre A", "Timbre", "Vitesse du LFO A", "Destination du LFO A"},
};

// Affiche le cran visé, ou les deux crans entre lesquels le LFO se partage
struct LucienneDestQuantity : ParamQuantity {
	int lfo = 0;
	std::string getDisplayValueString() override {
		float D = getValue() * (lucienne::SLOTS - 1);
		int a = (int) std::floor(D + 1e-4f);
		float t = D - a;
		if (t < 0.03f || a >= lucienne::SLOTS - 1)
			return LUC_SLOT_FULL[lfo][clamp(a, 0, lucienne::SLOTS - 1)];
		if (t > 0.97f)
			return LUC_SLOT_FULL[lfo][a + 1];
		return string::f("%s / %s", LUC_SLOT_FULL[lfo][a], LUC_SLOT_FULL[lfo][a + 1]);
	}
};


struct Lucienne : Module {
	enum ParamId {
		ENUMS(TUNE_PARAMS, lucienne::VOICES),
		ECART_PARAM,
		SERRAGE_PARAM,
		COURANT_PARAM,
		SOUFFLE_PARAM,
		HALO_PARAM,
		HAUTEUR_PARAM,
		TIMBRE_PARAM,
		DERIVE_PARAM,
		FREQ_A_PARAM,
		RESO_A_PARAM,
		ROUTAGE_PARAM,
		FREQ_B_PARAM,
		RESO_B_PARAM,
		CADENCE_PARAM,
		DENSITE_PARAM,
		BOUCLE_PARAM,
		MEMOIRE_PARAM,
		// Par LFO, dans cet ordre : vitesse, forme, destination, largeur, profondeur, éventail
		ENUMS(LFO_PARAMS, 12),
		STRIKE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		VOCT_INPUT,
		ECART_INPUT,
		SERRAGE_INPUT,
		COURANT_INPUT,
		SOUFFLE_INPUT,
		HALO_INPUT,
		FREQ_A_INPUT,
		FREQ_B_INPUT,
		DENSITE_INPUT,
		CLOCK_INPUT,
		// Par LFO : signal qui le remplace, vitesse, destination
		ENUMS(LFO_INPUTS, 6),
		TRIG_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(VOICE_OUTPUTS, lucienne::VOICES),
		STEP_OUTPUT,
		TICK_OUTPUT,
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		ENUMS(LFO_OUTPUTS, 2),
		RAIL_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		STRIKE_LIGHT,
		LIGHTS_LEN
	};
	enum LfoField { RATE, SHAPE, DEST, WIDTH, DEPTH, SPREAD, LFO_FIELDS };

	lucienne::Core core;
	lucienne::Params p;
	dsp::PulseGenerator tickPulse;
	dsp::SchmittTrigger trigInput, strikeButton;
	float strikeFlash = 0.f;
	bool clockHigh = false;

	Lucienne() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		lucienne::Params d;
		for (int i = 0; i < lucienne::VOICES; i++)
			configParam(TUNE_PARAMS + i, -12.f, 36.f, d.tune[i], i == 0 ? "Voix 1 (référence)" : string::f("Voix %d", i + 1), " demi-tons");
		configParam(ECART_PARAM, 0.f, 1.f, 0.5f, "Écart (0 : unisson, milieu : l'accord réglé, à fond : intervalles doublés)", "%", 0.f, 100.f);
		configParam(SERRAGE_PARAM, 0.f, 1.f, 0.2f, "Serrage (libre, accroché aux intervalles justes, FM croisée, chaos)", "%", 0.f, 100.f);
		configParam(COURANT_PARAM, 0.f, 1.f, 1.f, "Courant (alimentation propre à fond, affamée à zéro)", "%", 0.f, 100.f);
		configParam(SOUFFLE_PARAM, 0.f, 1.f, 0.8f, "Souffle (pincé à nappe)", "%", 0.f, 100.f);
		configParam(HALO_PARAM, 0.f, 1.f, 0.5f, "Halo", "%", 0.f, 100.f);
		configParam(HAUTEUR_PARAM, -1.f, 3.f, 0.5f, "Hauteur (octaves au-dessus de do 2)", " oct");
		configParam(TIMBRE_PARAM, 0.f, 1.f, 0.2f, "Timbre (triangle, carré, repli)", "%", 0.f, 100.f);
		configParam(DERIVE_PARAM, 0.f, 1.f, 0.3f, "Dérive (instabilité lente de chaque voix)", "%", 0.f, 100.f);
		configParam(FREQ_A_PARAM, 0.f, 1.f, 0.8f, "Filtre A, fréquence (échelle 24 dB)", " Hz", 548.7f, 30.f);
		configParam(RESO_A_PARAM, 0.f, 1.f, 0.15f, "Filtre A, résonance", "%", 0.f, 100.f);
		configParam(ROUTAGE_PARAM, 0.f, 1.f, 0.f, "Routage (série, parallèle, parallèle avec B en passe-bande)", "%", 0.f, 100.f);
		configParam(FREQ_B_PARAM, 0.f, 1.f, 0.9f, "Filtre B, fréquence (12 dB)", " Hz", 548.7f, 30.f);
		configParam(RESO_B_PARAM, 0.f, 1.f, 0.1f, "Filtre B, résonance", "%", 0.f, 100.f);
		configParam(CADENCE_PARAM, 0.f, 1.f, 0.4f, "Cadence du registre", " Hz", 256.f, 0.05f);
		configParam(DENSITE_PARAM, 0.f, 1.f, 1.f, "Densité (voix ouvertes ; à fond, drone)", "%", 0.f, 100.f);
		configParam(BOUCLE_PARAM, 0.f, 1.f, 0.3f, "Boucle (motif figé à mutation continue)", "%", 0.f, 100.f);
		configParam(MEMOIRE_PARAM, 0.f, 1.f, 0.f, "Mémoire (sauts d'octave, puis écarts libres)", "%", 0.f, 100.f);

		static const char* const NAMES[2] = {"LFO A", "LFO B"};
		// Au départ : A respire sur le filtre A, B fait à peine chanceler les hauteurs
		static const float DEST0[2] = {8.f / 11.f, 0.f}, DEPTH0[2] = {0.25f, 0.12f}, RATE0[2] = {0.35f, 0.45f}, SPREAD0[2] = {0.3f, 0.6f};
		for (int l = 0; l < 2; l++) {
			std::string n = NAMES[l];
			configParam(LFO_PARAMS + l * LFO_FIELDS + RATE, 0.f, 1.f, RATE0[l], n + ", vitesse", " Hz", 36000.f, 1.f / 1200.f);
			configParam(LFO_PARAMS + l * LFO_FIELDS + SHAPE, 0.f, 1.f, 0.f, n + ", forme (sinus, triangle, rampe, aléatoire lissé, en escalier)", "%", 0.f, 100.f);
			configParam<LucienneDestQuantity>(LFO_PARAMS + l * LFO_FIELDS + DEST, 0.f, 1.f, DEST0[l], n + ", destination")->lfo = l;
			configParam(LFO_PARAMS + l * LFO_FIELDS + WIDTH, 0.f, 1.f, 0.f, n + ", largeur (un cran, puis tout l'anneau)", "%", 0.f, 100.f);
			configParam(LFO_PARAMS + l * LFO_FIELDS + DEPTH, -1.f, 1.f, DEPTH0[l], n + ", profondeur", "%", 0.f, 100.f);
			configParam(LFO_PARAMS + l * LFO_FIELDS + SPREAD, 0.f, 1.f, SPREAD0[l], n + ", éventail (décalage de phase entre les voix)", "%", 0.f, 100.f);
			configInput(LFO_INPUTS + l * 3 + 0, n + ", signal qui remplace le LFO");
			configInput(LFO_INPUTS + l * 3 + 1, n + ", vitesse (1 V/oct)");
			configInput(LFO_INPUTS + l * 3 + 2, n + ", destination (10 V parcourent le cadran)");
			configOutput(LFO_OUTPUTS + l, n);
		}

		configInput(VOCT_INPUT, "V/OCT");
		configInput(ECART_INPUT, "Écart");
		configInput(SERRAGE_INPUT, "Serrage");
		configInput(COURANT_INPUT, "Courant");
		configInput(SOUFFLE_INPUT, "Souffle");
		configInput(HALO_INPUT, "Halo");
		configInput(FREQ_A_INPUT, "Filtre A, fréquence");
		configInput(FREQ_B_INPUT, "Filtre B, fréquence");
		configInput(DENSITE_INPUT, "Densité");
		configInput(CLOCK_INPUT, "Horloge du registre (remplace la cadence)");
		configInput(TRIG_INPUT, "Trig (branché, les voix ne tiennent plus : chaque frappe les fait monter puis retomber)");
		configButton(STRIKE_PARAM, "Frappe");
		for (int i = 0; i < lucienne::VOICES; i++)
			configOutput(VOICE_OUTPUTS + i, string::f("Voix %d (après sa porte, avant les filtres)", i + 1));
		configOutput(STEP_OUTPUT, "Pas du registre (0 à 5 V)");
		configOutput(TICK_OUTPUT, "Top du registre");
		configOutput(LEFT_OUTPUT, "Gauche");
		configOutput(RIGHT_OUTPUT, "Droite");
		configOutput(RAIL_OUTPUT, "Rail (tension de l'alimentation, 0 à 10 V)");

		core.init(48000.f, random::u64());
	}

	float knob(int param, int input) {
		return clamp(params[param].getValue() + inputs[input].getVoltage() / 10.f, 0.f, 1.f);
	}

	void onReset() override {
		core.init(core.sr, random::u64());
	}

	void process(const ProcessArgs& args) override {
		if (args.sampleRate != core.sr)
			core.init(args.sampleRate, random::u64());

		for (int i = 0; i < lucienne::VOICES; i++)
			p.tune[i] = params[TUNE_PARAMS + i].getValue();
		p.ecart = knob(ECART_PARAM, ECART_INPUT);
		p.serrage = knob(SERRAGE_PARAM, SERRAGE_INPUT);
		p.courant = knob(COURANT_PARAM, COURANT_INPUT);
		p.souffle = knob(SOUFFLE_PARAM, SOUFFLE_INPUT);
		p.halo = knob(HALO_PARAM, HALO_INPUT);
		p.hauteur = params[HAUTEUR_PARAM].getValue() + inputs[VOCT_INPUT].getVoltage();
		p.timbre = params[TIMBRE_PARAM].getValue();
		p.derive = params[DERIVE_PARAM].getValue();
		p.freqA = knob(FREQ_A_PARAM, FREQ_A_INPUT);
		p.resoA = params[RESO_A_PARAM].getValue();
		p.routage = params[ROUTAGE_PARAM].getValue();
		p.freqB = knob(FREQ_B_PARAM, FREQ_B_INPUT);
		p.resoB = params[RESO_B_PARAM].getValue();
		p.cadence = params[CADENCE_PARAM].getValue();
		p.densite = knob(DENSITE_PARAM, DENSITE_INPUT);
		p.boucle = params[BOUCLE_PARAM].getValue();
		p.memoire = params[MEMOIRE_PARAM].getValue();

		// Horloge externe : un seuil avec hystérésis, le cœur repère le front montant
		p.clockConnected = inputs[CLOCK_INPUT].isConnected();
		float clk = inputs[CLOCK_INPUT].getVoltage();
		if (clockHigh && clk < 0.5f)
			clockHigh = false;
		else if (!clockHigh && clk > 1.5f)
			clockHigh = true;
		p.clockHigh = clockHigh;

		// Frappe : front montant du trig ou du bouton
		bool trig = trigInput.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f);
		bool button = strikeButton.process(params[STRIKE_PARAM].getValue());
		p.trigMode = inputs[TRIG_INPUT].isConnected();
		p.strike = trig || button;
		if (p.strike)
			strikeFlash = 1.f;

		for (int l = 0; l < 2; l++) {
			lucienne::LfoParams& lp = p.lfo[l];
			int base = LFO_PARAMS + l * LFO_FIELDS;
			lp.rate = params[base + RATE].getValue();
			lp.shape = params[base + SHAPE].getValue();
			lp.dest = params[base + DEST].getValue();
			lp.width = params[base + WIDTH].getValue();
			lp.depth = params[base + DEPTH].getValue();
			lp.spread = params[base + SPREAD].getValue();
			lp.inConnected = inputs[LFO_INPUTS + l * 3 + 0].isConnected();
			lp.in = inputs[LFO_INPUTS + l * 3 + 0].getVoltage();
			lp.rateCv = inputs[LFO_INPUTS + l * 3 + 1].getVoltage();
			lp.destCv = inputs[LFO_INPUTS + l * 3 + 2].getVoltage();
		}

		float l, r;
		core.process(p, l, r);

		outputs[LEFT_OUTPUT].setVoltage(l);
		outputs[RIGHT_OUTPUT].setVoltage(r);
		for (int i = 0; i < lucienne::VOICES; i++)
			outputs[VOICE_OUTPUTS + i].setVoltage(core.voiceSignal[i]);
		for (int k = 0; k < 2; k++)
			outputs[LFO_OUTPUTS + k].setVoltage(5.f * core.lfo[k].value);
		outputs[STEP_OUTPUT].setVoltage(5.f * core.cells[0]);
		if (core.tickedNow)
			tickPulse.trigger(0.005f);
		outputs[TICK_OUTPUT].setVoltage(tickPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[RAIL_OUTPUT].setVoltage(10.f * clamp(core.railSmooth, 0.f, 1.f));
		lights[STRIKE_LIGHT].setBrightnessSmooth(strikeFlash, args.sampleTime);
		strikeFlash = std::max(0.f, strikeFlash - args.sampleTime * 8.f);
	}
};


// --- Le panneau : un schéma vivant, traits cuivre sur encre. Les coordonnées (mm) sont partagées avec
// tools/lucienne_panel.py, qui dessine le fond res/Lucienne.svg.

static const NVGcolor LUC_INK = nvgRGB(0x12, 0x14, 0x1f);
static const NVGcolor LUC_COPPER = nvgRGB(0xd0, 0x8c, 0x5c);
static const NVGcolor LUC_VERDIGRIS = nvgRGB(0x7f, 0xc0, 0xac);
static const NVGcolor LUC_BONE = nvgRGB(0xec, 0xe2, 0xcf);

static const float LUC_HEX_X = 142.24f, LUC_HEX_Y = 46.f, LUC_HEX_R = 27.f;
static const float LUC_DIAL_X[2] = {24.f, 260.48f}, LUC_DIAL_Y = 48.f;
static const float LUC_DIAL_LABEL_R = 18.5f, LUC_DIAL_ARC_R = 13.4f;
static const float LUC_BAND_Y = 90.f;
static const float LUC_BAND_X[13] = {59.24f, 72.24f, 85.24f, 98.24f, 116.24f, 129.24f, 142.24f, 155.24f, 168.24f, 186.24f, 199.24f, 212.24f, 225.24f};
static const float LUC_JACK_X0 = 59.74f, LUC_JACK_DX = 16.5f, LUC_IN_Y = 104.f, LUC_OUT_Y = 116.f;

static Vec lucVoicePos(int i, float r) {
	float a = i * M_PI / 3.f;
	return Vec(LUC_HEX_X + r * std::sin(a), LUC_HEX_Y - r * std::cos(a));
}


// L'écran du nœud : les six voix sur leur hexagone, qui s'allument avec leur porte. Les liens de l'anneau
// s'épaississent avec le serrage et tremblent avec le chaos ; au centre, le rail, qui rétrécit quand il s'affame.
struct LucienneKnot : TransparentWidget {
	Lucienne* module = NULL;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		Vec c = box.size.div(2.f);
		float nodeR = mm2px(10.f);
		float env[lucienne::VOICES];
		float coupling = 0.3f, chaos = 0.f, rail = 1.f;
		for (int i = 0; i < lucienne::VOICES; i++)
			env[i] = 0.35f + 0.5f * (i % 2);
		if (module) {
			const lucienne::Core& k = module->core;
			for (int i = 0; i < lucienne::VOICES; i++)
				env[i] = clamp(k.env[i] * k.aliveGain[i], 0.f, 1.f);
			coupling = k.coupling;
			chaos = k.chaos;
			rail = k.railSmooth;
		}
		Vec pts[lucienne::VOICES];
		for (int i = 0; i < lucienne::VOICES; i++) {
			float a = i * M_PI / 3.f;
			pts[i] = c.plus(Vec(std::sin(a), -std::cos(a)).mult(nodeR));
		}

		// Les liens : un trait découpé en petits segments, déplacés au hasard à chaque image selon le chaos
		for (int i = 0; i < lucienne::VOICES; i++) {
			Vec a = pts[i], b = pts[(i + 1) % lucienne::VOICES];
			Vec d = b.minus(a), n = Vec(-d.y, d.x).normalize();
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, a.x, a.y);
			const int SEG = 10;
			for (int s = 1; s < SEG; s++) {
				float t = (float) s / SEG;
				float j = (random::uniform() - 0.5f) * mm2px(1.6f) * chaos;
				Vec q = a.plus(d.mult(t)).plus(n.mult(j));
				nvgLineTo(args.vg, q.x, q.y);
			}
			nvgLineTo(args.vg, b.x, b.y);
			nvgStrokeColor(args.vg, nvgTransRGBA(LUC_COPPER, (unsigned char) (50 + 170 * coupling)));
			nvgStrokeWidth(args.vg, 0.6f + 1.8f * coupling);
			nvgStroke(args.vg);
		}

		// Les voix
		for (int i = 0; i < lucienne::VOICES; i++) {
			float e = env[i];
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, pts[i].x, pts[i].y, mm2px(1.2f + 2.4f * e));
			nvgFillColor(args.vg, nvgTransRGBA(LUC_COPPER, (unsigned char) (25 + 90 * e)));
			nvgFill(args.vg);
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, pts[i].x, pts[i].y, mm2px(0.55f + 0.5f * e));
			nvgFillColor(args.vg, nvgTransRGBA(LUC_BONE, (unsigned char) (90 + 165 * e)));
			nvgFill(args.vg);
		}

		// Le rail
		float flicker = 1.f - 0.5f * chaos * random::uniform();
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, mm2px(1.f + 3.2f * clamp(rail, 0.f, 1.f)));
		nvgStrokeColor(args.vg, nvgTransRGBA(LUC_VERDIGRIS, (unsigned char) (200 * flicker)));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
	}
};


// Le cadran d'un LFO : un arc qui montre où il se pose et sur quelle largeur, et un point qui bat avec sa valeur
struct LucienneDial : TransparentWidget {
	Lucienne* module = NULL;
	int index = 0;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		Vec c = box.size.div(2.f);
		float R = mm2px(LUC_DIAL_ARC_R);
		float D = index == 0 ? 8.f : 0.f, h = 1.f, value = 0.6f, depth = 0.5f;
		if (module) {
			const lucienne::Lfo& f = module->core.lfo[index];
			D = f.D;
			h = f.h;
			value = f.value;
			depth = std::fabs(module->params[Lucienne::LFO_PARAMS + index * Lucienne::LFO_FIELDS + Lucienne::DEPTH].getValue());
		}
		NVGcolor col = index == 0 ? LUC_COPPER : LUC_VERDIGRIS;
		float glow = 0.25f + 0.75f * std::min(1.f, depth * 2.f);
		auto at = [&](float slot) {
			float a = lucienne::slotAngle(slot);
			return c.plus(Vec(std::sin(a), -std::cos(a)).mult(R));
		};
		// L'arc, plus intense au centre de la fenêtre
		const float STEP = 0.08f;
		for (float x = -h; x < h; x += STEP) {
			float w = std::cos(0.5f * M_PI * std::fabs(x + STEP * 0.5f) / h);
			Vec a = at(D + x), b = at(D + x + STEP);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, a.x, a.y);
			nvgLineTo(args.vg, b.x, b.y);
			nvgStrokeColor(args.vg, nvgTransRGBA(col, (unsigned char) (230 * w * glow)));
			nvgStrokeWidth(args.vg, 1.6f);
			nvgLineCap(args.vg, NVG_ROUND);
			nvgStroke(args.vg);
		}
		Vec m = at(D);
		float v = clamp(0.5f + 0.5f * value, 0.f, 1.f);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, m.x, m.y, mm2px(0.6f + 1.4f * v));
		nvgFillColor(args.vg, nvgTransRGBA(LUC_BONE, (unsigned char) (80 + 175 * v * glow)));
		nvgFill(args.vg);
	}
};


struct LucienneWidget : ModuleWidget {
	void addLabel(Vec posMm, std::string text, float fontSize = 5.f, NVGcolor color = LUC_BONE) {
		PanelLabel* label = createWidget<PanelLabel>(mm2px(posMm));
		label->text = text;
		label->fontSize = fontSize;
		label->color = color;
		addChild(label);
	}

	LucienneWidget(Lucienne* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Lucienne.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addLabel(Vec(LUC_HEX_X, 5.f), "LUCIENNE", 11.f);

		// Le nœud : l'écran au centre, les six voix sur l'hexagone
		LucienneKnot* knot = createWidget<LucienneKnot>(mm2px(Vec(LUC_HEX_X - 14.5f, LUC_HEX_Y - 14.5f)));
		knot->box.size = mm2px(Vec(29.f, 29.f));
		knot->module = module;
		addChild(knot);
		for (int i = 0; i < lucienne::VOICES; i++) {
			addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(lucVoicePos(i, LUC_HEX_R)), module, Lucienne::TUNE_PARAMS + i));
			// Le numéro est la borne de l'écran, sur le rayon de sa voix
			addLabel(lucVoicePos(i, 17.8f), string::f("%d", i + 1), 5.f, LUC_COPPER);
		}

		// Les ailes : les quatre gestes principaux
		static const char* const WING_NAMES[4] = {"SERRAGE", "ÉCART", "COURANT", "SOUFFLE"};
		static const int WING_IDS[4] = {Lucienne::SERRAGE_PARAM, Lucienne::ECART_PARAM, Lucienne::COURANT_PARAM, Lucienne::SOUFFLE_PARAM};
		static const float WING_X[4] = {80.f, 80.f, 204.48f, 204.48f}, WING_Y[4] = {28.f, 62.f, 28.f, 62.f};
		for (int i = 0; i < 4; i++) {
			addLabel(Vec(WING_X[i], WING_Y[i] - 13.f), WING_NAMES[i], 7.f);
			addParam(createParamCentered<Davies1900hLargeWhiteKnob>(mm2px(Vec(WING_X[i], WING_Y[i])), module, WING_IDS[i]));
		}

		// La bande du bas : voix, filtres, registre
		static const char* const BAND_NAMES[13] = {"HAUTEUR", "TIMBRE", "DÉRIVE", "HALO", "FRÉQ A", "RÉSO A", "ROUTAGE", "FRÉQ B", "RÉSO B", "CADENCE", "DENSITÉ", "BOUCLE", "MÉMOIRE"};
		static const int BAND_IDS[13] = {Lucienne::HAUTEUR_PARAM, Lucienne::TIMBRE_PARAM, Lucienne::DERIVE_PARAM, Lucienne::HALO_PARAM,
			Lucienne::FREQ_A_PARAM, Lucienne::RESO_A_PARAM, Lucienne::ROUTAGE_PARAM, Lucienne::FREQ_B_PARAM, Lucienne::RESO_B_PARAM,
			Lucienne::CADENCE_PARAM, Lucienne::DENSITE_PARAM, Lucienne::BOUCLE_PARAM, Lucienne::MEMOIRE_PARAM};
		for (int i = 0; i < 13; i++) {
			addLabel(Vec(LUC_BAND_X[i], LUC_BAND_Y - 6.5f), BAND_NAMES[i], 4.6f);
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(LUC_BAND_X[i], LUC_BAND_Y)), module, BAND_IDS[i]));
		}
		// Les noms des sections, posés sur le rail comme des étiquettes de schéma
		static const char* const SECTIONS[3] = {"VOIX", "FILTRES", "REGISTRE"};
		static const float SECTION_X[3] = {52.5f, 109.5f, 179.5f};
		for (int i = 0; i < 3; i++) {
			addLabel(Vec(SECTION_X[i], 78.5f), SECTIONS[i], 4.6f, LUC_COPPER);
			((PanelLabel*) children.back())->align = NVG_ALIGN_LEFT;
		}

		// Les deux rangées de jacks
		static const char* const IN_NAMES[11] = {"V/OCT", "TRIG", "ÉCART", "SERRAGE", "COURANT", "SOUFFLE", "HALO", "FILTRE A", "FILTRE B", "DENSITÉ", "HORLOGE"};
		static const int IN_IDS[11] = {Lucienne::VOCT_INPUT, Lucienne::TRIG_INPUT, Lucienne::ECART_INPUT, Lucienne::SERRAGE_INPUT, Lucienne::COURANT_INPUT,
			Lucienne::SOUFFLE_INPUT, Lucienne::HALO_INPUT, Lucienne::FREQ_A_INPUT, Lucienne::FREQ_B_INPUT, Lucienne::DENSITE_INPUT, Lucienne::CLOCK_INPUT};
		static const char* const OUT_NAMES[11] = {"1", "2", "3", "4", "5", "6", "PAS", "TOP", "RAIL", "L", "R"};
		static const int OUT_IDS[11] = {Lucienne::VOICE_OUTPUTS + 0, Lucienne::VOICE_OUTPUTS + 1, Lucienne::VOICE_OUTPUTS + 2, Lucienne::VOICE_OUTPUTS + 3,
			Lucienne::VOICE_OUTPUTS + 4, Lucienne::VOICE_OUTPUTS + 5, Lucienne::STEP_OUTPUT, Lucienne::TICK_OUTPUT, Lucienne::RAIL_OUTPUT,
			Lucienne::LEFT_OUTPUT, Lucienne::RIGHT_OUTPUT};
		for (int i = 0; i < 11; i++) {
			float x = LUC_JACK_X0 + i * LUC_JACK_DX;
			addLabel(Vec(x, LUC_IN_Y - 6.5f), IN_NAMES[i], 4.4f);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, LUC_IN_Y)), module, IN_IDS[i]));
			addLabel(Vec(x, LUC_OUT_Y - 6.5f), OUT_NAMES[i], 4.4f, LUC_INK);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, LUC_OUT_Y)), module, OUT_IDS[i]));
		}

		// La frappe à la main, en haut à droite du nœud
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(170.f, 12.f)), module, Lucienne::STRIKE_PARAM, Lucienne::STRIKE_LIGHT));
		addLabel(Vec(175.f, 12.f), "FRAPPE", 5.f, LUC_COPPER);
		((PanelLabel*) children.back())->align = NVG_ALIGN_LEFT;

		// Les deux LFO, en miroir
		static const char* const SLOT_NAMES[2][lucienne::SLOTS] = {
			{"HAUT", "ÉCART", "SERR", "COUR", "SOUF", "DENS", "HALO", "FLT B", "FLT A", "TIMB", "VIT·B", "DST·B"},
			{"HAUT", "ÉCART", "SERR", "COUR", "SOUF", "DENS", "HALO", "FLT B", "FLT A", "TIMB", "VIT·A", "DST·A"},
		};
		for (int l = 0; l < 2; l++) {
			float x = LUC_DIAL_X[l];
			int base = Lucienne::LFO_PARAMS + l * Lucienne::LFO_FIELDS;
			NVGcolor accent = l == 0 ? LUC_COPPER : LUC_VERDIGRIS;
			addLabel(Vec(x, 5.f), l == 0 ? "LFO A" : "LFO B", 7.f, accent);

			addLabel(Vec(x - 9.f, 11.5f), "VITESSE", 4.6f);
			addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(x - 9.f, 19.5f)), module, base + Lucienne::RATE));
			addLabel(Vec(x + 11.f, 11.5f), "FORME", 4.6f);
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(x + 11.f, 19.5f)), module, base + Lucienne::SHAPE));

			LucienneDial* dial = createWidget<LucienneDial>(mm2px(Vec(x - 16.f, LUC_DIAL_Y - 16.f)));
			dial->box.size = mm2px(Vec(32.f, 32.f));
			dial->module = module;
			dial->index = l;
			addChild(dial);
			for (int s = 0; s < lucienne::SLOTS; s++) {
				float a = lucienne::slotAngle(s);
				addLabel(Vec(x + LUC_DIAL_LABEL_R * std::sin(a), LUC_DIAL_Y - LUC_DIAL_LABEL_R * std::cos(a)), SLOT_NAMES[l][s], 4.f);
			}
			ParamWidget* dest = createParamCentered<Davies1900hLargeWhiteKnob>(mm2px(Vec(x, LUC_DIAL_Y)), module, base + Lucienne::DEST);
			addParam(dest);

			static const char* const SMALL[3] = {"LARGEUR", "PROFONDEUR", "ÉVENTAIL"};
			static const int SMALL_F[3] = {Lucienne::WIDTH, Lucienne::DEPTH, Lucienne::SPREAD};
			for (int k = 0; k < 3; k++) {
				float kx = x + (k - 1) * 14.5f;
				addLabel(Vec(kx, 73.5f), SMALL[k], 4.f);
				addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(kx, 80.f)), module, base + SMALL_F[k]));
			}

			static const char* const JN[4] = {"IN", "VITESSE", "DEST", "OUT"};
			for (int k = 0; k < 4; k++) {
				float jx = x + (k % 2 ? 8.f : -8.f), jy = k < 2 ? LUC_IN_Y : LUC_OUT_Y;
				addLabel(Vec(jx, jy - 6.5f), JN[k], 4.4f, k == 3 ? LUC_INK : LUC_BONE);
				if (k < 3)
					addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jx, jy)), module, Lucienne::LFO_INPUTS + l * 3 + k));
				else
					addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jx, jy)), module, Lucienne::LFO_OUTPUTS + l));
			}
		}
	}
};


Model* modelLucienne = createModel<Lucienne, LucienneWidget>("Lucienne");
