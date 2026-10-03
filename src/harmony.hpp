#pragma once
#include <vector>

// Ensembles de perchoirs de Colette.
// Intervalles en demi-tons répétés à chaque octave ; HARMONICS suit la série harmonique, FREE n'a aucun perchoir.
// L'ordre compte : l'entrée HARMONY de Colette choisit l'indice à raison de 1 V par harmonie.
struct HarmonySet {
	const char* name;
	std::vector<float> semitones;
};
static const std::vector<HarmonySet> HARMONIES = {
	{"FIFTHS", {0, 7}},
	{"MAJOR", {0, 4, 7}},
	{"MINOR", {0, 3, 7}},
	{"SUS", {0, 2, 7}},
	{"MAJOR 9", {0, 2, 4, 7, 11}},
	{"MINOR 9", {0, 2, 3, 7, 10}},
	{"PENTATONIC", {0, 2, 4, 7, 9}},
	{"WHOLE TONE", {0, 2, 4, 6, 8, 10}},
	{"HARMONICS", {}},
	{"FREE", {}},
};
static const int HARMONY_COUNT = 10;
static const int HARMONY_HARMONICS = 8;
static const int HARMONY_FREE = 9;
