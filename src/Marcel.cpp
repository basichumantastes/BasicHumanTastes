#include "plugin.hpp"
#include "widgets.hpp"

// Marcel : boucleur / délai 8 bits à horloge variable (comme Proust, il se perd dans le temps retrouvé).
// Une mémoire de 16 384 cases de 8 bits, lue et écrite au rythme d'une horloge variable
// (SPEED + PITCH CV) : ralentir l'horloge allonge la boucle, baisse la hauteur et salit le son.
// Entrée et réinjection (FEEDBACK) ont chacune deux VCA, OPEN et CLOSE. REVERSE, RESET au début de la
// boucle, FREEZE pour figer la boucle, et une sortie CLOCK carrée à la fréquence de l'horloge.
// La signature de Marcel : ERODE, la mémoire qui oublie un peu plus à chaque tour, et TONE,
// un filtre dans la boucle. L'anneau en haut du panneau montre la mémoire et la tête de lecture.


static const int MEMORY_SIZE = 16384;


struct Marcel : Module {
	enum ParamId {
		SPEED_PARAM,
		PITCH_ATTEN_PARAM,
		INPUT_PARAM,
		FEEDBACK_PARAM,
		ERODE_PARAM,
		TONE_PARAM,
		FREEZE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AUDIO_INPUT,
		PITCH_INPUT,
		INPUT_OPEN_INPUT,
		INPUT_CLOSE_INPUT,
		FEEDBACK_OPEN_INPUT,
		FEEDBACK_CLOSE_INPUT,
		REVERSE_INPUT,
		RESET_INPUT,
		FREEZE_INPUT,
		ERODE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT_OUTPUT,
		CLOCK_OUTPUT,
		EOC_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		FREEZE_LIGHT,
		REVERSE_LIGHT,
		EOC_LIGHT,
		LIGHTS_LEN
	};

	int8_t memory[MEMORY_SIZE] = {};
	int position = 0;
	float clockPhase = 0.f;
	float held = 0.f;
	int squareCounter = 0;
	bool squareHigh = false;
	bool looping = false;
	// État de la boucle : filtre de TONE et trou d'érosion en cours
	float toneLowPass = 0.f;
	int dropoutLength = 0;
	int dropoutIndex = 0;
	float crackle = 0.f;

	dsp::BooleanTrigger freezeButton;
	dsp::SchmittTrigger freezeTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger reverseGate;
	dsp::PulseGenerator eocPulse;
	dsp::PulseGenerator eocLightPulse;

