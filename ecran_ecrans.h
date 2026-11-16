// =============================================================================
// Écran E-Ink reTerminal E1002 – mises en page
//   matin : vue Maison (météo, agenda, électricité d'hier, Tempo du jour et du lendemain)
//   meteo : tableau décoratif qui suit la météo réelle, encadré couleur Tempo du lendemain
// Le bouton vert bascule entre les deux (Home Assistant, sensor.ecran_moment).
// L'objet D doit fournir : draw_pixel_at et texte (voir reterminal-1002.yaml).
// =============================================================================
#pragma once
#include "ecran_dessins.h"
#include <cstdio>
#include <string>

namespace ecran {

enum Police { TITRE, SOUS_TITRE, CHIFFRES, CORPS, PETIT, GRAS, ICONE_G, ICONE_P };
enum Align { GAUCHE, CENTRE, DROITE };

struct Donnees {
  std::string moment = "matin", scene = "soleil", date, meteo, prev_cond;
  std::string agenda[4];
  std::string tempo_j, tempo_d;
  std::string maj;  // « 10/11 21:50 » : date et heure du dernier rafraîchissement
  float temp_ext = NAN, tmin = NAN, tmax = NAN;
  float conso = NAN, cout = NAN, tarif = NAN;
  float t_pieces[6] = {NAN, NAN, NAN, NAN, NAN, NAN};
  int lumieres = 0, fenetres = 0;
};

static const char *PIECES[6] = {"Chambre", "Cuisine", "Salle à manger", "Salle de bain", "Bureau", "Couloir"};

// --- petits outils de texte ---------------------------------------------------
inline std::string utf8(uint32_t cp) {
  std::string s;
  if (cp < 0x80) s += (char) cp;
  else if (cp < 0x800) { s += (char) (0xC0 | (cp >> 6)); s += (char) (0x80 | (cp & 0x3F)); }
  else if (cp < 0x10000) { s += (char) (0xE0 | (cp >> 12)); s += (char) (0x80 | ((cp >> 6) & 0x3F)); s += (char) (0x80 | (cp & 0x3F)); }
  else { s += (char) (0xF0 | (cp >> 18)); s += (char) (0x80 | ((cp >> 12) & 0x3F)); s += (char) (0x80 | ((cp >> 6) & 0x3F)); s += (char) (0x80 | (cp & 0x3F)); }
  return s;
}

inline std::string nombre(float v, int dec, const char *suffixe = "") {
  if (std::isnan(v)) return std::string("–") + suffixe;
  char b[24];
  const char *f = dec == 0 ? "%.0f" : dec == 1 ? "%.1f" : dec == 2 ? "%.2f" : "%.4f";
  snprintf(b, sizeof(b), f, v);
  std::string s(b);
  for (auto &c : s) if (c == '.') c = ',';
  if (s == "-0") s = "0";
  return s + suffixe;
}

inline std::string degres(float v) { return nombre(v, 0, "°"); }

// Icône selon la condition Home Assistant (sunny, rainy...) ou le code de scène
inline std::string icone_meteo(const std::string &c) {
  if (c == "sunny" || c == "soleil") return utf8(0xF0599);
  if (c == "clear-night" || c == "nuit") return utf8(0xF0594);
  if (c == "partlycloudy" || c == "nuageux" || c == "nuit_nuages") return utf8(0xF0595);
  if (c == "fog" || c == "brouillard") return utf8(0xF0591);
  if (c == "pouring") return utf8(0xF0596);
  if (c == "rainy" || c == "pluie" || c == "nuit_pluie" || c == "hail" || c == "snowy-rainy") return utf8(0xF0597);
  if (c == "snowy" || c == "neige") return utf8(0xF0598);
  if (c == "lightning" || c == "lightning-rainy" || c == "orage") return utf8(0xF0593);
  return utf8(0xF0590);  // nuageux par défaut
}

inline bool tempo_connue(const std::string &t) { return t == "Bleu" || t == "Blanc" || t == "Rouge"; }

// Ligne d'agenda « 20:00|Bébé chou » → heure, titre
inline void separer(const std::string &l, std::string &heure, std::string &titre) {
  auto p = l.find('|');
  if (p == std::string::npos) { heure = ""; titre = l; return; }
  heure = l.substr(0, p);
  titre = l.substr(p + 1);
}

template<typename D> void filet(D &d, int x0, int x1, int y, Color c, int ep = 1) {
  for (int yy = y; yy < y + ep; yy++)
    for (int x = x0; x <= x1; x++) point(d, x, yy, c);
}

template<typename D> void pointilles_v(D &d, int x, int y0, int y1, Color c) {
  for (int y = y0; y <= y1; y++) if ((y / 3) % 2 == 0) point(d, x, y, c);
}

template<typename D> void fond(D &d, Color c) { rect_mix(d, 0, 0, W, H, c, c, 0); }

// Pastille Tempo : rectangle arrondi coloré + texte
template<typename D> void pastille_tempo(D &d, int x, int y, int w, const char *quand, const std::string &couleur) {
  bool connue = tempo_connue(couleur);
  Color c = connue ? couleur_tempo(couleur) : BLANC;
  Color texte = (!connue || couleur == "Blanc") ? NOIR : BLANC;
  cartouche(d, x, y, w, 40, 10, c, NOIR, 2);
  d.texte(x + w / 2, y + 9, PETIT, texte, CENTRE, std::string(quand) + " · " + (connue ? couleur : std::string("?")));
}

// Encadré coloré sans texte : couleur Tempo du lendemain (rien si pas encore publiée)
template<typename D> void encadre_tempo(D &d, int x, int y, const std::string &couleur) {
  if (!tempo_connue(couleur)) return;
  cartouche(d, x, y, 72, 40, 10, couleur_tempo(couleur), NOIR, 2);
}

// =============================================================================
// Matin : météo, agenda, électricité d'hier
// =============================================================================
template<typename D> void ecran_matin(D &d, const Donnees &v) {
  fond(d, BLANC);
  // En-tête : date + Tempo aujourd'hui et demain
  d.texte(40, 32, SOUS_TITRE, NOIR, GAUCHE, v.date);
  pastille_tempo(d, 458, 36, 124, "Auj.", v.tempo_j);
  pastille_tempo(d, 592, 36, 168, "Demain", v.tempo_d);
  filet(d, 40, 760, 100, JAUNE, 4);

  // Colonne gauche : météo
  d.texte(40, 128, ICONE_G, JAUNE, GAUCHE, icone_meteo(v.prev_cond.empty() ? v.scene : v.prev_cond));
  d.texte(190, 132, CHIFFRES, NOIR, GAUCHE, degres(v.temp_ext));
  d.texte(40, 262, GRAS, NOIR, GAUCHE, v.meteo);
  d.texte(40, 296, CORPS, NOIR, GAUCHE, "Mini " + degres(v.tmin) + "   Maxi " + degres(v.tmax));

  // Électricité d'hier + heure de mise à jour
  cartouche(d, 40, 344, 330, 112, 14, BLANC, NOIR, 2);
  d.texte(62, 354, PETIT, NOIR, GAUCHE, "Électricité d'hier");
  d.texte(62, 382, GRAS, NOIR, GAUCHE, nombre(v.conso, 1, " kWh"));
  d.texte(350, 382, GRAS, ROUGE, DROITE, nombre(v.cout, 2, " €"));
  if (!v.maj.empty()) d.texte(62, 424, PETIT, NOIR, GAUCHE, v.maj);

  pointilles_v(d, 400, 128, 446, NOIR);

  // Colonne droite : agenda
  d.texte(430, 124, SOUS_TITRE, NOIR, GAUCHE, "Aujourd'hui");
  int y = 184, n = 0;
  for (int i = 0; i < 4; i++) {
    if (v.agenda[i].empty()) continue;
    std::string h, t;
    separer(v.agenda[i], h, t);
    d.texte(430, y, PETIT, h.rfind("Demain", 0) == 0 ? BLEU : ROUGE, GAUCHE, h);
    d.texte(430, y + 24, CORPS, NOIR, GAUCHE, t);
    y += 62;
    n++;
  }
  if (n == 0) d.texte(430, 190, CORPS, NOIR, GAUCHE, "Rien de prévu. Profitez-en.");
}

// =============================================================================
// Soir et nuit : tableau météo
// =============================================================================
template<typename D> void ecran_meteo(D &d, const Donnees &v) {
  scene(d, v.scene);
  cartouche(d, 24, 328, 452, 128, 16, BLANC, NOIR, 2);
  d.texte(44, 340, CHIFFRES, NOIR, GAUCHE, degres(v.temp_ext));
  d.texte(232, 348, GRAS, NOIR, GAUCHE, v.meteo);
  d.texte(232, 382, PETIT, NOIR, GAUCHE, "Rennes · " + degres(v.tmin) + " / " + degres(v.tmax));
  if (!v.maj.empty()) d.texte(232, 412, PETIT, NOIR, GAUCHE, v.maj);
  encadre_tempo(d, 704, 416, v.tempo_d);
}

// Point d'entrée
template<typename D> void dessiner(D &d, const Donnees &v) {
  if (v.moment == "meteo") ecran_meteo(d, v);
  else ecran_matin(d, v);
}

}  // namespace ecran
