#include "GastonModule.hpp"
#include "GastonLayout.hpp"
#include "GastounetLayout.hpp"

// Gastounet : les mémoires de Gaston, collé à sa gauche. 4 banques de 16 motifs, WRITE et COPY à la Korg, NEXT et
// RANDOM, et un morceau qui enchaîne des lignes « motif + durée en mesures ». Les motifs n'ont pas de longueur :
// LAUNCH quantifie le moment du changement (tout de suite, au temps, à la mesure, toutes les 2 ou 4 mesures, ou quand
// la voie 1 de Gaston reboucle), et RESTART / LEGATO dit si les voies repartent du pas 1 ou gardent leur position.
// Gastounet n'est qu'une surface de commande : les mémoires et le morceau vivent dans Gaston (son moteur et sa
// sauvegarde), qui reçoit les boutons et les prises par message, à chaque échantillon.


struct Gastounet : Module {
	enum ParamId {
		// Un bouton par GnButton, dans le même ordre
		PARAMS_LEN = GN_BUTTONS
	};
	enum InputId {
		PATTERN_INPUT,
		NEXT_INPUT,
		RANDOM_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		END_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	GastonFeedback fbMsg[2];

	Gastounet() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int k = 0; k < BANK_SIZE; k++)
			configButton(GN_SLOT0 + k, string::f("Pattern slot %02d", k + 1));
		for (int b = 0; b < BANKS; b++)
			configButton(GN_BANK0 + b, string::f("Bank %c", 'A' + b));
		configButton(GN_WRITE, "Write (then pick the slot to save into)");
		configButton(GN_COPY, "Copy (then pick the source, then the destination)");
		configButton(GN_NEXT, "Next pattern in the bank");
		configButton(GN_RANDOM, "Random pattern from the bank");
		configButton(GN_PATTERN, "Pattern mode");
		configButton(GN_SONG, "Song mode");
		configButton(GN_SONGREC, "Song record (each pattern slot adds a row)");
		configButton(GN_END, "Song end: loop or stop");
		for (int q = 0; q < QUANTS; q++)
			configButton(GN_QUANT0 + q, string::f("Launch: %s", QUANT_NAMES[q]));
		configButton(GN_RESTART, "Restart: every track starts from step 1 on a change");
		configButton(GN_LEGATO, "Legato: tracks keep their position on a change");
		configButton(GN_ROW_ADD, "Add a song row after the selected one");
		configButton(GN_ROW_DUP, "Duplicate the selected song row");
		configButton(GN_ROW_DEL, "Delete the selected song row");
		configInput(PATTERN_INPUT, "Pattern select (0 to 10 V across the 16 slots of the bank)");
		configInput(NEXT_INPUT, "Next pattern");
		configInput(RANDOM_INPUT, "Random pattern");
		configInput(RESET_INPUT, "Song reset (back to the first row)");
		configOutput(END_OUTPUT, "Pattern change (and song end)");
		rightExpander.producerMessage = &fbMsg[0];
		rightExpander.consumerMessage = &fbMsg[1];
	}

	// Gaston, s'il est collé à droite
	Gaston* gaston() {
		Module* r = rightExpander.module;
		return r && r->model == modelGaston ? (Gaston*) r : NULL;
	}

	void process(const ProcessArgs& args) override {
		Gaston* g = gaston();
		if (!g) {
			outputs[END_OUTPUT].setVoltage(0.f);
			return;
		}
		GastounetControls* c = (GastounetControls*) g->leftExpander.producerMessage;
		if (c) {
			for (int b = 0; b < GN_BUTTONS; b++)
				c->buttons[b] = params[b].getValue();
			c->cvConnected = inputs[PATTERN_INPUT].isConnected();
			c->cv = inputs[PATTERN_INPUT].getVoltage();
			c->next = inputs[NEXT_INPUT].getVoltage();
			c->random = inputs[RANDOM_INPUT].getVoltage();
			c->reset = inputs[RESET_INPUT].getVoltage();
			c->valid = true;
			g->leftExpander.requestMessageFlip();
		}
		GastonFeedback* fb = (GastonFeedback*) rightExpander.consumerMessage;
		outputs[END_OUTPUT].setVoltage(fb ? fb->end : 0.f);
	}
};


static Gaston* gnGaston(Gastounet* m) { return m ? m->gaston() : NULL; }