	Marcel() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		// Horloge de 1 kHz à 48 kHz : boucle de 16 s (sale et grave) à 0,34 s (propre et aiguë)
		configParam(SPEED_PARAM, 0.f, 1.f, 0.6f, "Speed (memory clock: slower = longer, lower, grittier loop)", " kHz", 48.f, 1.f);
		configParam(PITCH_ATTEN_PARAM, -1.f, 1.f, 0.f, "Pitch CV amount", "%", 0.f, 100.f);
		configParam(INPUT_PARAM, 0.f, 1.f, 0.8f, "Input level", "%", 0.f, 100.f);
		// Un peu au-delà de 100 % : la réinjection peut s'emballer et saturer en 8 bits
		configParam(FEEDBACK_PARAM, 0.f, 1.1f, 0.5f, "Feedback", "%", 0.f, 100.f);
		configParam(ERODE_PARAM, 0.f, 1.f, 0.f, "Erode (the memory forgets a little more on every pass)", "%", 0.f, 100.f);
		configParam(TONE_PARAM, -1.f, 1.f, 0.f, "Tone (left: each pass darker, right: each pass thinner)", "%", 0.f, 100.f);
		configButton(FREEZE_PARAM, "Freeze (hold the loop / release it)");
		configInput(AUDIO_INPUT, "Audio in");
		configInput(PITCH_INPUT, "Pitch CV (1V/oct with PITCH fully right)");
		configInput(INPUT_OPEN_INPUT, "Input level CV: 0-10 V opens");
		configInput(INPUT_CLOSE_INPUT, "Input level CV: 0-10 V closes");
		configInput(FEEDBACK_OPEN_INPUT, "Feedback CV: 0-10 V opens");
		configInput(FEEDBACK_CLOSE_INPUT, "Feedback CV: 0-10 V closes");
		configInput(REVERSE_INPUT, "Reverse (plays backwards while the gate is high)");
		configInput(RESET_INPUT, "Reset (trigger: jump back to the start of the loop)");
		configInput(FREEZE_INPUT, "Freeze toggle (trigger)");
		configInput(ERODE_INPUT, "Erode CV (added to the knob, 10 V = 100 %)");
		configOutput(OUT_OUTPUT, "Audio out");
		configOutput(CLOCK_OUTPUT, "Clock (square wave at the memory clock / 32)");
		configOutput(EOC_OUTPUT, "End of loop (10 V trigger on every pass)");
	}

	void onReset() override {
		std::memset(memory, 0, sizeof(memory));
		position = 0;
		clockPhase = held = toneLowPass = 0.f;
		dropoutLength = dropoutIndex = 0;
		crackle = 0.f;
		looping = false;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "looping", json_boolean(looping));
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* loopingJ = json_object_get(root, "looping");
		if (loopingJ)
			looping = json_boolean_value(loopingJ);
	}

	static float openGain(Input& input) {
		return input.isConnected() ? clamp(input.getVoltage() / 10.f, 0.f, 1.f) : 1.f;
	}

	static float closeGain(Input& input) {
		return input.isConnected() ? clamp(1.f - input.getVoltage() / 10.f, 0.f, 1.f) : 1.f;
	}

	// Ce qui arrive au son à chaque fois qu'il refait un tour de mémoire.
	// TONE : un filtre passe-bas d'un pôle, calculé au rythme de l'horloge (sa coupure suit donc
	// SPEED). À gauche on garde le grave, à droite on garde ce que le filtre enlève : le son
	// s'assombrit ou s'amincit un peu plus à chaque tour.
	// ERODE : la mémoire oublie. À chaque tour, le son perd un peu de volume puis il est tronqué
	// vers zéro sur moins de bits (de 8 à 4 en butée) : les passages doux s'éteignent en premier
	// et le reste se crispe. Des tronçons de la boucle s'effacent aussi, avec des bords fondus
	// pour ne pas claquer, et de petits craquements discrets s'y glissent. En butée, une boucle figée a presque disparu en 4 tours ;
	// vers 30 %, il en faut une douzaine pour qu'elle soit bien entamée.
	float recirculate(float value, float erode, float tone) {
		toneLowPass += 0.25f * (value - toneLowPass);
		if (tone < 0.f)
			value = crossfade(value, toneLowPass, -tone);
		else if (tone > 0.f)
			value = crossfade(value, value - toneLowPass, tone);

		if (erode <= 0.f)
			return value;

		value *= 1.f - 0.25f * erode * erode;
		float levels = std::pow(2.f, 7.f - 4.f * erode);
		value = std::trunc(value * levels) / levels;

		// Trous de 48 à 288 cases, jusqu'à ~25 par tour en butée, fondus sur un quart de leur
		// longueur (32 cases au plus) à l'entrée comme à la sortie
		if (dropoutLength == 0 && random::uniform() < 0.0015f * erode * std::sqrt(erode)) {
			dropoutLength = 48 + (int) (random::uniform() * 240.f);
			dropoutIndex = 0;
		}
		if (dropoutLength > 0) {
			int fade = std::min(32, dropoutLength / 4);
			int fromEdge = std::min(dropoutIndex, dropoutLength - 1 - dropoutIndex);
			float depth = std::min(1.f, (float) fromEdge / fade);
			value *= 1.f - depth;
			if (++dropoutIndex >= dropoutLength)
				dropoutLength = 0;
		}

		// Craquements façon vieux disque : une petite impulsion de ±0,15 au plus (environ
		// −16 dB), ajoutée au son et qui retombe en quelques cases au lieu d'un pic sec
		if (random::uniform() < 0.0003f * erode * erode)
			crackle = (random::uniform() - 0.5f) * 0.3f * erode;
		value += crackle;
		crackle *= 0.5f;
		return value;
	}

	void process(const ProcessArgs& args) override {
		// FREEZE : bouton ou trigger, chacun bascule entre enregistrement et boucle figée
		if (freezeButton.process(params[FREEZE_PARAM].getValue() > 0.f))
			looping = !looping;
		if (freezeTrigger.process(inputs[FREEZE_INPUT].getVoltage(), 0.1f, 1.f))
			looping = !looping;
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f))
			position = 0;
		reverseGate.process(inputs[REVERSE_INPUT].getVoltage(), 0.1f, 1.f);
		bool reverse = reverseGate.isHigh();

		float in = inputs[AUDIO_INPUT].getVoltage() / 5.f;
		float inputGain = params[INPUT_PARAM].getValue()
			* openGain(inputs[INPUT_OPEN_INPUT]) * closeGain(inputs[INPUT_CLOSE_INPUT]);
		float feedbackGain = params[FEEDBACK_PARAM].getValue()
			* openGain(inputs[FEEDBACK_OPEN_INPUT]) * closeGain(inputs[FEEDBACK_CLOSE_INPUT]);
		float erode = clamp(params[ERODE_PARAM].getValue() + inputs[ERODE_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		float tone = params[TONE_PARAM].getValue();
		// Boucle figée, bouton au centre et rien à éroder : on n'y touche pas du tout
		bool shaping = erode > 0.f || std::fabs(tone) > 0.01f;

		// Horloge : SPEED en exponentiel, PITCH CV en volts par octave dosés par l'atténuverseur
		float pitch = inputs[PITCH_INPUT].getVoltage() * params[PITCH_ATTEN_PARAM].getValue();
		float clockHz = 1000.f * std::pow(48.f, params[SPEED_PARAM].getValue()) * dsp::exp2_taylor5(clamp(pitch, -10.f, 10.f));
		clockHz = clamp(clockHz, 20.f, 4.f * args.sampleRate);

		// À chaque coup d'horloge : on lit la case, puis on y réécrit entrée + réinjection.
		// La boucle dure donc exactement MEMORY_SIZE coups d'horloge.
		// Figée par FREEZE, la boucle n'enregistre plus rien, mais ERODE et TONE continuent
		// d'agir : même un souvenir figé finit par s'effacer.
		clockPhase += clockHz * args.sampleTime;
		int ticks = 0;
		while (clockPhase >= 1.f && ticks < 8) {
			clockPhase -= 1.f;
			ticks++;

			held = memory[position] / 127.f;
			float value;
			if (!looping)
				value = in * inputGain + recirculate(held, erode, tone) * feedbackGain;
			else if (shaping)
				value = recirculate(held, erode, tone);
			else
				value = held;
			memory[position] = (int8_t) std::round(clamp(value, -1.f, 1.f) * 127.f);

			// Fin de cycle : la tête fait le tour de la mémoire, dans un sens ou dans l'autre
			position += reverse ? -1 : 1;
			bool endOfCycle = false;
			if (position >= MEMORY_SIZE) {
				position = 0;
				endOfCycle = true;
			}
			else if (position < 0) {
				position = MEMORY_SIZE - 1;
				endOfCycle = true;
			}
			if (endOfCycle) {
				eocPulse.trigger(1e-3f);
				eocLightPulse.trigger(0.1f);
			}

			if (++squareCounter >= 16) {
				squareCounter = 0;
				squareHigh = !squareHigh;
			}
		}
		if (clockPhase >= 1.f)
			clockPhase -= std::floor(clockPhase);

		// Sortie maintenue entre deux coups d'horloge, sans lissage : c'est là que vit le grain
		outputs[OUT_OUTPUT].setVoltage(5.f * held);
		outputs[CLOCK_OUTPUT].setVoltage(squareHigh ? 5.f : -5.f);
		outputs[EOC_OUTPUT].setVoltage(eocPulse.process(args.sampleTime) ? 10.f : 0.f);

		lights[FREEZE_LIGHT].setBrightness(looping);
		lights[REVERSE_LIGHT].setBrightness(reverse);
		lights[EOC_LIGHT].setBrightnessSmooth(eocLightPulse.process(args.sampleTime), args.sampleTime);
	}
};


