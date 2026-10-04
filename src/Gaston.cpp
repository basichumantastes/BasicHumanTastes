#include "GastonModule.hpp"
#include "GastonLayout.hpp"

// Gaston : le séquenceur maître du patch, pensé pour les Ernest. Huit voies de trigs (probabilité par pas) et quatre
// voies CV, chacune avec sa longueur (jusqu'à 64 pas), son rapport d'horloge et son sens : c'est ce qui fait le
// polyrythme. On édite une voie à la fois sur le ruban de 16 pas ; la carte montre les douze voies sur la page
// affichée, avec un bandeau qui avance et un carré qui s'allume quand un trig part vraiment. Horloge intégrée
// (swing, timing vintage 96 PPQN), transport façon CDJ (PLAY/PAUSE, CUE), pads et enregistrement en direct.
// Cahier des charges : docs/Gaston-spec.md.

// Bord gauche du pas k (0 à 15), en px, relatif à x0
static float stepX(int k) {
	return mm2px(gl::STEP_PITCH * k + gl::GROUP_EXTRA * (k / 4));
}

// --- Les potards : laiton (ou vert-de-gris pour les réglages d'une voie CV), dessinés en NanoVG

struct GastonKnob : app::Knob {
	Gaston* gmodule = NULL;
	bool laneKnob = false;

	GastonKnob() {
		minAngle = -0.75f * M_PI;
		maxAngle = 0.75f * M_PI;
	}

	void draw(const DrawArgs& args) override {
		float v = 0.5f;
		if (ParamQuantity* pq = getParamQuantity())
			v = pq->getScaledValue();
		bool cv = laneKnob && gmodule && gmodule->lane().cv;
		NVGcontext* vg = args.vg;
		Vec c = box.size.div(2.f);
		float r = c.x - 0.6f;
		nvgBeginPath(vg);
		nvgCircle(vg, c.x, c.y + r * 0.14f, r);
		gFill(vg, nvgRGBA(0, 0, 0, 100));
		NVGcolor light = cv ? nvgRGB(0xd6, 0xf2, 0xea) : nvgRGB(0xf0, 0xd9, 0xa4);
		NVGcolor dark = cv ? nvgRGB(0x2f, 0x5f, 0x57) : nvgRGB(0x7d, 0x65, 0x38);
		NVGpaint face = nvgRadialGradient(vg, c.x - r * 0.3f, c.y - r * 0.4f, r * 0.05f, r * 1.25f, light, dark);
		nvgBeginPath(vg);
		nvgCircle(vg, c.x, c.y, r);
		nvgFillPaint(vg, face);
		nvgFill(vg);
		gStroke(vg, cv ? G_VERD_EDGE : G_BRASS_EDGE, 0.8f);
		float a = math::rescale(v, 0.f, 1.f, minAngle, maxAngle);
		Vec d = Vec(std::sin(a), -std::cos(a));
		nvgBeginPath(vg);
		nvgMoveTo(vg, c.x + d.x * r * 0.3f, c.y + d.y * r * 0.3f);
		nvgLineTo(vg, c.x + d.x * r * 0.86f, c.y + d.y * r * 0.86f);
		nvgLineCap(vg, NVG_ROUND);
		gStroke(vg, G_INK, std::max(1.3f, r * 0.13f));
		Knob::draw(args);
	}
};


// --- Les boutons : transport, choix de voie, pads, mutes, sens

