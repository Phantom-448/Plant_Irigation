#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Führt die intelligente Auswertung von Feuchtigkeit, Temperatur und Profil aus
// und steuert entsprechend die Pumpe und den Timer.
void smart_logic_evaluate(void);

#ifdef __cplusplus
}
#endif