// L'anneau de mémoire : chaque rayon est l'amplitude d'un morceau de la boucle, le point la tête.
// Doré quand Marcel enregistre, crème quand la boucle est figée par FREEZE.
struct MemoryRing : TransparentWidget {
	Marcel* module = NULL;
	static const int SEGMENTS = 128;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		Vec c = box.size.div(2.f);
		float r0 = box.size.x * 0.26f;
		float span = box.size.x * 0.22f;
		bool looping = module && module->looping;
		NVGcolor ink = looping ? nvgRGB(0xe8, 0xde, 0xc6) : nvgRGB(0xd9, 0xa4, 0x41);

		// Cercle de base
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r0);
		nvgStrokeColor(args.vg, nvgTransRGBA(ink, 90));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStroke(args.vg);

		// Rayons : crête de chaque bloc de la mémoire (motif décoratif dans le navigateur de modules)
		nvgBeginPath(args.vg);
		const int block = MEMORY_SIZE / SEGMENTS;
		for (int k = 0; k < SEGMENTS; k++) {
			float amp;
			if (module) {
				int peak = 0;
				for (int i = k * block; i < (k + 1) * block; i += 4)
					peak = std::max(peak, std::abs((int) module->memory[i]));
				amp = peak / 127.f;
			}
			else {
				amp = 0.5f + 0.4f * std::sin(k * 0.3f) * std::sin(k * 0.07f);
			}
			float a = 2.f * M_PI * k / SEGMENTS - M_PI / 2.f;
			nvgMoveTo(args.vg, c.x + r0 * std::cos(a), c.y + r0 * std::sin(a));
			nvgLineTo(args.vg, c.x + (r0 + span * amp) * std::cos(a), c.y + (r0 + span * amp) * std::sin(a));
		}
		nvgStrokeColor(args.vg, ink);
		nvgStrokeWidth(args.vg, 1.1f);
		nvgLineCap(args.vg, NVG_ROUND);
		nvgStroke(args.vg);

		// Tête de lecture
		float head = module ? (float) module->position / MEMORY_SIZE : 0.f;
		float a = 2.f * M_PI * head - M_PI / 2.f;
		float rh = r0 - 3.5f;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x + rh * std::cos(a), c.y + rh * std::sin(a), 2.2f);
		nvgFillColor(args.vg, nvgRGB(0xff, 0xff, 0xff));
		nvgFill(args.vg);
	}
};