static bool blinkFast() { return std::fmod(system::getTime(), 0.4) < 0.2; }
static bool blinkSlow() { return std::fmod(system::getTime(), 1.0) < 0.65; }


// --- Les boutons : modes, banques, mémoires, actions, lancement

struct GnSwitch : app::Switch {
	enum Style { MODE, BANK, SLOT, ACTION, QUANT, CHAIN, ROWEDIT };
	Gastounet* gmodule = NULL;
	int style = SLOT;
	int index = 0;

	GnSwitch() { momentary = true; }

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			paint(args.vg);
		Switch::drawLayer(args, layer);
	}

	void paint(NVGcontext* vg) {
		Vec s = box.size, c = s.div(2.f);
		Gaston* g = gnGaston(gmodule);
		const Memory* mem = g ? &g->core.mem : NULL;
		bool pressed = false;
		if (ParamQuantity* pq = getParamQuantity())
			pressed = pq->getValue() > 0.5f;

		if (style == MODE) {
			bool lit = false;
			NVGcolor acc = G_BRASS;
			std::string label = index == 0 ? "PATTERN" : index == 1 ? "SONG" : index == 2 ? "SONG REC" : "END: LOOP";
			if (mem) {
				if (index == 0) lit = !mem->songMode;
				if (index == 1) lit = mem->songMode;
				if (index == 2) { lit = mem->songRec; acc = G_REC; }
				if (index == 3) label = mem->songLoop ? "END: LOOP" : "END: STOP";
			}
			else {
				lit = index == 0;
			}
			gRoundRect(vg, 0.5f, 0.5f, s.x - 1.f, s.y - 1.f, s.y / 2.f);
			gFill(vg, lit ? acc : (pressed ? nvgTransRGBA(G_BRASS, 50) : nvgRGBA(0, 0, 0, 0)));
			gStroke(vg, lit ? acc : G_BRASS_EDGE, 0.9f);
			gText(vg, c.x, c.y, label, 6.f, lit ? G_NAVY : G_BRASS_LIGHT, NVG_ALIGN_CENTER, 0.4f);
			return;
		}

		if (style == ROWEDIT) {
			static const char* const NAMES[3] = {"+ ROW", "DUP", "− ROW"};
			gRoundRect(vg, 0.5f, 0.5f, s.x - 1.f, s.y - 1.f, s.y / 2.f);
			gFill(vg, pressed ? G_BRASS : nvgRGBA(0, 0, 0, 0));
			gStroke(vg, G_BRASS_EDGE, 0.9f);
			gText(vg, c.x, c.y, NAMES[index], 5.f, pressed ? G_NAVY : G_BRASS_LIGHT, NVG_ALIGN_CENTER, 0.3f);
			return;
		}

		if (style == BANK) {
			bool lit = mem ? mem->bank == index : index == 0;
			gRoundRect(vg, 0.5f, 0.5f, s.x - 1.f, s.y - 1.f, 2.f);
			gFill(vg, lit ? G_BRASS : nvgRGB(0x10, 0x18, 0x29));
			gStroke(vg, lit ? G_BRASS_LIGHT : G_BRASS_DIM, 1.f);
			gText(vg, c.x, c.y, std::string(1, (char) ('A' + index)), 8.4f, lit ? G_NAVY : G_BRASS_LIGHT);
			return;
		}

		if (style == SLOT) {
			int bank = mem ? mem->bank : 0;
			int slot = bank * BANK_SIZE + index;
			bool filled = mem ? mem->slots[slot].filled : index < 6;
			bool active = mem ? mem->active == slot : index == 2;
			bool queued = mem && mem->queued == slot;
			bool edited = active && g && g->core.edited();
			NVGcolor fill = G_CELL, border = filled ? G_BRASS_EDGE : G_CELL_EDGE;
			NVGcolor ink = filled ? G_BRASS_LIGHT : nvgRGB(0x5d, 0x55, 0x44);
			if (active) {
				bool on = !edited || blinkSlow();
				fill = on ? G_BRASS : nvgTransRGBA(G_BRASS, 90);
				border = G_BRASS_LIGHT;
				ink = on ? G_NAVY : G_BRASS_LIGHT;
			}
			if (mem && mem->armed == ARM_WRITE)
				border = G_REC;
			if (mem && mem->armed == ARM_COPY && mem->copySrc == slot) {
				fill = G_IVORY;
				ink = G_NAVY;
			}
			if (queued && blinkFast())
				gGlow(vg, 0.f, 0.f, s.x, s.y, 2.f, 6.f, nvgTransRGBA(G_GLOW, 150));
			if (pressed)
				fill = nvgLerpRGBA(fill, G_GLOW, 0.4f);
			gRoundRect(vg, 0.5f, 0.5f, s.x - 1.f, s.y - 1.f, 2.f);
			gFill(vg, fill);
			gStroke(vg, queued && blinkFast() ? G_GLOW : border, queued ? 1.4f : 0.9f);
			gText(vg, c.x, c.y - mm2px(0.5f), string::f("%02d", index + 1), 7.2f, ink);
			if (filled) {
				nvgBeginPath(vg);
				nvgCircle(vg, c.x, s.y - mm2px(1.3f), 1.3f);
				gFill(vg, active ? ink : G_BRASS);
			}
			return;
		}

		if (style == ACTION) {
			bool lit = false;
			NVGcolor acc = G_BRASS;
			if (index == 0) { lit = mem && mem->armed == ARM_WRITE; acc = G_REC; }
			if (index == 1) { lit = mem && mem->armed == ARM_COPY; acc = G_IVORY; }
			lit = lit || pressed;
			float r = c.x - 1.f;
			nvgBeginPath(vg);
			nvgCircle(vg, c.x, c.y, r);
			gFill(vg, lit ? acc : nvgRGBA(0, 0, 0, 0));
			gStroke(vg, lit ? acc : G_BRASS, 1.4f);
			NVGcolor ink = lit ? G_NAVY : G_BRASS_LIGHT;
			float u = r * 0.42f;
			nvgLineCap(vg, NVG_ROUND);
			nvgLineJoin(vg, NVG_ROUND);
			if (index == 0) {
				// WRITE : une flèche qui descend dans un plateau
				nvgBeginPath(vg);
				nvgMoveTo(vg, c.x, c.y - u);
				nvgLineTo(vg, c.x, c.y + u * 0.4f);
				nvgMoveTo(vg, c.x - u * 0.5f, c.y - u * 0.1f);
				nvgLineTo(vg, c.x, c.y + u * 0.4f);
				nvgLineTo(vg, c.x + u * 0.5f, c.y - u * 0.1f);
				nvgMoveTo(vg, c.x - u, c.y + u * 0.3f);
				nvgLineTo(vg, c.x - u, c.y + u);
				nvgLineTo(vg, c.x + u, c.y + u);
				nvgLineTo(vg, c.x + u, c.y + u * 0.3f);
				gStroke(vg, ink, 1.4f);
			}
			else if (index == 1) {
				// COPY : deux feuilles
				gRoundRect(vg, c.x - u, c.y - u, u * 1.3f, u * 1.3f, 1.f);
				gStroke(vg, ink, 1.3f);
				gRoundRect(vg, c.x - u * 0.3f, c.y - u * 0.3f, u * 1.3f, u * 1.3f, 1.f);
				gFill(vg, lit ? acc : G_NAVY);
				gRoundRect(vg, c.x - u * 0.3f, c.y - u * 0.3f, u * 1.3f, u * 1.3f, 1.f);
				gStroke(vg, ink, 1.3f);
			}
			else if (index == 2) {
				// NEXT : ▶|
				nvgBeginPath(vg);
				nvgMoveTo(vg, c.x - u * 0.8f, c.y - u * 0.8f);
				nvgLineTo(vg, c.x + u * 0.3f, c.y);
				nvgLineTo(vg, c.x - u * 0.8f, c.y + u * 0.8f);
				nvgClosePath(vg);
				nvgMoveTo(vg, c.x + u * 0.8f, c.y - u * 0.8f);
				nvgLineTo(vg, c.x + u * 0.8f, c.y + u * 0.8f);
				gStroke(vg, ink, 1.4f);
			}
			else {
				// RANDOM : la face d'un dé
				static const float D[5][2] = {{-0.7f, -0.7f}, {0.7f, -0.7f}, {0.f, 0.f}, {-0.7f, 0.7f}, {0.7f, 0.7f}};
				for (int k = 0; k < 5; k++) {
					nvgBeginPath(vg);
					nvgCircle(vg, c.x + D[k][0] * u, c.y + D[k][1] * u, 1.5f);
					gFill(vg, ink);
				}
			}
			return;
		}

		// Cases de LAUNCH et RESTART / LEGATO
		bool lit;
		std::string label;
		if (style == QUANT) {
			lit = mem ? mem->quant == index : index == Q_BAR;
			label = QUANT_NAMES[index];
		}
		else {
			lit = mem ? (mem->legato == (index == 1)) : index == 0;
			label = index == 0 ? "RESTART" : "LEGATO";
		}
		nvgBeginPath(vg);
		nvgRect(vg, 0.f, 0.f, s.x, s.y);
		gFill(vg, lit ? G_BRASS : (pressed ? nvgTransRGBA(G_BRASS, 50) : nvgRGBA(0, 0, 0, 0)));
		gStroke(vg, G_BRASS_DIM, 0.8f);
		gText(vg, c.x, c.y, label, 5.f, lit ? G_NAVY : G_BRASS_LIGHT);
	}
};


