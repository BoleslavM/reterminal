// =============================================================================
// Écran E-Ink reTerminal E1002 – illustrations dessinées au pixel
// Palette Spectra 6 : noir, blanc, rouge, jaune, bleu, vert.
// Les nuances sont obtenues par tramage ordonné (matrice de Bayer 4x4) :
// deux couleurs pures mélangées point par point, comme en impression.
// Toutes les fonctions prennent l'écran (ou un canevas de test) en premier
// argument : il suffit qu'il ait draw_pixel_at(x, y, Color).
// =============================================================================
#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <algorithm>

namespace ecran {

using esphome::Color;

static const Color NOIR(0, 0, 0);
static const Color BLANC(255, 255, 255);
static const Color ROUGE(255, 0, 0);
static const Color JAUNE(255, 255, 0);
static const Color BLEU(0, 0, 255);
static const Color VERT(0, 255, 0);

static const int W = 800;
static const int H = 480;

// Seuils de Bayer 4x4 (0..15)
static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

// Mélange de deux couleurs : niveau 0 = tout a, 16 = tout b
inline bool trame(int x, int y, int niveau) { return niveau > BAYER[y & 3][x & 3]; }
inline Color melange(int x, int y, Color a, Color b, int niveau) { return trame(x, y, niveau) ? b : a; }

// Petit générateur pseudo-aléatoire reproductible (même image à chaque rafraîchissement)
struct Hasard {
  uint32_t s;
  explicit Hasard(uint32_t graine) : s(graine) {}
  uint32_t suivant() { s = s * 1664525u + 1013904223u; return s >> 8; }
  int entre(int a, int b) { return a + (int) (suivant() % (uint32_t) (b - a + 1)); }
};

template<typename D> inline void point(D &d, int x, int y, Color c) {
  if (x >= 0 && y >= 0 && x < W && y < H) d.draw_pixel_at(x, y, c);
}

// Rectangle tramé
template<typename D> void rect_mix(D &d, int x0, int y0, int w, int h, Color a, Color b, int niveau) {
  for (int y = y0; y < y0 + h; y++)
    for (int x = x0; x < x0 + w; x++) point(d, x, y, melange(x, y, a, b, niveau));
}

// Dégradé vertical tramé entre deux niveaux
template<typename D> void degrade(D &d, int x0, int y0, int w, int h, Color a, Color b, int n_haut, int n_bas) {
  for (int y = y0; y < y0 + h; y++) {
    int n = n_haut + (n_bas - n_haut) * (y - y0) / (h > 1 ? h - 1 : 1);
    for (int x = x0; x < x0 + w; x++) point(d, x, y, melange(x, y, a, b, n));
  }
}

// Disque plein tramé
template<typename D> void disque(D &d, int cx, int cy, int r, Color a, Color b, int niveau) {
  for (int y = cy - r; y <= cy + r; y++)
    for (int x = cx - r; x <= cx + r; x++)
      if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) point(d, x, y, melange(x, y, a, b, niveau));
}

// Voile : pose du blanc (ou une couleur) sur une partie des pixels sans toucher aux autres
template<typename D> void voile(D &d, int x0, int y0, int w, int h, Color c, int n_haut, int n_bas) {
  for (int y = y0; y < y0 + h; y++) {
    int n = n_haut + (n_bas - n_haut) * (y - y0) / (h > 1 ? h - 1 : 1);
    for (int x = x0; x < x0 + w; x++)
      if (trame(x, y, n)) point(d, x, y, c);
  }
}

// Relief : remplit sous une ligne de crête faite de sinus superposés
template<typename D>
void relief(D &d, int base, float amp1, float freq1, float phase1, float amp2, float freq2, float phase2, Color a,
            Color b, int niveau, int lisere = 0, Color c_lisere = Color(0, 0, 0)) {
  for (int x = 0; x < W; x++) {
    int crete = base + (int) (amp1 * sinf(x * freq1 + phase1) + amp2 * sinf(x * freq2 + phase2));
    for (int y = crete; y < H; y++) {
      Color c = (y - crete < lisere) ? c_lisere : melange(x, y, a, b, niveau);
      point(d, x, y, c);
    }
  }
}