struct GastonButton : app::Switch {
	enum Style { TRANSPORT, SELECT, PAD, MUTE, DIR };
	Gaston* gmodule = NULL;
	int style = PAD;
	int index = 0;
	uint32_t seenPad = 0;
	double flashTime = -1.0;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			paint(args.vg);
		Switch::drawLayer(args, layer);
	}

	void paint(NVGcontext* vg) {
		Vec s = box.size, c = s.div(2.f);
		float pressed = 0.f;
		if (ParamQuantity* pq = getParamQuantity())
			pressed = pq->getValue();
		const Core* core = gmodule ? &gmodule->core : NULL;
		double now = system::getTime();

		if (style == TRANSPORT) {
			float r = c.x - 1.f;
			bool lit = false;
			if (index == 1)
				lit = core && core->cueHeld;
			if (index == 2)
				lit = core && core->playing && !core->cueHeld;
			nvgBeginPath(vg);
			nvgCircle(vg, c.x, c.y, r);
			gFill(vg, lit ? G_BRASS : nvgRGBA(0, 0, 0, 0));
			gStroke(vg, G_BRASS, 1.4f);
			NVGcolor ink = lit ? G_NAVY : G_BRASS_LIGHT;
			if (index == 0) {
				bool armed = pressed > 0.5f;
				if (armed)
					gGlow(vg, c.x - 2.6f, c.y - 6.1f, 5.2f, 5.2f, 2.6f, 5.f, nvgTransRGBA(G_REC, 160));
				nvgBeginPath(vg);
				nvgCircle(vg, c.x, c.y - 3.5f, 2.6f);
				gFill(vg, armed ? G_REC : nvgRGB(0x4a, 0x2a, 0x22));
				gText(vg, c.x, c.y + 3.6f, "REC", 4.6f, G_BRASS_LIGHT, NVG_ALIGN_CENTER, 0.3f);
			}
			else if (index == 1) {
				gText(vg, c.x, c.y, "CUE", 7.f, ink, NVG_ALIGN_CENTER, 0.5f);
			}
			else {
				float h = r * 0.34f;
				nvgBeginPath(vg);
				nvgMoveTo(vg, c.x - h * 1.25f, c.y - h);
				nvgLineTo(vg, c.x - h * 0.05f, c.y);
				nvgLineTo(vg, c.x - h * 1.25f, c.y + h);
				nvgClosePath(vg);
				nvgMoveTo(vg, c.x + h * 0.45f, c.y - h);
				nvgLineTo(vg, c.x + h * 0.45f, c.y + h);
				nvgMoveTo(vg, c.x + h * 1.15f, c.y - h);
				nvgLineTo(vg, c.x + h * 1.15f, c.y + h);
				nvgLineJoin(vg, NVG_ROUND);
				nvgLineCap(vg, NVG_ROUND);
				gStroke(vg, ink, 1.5f);
			}
			return;
		}

		if (style == SELECT) {
			bool cv = index >= TRIG_LANES;
			bool sel = gmodule ? gmodule->selected == index : index == 0;
			bool muted = !cv && gmodule && gmodule->params[Gaston::MUTE_PARAMS + index].getValue() > 0.5f;
			std::string name = cv ? std::string(1, (char) ('A' + index - TRIG_LANES)) : string::f("%d", index + 1);
			if (cv)
				gRoundRect(vg, 1.f, 1.f, s.x - 2.f, s.y - 2.f, 2.f);
			else {
				nvgBeginPath(vg);
				nvgCircle(vg, c.x, c.y, c.x - 1.f);
			}
			NVGcolor fill = sel ? (cv ? G_VERD : G_BRASS) : (cv ? nvgRGB(0x0c, 0x1a, 0x1f) : nvgRGB(0x10, 0x18, 0x29));
			NVGcolor ring = sel ? (cv ? G_VERD_GLOW : G_BRASS_LIGHT) : (cv ? G_VERD_DIM : G_BRASS_DIM);
			gFill(vg, fill);
			gStroke(vg, ring, 1.6f);
			NVGcolor ink = sel ? G_NAVY : (cv ? G_VERD_LIGHT : (muted ? nvgRGB(0x5d, 0x55, 0x44) : G_BRASS_LIGHT));
			gText(vg, c.x, c.y, name, 9.f, ink);
			return;
		}

		if (style == PAD) {
			if (core && core->padFires[index] != seenPad) {
				seenPad = core->padFires[index];
				flashTime = now;
			}
			float hot = flashTime >= 0.0 ? clamp(1.f - (float) (now - flashTime) / 0.15f, 0.f, 1.f) : 0.f;
			hot = std::max(hot, pressed > 0.5f ? 1.f : 0.f);
			bool rec = gmodule && gmodule->params[Gaston::REC_PARAM].getValue() > 0.5f;
			NVGcolor lit = rec ? nvgRGB(0xe0, 0x80, 0x5f) : G_BRASS_LIGHT;
			gRoundRect(vg, 0.5f, 0.5f, s.x - 1.f, s.y - 1.f, 2.5f);
			gFill(vg, nvgLerpRGBA(nvgRGB(0x1a, 0x23, 0x38), lit, hot));
			gStroke(vg, nvgLerpRGBA(nvgRGB(0x5a, 0x4c, 0x33), G_GLOW, hot), 0.9f);
			// Petite ombre en bas, pour le relief
			gRoundRect(vg, 2.f, s.y - 2.4f, s.x - 4.f, 1.2f, 0.6f);
			gFill(vg, nvgRGBA(0, 0, 0, (unsigned char) (70 * (1.f - hot))));
			return;
		}

		if (style == MUTE) {
			bool on = pressed > 0.5f;
			gRoundRect(vg, 0.5f, 0.5f, s.x - 1.f, s.y - 1.f, s.y / 2.f);
			gFill(vg, on ? G_IVORY : G_CELL);
			gStroke(vg, on ? G_IVORY : nvgRGB(0x5a, 0x4c, 0x33), 0.9f);
			return;
		}

		if (style == DIR) {
			const Lane* l = gmodule ? &gmodule->lane() : NULL;
			bool cv = l && l->cv;
			NVGcolor col = cv ? G_VERD_LIGHT : G_BRASS_LIGHT;
			gRoundRect(vg, 0.6f, 0.6f, s.x - 1.2f, s.y - 1.2f, 2.f);
			gFill(vg, pressed > 0.5f ? nvgTransRGBA(col, 40) : nvgRGBA(0, 0, 0, 0));
			gStroke(vg, cv ? G_VERD_EDGE : G_BRASS_EDGE, 1.f);
			gDirIcon(vg, c.x, c.y, s.x * 0.5f, l ? l->dir : FORWARD, col);
		}
	}
};


// --- Le ruban : les 16 pas de la voie choisie, sur la page affichée

// Voie CV : bande de lecture en bas de chaque case (tension ou note), sous la zone de la barre (mm)
static const float RUBAN_READ_H = 2.4f;

struct GastonRuban : OpaqueWidget {
	Gaston* module = NULL;
	int dragStep = -1;
	bool dragCv = false;
	uint32_t seenFires = 0;
	double fireTime = -1.0;

	// Colonne sous la souris (-1 dans les interstices)
	int columnAt(float x) {
		for (int k = 0; k < PAGE; k++) {
			float x0 = stepX(k);
			if (x >= x0 && x < x0 + mm2px(gl::STEP_W))
				return k;
		}
		return -1;
	}

