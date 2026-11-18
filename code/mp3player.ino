#include <SoftwareSerial.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

int PIN_YX_RX = 2;
int PIN_YX_TX = 3;
int PIN_HC_RX = 4;
int PIN_HC_TX = 5;
int PIN_TFT_RST = 8;
int PIN_TFT_DC = 9;
int PIN_TFT_CS = 10;
int PIN_BTN_PP = 7;
int PIN_EC_CLK = A2;
int PIN_EC_DT = A1;
int PIN_EC_SW = A0;
int PIN_VOL = A3;
int PIN_BTN_SK = A4;
int PIN_BTN_RT = A5;

int ETAT_MENU_ALBUMS = 0;
int ETAT_MENU_PISTES = 1;
int ETAT_LECTURE = 2;

int NB_ALBUMS_MANUEL = 3;
int NB_PISTES_MANUEL[20] = {
  15, 17, 14,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int VOLUME_MAX = 30;
int MAX_ALBUMS = 20;
int LIGNES_VISIBLES = 12;
int DUREE_ESTIMEE = 180;
int DELAI_REBOND = 100;
int HYSTERESIS_VOL = 16;
int PERIODE_VOL = 60;

const uint16_t COULEUR_FOND = 0x0841;
const uint16_t COULEUR_CARTE = 0x10A2;
const uint16_t COULEUR_TEXTE = 0xFFFF;
const uint16_t COULEUR_ACCENT = 0x07FF;
const uint16_t COULEUR_SELECTION = 0x1C9F;
const uint16_t COULEUR_BARRE = 0x04FF;
const uint16_t COULEUR_GRIS = 0x4208;
const uint16_t COULEUR_PISTE_OFF = 0x2945;
const uint16_t COULEUR_OMBRE = 0x0000;

const uint16_t PALETTE_FOND[8] = {
  0x0228, 0x2000, 0x0480, 0x3009, 0x6200, 0x0249, 0x2C20, 0x4008
};
const uint16_t PALETTE_C1[8] = {
  0x045F, 0xC180, 0x07E0, 0x701F, 0xFD20, 0x055F, 0x3FE0, 0xF81F
};
const uint16_t PALETTE_C2[8] = {
  0x07FF, 0xFBE0, 0xB7E0, 0xF81F, 0xFFE0, 0x87FF, 0xA7E0, 0xFFD0
};

SoftwareSerial yxSerial(PIN_YX_RX, PIN_YX_TX);
SoftwareSerial btSerial(PIN_HC_RX, PIN_HC_TX);
Adafruit_ST7789 tft = Adafruit_ST7789(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

int  etatCourant = 0;
int  albumCourant = 0;
int  pisteCourante = 0;
int  albumEnLecture = 0;
int  pisteEnLecture = 0;
int  nbAlbums = 0;
int  nbPistesAlbum[20];

int  volumeCourant = 20;
bool enLecture = false;
bool dejaJoue = false;

unsigned long tempsDepartPiste = 0;
unsigned long tempsPauseDebut = 0;

unsigned long dernierTempsNav = 0;

int debutFenetrePistes = 0;

bool clkPrecedent = HIGH;

bool etatBtnPP = HIGH;
bool etatBtnSK = HIGH;
bool etatBtnRT = HIGH;
unsigned long tempsBtnPP = 0;
unsigned long tempsBtnSK = 0;
unsigned long tempsBtnRT = 0;

bool swPrecedent = HIGH;
unsigned long tempsClic = 0;

//module MP3
void yx_envoyer(int cmd, int dh, int dl) {

  int somme = 0xFF + 0x06 + cmd + 0x00 + dh + dl;
  int chk = (-somme) & 0xFFFF;

  uint8_t trame[10];
  trame[0] = 0x7E;
  trame[1] = 0xFF;
  trame[2] = 0x06;
  trame[3] = cmd;
  trame[4] = 0x00;
  trame[5] = dh;
  trame[6] = dl;
  trame[7] = (chk >> 8) & 0xFF;
  trame[8] = chk & 0xFF;
  trame[9] = 0xEF;
  yxSerial.write(trame, 10);
  delay(20);
}

void yx_jouerFichier(int dossier, int fichier) { yx_envoyer(0x0F, dossier, fichier); }
void yx_pause()                                { yx_envoyer(0x0E, 0, 0); }
void yx_reprendre()                            { yx_envoyer(0x0D, 0, 0); }
void yx_setVolume(int v)                       { yx_envoyer(0x06, 0, v); }
void yx_reset()                                { yx_envoyer(0x0C, 0, 0); }

int yx_lireOctet(unsigned long timeout_ms) {
  unsigned long debut = millis();
  while (millis() - debut < timeout_ms) {
    if (yxSerial.available()) return yxSerial.read();
  }
  return -1;
}

int yx_getNbDossiers() {
  return NB_ALBUMS_MANUEL;
}

int yx_getNbFichiersDossier(int dossier) {
  if (dossier < 1 || dossier > MAX_ALBUMS) return 1;
  return NB_PISTES_MANUEL[dossier - 1];
}

bool yx_verifierFinDePiste() {
  if (!yxSerial.available()) return false;

  int b0 = yx_lireOctet(5);
  if (b0 != 0x7E) return false;

  int b1 = yx_lireOctet(20);
  int b2 = yx_lireOctet(20);
  int cmd = yx_lireOctet(20);
  int b4 = yx_lireOctet(20);
  int dh = yx_lireOctet(20);
  int dl = yx_lireOctet(20);
  int chkH = yx_lireOctet(20);
  int chkL = yx_lireOctet(20);
  int b9 = yx_lireOctet(20);

  if (b1 == 0xFF && b9 == 0xEF && cmd == 0x3D) {
    return true;
  }
  return false;
}

void nombrePad2(int n) {
  if (n < 10) tft.print('0');
  tft.print(n);
}

void afficherTemps(int secondes) {
  tft.print(secondes / 60);
  tft.print(':');
  if (secondes % 60 < 10) tft.print('0');
  tft.print(secondes % 60);
}

void iconePlay(int x, int y, int h, uint16_t c) {
  tft.fillTriangle(x, y, x, y + h, x + (h * 9) / 10, y + h / 2, c);
}

void iconePause(int x, int y, int h, uint16_t c) {
  int l = h / 3;
  tft.fillRect(x,         y, l, h, c);
  tft.fillRect(x + 2 * l, y, l, h, c);
}

void iconePrecedent(int x, int y, int h, uint16_t c) {
  tft.fillRect(x, y, 3, h, c);
  tft.fillTriangle(x + h, y, x + h, y + h, x + 5, y + h / 2, c);
}

void iconeSuivant(int x, int y, int h, uint16_t c) {
  tft.fillTriangle(x, y, x, y + h, x + h - 5, y + h / 2, c);
  tft.fillRect(x + h - 3, y, 3, h, c);
}

void iconeVolume(int x, int y, int niveau) {
  tft.fillRect(x, y - 3, 4, 7, COULEUR_TEXTE);
  tft.fillTriangle(x + 4, y, x + 10, y - 7, x + 10, y + 7, COULEUR_TEXTE);
  if (niveau >= 1) tft.drawCircleHelper(x + 12, y, 4,  0x06, COULEUR_ACCENT);
  if (niveau >= 2) tft.drawCircleHelper(x + 12, y, 7,  0x06, COULEUR_ACCENT);
  if (niveau >= 3) tft.drawCircleHelper(x + 12, y, 10, 0x06, COULEUR_ACCENT);
  if (niveau == 0) {
    tft.drawLine(x + 14, y - 4, x + 22, y + 4, COULEUR_GRIS);
    tft.drawLine(x + 14, y + 4, x + 22, y - 4, COULEUR_GRIS);
  }
}

int niveauVolume() {
  if (volumeCourant == 0)  return 0;
  if (volumeCourant <= 10) return 1;
  if (volumeCourant <= 20) return 2;
  return 3;
}

void dessinerPochette(int idx, int x, int y) {
  int theme = idx % 8;

  tft.fillRect(x, y, 196, 196, PALETTE_FOND[theme]);

  int k;
  for (k = 0; k < 3; k++) {
    tft.drawLine(x + 8 + k * 6, y + 188, x + 78 + k * 6, y + 118, PALETTE_C2[theme]);
  }

  int r1 = 60 + (idx % 3) * 10;
  int cx = x + 77 + (idx % 4) * 14;
  int cy = y + 70 + (idx % 3) * 16;

  if (cx - r1 < x + 2)       cx = x + r1 + 2;
  if (cx + r1 > x + 193)     cx = x + 193 - r1;
  if (cy - r1 < y + 2)       cy = y + r1 + 2;
  if (cy + r1 > y + 193)     cy = y + 193 - r1;

  tft.fillCircle(cx, cy, r1,      PALETTE_C1[theme]);
  tft.fillCircle(cx, cy, r1 - 12, PALETTE_FOND[theme]);
  tft.fillCircle(cx, cy, r1 - 24, PALETTE_C1[theme]);
  tft.fillCircle(cx, cy, r1 - 34, PALETTE_FOND[theme]);
  tft.fillCircle(cx, cy, 6,       PALETTE_C2[theme]);

  int r2  = 24 + (idx % 3) * 7;
  int cx2 = x + 34 + (idx % 5) * 9;
  int cy2 = y + 150 - (idx % 3) * 12;
  if (cx2 - r2 < x + 2)   cx2 = x + r2 + 2;
  if (cx2 + r2 > x + 193) cx2 = x + 193 - r2;
  if (cy2 - r2 < y + 2)   cy2 = y + r2 + 2;
  if (cy2 + r2 > y + 193) cy2 = y + 193 - r2;

  tft.fillCircle(cx2, cy2, r2,      PALETTE_C2[theme]);
  tft.fillCircle(cx2, cy2, r2 - 10, PALETTE_FOND[theme]);

  tft.setTextColor(0xC618);
  tft.setTextSize(1);
  tft.setCursor(x + 8, y + 8);
  tft.print(F("ALBUM "));
  nombrePad2(idx + 1);

  tft.setTextSize(3);
  tft.setTextColor(COULEUR_OMBRE);
  tft.setCursor(x + 11, y + 163);
  nombrePad2(idx + 1);
  tft.setTextColor(COULEUR_TEXTE);
  tft.setCursor(x + 10, y + 162);
  nombrePad2(idx + 1);
  tft.setTextSize(1);
}

void cadrePochette(int x, int y) {
  tft.drawRect(x - 1, y - 1, 198, 198, COULEUR_GRIS);
  tft.fillRect(x + 197, y + 3, 3, 196, COULEUR_OMBRE);
  tft.fillRect(x + 3, y + 197, 196, 3, COULEUR_OMBRE);
}

void dessinerVolFooter(int y) {
  tft.fillRect(190, y - 2, 50, 12, COULEUR_FOND);
  tft.setTextSize(1);
  tft.setTextColor(COULEUR_GRIS);
  tft.setCursor(192, y);
  tft.print(F("VOL "));
  nombrePad2(volumeCourant);
}

void dessinerVolStatus() {
  tft.fillRect(152, 1, 88, 21, COULEUR_CARTE);
  iconeVolume(160, 11, niveauVolume());
  tft.setTextSize(1);
  tft.setTextColor(COULEUR_TEXTE);
  tft.setCursor(214, 8);
  nombrePad2(volumeCourant);
}

void dessinerEtatStatus() {
  tft.fillRect(1, 1, 150, 21, COULEUR_CARTE);
  tft.setTextSize(1);
  tft.setCursor(8, 8);
  if (!dejaJoue) {
    tft.setTextColor(COULEUR_GRIS);
    tft.print(F("PRET"));
  } else if (enLecture) {
    tft.setTextColor(COULEUR_ACCENT);
    tft.print(F("EN LECTURE"));
  } else {
    tft.setTextColor(COULEUR_GRIS);
    tft.print(F("PAUSE"));
  }
}

void dessinerBadgeLecture() {
  tft.fillRect(4, 304, 182, 14, COULEUR_FOND);
  if (!dejaJoue) return;
  tft.setTextSize(1);
  tft.setCursor(8, 306);
  if (enLecture) {
    tft.setTextColor(COULEUR_ACCENT);
    tft.print(F("> "));
  } else {
    tft.setTextColor(COULEUR_GRIS);
    tft.print(F("| "));
  }
  tft.print(F("Album "));
  tft.print(albumEnLecture + 1);
  tft.print(F(" - Piste "));
  tft.print(pisteEnLecture + 1);
}

void majVolumeAffiche() {
  if      (etatCourant == ETAT_LECTURE)     dessinerVolStatus();
  else if (etatCourant == ETAT_MENU_ALBUMS) dessinerVolFooter(306);
  else                                      dessinerVolFooter(310);
}


void afficherEcranAlbum() {
  tft.fillScreen(COULEUR_FOND);

  tft.fillRect(0, 0, 240, 27, COULEUR_CARTE);
  tft.drawFastHLine(0, 27, 240, COULEUR_GRIS);
  tft.setTextColor(COULEUR_TEXTE);
  tft.setTextSize(2);
  tft.setCursor(8, 7);
  tft.print(F("ALBUMS"));
  tft.setTextSize(1);
  tft.setTextColor(COULEUR_GRIS);
  tft.setCursor(196, 10);
  nombrePad2(albumCourant + 1);
  tft.print('/');
  nombrePad2(nbAlbums);

  if (nbAlbums > 1) {
    int prec = (albumCourant + nbAlbums - 1) % nbAlbums;
    int suiv = (albumCourant + 1) % nbAlbums;
    tft.fillRect(0,   40, 10, 196, PALETTE_FOND[prec % 8]);
    tft.fillRect(230, 40, 10, 196, PALETTE_FOND[suiv % 8]);
  }

  dessinerPochette(albumCourant, 22, 40);
  cadrePochette(22, 40);

  tft.setTextSize(2);
  tft.setTextColor(COULEUR_TEXTE);
  tft.setCursor(60, 246);
  tft.print(F("Album "));
  nombrePad2(albumCourant + 1);

  tft.setTextSize(1);
  tft.setTextColor(COULEUR_GRIS);
  tft.setCursor(90, 266);
  tft.print(nbPistesAlbum[albumCourant]);
  tft.print(F(" pistes"));

  tft.fillRect(20, 278, 200, 3, COULEUR_PISTE_OFF);
  int lThumb = 200 / (nbAlbums > 0 ? nbAlbums : 1);
  if (lThumb < 10) lThumb = 10;
  int xThumb = 20;
  if (nbAlbums > 1)
    xThumb = 20 + (int)((long)albumCourant * (200 - lThumb) / (nbAlbums - 1));
  tft.fillRect(xThumb, 278, lThumb, 3, COULEUR_ACCENT);

  tft.drawFastHLine(0, 290, 240, COULEUR_GRIS);
  tft.setTextColor(COULEUR_GRIS);
  tft.setCursor(8, 294);
  tft.print(F("< > naviguer    clic : ouvrir"));
  dessinerBadgeLecture();
  dessinerVolFooter(306);
}

void dessinerLignePiste(int i, bool selectionnee) {
  int rangVisuel = (i == -1) ? 0 : (i - debutFenetrePistes + 1);
  int yPos = 38 + rangVisuel * 22;

  tft.fillRect(0, yPos, 234, 22, COULEUR_FOND);
  if (selectionnee) {
    tft.fillRoundRect(4, yPos, 228, 22, 4, COULEUR_SELECTION);
  }

  tft.setTextSize(1);

  if (i == -1) {
    tft.setTextColor(selectionnee ? COULEUR_TEXTE : COULEUR_GRIS);
    tft.setCursor(14, yPos + 7);
    tft.print(F("..  retour albums"));
    return;
  }

  tft.setTextColor(selectionnee ? COULEUR_ACCENT : COULEUR_GRIS);
  tft.setCursor(14, yPos + 7);
  nombrePad2(i + 1);

  tft.setTextColor(selectionnee ? COULEUR_TEXTE : COULEUR_GRIS);
  tft.setCursor(44, yPos + 7);
  tft.print(F("Piste "));
  tft.print(i + 1);

  if (enLecture && albumEnLecture == albumCourant && pisteEnLecture == i) {
    tft.setTextColor(COULEUR_ACCENT);
    tft.setCursor(216, yPos + 7);
    tft.print('>');
  }
}

int calculerFenetrePistes() {
  if (pisteCourante == -1) return 0;
  int total = nbPistesAlbum[albumCourant];
  int debut = pisteCourante - 5;
  if (debut < 0) debut = 0;
  if (debut + LIGNES_VISIBLES > total) debut = total - LIGNES_VISIBLES;
  if (debut < 0) debut = 0;
  return debut;
}

void dessinerScrollbarPistes() {
  int total = nbPistesAlbum[albumCourant];
  tft.fillRect(236, 38, 3, 264, COULEUR_PISTE_OFF);
  if (total <= LIGNES_VISIBLES) {
    tft.fillRect(236, 38, 3, 264, COULEUR_GRIS);
    return;
  }
  int h = (int)((long)264 * LIGNES_VISIBLES / total);
  if (h < 12) h = 12;
  int y = 38 + (int)((long)264 * debutFenetrePistes / total);
  tft.fillRect(236, y, 3, h, COULEUR_ACCENT);
}

void afficherEcranPistes() {
  tft.fillScreen(COULEUR_FOND);

  int theme = albumCourant % 8;
  debutFenetrePistes = calculerFenetrePistes();

  tft.fillRect(0, 0, 240, 33, PALETTE_FOND[theme]);
  tft.fillRect(0, 0, 4, 33, PALETTE_C1[theme]);
  tft.drawFastHLine(0, 33, 240, PALETTE_C1[theme]);
  tft.setTextColor(COULEUR_TEXTE);
  tft.setTextSize(2);
  tft.setCursor(12, 9);
  tft.print(F("ALBUM "));
  nombrePad2(albumCourant + 1);
  tft.setTextSize(1);
  tft.setCursor(190, 13);
  tft.print(nbPistesAlbum[albumCourant]);
  tft.print(F(" pistes"));

  dessinerLignePiste(-1, pisteCourante == -1);

  int fin = debutFenetrePistes + LIGNES_VISIBLES;
  if (fin > nbPistesAlbum[albumCourant]) fin = nbPistesAlbum[albumCourant];

  int i;
  for (i = debutFenetrePistes; i < fin; i++) {
    dessinerLignePiste(i, i == pisteCourante);
  }

  dessinerScrollbarPistes();

  tft.drawFastHLine(0, 304, 240, COULEUR_GRIS);
  tft.setTextColor(COULEUR_GRIS);
  tft.setCursor(8, 310);
  tft.print(F("clic : lire    RETOUR : albums"));
  dessinerVolFooter(310);
}

void deplacerSelectionPiste(int direction) {
  int nbP      = nbPistesAlbum[albumCourant];
  int ancienne = pisteCourante;

  if (direction == -1) {
    if (pisteCourante == -1) pisteCourante = nbP - 1;
    else pisteCourante--;
  } else {
    if (pisteCourante == nbP - 1) pisteCourante = -1;
    else pisteCourante++;
  }

  int nouvelleFenetre = calculerFenetrePistes();

  if (nouvelleFenetre != debutFenetrePistes) {
    afficherEcranPistes();
  } else {
    dessinerLignePiste(ancienne, false);
    dessinerLignePiste(pisteCourante, true);
  }
}

void dessinerTransportCentre() {
  tft.fillCircle(120, 306, 11, COULEUR_FOND);
  tft.drawCircle(120, 306, 12, COULEUR_ACCENT);
  tft.drawCircle(120, 306, 13, COULEUR_ACCENT);
  if (enLecture) iconePause(115, 300, 12, COULEUR_TEXTE);
  else           iconePlay(116, 300, 12, COULEUR_TEXTE);
}

int secondesEcoulees() {
  if (!dejaJoue) return 0;
  unsigned long ref = enLecture ? millis() : tempsPauseDebut;
  int e = (int)((ref - tempsDepartPiste) / 1000);
  if (e < 0) e = 0;
  if (e > DUREE_ESTIMEE) e = DUREE_ESTIMEE;
  return e;
}

void dessinerProgression() {
  if (etatCourant != ETAT_LECTURE) return;

  int elapsed = secondesEcoulees();

  static int dernierElapsed = -1;
  if (elapsed == dernierElapsed) return;
  dernierElapsed = elapsed;

  int l = (int)((long)elapsed * 200 / DUREE_ESTIMEE);
  int xc = 20 + l;
  if (xc < 24)  xc = 24;
  if (xc > 216) xc = 216;

  int lPrec = (int)((long)(elapsed - 1) * 200 / DUREE_ESTIMEE);
  int xcPrec = 20 + lPrec;
  if (xcPrec < 24)  xcPrec = 24;
  if (xcPrec > 216) xcPrec = 216;

  tft.fillRect(xcPrec - 5, 282, 11, 12, COULEUR_FOND);

  tft.fillRoundRect(20, 284, 200, 6, 3, COULEUR_PISTE_OFF);
  if (l > 5) tft.fillRoundRect(20, 284, l, 6, 3, COULEUR_BARRE);

  tft.fillCircle(xc, 287, 4, COULEUR_TEXTE);

  tft.fillRect(20, 268, 42, 9, COULEUR_FOND);
  tft.setTextSize(1);
  tft.setTextColor(COULEUR_TEXTE);
  tft.setCursor(20, 268);
  afficherTemps(elapsed);
}

void afficherEcranLecture() {
  tft.fillScreen(COULEUR_FOND);

  tft.fillRect(0, 0, 240, 23, COULEUR_CARTE);
  tft.drawFastHLine(0, 23, 240, COULEUR_GRIS);
  dessinerEtatStatus();
  dessinerVolStatus();


  dessinerPochette(albumEnLecture, 22, 32);
  cadrePochette(22, 32);

  tft.setTextSize(2);
  tft.setTextColor(COULEUR_TEXTE);
  tft.setCursor(20, 238);
  tft.print(F("Piste "));
  nombrePad2(pisteEnLecture + 1);

  tft.setTextSize(1);
  tft.setTextColor(COULEUR_GRIS);
  tft.setCursor(200, 243);
  nombrePad2(pisteEnLecture + 1);
  tft.print('/');
  nombrePad2(nbPistesAlbum[albumEnLecture]);

  tft.setCursor(20, 258);
  tft.print(F("Album "));
  tft.print(albumEnLecture + 1);
  tft.print(F("  -  "));
  tft.print(nbPistesAlbum[albumEnLecture]);
  tft.print(F(" pistes"));

  tft.setTextColor(COULEUR_GRIS);
  tft.setCursor(200, 268);
  afficherTemps(DUREE_ESTIMEE);

  dessinerProgression();

  iconePrecedent(56, 300, 12, COULEUR_GRIS);
  dessinerTransportCentre();
  iconeSuivant(168, 300, 12, COULEUR_GRIS);
}

void lancerPiste(int album, int piste, bool manuel) {
  albumEnLecture = album;
  pisteEnLecture = piste;
  enLecture = true;
  dejaJoue = true;
  tempsDepartPiste = millis();

  yx_jouerFichier(album + 1, piste + 1);

  if (manuel) {
    etatCourant = ETAT_LECTURE;
    afficherEcranLecture();
    return;
  }

  if (etatCourant == ETAT_LECTURE) {
    afficherEcranLecture();
  } else {
    dessinerBadgeLecture();
  }
}

void gererFinDePiste() {
  if (millis() - tempsDepartPiste < 1000) return;
  if (!enLecture) return;

  int nbP = nbPistesAlbum[albumEnLecture];

  if (pisteEnLecture + 1 < nbP) {
    lancerPiste(albumEnLecture, pisteEnLecture + 1, false);
  } else {
    lancerPiste(albumEnLecture, 0, false);
  }
}

void gererEncodeur() {
  bool clkActuel = digitalRead(PIN_EC_CLK);

  if (clkActuel == LOW && clkPrecedent == HIGH) {
    dernierTempsNav = millis();

    bool dtActuel = digitalRead(PIN_EC_DT);

    if (dtActuel == HIGH) {
      if (etatCourant == ETAT_MENU_ALBUMS) {
        albumCourant = (albumCourant + 1) % nbAlbums;
        afficherEcranAlbum();
      } else if (etatCourant == ETAT_MENU_PISTES) {
        deplacerSelectionPiste(+1);
      }
    } else {
      if (etatCourant == ETAT_MENU_ALBUMS) {
        albumCourant = (albumCourant + nbAlbums - 1) % nbAlbums;
        afficherEcranAlbum();
      } else if (etatCourant == ETAT_MENU_PISTES) {
        deplacerSelectionPiste(-1);
      }
    }
  }

  clkPrecedent = clkActuel;
}

void gererClicEncodeur() {
  bool swActuel = digitalRead(PIN_EC_SW);

  if (swActuel == LOW && swPrecedent == HIGH && millis() - tempsClic > 200) {
    tempsClic       = millis();
    dernierTempsNav = millis();

    if (etatCourant == ETAT_MENU_ALBUMS) {
      pisteCourante = -1;
      etatCourant   = ETAT_MENU_PISTES;
      afficherEcranPistes();

    } else if (etatCourant == ETAT_MENU_PISTES) {
      if (pisteCourante == -1) {
        etatCourant = ETAT_MENU_ALBUMS;
        afficherEcranAlbum();
      } else {
        lancerPiste(albumCourant, pisteCourante, true);
      }

    } else {
      albumCourant  = albumEnLecture;
      pisteCourante = pisteEnLecture;
      etatCourant   = ETAT_MENU_PISTES;
      afficherEcranPistes();
    }
  }

  swPrecedent = swActuel;
}

void basculerPlayPause() {
  if (!dejaJoue) {
    lancerPiste(albumCourant, 0, true);
    return;
  }

  if (enLecture) {
    yx_pause();
    enLecture       = false;
    tempsPauseDebut = millis();
  } else {
    yx_reprendre();
    enLecture = true;
    tempsDepartPiste += millis() - tempsPauseDebut;
  }

  if (etatCourant == ETAT_LECTURE) {
    dessinerTransportCentre();
    dessinerEtatStatus();
  } else if (etatCourant == ETAT_MENU_ALBUMS) {
    dessinerBadgeLecture();
  } else {
    if (albumCourant == albumEnLecture) {
      dessinerLignePiste(pisteEnLecture, pisteEnLecture == pisteCourante);
    }
  }
}

void pisteSuivante() {
  if (!dejaJoue) { lancerPiste(albumCourant, 0, false); return; }
  int nbP = nbPistesAlbum[albumEnLecture];
  lancerPiste(albumEnLecture, (pisteEnLecture + 1) % nbP, false);
}

void pistePrecedente() {
  if (!dejaJoue) { lancerPiste(albumCourant, 0, false); return; }
  int nbP = nbPistesAlbum[albumEnLecture];
  int prec = pisteEnLecture == 0 ? nbP - 1 : pisteEnLecture - 1;
  lancerPiste(albumEnLecture, prec, false);
}

void gererBoutons() {
  bool lecturePP = digitalRead(PIN_BTN_PP);
  bool lectureSK = digitalRead(PIN_BTN_SK);
  bool lectureRT = digitalRead(PIN_BTN_RT);

  if (lecturePP == LOW && etatBtnPP == HIGH &&
      millis() - tempsBtnPP > DELAI_REBOND) {
    tempsBtnPP      = millis();
    dernierTempsNav = millis();
    basculerPlayPause();
  }
  if (lecturePP == HIGH && etatBtnPP == LOW &&
      millis() - tempsBtnPP > DELAI_REBOND) {
  }
  etatBtnPP = lecturePP;

  if (lectureSK == LOW && etatBtnSK == HIGH &&
      millis() - tempsBtnSK > DELAI_REBOND) {
    tempsBtnSK      = millis();
    dernierTempsNav = millis();
    pisteSuivante();
  }
  if (lectureSK == HIGH && etatBtnSK == LOW &&
      millis() - tempsBtnSK > DELAI_REBOND) {
  }
  etatBtnSK = lectureSK;

  if (lectureRT == LOW && etatBtnRT == HIGH &&
      millis() - tempsBtnRT > DELAI_REBOND) {
    tempsBtnRT      = millis();
    dernierTempsNav = millis();

    if (etatCourant == ETAT_MENU_PISTES) {
      etatCourant = ETAT_MENU_ALBUMS;
      afficherEcranAlbum();
    } else {
      pistePrecedente();
    }
  }
  if (lectureRT == HIGH && etatBtnRT == LOW &&
      millis() - tempsBtnRT > DELAI_REBOND) {
  }
  etatBtnRT = lectureRT;
}

void gererVolume() {
  static unsigned long dernierEch = 0;
  static int           rawRef     = -1;

  if (millis() - dernierEch < PERIODE_VOL) return;
  dernierEch = millis();

  int raw = analogRead(PIN_VOL);

  if (rawRef < 0) {
    rawRef = raw;
    return;
  }

  int diff = raw - rawRef;
  if (diff < 0) diff = -diff;
  if (diff < HYSTERESIS_VOL) return;

  rawRef = raw;

  int nouvVol = (int)map(raw, 0, 1023, 0, VOLUME_MAX);

  if (nouvVol != volumeCourant) {
    volumeCourant = nouvVol;
    yx_setVolume(volumeCourant);
    majVolumeAffiche();
  }
}

void gererBluetooth() {
  btSerial.listen();
  if (!btSerial.available()) {
    yxSerial.listen();
    return;
  }

  char c = (char)btSerial.read();
  dernierTempsNav = millis();

  if (c == 'p') {
    basculerPlayPause();
    if (enLecture) btSerial.println(F("Lecture"));
    else           btSerial.println(F("Pause"));

  } else if (c == 's') {
    pisteSuivante();
    btSerial.print(F("Piste "));
    btSerial.println(pisteEnLecture + 1);

  } else if (c == 'b') {
    pistePrecedente();
    btSerial.print(F("Piste "));
    btSerial.println(pisteEnLecture + 1);

  } else if (c == '+') {
    if (volumeCourant < VOLUME_MAX) {
      volumeCourant++;
      yx_setVolume(volumeCourant);
      majVolumeAffiche();
    }
    btSerial.print(F("Volume : "));
    btSerial.println(volumeCourant);

  } else if (c == '-') {
    if (volumeCourant > 0) {
      volumeCourant--;
      yx_setVolume(volumeCourant);
      majVolumeAffiche();
    }
    btSerial.print(F("Volume : "));
    btSerial.println(volumeCourant);
  }

  yxSerial.listen();
}

void gererTimeout() {
  unsigned long TIMEOUT_NAV = 10000;
  if (etatCourant != ETAT_LECTURE && enLecture &&
      millis() - dernierTempsNav > TIMEOUT_NAV) {
    etatCourant = ETAT_LECTURE;
    afficherEcranLecture();
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("=== SETUP START ==="));

  btSerial.begin(9600);
  yxSerial.begin(9600);

  pinMode(PIN_BTN_PP, INPUT_PULLUP);
  pinMode(PIN_BTN_SK, INPUT_PULLUP);
  pinMode(PIN_BTN_RT, INPUT_PULLUP);
  pinMode(PIN_EC_CLK, INPUT_PULLUP);
  pinMode(PIN_EC_DT,  INPUT_PULLUP);
  pinMode(PIN_EC_SW,  INPUT_PULLUP);

  tft.init(240, 320);
  tft.setRotation(0);
  tft.invertDisplay(false);
  tft.cp437(true);
  tft.fillScreen(COULEUR_FOND);

  delay(1000);
  yx_reset();
  delay(3000);

  volumeCourant = (int)map(analogRead(PIN_VOL), 0, 1023, 0, VOLUME_MAX);
  yx_setVolume(volumeCourant);
  delay(100);

  yxSerial.listen();
  nbAlbums = yx_getNbDossiers();

  int i;
  for (i = 0; i < nbAlbums; i++) {
    nbPistesAlbum[i] = yx_getNbFichiersDossier(i + 1);
    if (nbPistesAlbum[i] == 0) nbPistesAlbum[i] = 1;
  }

  dernierTempsNav = millis();
  etatCourant     = ETAT_MENU_ALBUMS;
  afficherEcranAlbum();

  btSerial.println(F("Lecteur MP3 pret."));
}

void loop() {
  if (yx_verifierFinDePiste()) {
    gererFinDePiste();
  }

  gererEncodeur();
  gererClicEncodeur();
  gererBoutons();
  gererBluetooth();
  gererVolume();

  gererTimeout();

  static unsigned long derniereMAJBarre = 0;
  if (millis() - derniereMAJBarre >= 1000) {
    derniereMAJBarre = millis();
    if (enLecture) dessinerProgression();
  }
}