// Nuage : grappe de disques, avec une base légèrement ombrée
template<typename D> void nuage(D &d, int cx, int cy, int taille, Color clair, Color ombre, int n_ombre) {
  const int bulles[6][3] = {{-60, 10, 34}, {-25, -12, 44}, {20, -20, 50}, {62, 2, 36}, {0, 14, 40}, {35, 16, 34}};
  for (auto &bu : bulles) {
    int r = bu[2] * taille / 100;
    int bx = cx + bu[0] * taille / 100, by = cy + bu[1] * taille / 100;
    for (int y = by - r; y <= by + r; y++)
      for (int x = bx - r; x <= bx + r; x++) {
        if ((x - bx) * (x - bx) + (y - by) * (y - by) > r * r) continue;
        // l'ombre monte progressivement vers le bas du nuage
        int debut = cy - 10 * taille / 100, fin = cy + 50 * taille / 100;
        int n = (y <= debut) ? 0 : (y >= fin ? n_ombre : n_ombre * (y - debut) / (fin - debut));
        point(d, x, y, melange(x, y, clair, ombre, n));
      }
  }
}

// Gouttes de pluie obliques
template<typename D> void pluie(D &d, int y0, int y1, Color c, int densite, uint32_t graine) {
  Hasard h(graine);
  for (int i = 0; i < densite; i++) {
    int x = h.entre(-40, W), y = h.entre(y0, y1);
    int l = h.entre(10, 22);
    for (int k = 0; k < l; k++) {
      point(d, x + k / 3, y + k, c);
      point(d, x + k / 3 + 1, y + k, c);
    }
  }
}

// Sapin stylisé
template<typename D> void sapin(D &d, int x, int sol, int haut, Color feuille, Color f2, int niveau) {
  for (int y = sol - haut; y < sol; y++) {
    int t = y - (sol - haut);
    int demi = (t * haut / 3) / haut + 2;
    // trois étages
    int etage = (t * 3) / haut;
    int largeur = demi + (t - etage * haut / 3) / 3;
    for (int xx = x - largeur; xx <= x + largeur; xx++) point(d, xx, y, melange(xx, y, feuille, f2, niveau));
  }
  for (int y = sol; y < sol + haut / 8; y++)
    for (int xx = x - 3; xx <= x + 3; xx++) point(d, xx, y, NOIR);
}

// Arbre rond (feuillu)
template<typename D> void arbre(D &d, int x, int sol, int r, Color feuille, Color f2, int niveau) {
  for (int y = sol - r; y < sol + 2; y++)
    for (int xx = x - 4; xx <= x + 4; xx++) point(d, xx, y, NOIR);
  disque(d, x, sol - r - r / 2, r, feuille, f2, niveau);
  disque(d, x - r / 2, sol - r - r / 4, r * 2 / 3, feuille, f2, niveau);
  disque(d, x + r / 2, sol - r - r / 5, r * 2 / 3, feuille, f2, niveau);
}

// Petite maison (fenêtre allumée la nuit)
template<typename D> void maison(D &d, int x, int sol, bool nuit) {
  Color mur = nuit ? NOIR : BLANC;
  Color toit = nuit ? NOIR : ROUGE;
  for (int y = sol - 50; y < sol; y++)
    for (int xx = x; xx < x + 70; xx++) point(d, xx, y, mur);
  for (int t = 0; t < 34; t++)
    for (int xx = x - 6 + t; xx < x + 76 - t; xx++) point(d, xx, sol - 50 - t, toit);
  // fenêtre et porte
  Color vitre = nuit ? JAUNE : BLEU;
  for (int y = sol - 38; y < sol - 20; y++)
    for (int xx = x + 10; xx < x + 30; xx++) point(d, xx, y, vitre);
  for (int y = sol - 30; y < sol; y++)
    for (int xx = x + 44; xx < x + 60; xx++) point(d, xx, y, nuit ? NOIR : NOIR);
  if (!nuit) {
    for (int xx = x; xx < x + 70; xx++) { point(d, xx, sol - 50, NOIR); }
    for (int y = sol - 50; y < sol; y++) { point(d, x, y, NOIR); point(d, x + 69, y, NOIR); }
  }
}

// Étoiles
template<typename D> void etoiles(D &d, int y1, int n, uint32_t graine) {
  Hasard h(graine);
  for (int i = 0; i < n; i++) {
    int x = h.entre(0, W - 1), y = h.entre(0, y1);
    point(d, x, y, BLANC);
    if (h.entre(0, 6) == 0) {  // quelques étoiles plus grosses
      point(d, x + 1, y, BLANC); point(d, x - 1, y, BLANC);
      point(d, x, y + 1, BLANC); point(d, x, y - 1, BLANC);
    }
  }
}

