#include "plugin.hpp"
#include "widgets.hpp"

// Ernest : oscillateur de percussion. Sinus ou triangle, hauteur 20 Hz à 12 kHz, modulation de hauteur
// à 6 formes (saw down, square, triangle, random, noise, envelope), profondeur bipolaire, vitesse de
// 0,1 Hz à 5 kHz (au-delà de ~20 Hz on passe en modulation croisée), enveloppe de volume à simple
// decay, BASS (grave puis saturation) et modulation en anneau. CV sur la forme, la profondeur, la vitesse
// et le decay.


// Noms pour l'afficheur 14 segments : « ! » est l'espace pleine largeur de la police DSEG
static const char* const MOD_TYPE_NAMES[] = {"SAW!DOWN", "SQUARE", "TRIANGLE", "RANDOM", "NOISE", "ENVELOPE"};

// Bornes de la hauteur, en octaves par rapport à C4 (comme le V/Oct de Rack)
static const float PITCH_MIN = std::log2(20.f / dsp::FREQ_C4);
static const float PITCH_MAX = std::log2(12000.f / dsp::FREQ_C4);
static const float PITCH_DEFAULT = std::log2(55.f / dsp::FREQ_C4);

// Avec V/OCT branchée, PITCH transpose par demi-tons entiers depuis sa position de départ
static int pitchSemitones(float knob) {
	return (int) std::round((knob - PITCH_DEFAULT) * 12.f);
}

// Le nom de la note pour l'afficheur (bémols : la police DSEG n'a pas de dièse)
static std::string noteName(float pitch) {
	static const char* const NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
	int n = (int) std::round(pitch * 12.f);
	return string::f("%s%d", NAMES[((n % 12) + 12) % 12], 4 + (int) std::floor(n / 12.f));
}

// L'infobulle de PITCH : des Hz, ou une transposition quand V/OCT est branchée
struct ErnestPitchQuantity : ParamQuantity {
	int voctInput = 0;
	bool voct() { return module && module->inputs[voctInput].isConnected(); }
	std::string getLabel() override { return voct() ? "Transposition (V/OCT branchée)" : ParamQuantity::getLabel(); }
	std::string getUnit() override { return voct() ? " demi-tons" : ParamQuantity::getUnit(); }
	std::string getDisplayValueString() override {
		return voct() ? string::f("%+d", pitchSemitones(getValue())) : ParamQuantity::getDisplayValueString();
	}
	void setDisplayValueString(std::string s) override {
		if (!voct()) {
			ParamQuantity::setDisplayValueString(s);
			return;
		}
		setValue(clamp(PITCH_DEFAULT + std::atoi(s.c_str()) / 12.f, getMinValue(), getMaxValue()));
	}
};


struct Ernest : Module {
	enum ParamId {
		PITCH_PARAM,
		WAVE_PARAM,
		MOD_TYPE_PARAM,
		MOD_DEPTH_PARAM,
		MOD_SPEED_PARAM,
		DECAY_PARAM,
		LEVEL_PARAM,
		LOW_BOOST_PARAM,
		RING_PARAM,
		RETRIG_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		TRIG_INPUT,
		VOCT_INPUT,
		DEPTH_INPUT,
		SPEED_INPUT,
		RING_INPUT,
		// Ajoutées après coup : en fin de liste pour que les câbles des patchs existants restent en place
		TYPE_INPUT,
		DECAY_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		MOD_OUTPUT,
		OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		WAVE_LIGHT,
		RING_LIGHT,
		RETRIG_LIGHT,
		TRIG_LIGHT,
		LIGHTS_LEN
	};
	enum ModType {
		MOD_SAW_DOWN,
		MOD_SQUARE,
		MOD_TRIANGLE,
		MOD_SAMPLE_HOLD,
		MOD_NOISE,
		MOD_ENVELOPE,
		MOD_TYPES_LEN
	};