	// Coordonnées locales : l'origine du widget est en (STEP_X0, NUM_Y - 1.6) mm
	float yOf(float mm) { return mm2px(mm - (gl::NUM_Y - 1.6f)); }
	// Hauteur de la zone de la barre d'une case CV
	float barH() { return mm2px(gl::STEP_H - RUBAN_READ_H); }

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			paint(args.vg);
		OpaqueWidget::drawLayer(args, layer);
	}

	void paint(NVGcontext* vg) {
		const Core* core = module ? &module->core : NULL;
		int page = module ? module->page : 0;
		const Lane* lp = module ? &module->lane() : NULL;
		static Lane demo;
		if (!lp) {
			demo.on[0] = demo.on[4] = demo.on[8] = demo.on[12] = demo.on[10] = true;
			demo.prob[10] = 0.5f;
			lp = &demo;
		}
		const Lane& l = *lp;
		bool running = core && laneRunning(*core, l);
		int cur = running ? l.pos : -1;
		if (core && core->seqFires[module->selected] != seenFires) {
			seenFires = core->seqFires[module->selected];
			fireTime = system::getTime();
		}
		bool firing = fireTime >= 0.0 && system::getTime() - fireTime < 0.1;
		NVGcolor accent = l.cv ? G_VERD_LIGHT : G_BRASS_LIGHT;
		float w = mm2px(gl::STEP_W);

		for (int k = 0; k < PAGE; k++) {
			int idx = page * PAGE + k;
			float x = stepX(k);
			bool play = idx == cur;
			gText(vg, x + w / 2, yOf(gl::NUM_Y), string::f("%d", idx + 1), 7.f, play ? accent : G_NUM);
			nvgGlobalAlpha(vg, idx >= l.length ? 0.28f : 1.f);
			if (!l.cv) {
				float y = yOf(gl::STEP_Y0), h = mm2px(gl::STEP_H);
				bool on = l.on[idx];
				if (play)
					gGlow(vg, x, y, w, h, 2.f, 6.f, nvgTransRGBA(G_BRASS_LIGHT, 110));
				gRoundRect(vg, x, y, w, h, 2.f);
				gFill(vg, on ? ((play && firing) ? G_GLOW : G_BRASS) : G_CELL);
				gStroke(vg, play ? G_BRASS_LIGHT : (on ? G_BRASS_EDGE : G_CELL_EDGE), play ? 1.4f : 0.9f);
				// La jauge de probabilité
				float gy = yOf(gl::GAUGE_Y0), gh = mm2px(gl::GAUGE_H);
				gRoundRect(vg, x, gy, w, gh, 1.f);
				gFill(vg, G_CELL);
				if (on) {
					gRoundRect(vg, x, gy, w * l.prob[idx], gh, 1.f);
					gFill(vg, nvgRGB(0xa8, 0x87, 0x4c));
					gRoundRect(vg, x, gy, w, gh, 1.f);
					gStroke(vg, G_BRASS_DIM, 0.7f);
				}
				else {
					// Pointillés
					nvgBeginPath(vg);
					for (float dx = 1.f; dx < w - 1.f; dx += 3.f) {
						nvgMoveTo(vg, x + dx, gy + 0.4f);
						nvgLineTo(vg, x + std::min(dx + 1.5f, w - 1.f), gy + 0.4f);
						nvgMoveTo(vg, x + dx, gy + gh - 0.4f);
						nvgLineTo(vg, x + std::min(dx + 1.5f, w - 1.f), gy + gh - 0.4f);
					}
					gStroke(vg, G_CELL_EDGE, 0.7f);
				}
			}
			else {
				bool active = l.on[idx];
				float y = yOf(gl::STEP_Y0), h = mm2px(gl::STEP_H);
				if (play)
					gGlow(vg, x, y, w, h, 2.f, 6.f, nvgTransRGBA(G_VERD, 110));
				gRoundRect(vg, x, y, w, h, 1.5f);
				gFill(vg, nvgRGB(0x08, 0x13, 0x16));
				gStroke(vg, play ? G_VERD_LIGHT : G_VERD_DIM, play ? 1.3f : 0.9f);
				// La rangée d'activation : pleine si le pas pose sa tension, en pointillés s'il garde la précédente
				float gy = yOf(gl::GAUGE_Y0), gh = mm2px(gl::GAUGE_H);
				if (active) {
					gRoundRect(vg, x, gy, w, gh, 1.f);
					gFill(vg, G_VERD);
				}
				else {
					nvgBeginPath(vg);
					for (float dx = 1.f; dx < w - 1.f; dx += 3.f) {
						nvgMoveTo(vg, x + dx, gy + 0.4f);
						nvgLineTo(vg, x + std::min(dx + 1.5f, w - 1.f), gy + 0.4f);
						nvgMoveTo(vg, x + dx, gy + gh - 0.4f);
						nvgLineTo(vg, x + std::min(dx + 1.5f, w - 1.f), gy + gh - 0.4f);
					}
					gStroke(vg, G_VERD_DIM, 0.7f);
				}
				float v = l.value[idx];
				float zh = barH(), zero, top, bh;
				if (l.unipolar) {
					float u = (v + 1.f) / 2.f;
					zero = y + zh - 1.f;
					top = y + zh - u * zh;
					bh = u * zh;
				}
				else {
					zero = y + zh / 2;
					top = v >= 0.f ? zero - v * zh / 2 : zero;
					bh = std::fabs(v) * zh / 2;
				}
				nvgBeginPath(vg);
				nvgRect(vg, x, zero, w, 0.8f);
				gFill(vg, G_VERD_DIM);
				gRoundRect(vg, x + w * 0.26f, top, w * 0.48f, std::max(bh, 1.2f), 0.8f);
				if (active)
					gFill(vg, play ? G_VERD_GLOW : G_VERD);
				else
					gStroke(vg, nvgTransRGBA(G_VERD, 120), 0.8f);
				// La lecture : tension, ou note si la voie est quantifiée
				nvgBeginPath(vg);
				nvgRect(vg, x + 1.5f, y + zh, w - 3.f, 0.6f);
				gFill(vg, nvgTransRGBA(G_VERD_DIM, 140));
				NVGcolor ink = active ? (play ? G_VERD_GLOW : G_VERD_LIGHT) : nvgTransRGBA(G_VERD_LIGHT, 110);
				gText(vg, x + w / 2, y + (zh + h) / 2 + 0.3f, cvText(l, l.volts(idx)), 4.6f, ink);
			}
			nvgGlobalAlpha(vg, 1.f);
		}
	}

	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS)
			return;
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			openMenu();
			e.consume(this);
			return;
		}
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		int k = columnAt(e.pos.x);
		// Les interstices avalent aussi le clic, pour ne pas déplacer le module en éditant
		e.consume(this);
		if (k < 0)
			return;
		Lane& l = module->lane();
		int idx = module->page * PAGE + k;
		if ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
			l.length = idx + 1;
			return;
		}
		if (l.cv && e.pos.y >= yOf(gl::GAUGE_Y0) - mm2px(0.4f)) {
			// La rangée d'activation : le pas pose sa tension, ou garde celle du pas actif précédent
			l.on[idx] = !l.on[idx];
			dragStep = -1;
			return;
		}
		if (l.cv) {
			// Un clic sur la barre pose la tension à la hauteur de la souris, le glisser l'ajuste ;
			// un clic sur la lecture ne la change pas, le glisser l'ajuste sans saut
			float y = e.pos.y - yOf(gl::STEP_Y0);
			if (y < barH())
				l.value[idx] = clamp(1.f - y / barH(), 0.f, 1.f) * 2.f - 1.f;
			dragStep = idx;
			dragCv = true;
			return;
		}
		if (e.pos.y < yOf(gl::GAUGE_Y0) - mm2px(0.4f)) {
			l.on[idx] = !l.on[idx];
			if (l.on[idx])
				l.prob[idx] = 1.f;
			dragStep = -1;
		}
		else if (l.on[idx]) {
			dragStep = idx;
			dragCv = false;
		}
	}

	void onDragMove(const DragMoveEvent& e) override {
		if (!module || dragStep < 0)
			return;
		Lane& l = module->lane();
		float dy = -e.mouseDelta.y / getAbsoluteZoom();
		if (dragCv)
			l.value[dragStep] = clamp(l.value[dragStep] + dy / barH() * 2.f, -1.f, 1.f);
		else
			l.prob[dragStep] = clamp(l.prob[dragStep] + dy / mm2px(14.f), 0.01f, 1.f);
	}

	void onDragEnd(const DragEndEvent& e) override {
		dragStep = -1;
	}

	void openMenu() {
		Gaston* m = module;
		Lane& l = m->lane();
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel(l.cv ? string::f("CV track %c", 'A' + m->selected - TRIG_LANES) : string::f("Track %d", m->selected + 1)));
		menu->addChild(createMenuItem("Clear track", "", [=]() { m->lane().clear(); }));
		menu->addChild(createMenuItem("Randomize", "", [=]() { m->core.randomizeLane(m->selected); }));
		menu->addChild(createMenuItem("Shift one step left", "", [=]() { m->core.shiftLane(m->selected, -1); }));
		menu->addChild(createMenuItem("Shift one step right", "", [=]() { m->core.shiftLane(m->selected, 1); }));
		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuItem("Copy track", "", [=]() { m->copyLane(); }));
		menu->addChild(createMenuItem("Paste track", "", [=]() { m->pasteLane(); }, !m->canPasteLane()));
		menu->addChild(createMenuItem(string::f("Copy page %d", m->page + 1), "", [=]() { m->copyPage(); }));
		menu->addChild(createMenuItem(string::f("Paste to page %d", m->page + 1), "", [=]() { m->pastePage(); }, !m->canPastePage()));
	}
};