// --- L'écran : le motif, la mesure, ce qui attend, et le morceau qu'on édite à la souris

struct GnScreen : OpaqueWidget {
	Gastounet* module = NULL;
	int dragRow = -1;
	bool dragBars = false;
	float dragAcc = 0.f;

	static float X(float mm) { return mm2px(mm); }
	float lx(float mm) { return mm2px(mm - gnl::SCREEN[0]); }
	float ly(float mm) { return mm2px(mm - gnl::SCREEN[1]); }

	// La première ligne visible : la sélection reste dans la fenêtre
	int firstRow(const Memory& m) {
		return clamp(m.songSel - 1, 0, std::max(0, m.songLen - gnl::ROWS));
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			paint(args.vg);
		OpaqueWidget::drawLayer(args, layer);
	}

	void paint(NVGcontext* vg) {
		Gaston* g = gnGaston(module);
		if (module && !g) {
			gText(vg, lx(50.8f), ly(24.0f), "ATTACH TO THE LEFT", 7.f, G_LABEL, NVG_ALIGN_CENTER, 0.6f);
			gText(vg, lx(50.8f), ly(30.0f), "OF A GASTON", 7.f, G_LABEL, NVG_ALIGN_CENTER, 0.6f);
			return;
		}
		// Dans le navigateur de modules : un petit morceau d'exemple
		static Memory demo;
		static bool demoReady = false;
		if (!demoReady) {
			demo.active = 2;
			static const int PAT[4] = {0, 1, 0, 16}, BARS[4] = {8, 4, 8, 2};
			for (int r = 0; r < 4; r++) {
				demo.song[r] = SongRow(PAT[r], BARS[r]);
				demo.slots[PAT[r]].filled = true;
			}
			demo.songLen = 4;
			demoReady = true;
		}
		const Memory& m = g ? g->core.mem : demo;
		bool songMode = m.songMode;

		// En-tête : le mode, le motif chargé, la mesure
		float hy = ly(16.8f);
		gText(vg, lx(6.6f), hy, songMode ? "SONG" : "PATTERN", 6.2f, G_LABEL_DIM, NVG_ALIGN_LEFT, 0.5f);
		gText(vg, lx(23.f), ly(16.6f), slotName(m.active), 13.f, G_BRASS_LIGHT, NVG_ALIGN_LEFT, 0.4f);
		if (g && g->core.edited())
			gText(vg, lx(37.6f), hy, "EDITED", 5.6f, nvgRGB(0xe0, 0x80, 0x5f), NVG_ALIGN_LEFT, 0.4f);
		double master = g ? g->core.master : 0.0;
		bool running = g && (g->core.playing || g->core.pos > 0.0);
		long step = running ? (long) std::floor(master + 1e-9) : 0;
		gText(vg, lx(76.f), hy, string::f("BAR %ld · BEAT %ld", step / BAR_STEPS + 1, (step % BAR_STEPS) / 4 + 1), 5.f, G_LABEL_DIM, NVG_ALIGN_RIGHT, 0.3f);

		// Ligne d'état
		std::string hint;
		NVGcolor hintColor = G_LABEL_DIM;
		if (m.armed == ARM_WRITE) {
			hint = "WRITE: PICK A SLOT";
			hintColor = nvgRGB(0xe0, 0x80, 0x5f);
		}
		else if (m.armed == ARM_COPY) {
			hint = m.copySrc < 0 ? "COPY: PICK THE SOURCE" : "COPY " + slotName(m.copySrc) + ": PICK THE DESTINATION";
			hintColor = G_IVORY;
		}
		else if (songMode && m.songRec) {
			hint = "SONG REC: EACH SLOT ADDS A ROW";
			hintColor = nvgRGB(0xe0, 0x80, 0x5f);
		}
		else if (songMode) {
			if (m.songLen == 0)
				hint = "EMPTY SONG: USE SONG REC OR + ROW";
			else if (m.songEnded)
				hint = "SONG ENDED";
			else if (m.songPending)
				hint = "SONG STARTS ON THE NEXT BAR";
			else
				hint = string::f("ROW %d · BAR %d/%d", m.songRow + 1, m.rowBar + 1, m.song[clamp(m.songRow, 0, SONG_ROWS - 1)].bars);
		}
		else if (m.queued >= 0) {
			hint = "NEXT: " + slotName(m.queued);
			int q = QUANT_STEPS[m.quant];
			if (m.quant == Q_TRACK1)
				hint += " WHEN TRACK 1 LOOPS";
			else if (q > 1 && running) {
				long left = q - (step % q);
				hint += left >= 4 ? string::f(" IN %ld BEAT%s", (left + 3) / 4, (left + 3) / 4 > 1 ? "S" : "") : string::f(" IN %ld STEP%s", left, left > 1 ? "S" : "");
			}
			hintColor = G_BRASS_LIGHT;
		}
		else {
			hint = std::string("LAUNCH: ") + QUANT_NAMES[m.quant] + (m.legato ? " · LEGATO" : " · RESTART");
		}
		gText(vg, lx(6.6f), ly(21.4f), hint, 5.4f, hintColor, NVG_ALIGN_LEFT, 0.3f);

		// Le morceau
		int sel = clamp(m.songSel, 0, std::max(0, m.songLen - 1));
		int first = firstRow(m);
		float alpha = songMode ? 1.f : 0.55f;
		nvgGlobalAlpha(vg, alpha);
		for (int k = 0; k < gnl::ROWS; k++) {
			int r = first + k;
			if (r >= m.songLen)
				break;
			float y0 = ly(gnl::ROW_Y0 + k * gnl::ROW_PITCH), y = y0 + X(gnl::ROW_PITCH / 2.f), w = lx(gnl::SCREEN[2] - 2.f) - lx(gnl::SCREEN[0] + 2.f);
			bool playing = songMode && r == m.songRow && !m.songEnded;
			if (r == sel) {
				gRoundRect(vg, lx(gnl::SCREEN[0] + 2.f), y0 + X(0.2f), w, X(gnl::ROW_PITCH - 0.4f), 1.f);
				gFill(vg, nvgRGBA(0xec, 0xe3, 0xcf, 18));
			}
			if (playing && running) {
				float prog = clamp((m.rowBar + (step % BAR_STEPS) / (float) BAR_STEPS) / std::max(1, m.song[r].bars), 0.f, 1.f);
				gRoundRect(vg, lx(gnl::SCREEN[0] + 2.f), y0 + X(0.2f), w * prog, X(gnl::ROW_PITCH - 0.4f), 1.f);
				gFill(vg, nvgRGBA(200, 162, 92, 42));
			}
			NVGcolor col = playing ? G_GLOW : (m.slots[m.song[r].pattern].filled ? G_BRASS_LIGHT : G_NUM);
			gText(vg, lx(6.6f), y, string::f("%02d", r + 1), 5.2f, G_NUM, NVG_ALIGN_LEFT);
			gText(vg, lx(14.f), y, slotName(m.song[r].pattern), 6.6f, col, NVG_ALIGN_LEFT, 0.3f);
			gText(vg, lx(27.f), y, string::f("%d BAR%s", m.song[r].bars, m.song[r].bars > 1 ? "S" : ""), 5.6f, col, NVG_ALIGN_LEFT, 0.3f);
			if (playing) {
				float px = lx(gnl::SCREEN[2] - 4.f);
				nvgBeginPath(vg);
				nvgMoveTo(vg, px - 2.f, y - 2.5f);
				nvgLineTo(vg, px + 2.f, y);
				nvgLineTo(vg, px - 2.f, y + 2.5f);
				nvgClosePath(vg);
				gFill(vg, G_GLOW);
			}
		}
		nvgGlobalAlpha(vg, 1.f);
	}