	dsp::SchmittTrigger trigger;
	dsp::PulseGenerator trigPulse;
	float phase = 0.f;
	float modPhase = 0.f;
	float sampleHold = 0.f;
	float pitchEnv = 0.f;
	float ampEnv = 0.f;
	float lowPass = 0.f;
	// La hauteur de base (sans la modulation), pour l'afficheur
	float basePitch = PITCH_DEFAULT;
	bool voctMode = false;
	// La forme jouée (bouton + CV), pour l'afficheur
	int modType = MOD_ENVELOPE;

	Ernest() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		// Réglages par défaut : un kick analogique (55 Hz, enveloppe de hauteur)
		configParam<ErnestPitchQuantity>(PITCH_PARAM, PITCH_MIN, PITCH_MAX, PITCH_DEFAULT, "Pitch", " Hz", 2.f, dsp::FREQ_C4)->voctInput = VOCT_INPUT;
		configSwitch(WAVE_PARAM, 0.f, 1.f, 0.f, "Wave", {"Sine", "Triangle"});
		configSwitch(MOD_TYPE_PARAM, 0.f, MOD_TYPES_LEN - 1, MOD_ENVELOPE, "Pitch modulation shape", {"Saw down (pitch falls, repeats)", "Square (two pitches alternate)", "Triangle (pitch rises and falls)", "Random steps", "Noise bursts (snares)", "Envelope (kicks, toms)"});
		configParam(MOD_DEPTH_PARAM, -1.f, 1.f, 0.5f, "Pitch modulation depth (negative = inverted)", "%", 0.f, 100.f);
		// 0,1 Hz × 50 000^x : de 0,1 Hz à 5 kHz
		configParam(MOD_SPEED_PARAM, 0.f, 1.f, 0.5f, "Pitch modulation rate (envelope: decay speed)", " Hz", 50000.f, 0.1f);
		configParam(DECAY_PARAM, 0.f, 1.f, 0.65f, "Decay (volume envelope length)", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f);
		configParam(LOW_BOOST_PARAM, 0.f, 1.f, 0.f, "Bass (low boost, then drive)", "%", 0.f, 100.f);
		configSwitch(RING_PARAM, 0.f, 1.f, 0.f, "Ring modulation with RING IN", {"Off", "On"});
		configSwitch(RETRIG_PARAM, 0.f, 1.f, 1.f, "Retrigger modulation on each hit", {"Off (modulation runs freely)", "On (every hit sounds the same)"});
		configInput(TRIG_INPUT, "Trigger (plays a hit)");
		configInput(VOCT_INPUT, "Pitch (1V/oct)");
		configInput(DEPTH_INPUT, "Modulation depth CV (±5 V = full range)");
		configInput(SPEED_INPUT, "Modulation rate CV (1 V = a tenth of the range)");
		configInput(RING_INPUT, "Ring modulation source (switch RING on)");
		configInput(TYPE_INPUT, "Modulation shape CV (1 V = one shape)");
		configInput(DECAY_INPUT, "Decay CV (1 V = a tenth of the range)");
		configOutput(MOD_OUTPUT, "Pitch modulation (after depth)");
		configOutput(OUT_OUTPUT, "Audio");
	}

	void onReset() override {
		phase = modPhase = sampleHold = pitchEnv = ampEnv = lowPass = 0.f;
	}

	// Signal de modulation pour la forme choisie. Les formes cycliques vont de 0 à 1,
	// pour que la hauteur réglée au bouton Pitch reste la hauteur « au repos ».
	// S&H et Noise sont bipolaires (−1 à 1), le hasard part dans les deux sens.
	float modSignal(int type) {
		switch (type) {
			case MOD_SAW_DOWN: return 1.f - modPhase;
			case MOD_SQUARE: return modPhase < 0.5f ? 1.f : 0.f;
			case MOD_TRIANGLE: return modPhase < 0.5f ? 2.f * modPhase : 2.f - 2.f * modPhase;
			case MOD_SAMPLE_HOLD: return sampleHold;
			// Du bruit ajouté par bouffées qui retombent à chaque cycle : pour les caisses claires
			case MOD_NOISE: return random::normal() * 0.5f * (1.f - modPhase);
			case MOD_ENVELOPE: return pitchEnv;
			default: return 0.f;
		}
	}

	void process(const ProcessArgs& args) override {
		// CV de forme : 1 V par forme, ajouté au sélecteur
		modType = (int) std::round(params[MOD_TYPE_PARAM].getValue() + inputs[TYPE_INPUT].getVoltage());
		modType = clamp(modType, 0, MOD_TYPES_LEN - 1);

		float depth = params[MOD_DEPTH_PARAM].getValue() + inputs[DEPTH_INPUT].getVoltage() / 5.f;
		depth = clamp(depth, -1.f, 1.f);
		float speedKnob = params[MOD_SPEED_PARAM].getValue() + inputs[SPEED_INPUT].getVoltage() / 10.f;
		float modSpeed = 0.1f * std::pow(50000.f, clamp(speedKnob, 0.f, 1.f));

		// Chaque trigger relance l'oscillateur et les deux enveloppes. Avec Retrig, le modulateur
		// repart aussi de zéro et chaque coup sonne pareil, comme sur une boîte à rythmes.
		// Sans RETRIG, il tourne librement et chaque coup tombe à un endroit différent du cycle.
		bool retrig = params[RETRIG_PARAM].getValue() > 0.5f;
		if (trigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f)) {
			phase = 0.f;
			if (retrig) {
				modPhase = 0.f;
				sampleHold = random::uniform() * 2.f - 1.f;
			}
			pitchEnv = 1.f;
			ampEnv = 1.f;
			trigPulse.trigger(0.05f);
		}

		// Modulateur : un cycle de 0 à 1 à la vitesse RATE
		modPhase += modSpeed * args.sampleTime;
		if (modPhase >= 1.f) {
			modPhase -= std::floor(modPhase);
			sampleHold = random::uniform() * 2.f - 1.f;
		}
		// En mode Envelope, Mod Speed règle la vitesse de retombée de l'enveloppe de hauteur
		pitchEnv *= std::exp(-modSpeed * args.sampleTime);

		// Profondeur : courbe au carré pour être fin près du centre, jusqu'à ±8 octaves en butée
		float depthOct = 8.f * depth * std::fabs(depth);
		float mod = depthOct * modSignal(modType);

		// V/OCT branchée : Ernest joue la note reçue (0 V = do 4, comme partout dans Rack), et PITCH
		// la transpose par demi-tons entiers. Débranchée : PITCH règle la hauteur, en Hz.
		voctMode = inputs[VOCT_INPUT].isConnected();
		if (voctMode)
			basePitch = inputs[VOCT_INPUT].getVoltage() + pitchSemitones(params[PITCH_PARAM].getValue()) / 12.f;
		else
			basePitch = params[PITCH_PARAM].getValue();
		float pitch = basePitch + mod;
		float freq = dsp::FREQ_C4 * dsp::exp2_taylor5(clamp(pitch, -10.f, 10.f));
		freq = clamp(freq, 0.f, 0.45f * args.sampleRate);

		phase += freq * args.sampleTime;
		phase -= std::floor(phase);

		bool triangle = params[WAVE_PARAM].getValue() > 0.5f;
		float osc;
		if (triangle)
			// Triangle calé pour démarrer à 0 en montant, comme le sinus
			osc = 4.f * std::fabs(phase - 0.75f - std::round(phase - 0.75f)) - 1.f;
		else
			osc = std::sin(2.f * M_PI * phase);

		// Modulation en anneau : le son ne passe que si l'autre signal est présent aussi
		bool ring = params[RING_PARAM].getValue() > 0.5f;
		if (ring)
			osc *= inputs[RING_INPUT].getVoltage() / 5.f;

		// Enveloppe de volume à simple Decay : de 3 ms à 3 s. Sans câble de trigger,
		// le module sonne en continu, pratique pour régler la hauteur et la modulation.
		float decay = clamp(params[DECAY_PARAM].getValue() + inputs[DECAY_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		float decayTime = 0.003f * std::pow(1000.f, decay);
		ampEnv *= std::exp(-args.sampleTime / decayTime);
		float amp = inputs[TRIG_INPUT].isConnected() ? ampEnv : 1.f;
		float out = osc * amp;

		// Low Boost : on rajoute les graves puis on sature, jusqu'à la distorsion en butée
		float boost = params[LOW_BOOST_PARAM].getValue();
		float lpCoeff = 1.f - std::exp(-2.f * M_PI * 150.f * args.sampleTime);
		lowPass += lpCoeff * (out - lowPass);
		if (boost > 0.f)
			out = std::tanh((out + 4.f * boost * lowPass) * (1.f + 3.f * boost));

		outputs[OUT_OUTPUT].setVoltage(5.f * out * params[LEVEL_PARAM].getValue());
		outputs[MOD_OUTPUT].setVoltage(clamp(5.f * mod / 8.f, -10.f, 10.f));

		lights[WAVE_LIGHT].setBrightness(triangle);
		lights[RING_LIGHT].setBrightness(ring);
		lights[RETRIG_LIGHT].setBrightness(retrig);
		lights[TRIG_LIGHT].setBrightnessSmooth(trigPulse.process(args.sampleTime), args.sampleTime);
	}
};