// --- La carte : les douze voies sur la page affichée, le bandeau qui avance, les trigs qui partent

struct GastonMap : OpaqueWidget {
	Gaston* module = NULL;
	uint32_t seenFires[LANES] = {};
	double fireTime[LANES];
	int firePos[LANES] = {};

	GastonMap() {
		for (int i = 0; i < LANES; i++)
			fireTime[i] = -1.0;
	}

	float rowY(int r) { return mm2px(gl::MAP_ROW_Y0 - gl::MAP_BOX[1] + r * gl::MAP_ROW_PITCH); }
	float colX(int k) { return mm2px(gl::STEP_X0 - gl::MAP_BOX[0]) + stepX(k); }

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			paint(args.vg);
		OpaqueWidget::drawLayer(args, layer);
	}

	void paint(NVGcontext* vg) {
		const Core* core = module ? &module->core : NULL;
		int page = module ? module->page : 0;
		int selected = module ? module->selected : 0;
		double now = system::getTime();
		float rowH = mm2px(gl::MAP_ROW_H), sq = mm2px(gl::MAP_SQ), w = mm2px(gl::STEP_W);
		float rail = rowH * 0.42f;

		for (int i = 0; i < LANES; i++) {
			float y = rowY(i), cy = y + rowH / 2;
			bool cv = i >= TRIG_LANES;
			if (i == selected) {
				gRoundRect(vg, colX(0) - mm2px(0.8f), y, colX(15) + w - colX(0) + mm2px(1.6f), rowH, 1.f);
				gFill(vg, nvgRGBA(0xec, 0xe3, 0xcf, 16));
			}
			if (!core) {
				for (int k = 0; k < PAGE; k++) {
					float x = colX(k) + w / 2;
					bool on = cv || (k * (i + 3)) % 5 < 2;
					gRoundRect(vg, x - sq / 2, cy - sq / 2, sq, sq, 0.8f);
					gFill(vg, on ? (cv ? nvgRGBA(175, 245, 226, 140) : nvgRGB(246, 206, 122)) : nvgRGBA(236, 227, 207, 18));
				}
				continue;
			}
			const Lane& l = core->lanes[i];
			bool running = laneRunning(*core, l);
			if (core->seqFires[i] != seenFires[i]) {
				seenFires[i] = core->seqFires[i];
				fireTime[i] = now;
				firePos[i] = core->firePos[i];
			}
			int P0 = page * PAGE, P1 = std::min(P0 + PAGE - 1, l.length - 1);

			// Le bandeau : du début du cycle jusqu'au pas courant
			if (running && P1 >= P0 && l.dir != RANDOM) {
				bool right = l.movingRight();
				int from = -1, to = -1;
				if (right && l.pos >= P0) {
					from = P0;
					to = std::min(l.pos, P1);
				}
				if (!right && l.pos <= P1) {
					from = std::max(l.pos, P0);
					to = P1;
				}
				if (from >= 0 && from <= to) {
					float x0 = colX(from - P0), x1 = colX(to - P0) + w;
					gRoundRect(vg, x0, cy - rail / 2, x1 - x0, rail, rail / 2);
					gFill(vg, cv ? nvgRGBA(116, 194, 178, 44) : nvgRGBA(200, 162, 92, 46));
					if (l.pos >= P0 && l.pos <= P1) {
						float ex = right ? x1 - 1.2f : x0;
						nvgBeginPath(vg);
						nvgRect(vg, ex, cy - rail / 2 - 0.6f, 1.2f, rail + 1.2f);
						gFill(vg, cv ? nvgRGBA(160, 235, 215, 240) : nvgRGBA(240, 205, 130, 240));
					}
				}
			}

			bool flash = !cv && fireTime[i] >= 0.0 && now - fireTime[i] < 0.11;
			for (int k = 0; k < PAGE; k++) {
				int idx = P0 + k;
				if (idx >= l.length)
					continue;
				float x = colX(k) + w / 2;
				NVGcolor col;
				if (cv && l.on[idx])
					col = nvgRGBA(175, 245, 226, (unsigned char) (255 * (0.2f + 0.8f * std::fabs(l.value[idx]))));
				else if (cv)
					col = nvgRGBA(236, 227, 207, 18);
				else if (l.on[idx])
					col = nvgRGBA(246, 206, 122, (unsigned char) (255 * (0.4f + 0.6f * l.prob[idx])));
				else
					col = nvgRGBA(236, 227, 207, 18);
				if (flash && firePos[i] == idx) {
					gGlow(vg, x - sq / 2, cy - sq / 2, sq, sq, 1.f, 5.f, nvgRGBA(255, 215, 140, 200));
					col = nvgRGB(255, 255, 255);
				}
				gRoundRect(vg, x - sq / 2, cy - sq / 2, sq, sq, 0.8f);
				gFill(vg, col);
			}
		}
	}

	// Un clic sur une ligne choisit la voie
	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		for (int i = 0; i < LANES; i++) {
			float y = rowY(i);
			if (e.pos.y >= y - 0.5f && e.pos.y < y + mm2px(gl::MAP_ROW_PITCH) - 0.5f) {
				module->selected = i;
				break;
			}
		}
		e.consume(this);
	}
};