struct MarcelWidget : ModuleWidget {
	void addLabel(Vec posMm, std::string text, float fontSize = 7.f, bool dark = false, int align = NVG_ALIGN_CENTER) {
		PanelLabel* label = createWidget<PanelLabel>(mm2px(posMm));
		label->text = text;
		label->fontSize = fontSize;
		label->align = align;
		label->color = dark ? nvgRGB(0x1d, 0x24, 0x33) : nvgRGB(0xef, 0xe6, 0xd2);
		addChild(label);
	}

	MarcelWidget(Marcel* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Marcel.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addLabel(Vec(30.48, 8.0), "MARCEL", 12.f);

		MemoryRing* ring = createWidget<MemoryRing>(mm2px(Vec(30.48 - 16.f, 13.0)));
		ring->box.size = mm2px(Vec(32.f, 32.f));
		ring->module = module;
		addChild(ring);

		// Fin de cycle, dans le coin en haut à droite, sur une petite plaque crème comme les sorties
		addLabel(Vec(52.6, 33.0), "END", 6.f, true);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(54.0, 39.5)), module, Marcel::EOC_OUTPUT));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(58.0, 33.0)), module, Marcel::EOC_LIGHT));

		// Volumes et vitesse
		addLabel(Vec(10.0, 49.0), "INPUT");
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.0, 56.0)), module, Marcel::INPUT_PARAM));
		addLabel(Vec(30.48, 47.5), "SPEED", 8.f);
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(30.48, 57.0)), module, Marcel::SPEED_PARAM));
		addLabel(Vec(51.0, 49.0), "FEEDBACK", 6.f);
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(51.0, 56.0)), module, Marcel::FEEDBACK_PARAM));

		// ERODE et son CV à gauche, atténuverseur de pitch au centre, TONE à droite
		addLabel(Vec(9.0, 66.5), "ERODE", 6.f);
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(9.0, 73.5)), module, Marcel::ERODE_PARAM));
		addLabel(Vec(19.5, 66.5), "CV", 6.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(19.5, 73.5)), module, Marcel::ERODE_INPUT));
		addLabel(Vec(30.48, 66.5), "PITCH", 6.f);
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(30.48, 73.5)), module, Marcel::PITCH_ATTEN_PARAM));
		addLabel(Vec(51.0, 66.5), "TONE", 6.f);
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(51.0, 73.5)), module, Marcel::TONE_PARAM));
		addLabel(Vec(44.2, 76.5), "-", 6.f);
		addLabel(Vec(57.8, 76.5), "+", 6.f);

		// Transport : reverse, reset, freeze
		addLabel(Vec(9.0, 82.5), "REVERSE", 6.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.0, 89.0)), module, Marcel::REVERSE_INPUT));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(14.2, 85.0)), module, Marcel::REVERSE_LIGHT));
		addLabel(Vec(23.3, 82.5), "RESET", 6.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(23.3, 89.0)), module, Marcel::RESET_INPUT));
		addLabel(Vec(37.6, 82.5), "TRIG", 6.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(37.6, 89.0)), module, Marcel::FREEZE_INPUT));
		addLabel(Vec(52.6, 82.5), "FREEZE", 6.f);
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(52.0, 89.0)), module, Marcel::FREEZE_PARAM, Marcel::FREEZE_LIGHT));

		// VCA : OPEN ouvre, CLOSE ferme ; deux pour l'entrée, deux pour la réinjection
		addLabel(Vec(16.15, 97.0), "INPUT", 5.5f);
		addLabel(Vec(44.8, 97.0), "FEEDBACK", 5.5f);
		addLabel(Vec(9.0, 99.5), "OPEN", 5.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.0, 104.5)), module, Marcel::INPUT_OPEN_INPUT));
		addLabel(Vec(23.3, 99.5), "CLOSE", 5.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(23.3, 104.5)), module, Marcel::INPUT_CLOSE_INPUT));
		addLabel(Vec(37.6, 99.5), "OPEN", 5.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(37.6, 104.5)), module, Marcel::FEEDBACK_OPEN_INPUT));
		addLabel(Vec(52.0, 99.5), "CLOSE", 5.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(52.0, 104.5)), module, Marcel::FEEDBACK_CLOSE_INPUT));

		// Entrées audio / pitch, sorties sur la plaque crème
		addLabel(Vec(9.0, 111.8), "IN", 6.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.0, 117.5)), module, Marcel::AUDIO_INPUT));
		addLabel(Vec(23.3, 111.8), "PITCH", 6.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(23.3, 117.5)), module, Marcel::PITCH_INPUT));
		addLabel(Vec(37.6, 111.8), "CLOCK", 6.f, true);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(37.6, 117.5)), module, Marcel::CLOCK_OUTPUT));
		addLabel(Vec(52.0, 111.8), "OUT", 6.f, true);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(52.0, 117.5)), module, Marcel::OUT_OUTPUT));
	}
};


Model* modelMarcel = createModel<Marcel, MarcelWidget>("Marcel");