	void onButton(const ButtonEvent& e) override {
		e.consume(this);
		Gaston* g = gnGaston(module);
		if (!g || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		Memory& m = g->core.mem;
		dragRow = -1;
		// Les lignes : choisir, puis glisser sur la mémoire ou la durée
		int first = firstRow(m);
		for (int k = 0; k < gnl::ROWS; k++) {
			float y0 = ly(gnl::ROW_Y0 + k * gnl::ROW_PITCH);
			int r = first + k;
			if (r >= m.songLen || e.pos.y < y0 || e.pos.y >= y0 + X(gnl::ROW_PITCH))
				continue;
			m.songSel = r;
			if (e.pos.x >= lx(13.f) && e.pos.x < lx(26.f)) {
				dragRow = r;
				dragBars = false;
			}
			else if (e.pos.x >= lx(26.f) && e.pos.x < lx(50.f)) {
				dragRow = r;
				dragBars = true;
			}
			dragAcc = 0.f;
			return;
		}
	}

	void onDragMove(const DragMoveEvent& e) override {
		Gaston* g = gnGaston(module);
		if (!g || dragRow < 0 || dragRow >= g->core.mem.songLen)
			return;
		dragAcc += -e.mouseDelta.y / getAbsoluteZoom();
		float stepPx = X(1.6f);
		int d = (int) (dragAcc / stepPx);
		if (d == 0)
			return;
		dragAcc -= d * stepPx;
		SongRow& row = g->core.mem.song[dragRow];
		if (dragBars)
			row.bars = clamp(row.bars + d, 1, 64);
		else
			row.pattern = clamp(row.pattern + d, 0, SLOTS - 1);
	}

	void onDragEnd(const DragEndEvent& e) override {
		dragRow = -1;
	}

	// La molette ajuste aussi la ligne choisie : la durée, ou la mémoire avec Maj
	void onHoverScroll(const HoverScrollEvent& e) override {
		Gaston* g = gnGaston(module);
		if (!g || g->core.mem.songLen == 0)
			return;
		SongRow& row = g->core.mem.song[clamp(g->core.mem.songSel, 0, g->core.mem.songLen - 1)];
		int d = e.scrollDelta.y > 0 ? 1 : -1;
		if ((APP->window->getMods() & RACK_MOD_MASK) == GLFW_MOD_SHIFT)
			row.pattern = clamp(row.pattern + d, 0, SLOTS - 1);
		else
			row.bars = clamp(row.bars + d, 1, 64);
		e.consume(this);
	}
};


// Le titre du bloc des mémoires, avec la banque affichée
struct GnOverlay : TransparentWidget {
	Gastounet* module = NULL;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			Gaston* g = gnGaston(module);
			int bank = g ? g->core.mem.bank : 0;
			gText(args.vg, mm2px(7.f), mm2px(55.f), string::f("PATTERNS · BANK %c", 'A' + bank), 6.4f, G_LABEL_DIM, NVG_ALIGN_LEFT, 0.9f);
		}
		TransparentWidget::drawLayer(args, layer);
	}
};