// --- Les quatre pages

struct GastonPages : OpaqueWidget {
	Gaston* module = NULL;

	float centerX(int p) { return box.size.x / 2 + mm2px((p - 1.5f) * gl::PAGE_PITCH); }

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			paint(args.vg);
		OpaqueWidget::drawLayer(args, layer);
	}

	void paint(NVGcontext* vg) {
		const Lane* l = module ? &module->lane() : NULL;
		int page = module ? module->page : 0;
		bool cv = l && l->cv;
		int pages = l ? (l->length + PAGE - 1) / PAGE : 1;
		bool running = module && l && laneRunning(module->core, *l);
		int playPage = running ? l->pos / PAGE : -1;
		bool blink = std::fmod(system::getTime(), 0.5) < 0.25;
		NVGcolor acc = cv ? G_VERD : G_BRASS;
		float w = mm2px(gl::PAGE_W), h = mm2px(gl::PAGE_H);
		for (int p = 0; p < PAGES; p++) {
			float x = centerX(p) - w / 2, y = box.size.y / 2 - h / 2;
			bool active = p == page, has = p < pages;
			gRoundRect(vg, x, y, w, h, h / 2);
			gFill(vg, active ? acc : nvgRGBA(0, 0, 0, 0));
			gStroke(vg, has ? acc : nvgRGB(0x4a, 0x42, 0x32), 0.9f);
			NVGcolor ink = active ? G_NAVY : (cv ? G_VERD_LIGHT : G_BRASS_LIGHT);
			gText(vg, x + w / 2, y + h / 2, string::f("%d", p + 1), 7.f, has ? ink : nvgTransRGBA(ink, 100));
			if (p == playPage && !active && blink) {
				nvgBeginPath(vg);
				nvgCircle(vg, x + w - 2.f, y + 0.5f, 1.6f);
				gFill(vg, cv ? G_VERD_GLOW : G_GLOW);
			}
		}
	}

	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		for (int p = 0; p < PAGES; p++) {
			if (std::fabs(e.pos.x - centerX(p)) < mm2px(gl::PAGE_W) / 2 + 1.f) {
				module->page = p;
				break;
			}
		}
		e.consume(this);
	}
};


// --- Plage et quantification d'une voie CV : deux commutateurs à deux cases

struct GastonSegments : OpaqueWidget {
	Gaston* module = NULL;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			paint(args.vg);
		OpaqueWidget::drawLayer(args, layer);
	}

	void paint(NVGcontext* vg) {
		static const char* const NAMES[2][2] = {{"±5 V", "0–10 V"}, {"FREE", "SEMI"}};
		const Lane* l = module ? &module->lane() : NULL;
		float w = mm2px(gl::SEG_W), h = mm2px(gl::SEG_H);
		for (int row = 0; row < 2; row++) {
			float y = mm2px(gl::SEG_Y[row] - gl::SEG_Y[0]);
			int active = !l ? 0 : (row == 0 ? (l->unipolar ? 1 : 0) : (l->quantize ? 1 : 0));
			gRoundRect(vg, 0.f, y, 2 * w, h, 1.2f);
			gStroke(vg, G_VERD_DIM, 0.9f);
			for (int s = 0; s < 2; s++) {
				if (s == active) {
					gRoundRect(vg, s * w, y, w, h, 1.2f);
					gFill(vg, G_VERD);
				}
				gText(vg, s * w + w / 2, y + h / 2, NAMES[row][s], 5.2f, s == active ? G_NAVY : G_VERD_LIGHT);
			}
		}
	}

	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		Lane& l = module->lane();
		int s = e.pos.x < mm2px(gl::SEG_W) ? 0 : 1;
		int row = e.pos.y < mm2px(gl::SEG_Y[1] - gl::SEG_Y[0]) - 0.5f ? 0 : 1;
		if (row == 0)
			l.unipolar = s == 1;
		else
			l.quantize = s == 1;
		e.consume(this);
	}
};