// Couleurs d'Ernest : textes anthracite sur l'ocre, crème sur l'anthracite
static const NVGcolor ERNEST_CHARCOAL = nvgRGB(0x26, 0x23, 0x1f);
static const NVGcolor ERNEST_CREAM = nvgRGB(0xf4, 0xec, 0xd8);

// Voyants vert d'eau, assortis à l'écran
template <typename TBase = GrayModuleLightWidget>
struct TVfdLight : TBase {
	TVfdLight() {
		this->addBaseColor(nvgRGB(0x5f, 0xf2, 0xd6));
	}
};
using VfdLight = TVfdLight<>;


// Afficheur 14 segments, vert d'eau façon tube fluorescent (police DSEG) : forme de modulation et hauteur de base
struct ErnestDisplay : TransparentWidget {
	Ernest* module = NULL;

	// Dessine une ligne de 8 caractères, avec les segments éteints en fond comme sur un vrai afficheur
	void drawLedLine(NVGcontext* vg, float x, float y, int align, const char* text) {
		nvgTextAlign(vg, align | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, nvgRGB(0x12, 0x30, 0x2a));
		nvgText(vg, x, y, "~~~~~~~~", NULL);
		nvgFillColor(vg, nvgRGB(0x5f, 0xf2, 0xd6));
		nvgText(vg, x, y, text, NULL);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DSEG14Classic-Bold.ttf"));
		if (!font)
			return;

		int modType = Ernest::MOD_ENVELOPE;
		float pitch = PITCH_DEFAULT;
		bool voct = false;
		if (module) {
			modType = module->modType;
			pitch = module->basePitch;
			voct = module->voctMode;
		}
		float freq = dsp::FREQ_C4 * std::pow(2.f, pitch);
		std::string freqText = freq < 1000.f ? string::f("%.0fHZ", freq) : string::f("%.2fKHZ", freq / 1000.f);
		// Avec V/OCT, la note jouée plutôt que la fréquence (« ! » : espace pleine largeur)
		if (voct)
			freqText = "NOTE!" + noteName(pitch);

		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 10.f);
		drawLedLine(args.vg, 5.f, box.size.y * 0.3f, NVG_ALIGN_LEFT, MOD_TYPE_NAMES[clamp(modType, 0, 5)]);
		drawLedLine(args.vg, box.size.x - 5.f, box.size.y * 0.72f, NVG_ALIGN_RIGHT, freqText.c_str());
	}
};