// Éclair
template<typename D> void eclair(D &d, int x, int y) {
  const int pts[6][2] = {{0, 0}, {-34, 90}, {-6, 90}, {-30, 190}, {40, 70}, {8, 70}};
  // deux triangles pleins approximés par lignes horizontales
  auto tri = [&](int x0, int y0, int x1, int y1, int x2, int y2) {
    int ymin = std::min(y0, std::min(y1, y2)), ymax = std::max(y0, std::max(y1, y2));
    for (int yy = ymin; yy <= ymax; yy++)
      for (int xx = x - 60; xx <= x + 60; xx++) {
        float d1 = (float) (xx - x1) * (y0 - y1) - (float) (x0 - x1) * (yy - y1);
        float d2 = (float) (xx - x2) * (y1 - y2) - (float) (x1 - x2) * (yy - y2);
        float d3 = (float) (xx - x0) * (y2 - y0) - (float) (x2 - x0) * (yy - y0);
        bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0), pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        if (!(neg && pos)) point(d, xx, yy, JAUNE);
      }
  };
  tri(x + pts[0][0], y + pts[0][1], x + pts[1][0], y + pts[1][1], x + pts[5][0] + 20, y + pts[5][1]);
  tri(x + pts[0][0], y + pts[0][1], x + 30, y, x + pts[5][0] + 20, y + pts[5][1]);
  tri(x + pts[2][0], y + pts[2][1] - 25, x + pts[4][0], y + pts[4][1], x + pts[3][0], y + pts[3][1]);
}

// -----------------------------------------------------------------------------
// Paysages complets 800x480
// -----------------------------------------------------------------------------

// Collines de jour (vert), plus ternes si le ciel est gris
template<typename D> void collines_jour(D &d, bool terne) {
  // lointaines : bleutées
  relief(d, 300, 22, 0.011f, 0.6f, 9, 0.031f, 1.4f, VERT, BLEU, terne ? 9 : 7);
  // moyennes
  relief(d, 340, 26, 0.008f, 2.1f, 10, 0.023f, 0.3f, VERT, terne ? NOIR : JAUNE, terne ? 5 : 3);
  // proches : plus sombres
  relief(d, 395, 30, 0.006f, 4.0f, 8, 0.019f, 2.2f, VERT, NOIR, terne ? 9 : 6, 0);
  arbre(d, 120, 410, 26, VERT, JAUNE, terne ? 2 : 5);
  arbre(d, 168, 418, 18, VERT, JAUNE, terne ? 1 : 3);
  maison(d, 560, 412, false);
  sapin(d, 690, 420, 70, VERT, NOIR, 9);
  sapin(d, 730, 426, 54, VERT, NOIR, 10);
}

template<typename D> void collines_nuit(D &d) {
  relief(d, 300, 22, 0.011f, 0.6f, 9, 0.031f, 1.4f, BLEU, NOIR, 9);
  relief(d, 340, 26, 0.008f, 2.1f, 10, 0.023f, 0.3f, BLEU, NOIR, 12);
  relief(d, 395, 30, 0.006f, 4.0f, 8, 0.019f, 2.2f, NOIR, NOIR, 0);
  maison(d, 560, 412, true);
  sapin(d, 690, 420, 70, NOIR, NOIR, 0);
  sapin(d, 730, 426, 54, NOIR, NOIR, 0);
}

template<typename D> void soleil(D &d, int cx, int cy) {
  disque(d, cx, cy, 92, BLEU, BLANC, 12);   // halo
  disque(d, cx, cy, 76, BLANC, JAUNE, 9);
  disque(d, cx, cy, 60, JAUNE, JAUNE, 0);
}

template<typename D> void lune(D &d, int cx, int cy) {
  disque(d, cx, cy, 44, JAUNE, BLANC, 3);
  // la découpe reprend exactement la trame du ciel (même position, même niveau)
  for (int y = cy - 46; y <= cy + 46; y++)
    for (int x = cx - 20; x <= cx + 60; x++) {
      int dx = x - (cx + 20), dy = y - (cy - 12);
      if (dx * dx + dy * dy <= 40 * 40) {
        int n = 4 + 4 * y / 300;  // même formule que le ciel de nuit
        point(d, x, y, melange(x, y, NOIR, BLEU, n));
      }
    }
}