// --- Tout ce qui s'affiche sans se toucher : écrans, lectures des réglages, diodes des voies, valeurs des CV

struct GastonOverlay : TransparentWidget {
	Gaston* module = NULL;
	uint32_t seenSeq[LANES] = {}, seenPad[TRIG_LANES] = {}, seenStep[LANES] = {};
	double ledTime[LANES];

	GastonOverlay() {
		for (int i = 0; i < LANES; i++)
			ledTime[i] = -1.0;
	}

	static float X(float mm) { return mm2px(mm); }

	void draw(const DrawArgs& args) override {
		// Sur une voie CV, le bloc PAS prend la patine : cadre et filet vert-de-gris
		if (module && module->lane().cv) {
			NVGcontext* vg = args.vg;
			const float* b = gl::SEQ_BOX;
			gRoundRect(vg, X(b[0]), X(b[1]), X(b[2] - b[0]), X(b[3] - b[1]), X(1.f));
			gFill(vg, nvgRGBA(6, 20, 22, 70));
			gStroke(vg, nvgRGBA(116, 194, 178, 110), X(0.22f));
			nvgBeginPath(vg);
			nvgRect(vg, X(b[0] + 2.4f), X(gl::RULE_Y) - 0.3f, X(b[2] - b[0] - 4.8f), 0.6f);
			gFill(vg, nvgRGBA(116, 194, 178, 60));
		}
		TransparentWidget::draw(args);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			paint(args.vg);
		TransparentWidget::drawLayer(args, layer);
	}

	void paint(NVGcontext* vg) {
		Gaston* m = module;
		const Lane* lp = m ? &m->lane() : NULL;
		bool cv = lp && lp->cv;
		NVGcolor acc = cv ? G_VERD_LIGHT : G_BRASS_LIGHT;
		double now = system::getTime();

		// Témoin de Gastounet, collé à gauche : le motif chargé (et s'il a été modifié depuis sa mémoire)
		if (m && m->gnConnected) {
			const Memory& mem = m->core.mem;
			float x = X(47.f), y = X(7.0f);
			nvgBeginPath(vg);
			nvgMoveTo(vg, x + X(1.4f), y - X(1.1f));
			nvgLineTo(vg, x, y);
			nvgLineTo(vg, x + X(1.4f), y + X(1.1f));
			nvgClosePath(vg);
			gFill(vg, G_BRASS);
			std::string t = std::string(mem.songMode ? "SONG " : "PATTERN ") + slotName(mem.active);
			if (m->core.edited())
				t += " · EDITED";
			gText(vg, x + X(2.6f), y, t, 5.2f, G_BRASS_LIGHT, NVG_ALIGN_LEFT, 0.4f);
		}

		// Écran d'horloge
		const float* bs = gl::BPM_SCREEN;
		float bpm = m ? m->core.displayBpm(m->st) : 120.f;
		bool ext = m && m->st.extClock;
		float by = X((bs[1] + bs[3]) / 2);
		gText(vg, X(bs[0] + 2.f), by, string::f("%.1f", bpm), 13.f, G_BRASS_LIGHT, NVG_ALIGN_LEFT);
		gText(vg, X(bs[2] - 1.8f), by + X(0.5f), ext ? "EXT" : "BPM", 5.6f, ext ? G_BRASS_LIGHT : G_LABEL_DIM, NVG_ALIGN_RIGHT, 0.4f);
		float swing = m ? m->params[Gaston::SWING_PARAM].getValue() : 50.f;
		if (m && m->st.vintage)
			swing = std::round(swing / 100.f * 48.f) / 48.f * 100.f;
		gText(vg, X(gl::SWING_X), X(gl::SWING_READ_Y), string::f("%.0f %%", swing), 7.f, G_BRASS_LIGHT);
		if (m && m->st.vintage)
			gText(vg, X(gl::SWING_X), X(gl::SWING_READ_Y) - X(2.2f), "96", 4.6f, G_LABEL_DIM, NVG_ALIGN_CENTER, 0.3f);

		// Écran de voie
		const float* sc = gl::SCREEN;
		float sy = X((sc[1] + sc[3]) / 2);
		int sel = m ? m->selected : 0;
		std::string title = cv ? string::f("CV %c", 'A' + sel - TRIG_LANES) : string::f("TRACK %d", sel + 1);
		std::string kind = "TRIGS";
		if (cv)
			kind = std::string(lp->unipolar ? "0–10 V" : "±5 V") + (lp->quantize ? " · SEMITONES" : " · FREE");
		else if (m && m->params[Gaston::REC_PARAM].getValue() > 0.5f)
			kind = "TRIGS · REC";
		gText(vg, X(sc[0] + 3.f), sy - X(1.3f), title, 9.5f, acc, NVG_ALIGN_LEFT, 0.6f);
		gText(vg, X(sc[0] + 3.f), sy + X(2.5f), kind, 5.f, G_LABEL_DIM, NVG_ALIGN_LEFT, 0.4f);
		int len = lp ? lp->length : 16;
		int ratio = lp ? lp->ratio : RATIO_X1;
		int dir = lp ? lp->dir : FORWARD;
		bool running = m && laneRunning(m->core, *lp);
		int pos = running ? lp->pos + 1 : 1;
		int page = m ? m->page : 0;
		static const char* const HEADS[5] = {"LENGTH", "RATIO", "DIR", "STEP", "PAGE"};
		for (int k = 0; k < 5; k++) {
			float x = X(sc[0] + 42.f + k * 18.f);
			gText(vg, x, sy - X(1.4f), HEADS[k], 6.f, G_LABEL_DIM, NVG_ALIGN_LEFT, 0.4f);
			float vy = sy + X(1.5f);
			if (k == 2) {
				gDirIcon(vg, x + X(2.7f), vy, X(5.4f), dir, acc);
				continue;
			}
			std::string v = k == 0 ? string::f("%d", len) : k == 1 ? RATIO_NAMES[ratio] : k == 3 ? string::f("%d", pos)
				: string::f("%d/%d", page + 1, std::max(1, (len + PAGE - 1) / PAGE));
			gText(vg, x, vy, v, 9.6f, acc, NVG_ALIGN_LEFT);
		}

		// Lectures des réglages de voie
		static const char* const CTRL[4] = {"LENGTH", "RATIO", "DIR", "SLEW"};
		for (int k = 0; k < (cv ? 4 : 3); k++) {
			float x = X(gl::CTRL_X[k] + gl::CTRL_KNOB_R + 1.6f), y = X(gl::CTRL_Y);
			std::string v = k == 0 ? string::f("%d", len) : k == 1 ? RATIO_NAMES[ratio] : k == 2 ? DIR_NAMES[dir]
				: string::f("%.0f %%", (lp ? lp->slew : 0.f) * 100.f);
			gText(vg, x, y - X(1.4f), CTRL[k], 5.8f, G_LABEL, NVG_ALIGN_LEFT, 0.5f);
			gText(vg, x, y + X(1.8f), v, 7.f, acc, NVG_ALIGN_LEFT);
		}

		// Diodes des voies : elles s'allument à chaque trig (séquence ou pad), à chaque pas pour les CV
		for (int i = 0; i < LANES; i++) {
			bool c = i >= TRIG_LANES;
			if (m) {
				const Core& core = m->core;
				if (core.seqFires[i] != seenSeq[i]) {
					seenSeq[i] = core.seqFires[i];
					ledTime[i] = now;
				}
				if (!c && core.padFires[i] != seenPad[i]) {
					seenPad[i] = core.padFires[i];
					ledTime[i] = now;
				}
				if (c && core.stepCount[i] != seenStep[i]) {
					seenStep[i] = core.stepCount[i];
					ledTime[i] = now;
				}
			}
			float lit = ledTime[i] >= 0.0 ? clamp(1.f - (float) (now - ledTime[i]) / 0.12f, 0.f, 1.f) : 0.f;
			float x = X(c ? gl::CV_COL_X[i - TRIG_LANES] : gl::TRIG_COL_X[i]), y = X(gl::LED_Y);
			NVGcolor off = c ? nvgRGB(0x1f, 0x3a, 0x36) : nvgRGB(0x3a, 0x32, 0x22);
			NVGcolor on = c ? G_VERD_GLOW : G_GLOW;
			if (lit > 0.f)
				gGlow(vg, x - 1.5f, y - 1.5f, 3.f, 3.f, 1.5f, 4.f, nvgTransRGBA(on, (unsigned char) (150 * lit)));
			nvgBeginPath(vg);
			nvgCircle(vg, x, y, 1.6f);
			gFill(vg, nvgLerpRGBA(off, on, lit));
		}

		// Valeurs courantes des voies CV
		const float* cr = gl::CV_READ;
		for (int c = 0; c < CV_LANES; c++) {
			float x = X(gl::CV_COL_X[c]);
			const Lane* l = m ? &m->core.lanes[TRIG_LANES + c] : NULL;
			float v = m ? m->core.cvOut[c] : 0.f;
			float meter = l && l->unipolar ? v / 10.f : (v + 5.f) / 10.f;
			meter = clamp(meter, 0.f, 1.f);
			float mh = X(4.2f), mt = X(cr[1] + 1.2f);
			gRoundRect(vg, x - 1.f, mt, 2.f, mh, 1.f);
			gFill(vg, nvgRGB(0x15, 0x29, 0x2a));
			gRoundRect(vg, x - 1.f, mt + mh * (1.f - meter), 2.f, mh * meter, 1.f);
			gFill(vg, G_VERD);
			gText(vg, x, X(cr[3] - 1.6f), l ? cvText(*l, v) : "+0.0", 6.f, G_VERD_LIGHT);
		}
	}
};