struct ErnestWidget : ModuleWidget {
	void addLabel(Vec posMm, std::string text, bool light = false, float fontSize = 8.f) {
		PanelLabel* label = createWidget<PanelLabel>(mm2px(posMm));
		label->text = text;
		label->fontSize = fontSize;
		if (light)
			label->color = ERNEST_CREAM;
		else
			label->color = ERNEST_CHARCOAL;
		addChild(label);
	}

	ErnestWidget(Ernest* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Ernest.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addLabel(Vec(30.48, 9.0), "ERNEST", true, 12.f);

		ErnestDisplay* display = createWidget<ErnestDisplay>(mm2px(Vec(6.0, 14.0)));
		display->box.size = mm2px(Vec(48.96, 10.0));
		display->module = module;
		addChild(display);

		// Oscillateur
		addLabel(Vec(20.0, 30.5), "PITCH");
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(20.0, 40.0)), module, Ernest::PITCH_PARAM));
		addLabel(Vec(36.5, 30.5), "WAVE");
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<VfdLight>>>(mm2px(Vec(36.5, 40.0)), module, Ernest::WAVE_PARAM, Ernest::WAVE_LIGHT));
		addLabel(Vec(36.5, 46.0), "SIN/TRI", false, 6.f);
		addLabel(Vec(52.5, 30.5), "RETRIG");
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<VfdLight>>>(mm2px(Vec(52.5, 40.0)), module, Ernest::RETRIG_PARAM, Ernest::RETRIG_LIGHT));

		// Modulation
		// Pas d'étiquette TYPE : les icônes de formes d'onde du panneau en tiennent lieu
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(12.0, 60.0)), module, Ernest::MOD_TYPE_PARAM));
		addLabel(Vec(30.48, 52.5), "DEPTH");
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(30.48, 60.0)), module, Ernest::MOD_DEPTH_PARAM));
		addLabel(Vec(49.0, 52.5), "RATE");
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(49.0, 60.0)), module, Ernest::MOD_SPEED_PARAM));

		// Ampli
		addLabel(Vec(12.0, 72.5), "DECAY");
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(12.0, 80.0)), module, Ernest::DECAY_PARAM));
		addLabel(Vec(30.48, 72.5), "LEVEL");
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(30.48, 80.0)), module, Ernest::LEVEL_PARAM));
		addLabel(Vec(49.0, 72.5), "BASS");
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(49.0, 80.0)), module, Ernest::LOW_BOOST_PARAM));

		// Entrées et sorties : deux rangées de cinq colonnes, CV de hauteur et de modulation en haut
		addLabel(Vec(7.0, 91.0), "TRIG", false, 7.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.0, 98.0)), module, Ernest::TRIG_INPUT));
		addChild(createLightCentered<SmallLight<VfdLight>>(mm2px(Vec(11.8, 93.0)), module, Ernest::TRIG_LIGHT));
		addLabel(Vec(18.5, 91.0), "V/OCT", false, 7.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.5, 98.0)), module, Ernest::VOCT_INPUT));
		addLabel(Vec(30.48, 91.0), "TYPE", false, 7.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30.48, 98.0)), module, Ernest::TYPE_INPUT));
		addLabel(Vec(42.5, 91.0), "DEPTH", false, 7.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(42.5, 98.0)), module, Ernest::DEPTH_INPUT));
		addLabel(Vec(54.5, 91.0), "RATE", false, 7.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(54.5, 98.0)), module, Ernest::SPEED_INPUT));

		// Le bouton RING entre les deux jacks : DECAY et RING IN sont trop larges pour être voisins
		addLabel(Vec(7.0, 107.0), "DECAY", false, 7.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.0, 114.0)), module, Ernest::DECAY_INPUT));
		addLabel(Vec(18.5, 107.0), "RING", false, 7.f);
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<VfdLight>>>(mm2px(Vec(18.5, 114.0)), module, Ernest::RING_PARAM, Ernest::RING_LIGHT));
		addLabel(Vec(30.48, 107.0), "RING IN", false, 7.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30.48, 114.0)), module, Ernest::RING_INPUT));
		addLabel(Vec(42.5, 107.0), "MOD", true, 7.f);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(42.5, 114.0)), module, Ernest::MOD_OUTPUT));
		addLabel(Vec(54.5, 107.0), "OUT", true, 7.f);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(54.5, 114.0)), module, Ernest::OUT_OUTPUT));
	}
};


Model* modelErnest = createModel<Ernest, ErnestWidget>("Ernest");