struct GastounetWidget : ModuleWidget {
	template <class T>
	T* place(T* w, float cxMm, float cyMm, float wMm, float hMm) {
		w->box.size = mm2px(Vec(wMm, hMm));
		w->box.pos = mm2px(Vec(cxMm, cyMm)).minus(w->box.size.div(2.f));
		return w;
	}

	void button(Gastounet* module, int id, int style, int index, float x, float y, float w, float h) {
		GnSwitch* b = createParam<GnSwitch>(Vec(), module, id);
		place(b, x, y, w, h);
		b->gmodule = module;
		b->style = style;
		b->index = index;
		b->initParamQuantity();
		addParam(b);
	}

	GastounetWidget(Gastounet* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Gastounet.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		for (const gl::Label& l : gnl::LABELS) {
			PanelLabel* label = createWidget<PanelLabel>(mm2px(Vec(l.x, l.y)));
			label->text = l.text;
			label->fontSize = l.size * G_FONT_SCALE;
			label->color = l.color;
			label->align = l.align;
			label->letterSpacing = l.spacing;
			label->embolden = 0.35f;
			addChild(label);
		}
		GnOverlay* overlay = createWidget<GnOverlay>(Vec(0, 0));
		overlay->box.size = box.size;
		overlay->module = module;
		addChild(overlay);

		GnScreen* screen = createWidget<GnScreen>(mm2px(Vec(gnl::SCREEN[0], gnl::SCREEN[1])));
		screen->box.size = mm2px(Vec(gnl::SCREEN[2] - gnl::SCREEN[0], gnl::SCREEN[3] - gnl::SCREEN[1]));
		screen->module = module;
		addChild(screen);

		static const int MODES[4] = {GN_PATTERN, GN_SONG, GN_SONGREC, GN_END};
		for (int k = 0; k < 4; k++)
			button(module, MODES[k], GnSwitch::MODE, k, gnl::MODE_X[k], gnl::MODE_Y, gnl::MODE_W, gnl::MODE_H);
		for (int b = 0; b < BANKS; b++)
			button(module, GN_BANK0 + b, GnSwitch::BANK, b, gnl::BANK_X, gnl::ROW_Y[b], gnl::BANK_W, gnl::BANK_H);
		for (int k = 0; k < BANK_SIZE; k++)
			button(module, GN_SLOT0 + k, GnSwitch::SLOT, k, gnl::SLOT_X[k % 4], gnl::ROW_Y[k / 4], gnl::SLOT_W, gnl::SLOT_H);
		static const int ACTIONS[4] = {GN_WRITE, GN_COPY, GN_NEXT, GN_RANDOM};
		for (int k = 0; k < 4; k++)
			button(module, ACTIONS[k], GnSwitch::ACTION, k, gnl::ACT_X[k % 2], gnl::ACT_Y[k / 2], 2 * gnl::ACT_R, 2 * gnl::ACT_R);
		for (int q = 0; q < QUANTS; q++)
			button(module, GN_QUANT0 + q, GnSwitch::QUANT, q, gnl::QUANT_X0 + (q + 0.5f) * gnl::QUANT_W, gnl::SEG_Y, gnl::QUANT_W, gnl::SEG_H);
		button(module, GN_RESTART, GnSwitch::CHAIN, 0, gnl::CHAIN_X0 + 0.5f * gnl::CHAIN_W, gnl::SEG_Y, gnl::CHAIN_W, gnl::SEG_H);
		button(module, GN_LEGATO, GnSwitch::CHAIN, 1, gnl::CHAIN_X0 + 1.5f * gnl::CHAIN_W, gnl::SEG_Y, gnl::CHAIN_W, gnl::SEG_H);
		static const int ROW_EDITS[3] = {GN_ROW_ADD, GN_ROW_DUP, GN_ROW_DEL};
		for (int k = 0; k < 3; k++)
			button(module, ROW_EDITS[k], GnSwitch::ROWEDIT, k, gnl::EDIT_X, gnl::EDIT_Y[k], gnl::EDIT_W, gnl::EDIT_H);

		static const int INS[4] = {Gastounet::PATTERN_INPUT, Gastounet::NEXT_INPUT, Gastounet::RANDOM_INPUT, Gastounet::RESET_INPUT};
		for (int k = 0; k < 4; k++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(gnl::IN_X[k], gnl::JACK_Y)), module, INS[k]));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(gnl::END_X, gnl::JACK_Y)), module, Gastounet::END_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Gastounet* m = getModule<Gastounet>();
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		Gaston* g = m->gaston();
		if (!g) {
			menu->addChild(createMenuLabel("Attach Gastounet to the left of a Gaston"));
			return;
		}
		Memory& mem = g->core.mem;
		menu->addChild(createMenuLabel("Patterns"));
		menu->addChild(createMenuItem(string::f("Clear slot %s", slotName(mem.active).c_str()), "", [=]() { g->core.clearSlot(g->core.mem.active); }));
		menu->addChild(createMenuItem(string::f("Clear bank %c", 'A' + mem.bank), "", [=]() {
			for (int k = 0; k < BANK_SIZE; k++)
				g->core.clearSlot(g->core.mem.bank * BANK_SIZE + k);
		}));
		menu->addChild(createMenuItem("Clear all patterns", "", [=]() {
			for (int k = 0; k < SLOTS; k++)
				g->core.clearSlot(k);
		}));
		menu->addChild(createMenuLabel("Song"));
		menu->addChild(createMenuItem("Clear song", "", [=]() {
			g->core.mem.songLen = 0;
			g->core.mem.songRow = 0;
			g->core.mem.rowBar = 0;
		}));
	}
};


Model* modelGastounet = createModel<Gastounet, GastounetWidget>("Gastounet");