struct GastonWidget : ModuleWidget {
	GastonKnob* slewKnob = NULL;
	GastonSegments* segments = NULL;

	template <class T>
	T* place(T* w, float cxMm, float cyMm, float wMm, float hMm) {
		w->box.size = mm2px(Vec(wMm, hMm));
		w->box.pos = mm2px(Vec(cxMm, cyMm)).minus(w->box.size.div(2.f));
		return w;
	}

	GastonKnob* knob(Gaston* module, int id, float x, float y, float r, bool laneKnob) {
		GastonKnob* k = createParam<GastonKnob>(Vec(), module, id);
		place(k, x, y, 2 * r, 2 * r);
		k->gmodule = module;
		k->laneKnob = laneKnob;
		if (laneKnob) {
			// Pas de lissage : la valeur doit sauter tout de suite à celle de la voie choisie
			k->smooth = false;
			if (ParamQuantity* pq = k->getParamQuantity())
				pq->smoothEnabled = false;
		}
		addParam(k);
		return k;
	}

	GastonButton* button(Gaston* module, int id, int style, int index, float x, float y, float w, float h, bool momentary) {
		GastonButton* b = createParam<GastonButton>(Vec(), module, id);
		place(b, x, y, w, h);
		b->gmodule = module;
		b->style = style;
		b->index = index;
		b->momentary = momentary;
		b->initParamQuantity();
		addParam(b);
		return b;
	}