// Scène selon le code envoyé par Home Assistant (sensor.ecran_scene_meteo)
template<typename D> void scene(D &d, const std::string &code) {
  if (code == "nuit" || code == "nuit_nuages" || code == "nuit_pluie") {
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) point(d, x, y, melange(x, y, NOIR, BLEU, 4 + 4 * y / 300));
    etoiles(d, 300, code == "nuit" ? 260 : 120, 7);
    lune(d, 610, 120);
    if (code != "nuit") {
      nuage(d, 420, 170, 110, BLEU, NOIR, 10);
      nuage(d, 200, 100, 130, BLEU, NOIR, 10);
    }
    collines_nuit(d);
    if (code == "nuit_pluie") pluie(d, 0, H, BLEU, 420, 3);
    return;
  }
  if (code == "couvert" || code == "pluie" || code == "orage") {
    int clair = code == "orage" ? 5 : (code == "pluie" ? 9 : 12);
    degrade(d, 0, 0, W, 340, NOIR, BLANC, clair - 2, clair + 3);
    nuage(d, 200, 90, 150, BLANC, NOIR, clair);
    nuage(d, 560, 60, 170, BLANC, NOIR, clair - 1);
    nuage(d, 420, 170, 120, BLANC, NOIR, clair - 2);
    if (code == "orage") eclair(d, 470, 150);
    collines_jour(d, true);
    if (code != "couvert") pluie(d, 140, H, code == "orage" ? BLANC : BLEU, code == "orage" ? 520 : 420, 11);
    return;
  }
  if (code == "neige") {
    degrade(d, 0, 0, W, 340, BLEU, BLANC, 9, 14);
    relief(d, 300, 22, 0.011f, 0.6f, 9, 0.031f, 1.4f, BLANC, BLEU, 5);
    relief(d, 350, 26, 0.008f, 2.1f, 10, 0.023f, 0.3f, BLANC, BLEU, 2);
    relief(d, 405, 24, 0.006f, 4.0f, 8, 0.019f, 2.2f, BLANC, BLANC, 0);
    sapin(d, 120, 420, 90, VERT, NOIR, 9);
    sapin(d, 170, 430, 62, VERT, NOIR, 10);
    sapin(d, 690, 420, 70, VERT, NOIR, 9);
    maison(d, 520, 422, false);
    Hasard h(5);
    for (int i = 0; i < 380; i++) disque(d, h.entre(0, W), h.entre(0, H), h.entre(1, 3), BLANC, BLANC, 0);
    return;
  }
  // Jour : soleil, éclaircies, brouillard, coucher
  if (code == "coucher") {
    degrade(d, 0, 0, W, 320, ROUGE, JAUNE, 2, 15);
    disque(d, 400, 320, 110, JAUNE, BLANC, 6);
    relief(d, 320, 18, 0.011f, 0.6f, 9, 0.031f, 1.4f, ROUGE, NOIR, 10);
    relief(d, 365, 22, 0.008f, 2.1f, 10, 0.023f, 0.3f, NOIR, NOIR, 0);
    sapin(d, 120, 440, 90, NOIR, NOIR, 0);
    sapin(d, 690, 430, 70, NOIR, NOIR, 0);
    return;
  }
  degrade(d, 0, 0, W, 340, BLEU, BLANC, 3, 14);
  soleil(d, 620, 130);
  if (code == "nuageux") {
    nuage(d, 230, 110, 120, BLANC, BLEU, 5);
    nuage(d, 540, 200, 95, BLANC, BLEU, 5);
  }
  collines_jour(d, false);
  if (code == "brouillard") {
    voile(d, 0, 0, W, 300, BLANC, 9, 12);
    voile(d, 0, 300, W, 180, BLANC, 12, 7);
  }
}

// -----------------------------------------------------------------------------
// Éléments d'interface
// -----------------------------------------------------------------------------

// Cartouche aux coins arrondis, avec bordure
template<typename D> void cartouche(D &d, int x0, int y0, int w, int h, int r, Color fond, Color bord, int ep = 2) {
  for (int y = y0; y < y0 + h; y++)
    for (int x = x0; x < x0 + w; x++) {
      int dx = 0, dy = 0;
      if (x < x0 + r) dx = x0 + r - x; else if (x >= x0 + w - r) dx = x - (x0 + w - r - 1);
      if (y < y0 + r) dy = y0 + r - y; else if (y >= y0 + h - r) dy = y - (y0 + h - r - 1);
      int d2 = dx * dx + dy * dy;
      if (d2 > r * r) continue;
      bool sur_bord = d2 > (r - ep) * (r - ep) || ((dx == 0 || dy == 0) &&
                      (x < x0 + ep || x >= x0 + w - ep || y < y0 + ep || y >= y0 + h - ep));
      point(d, x, y, sur_bord ? bord : fond);
    }
}

// Couleur Tempo → couleur d'écran
inline Color couleur_tempo(const std::string &t) {
  if (t == "Rouge") return ROUGE;
  if (t == "Blanc") return BLANC;
  if (t == "Bleu") return BLEU;
  return NOIR;
}

}  // namespace ecran