	GastonWidget(Gaston* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Gaston.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		GastonOverlay* overlay = createWidget<GastonOverlay>(Vec(0, 0));
		overlay->box.size = box.size;
		overlay->module = module;
		addChild(overlay);

		for (const gl::Label& l : gl::LABELS) {
			PanelLabel* label = createWidget<PanelLabel>(mm2px(Vec(l.x, l.y)));
			label->text = l.text;
			label->fontSize = l.size * G_FONT_SCALE;
			label->color = l.color;
			label->align = l.align;
			label->letterSpacing = l.spacing;
			label->embolden = 0.35f;
			addChild(label);
		}

		// Horloge et transport
		knob(module, Gaston::TEMPO_PARAM, gl::TEMPO_X, gl::TEMPO_Y, gl::TEMPO_R, false);
		knob(module, Gaston::SWING_PARAM, gl::SWING_X, gl::SWING_Y, gl::SWING_R, false);
		static const int TRANSPORT_IDS[3] = {Gaston::REC_PARAM, Gaston::CUE_PARAM, Gaston::PLAY_PARAM};
		for (int k = 0; k < 3; k++) {
			float d = 2 * gl::TRANSPORT_R[k];
			button(module, TRANSPORT_IDS[k], GastonButton::TRANSPORT, k, gl::TRANSPORT_X[k], gl::TRANSPORT_Y, d, d, k != 0);
		}
		static const int IN_IDS[4] = {Gaston::CLOCK_INPUT, Gaston::PLAY_INPUT, Gaston::RESET_INPUT, Gaston::PADS_INPUT};
		for (int k = 0; k < 4; k++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(gl::IN_X[k], gl::IN_Y[k])), module, IN_IDS[k]));
		static const int OUT_L_IDS[3] = {Gaston::CLOCK_OUTPUT, Gaston::RUN_OUTPUT, Gaston::RESET_OUTPUT};
		for (int k = 0; k < 3; k++)
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(gl::OUT_L_X[k], gl::OUT_JACK_Y)), module, OUT_L_IDS[k]));

		// Le ruban, la carte, les pages
		GastonRuban* ruban = createWidget<GastonRuban>(mm2px(Vec(gl::STEP_X0, gl::NUM_Y - 1.6f)));
		ruban->box.size = mm2px(Vec(127.6f, gl::GAUGE_Y0 + gl::GAUGE_H - (gl::NUM_Y - 1.6f) + 0.4f));
		ruban->module = module;
		addChild(ruban);
		GastonMap* map = createWidget<GastonMap>(mm2px(Vec(gl::MAP_BOX[0], gl::MAP_BOX[1])));
		map->box.size = mm2px(Vec(gl::MAP_BOX[2] - gl::MAP_BOX[0], gl::MAP_BOX[3] - gl::MAP_BOX[1]));
		map->module = module;
		addChild(map);
		GastonPages* pages = createWidget<GastonPages>(Vec());
		place(pages, gl::MAIN_CX, gl::PAGE_Y, 4 * gl::PAGE_PITCH + 1.f, gl::PAGE_H + 1.6f);
		pages->module = module;
		addChild(pages);

		// Les réglages de la voie choisie
		knob(module, Gaston::LENGTH_PARAM, gl::CTRL_X[0], gl::CTRL_Y, gl::CTRL_KNOB_R, true);
		knob(module, Gaston::DIVISION_PARAM, gl::CTRL_X[1], gl::CTRL_Y, gl::CTRL_KNOB_R, true);
		button(module, Gaston::DIR_PARAM, GastonButton::DIR, 0, gl::CTRL_X[2], gl::CTRL_Y, 2 * gl::CTRL_KNOB_R, 2 * gl::CTRL_KNOB_R, true);
		slewKnob = knob(module, Gaston::SLEW_PARAM, gl::CTRL_X[3], gl::CTRL_Y, gl::CTRL_KNOB_R, true);
		segments = createWidget<GastonSegments>(mm2px(Vec(gl::SEG_X0, gl::SEG_Y[0] - gl::SEG_H / 2)));
		segments->box.size = mm2px(Vec(2 * gl::SEG_W, gl::SEG_Y[1] - gl::SEG_Y[0] + gl::SEG_H));
		segments->module = module;
		addChild(segments);

		// Les voies : choix, pads, mutes, sorties
		for (int i = 0; i < TRIG_LANES; i++) {
			float x = gl::TRIG_COL_X[i];
			button(module, Gaston::SELECT_PARAMS + i, GastonButton::SELECT, i, x, gl::SEL_Y, gl::SEL_SIZE, gl::SEL_SIZE, true);
			button(module, Gaston::PAD_PARAMS + i, GastonButton::PAD, i, x, gl::PAD_Y, gl::PAD_W, gl::PAD_H, true);
			button(module, Gaston::MUTE_PARAMS + i, GastonButton::MUTE, i, x, gl::MUTE_Y, gl::MUTE_W, gl::MUTE_H, false);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, gl::OUT_JACK_Y)), module, Gaston::TRIG_OUTPUTS + i));
		}
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(gl::POLY_X, gl::OUT_JACK_Y)), module, Gaston::POLY_OUTPUT));
		for (int c = 0; c < CV_LANES; c++) {
			float x = gl::CV_COL_X[c];
			button(module, Gaston::SELECT_PARAMS + TRIG_LANES + c, GastonButton::SELECT, TRIG_LANES + c, x, gl::SEL_Y, gl::SEL_SIZE, gl::SEL_SIZE, true);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, gl::OUT_JACK_Y)), module, Gaston::CV_OUTPUTS + c));
		}
	}

	void step() override {
		Gaston* m = getModule<Gaston>();
		bool cv = m && m->lane().cv;
		if (slewKnob)
			slewKnob->visible = cv;
		if (segments)
			segments->visible = cv;
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		Gaston* m = getModule<Gaston>();
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Clock"));
		std::vector<std::string> res;
		for (int k = 0; k < CLOCK_RES; k++)
			res.push_back(CLOCK_RES_NAMES[k]);
		menu->addChild(createIndexPtrSubmenuItem("CLOCK IN resolution", res, &m->st.clockInRes));
		menu->addChild(createIndexPtrSubmenuItem("CLOCK OUT resolution", res, &m->st.clockOutRes));
		menu->addChild(createBoolPtrMenuItem("Vintage timing (96 PPQN)", "", &m->st.vintage));
		menu->addChild(createIndexPtrSubmenuItem("Realign all tracks",
			{"Never", "Every 16 steps", "Every 32 steps", "Every 64 steps"}, &m->st.realign));
	}
};


Model* modelGaston = createModel<Gaston, GastonWidget>("Gaston");
