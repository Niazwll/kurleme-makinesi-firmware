#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

// ================= PINLER =================
#define TFT_CS    33
#define TFT_DC    34
#define TFT_MOSI  35
#define TFT_SCLK  36
#define TFT_RST   21
#define TFT_BLK   15

#define TMC_CS    18          // şemada M/CS
#define TMC_MISO  37          // şemada MISO (TMC2130 SDO)

#define ENC_CLK   11
#define ENC_DT    10
#define ENC_SW    8

#define LED_PIN    14
#define LED_SAYISI 28

#define LIMIT_PIN    16
#define LIMIT_BASILI HIGH     // switch ters bağlıysa (NC) LOW yap

#define STEP_PIN  4
#define DIR_PIN   5
#define EN_PIN    2           // TMC2130: LOW = motor aktif
#define YON_ILERI HIGH        // motor ters dönüyorsa LOW yap

#define FAN_PWM_PIN 13
#define FAN_RPM_PIN 39

#define VALF_PIN  12          // şemada GPIO_FREE (CN8)
#define VALF_ACIK HIGH        // röle ters çalışıyorsa LOW yap

#define BUZZER_PIN 6
#define DHT_PIN    7

// ================= AYARLAR =================
#define FAN_AKTIF   1         // 0 yaparsan fan tamamen kapanır
#define LED_AKTIF   1
#define DHT_AKTIF   1
#define BUZZER_TIP  0         // 0 = aktif buzzer, 1 = pasif buzzer
#define ENC_YON     1         // çevirme yönü ters ise -1 yap
#define ADIM_BASINA 4         // encoder: 1 tık = kaç adım

#define FAN_FREKANS 25000
#define FAN_DARBE   2
#define FAN_SOGUTMA 50        // durunca 15 sn soğutma gücü (%)
#define FAN_KICK_MS 1500      // fan açılırken ilk %100 verilen süre

// Bed speed (step motor, adım/sn)
const char* const BED_AD[3] = {"SLOW", "MEDIUM", "FAST"};
const int BED_HIZ[3]        = {800, 2000, 4000};

// Mode (fan gücü %)
const char* const MOD_AD[3] = {"SOFT", "STANDART", "POWER"};
const int MOD_FAN[3]        = {50, 75, 100};

// ================= RENKLER =================
#define RGB565(r,g,b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
const uint16_t C_BG      = RGB565(8, 10, 18);
const uint16_t C_KART    = RGB565(26, 30, 46);
const uint16_t C_CIZGI   = RGB565(48, 55, 78);
const uint16_t C_ACIK    = RGB565(0, 150, 255);
const uint16_t C_YESIL   = RGB565(0, 220, 140);
const uint16_t C_TURUNCU = RGB565(255, 150, 30);
const uint16_t C_KIRMIZI = RGB565(255, 70, 80);
const uint16_t C_SARI    = RGB565(255, 210, 60);
const uint16_t C_METIN   = RGB565(240, 244, 255);
const uint16_t C_SOLUK   = RGB565(125, 135, 160);
const uint16_t MOD_RENK[3] = {C_YESIL, C_ACIK, C_TURUNCU};

#define F9   (&FreeSans9pt7b)
#define F9B  (&FreeSansBold9pt7b)
#define F24B (&FreeSansBold24pt7b)

// ================= NESNELER =================
Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 kanvas(280, 240);
Adafruit_NeoPixel led(LED_SAYISI, LED_PIN, NEO_GRB + NEO_KHZ800);
hw_timer_t *stepTimer = NULL;

// ================= DURUM =================
enum Ekran { E_ANA, E_SURE, E_CALIS, E_BITTI, E_MOD, E_AYAR, E_BED, E_INFO, E_ABOUT };
Ekran ekran = E_ANA;

int anaImlec = 0;
int ayarImlec = 0;
int imlec = 0;
int sureDk = 5;
int mod = 1;                   // 0 soft, 1 standart, 2 power
int bedHiz = 1;                // 0 slow, 1 medium, 2 fast
bool buzzerAcik = true;
bool valfAyar = true;

bool cizimIstek = true;
unsigned long sonCizim = 0;

// Geri sayım (sadece switch basılıyken akar)
uint32_t calisSureMs = 0;
uint32_t calisGecenMs = 0;
unsigned long sonTick = 0;
bool calisAktif = false;
int sonSaniye = -1;

// Bitiş ekranı
const char* bittiMesaj = "";
uint16_t bittiRenk = C_YESIL;
int bittiIkon = 0;             // 0 tik, 1 duraklat, 2 uyari
unsigned long bittiZaman = 0;

// Uyarı
const char* uyariMetni = "";
unsigned long uyariBitis = 0;

// Motor
int hiz = 0;
bool motorGuncelle = false;
volatile bool stepDurum = false;
volatile int32_t konum = 0;

// Valf
bool valfAcik = false;

// Fan
volatile uint32_t tachSayac = 0;
volatile uint32_t sonTach = 0;
int fanRpm = 0;
int fanYuzde = 0;
int fanUygulanan = -1;
unsigned long fanKickBitis = 0;
unsigned long sonRpm = 0;
unsigned long fanSogumaBitis = 0;

// Sıcaklık
int sicaklik = 0;
int nem = 0;
bool sicaklikGecerli = false;
int dhtHata = -1;              // -1 henüz okunmadı, 0 tamam, 1 cevap yok, 2 bozuk veri, 3 yarıda kesildi
unsigned long sonDht = 0;

// Limit
bool limitBasili = false;
bool limitOnceki = false;
unsigned long limitZaman = 0;

// Buton
bool oncekiButon = HIGH;
unsigned long basmaZamani = 0;
bool uzunBasma = false;

// Buzzer
int bipKalan = 0;
bool bipAcik = false;
unsigned long bipZaman = 0;
int bipSure = 100;

// TMC
bool tmcOk = false;

// LED
#define RENK_SAYISI 14
uint32_t renkler[RENK_SAYISI];
int kaydir = 0;
unsigned long sonLed = 0;

unsigned long sonInfoSn = 0;

// ================= INTERRUPTLAR =================
void IRAM_ATTR stepISR()
{
  stepDurum = !stepDurum;
  digitalWrite(STEP_PIN, stepDurum);
  if (stepDurum) konum++;
}

void IRAM_ATTR tachISR()
{
  uint32_t t = micros();
  if (t - sonTach > 1000) { tachSayac++; sonTach = t; }
}

volatile int32_t adim = 0;
volatile uint8_t durum = 0;
const int8_t tablo[16] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};

void IRAM_ATTR encISR()
{
  durum = ((durum << 2) | (digitalRead(ENC_CLK) << 1) | digitalRead(ENC_DT)) & 0x0F;
  adim += tablo[durum];
}

// ================= TMC2130 =================
uint32_t tmcTransfer(uint8_t adres, uint32_t veri)
{
  uint32_t cevap = 0;
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
  digitalWrite(TMC_CS, LOW);
  SPI.transfer(adres);
  for (int i = 3; i >= 0; i--)
    cevap = (cevap << 8) | SPI.transfer((veri >> (8 * i)) & 0xFF);
  digitalWrite(TMC_CS, HIGH);
  SPI.endTransaction();
  return cevap;
}

void tmcYaz(uint8_t reg, uint32_t veri) { tmcTransfer(reg | 0x80, veri); }

uint32_t tmcOku(uint8_t reg)
{
  tmcTransfer(reg & 0x7F, 0);
  return tmcTransfer(reg & 0x7F, 0);
}

void tmcBaslat()
{
  tmcYaz(0x00, 0x00000000);   // GCONF
  tmcYaz(0x6C, 0x140100C3);   // CHOPCONF: 16 mikro adım + interpolasyon
  tmcYaz(0x10, 0x00061008);   // IHOLD=8, IRUN=16 (maks 31)
  tmcYaz(0x11, 0x0000000A);   // TPOWERDOWN
  uint32_t ioin = tmcOku(0x04);
  tmcOk = ((ioin >> 24) == 0x11);
}

// ================= DHT11 (kendi okuma kodumuz) =================
int32_t dhtBekle(int seviye, uint32_t maxUs)
{
  uint32_t b = micros();
  while (digitalRead(DHT_PIN) == seviye) {
    if (micros() - b > maxUs) return -1;
  }
  return (int32_t)(micros() - b);
}

int dhtOku(int &sic, int &nm)
{
  uint8_t d[5] = {0, 0, 0, 0, 0};

  pinMode(DHT_PIN, OUTPUT);
  digitalWrite(DHT_PIN, LOW);
  delay(20);
  digitalWrite(DHT_PIN, HIGH);
  delayMicroseconds(30);
  pinMode(DHT_PIN, INPUT_PULLUP);

  int hata = 0;
  noInterrupts();
  if (dhtBekle(HIGH, 150) < 0) hata = 1;
  else if (dhtBekle(LOW, 150) < 0) hata = 1;
  else if (dhtBekle(HIGH, 150) < 0) hata = 1;
  else {
    for (int i = 0; i < 40; i++) {
      if (dhtBekle(LOW, 100) < 0) { hata = 3; break; }
      int32_t h = dhtBekle(HIGH, 120);
      if (h < 0) { hata = 3; break; }
      d[i / 8] <<= 1;
      if (h > 40) d[i / 8] |= 1;
    }
  }
  interrupts();

  if (hata) return hata;
  if (((d[0] + d[1] + d[2] + d[3]) & 0xFF) != d[4]) return 2;
  nm = d[0];
  sic = d[2];
  return 0;
}

void sicaklikOku()
{
  int s, n;
  int sonuc = dhtOku(s, n);
  dhtHata = sonuc;
  if (sonuc == 0) {
    sicaklik = s;
    nem = n;
    sicaklikGecerli = true;
  }
  cizimIstek = true;
}

// ================= MOTOR / FAN / VALF / BUZZER =================
void motoruUygula()
{
  if (hiz == 0) {
    timerStop(stepTimer);
    stepDurum = false;
    digitalWrite(STEP_PIN, LOW);
    digitalWrite(EN_PIN, HIGH);          // motor serbest
    return;
  }
  digitalWrite(EN_PIN, LOW);
  digitalWrite(DIR_PIN, YON_ILERI);
  uint32_t aralik = 1000000UL / (2UL * hiz);
  timerStop(stepTimer);
  timerWrite(stepTimer, 0);
  timerAlarm(stepTimer, aralik, true, 0);
  timerStart(stepTimer);
}

void fanUygula(int yuzde)
{
#if FAN_AKTIF
  ledcWrite(FAN_PWM_PIN, yuzde * 255 / 100);
#endif
}

void buzzerAyarla(bool ac)
{
#if BUZZER_TIP == 1
  ledcWrite(BUZZER_PIN, ac ? 128 : 0);
#else
  digitalWrite(BUZZER_PIN, ac ? HIGH : LOW);
#endif
}

void bip(int adet, int sure)
{
  if (!buzzerAcik) return;
  buzzerAyarla(false);
  bipKalan = adet;
  bipSure = sure;
  bipAcik = false;
  bipZaman = millis() - sure;
}

void bipIsle(unsigned long simdi)
{
  if (!buzzerAcik) {
    if (bipAcik || bipKalan) { buzzerAyarla(false); bipAcik = false; bipKalan = 0; }
    return;
  }
  if (bipAcik) {
    if (simdi - bipZaman >= (unsigned long)bipSure) {
      buzzerAyarla(false); bipAcik = false; bipZaman = simdi;
    }
  } else if (bipKalan > 0) {
    if (simdi - bipZaman >= (unsigned long)bipSure) {
      buzzerAyarla(true); bipAcik = true; bipZaman = simdi; bipKalan--;
    }
  }
}

// ================= ÇİZİM YARDIMCILARI =================
void yazi(int x, int y, const char* s, const GFXfont* f, uint8_t boyut, uint16_t renk)
{
  kanvas.setFont(f);
  kanvas.setTextSize(boyut);
  kanvas.setTextColor(renk);
  kanvas.setCursor(x, y);
  kanvas.print(s);
}

int yaziGenislik(const char* s, const GFXfont* f, uint8_t boyut)
{
  int16_t x1, y1;
  uint16_t w, h;
  kanvas.setFont(f);
  kanvas.setTextSize(boyut);
  kanvas.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return w;
}

void ortala(int cx, int y, const char* s, const GFXfont* f, uint8_t boyut, uint16_t renk)
{
  int16_t x1, y1;
  uint16_t w, h;
  kanvas.setFont(f);
  kanvas.setTextSize(boyut);
  kanvas.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  yazi(cx - (int)w / 2 - x1, y, s, f, boyut, renk);
}

void hapCiz(int x, int y, int w, int h, const char* s, const GFXfont* f, uint16_t dolgu, uint16_t yRenk)
{
  int r = h / 2 > 14 ? 14 : h / 2;
  kanvas.fillRoundRect(x, y, w, h, r, dolgu);
  ortala(x + w / 2, y + h / 2 + 6, s, f, 1, yRenk);
}

void halka(int cx, int cy, int rd, int ri, float oran, uint16_t renk, uint16_t arka)
{
  int dolu = (int)(oran * 720);
  for (int a = 0; a < 720; a++) {
    float aci = (a * 0.5f - 90.0f) * DEG_TO_RAD;
    float c = cosf(aci), s = sinf(aci);
    kanvas.drawLine((int16_t)(cx + c * ri), (int16_t)(cy + s * ri),
                    (int16_t)(cx + c * rd), (int16_t)(cy + s * rd),
                    a < dolu ? renk : arka);
  }
}

void altYol(const char* s)
{
  yazi(14, 234, s, F9, 1, C_SOLUK);
}

void ustBar()
{
  kanvas.fillRect(0, 0, 280, 28, C_KART);

  // termometre ikonu
  kanvas.fillRoundRect(12, 5, 4, 12, 2, C_TURUNCU);
  kanvas.fillCircle(14, 19, 4, C_TURUNCU);

  char sb[8];
  if (sicaklikGecerli) sprintf(sb, "%d", sicaklik);
  else strcpy(sb, "--");
  yazi(24, 20, sb, F9B, 1, C_METIN);
  int w = yaziGenislik(sb, F9B, 1);
  kanvas.drawCircle(24 + w + 5, 10, 2, C_METIN);
  yazi(24 + w + 10, 20, "C", F9B, 1, C_METIN);

  // mod hapı
  const char* m = MOD_AD[mod];
  int pw = yaziGenislik(m, F9B, 1) + 22;
  hapCiz(272 - pw, 3, pw, 22, m, F9B, MOD_RENK[mod], C_BG);
}

// ================= EKRANLAR =================
void cizAna()
{
  kanvas.fillScreen(C_BG);
  ustBar();

  char b[8];
  sprintf(b, "%02d:00", sureDk);
  ortala(140, 112, b, F24B, 2, C_METIN);

  char c1[20], c2[16];
  sprintf(c1, "BED %s", BED_AD[bedHiz]);
  sprintf(c2, "VALF %s", valfAyar ? "ON" : "OFF");
  int w1 = yaziGenislik(c1, F9, 1) + 24;
  int w2 = yaziGenislik(c2, F9, 1) + 24;
  int x = (280 - (w1 + w2 + 8)) / 2;
  hapCiz(x, 144, w1, 28, c1, F9, C_KART, C_SOLUK);
  hapCiz(x + w1 + 8, 144, w2, 28, c2, F9, C_KART, valfAyar ? C_YESIL : C_SOLUK);

  const char* tab[3] = {"START", "SETTINGS", "MODE"};
  int tw[3], toplam = 0;
  for (int i = 0; i < 3; i++) {
    tw[i] = yaziGenislik(tab[i], F9B, 1) + 26;
    toplam += tw[i];
  }
  toplam += 12;
  x = (280 - toplam) / 2;
  for (int i = 0; i < 3; i++) {
    bool s = (i == anaImlec);
    hapCiz(x, 192, tw[i], 40, tab[i], s ? F9B : F9, s ? C_ACIK : C_KART, s ? C_METIN : C_SOLUK);
    x += tw[i] + 6;
  }
}

void cizSure()
{
  kanvas.fillScreen(C_BG);
  ustBar();

  char b[8];
  sprintf(b, "%02d:00", sureDk);
  ortala(140, 112, b, F24B, 2, C_SARI);

  kanvas.fillTriangle(56, 152, 70, 142, 70, 162, C_SOLUK);
  kanvas.fillTriangle(224, 152, 210, 142, 210, 162, C_SOLUK);
  if (millis() < uyariBitis) ortala(140, 158, uyariMetni, F9B, 1, C_KIRMIZI);
  else                       ortala(140, 158, "DAKIKA SEC", F9B, 1, C_ACIK);

  int w1 = yaziGenislik("BAS: BASLAT", F9B, 1) + 28;
  int w2 = yaziGenislik("UZUN: GERI", F9, 1) + 28;
  int x = (280 - (w1 + w2 + 8)) / 2;
  hapCiz(x, 196, w1, 34, "BAS: BASLAT", F9B, C_YESIL, C_BG);
  hapCiz(x + w1 + 8, 196, w2, 34, "UZUN: GERI", F9, C_KART, C_SOLUK);
}

void cizCalis()
{
  kanvas.fillScreen(C_BG);
  ustBar();

  uint32_t gecen = calisGecenMs;
  if (gecen > calisSureMs) gecen = calisSureMs;
  uint32_t kalanMs = calisSureMs - gecen;
  float oran = (float)kalanMs / (float)calisSureMs;

  uint16_t renk;
  if (!calisAktif)      renk = C_SOLUK;
  else if (oran > 0.25f) renk = C_YESIL;
  else if (oran > 0.10f) renk = C_TURUNCU;
  else                   renk = C_KIRMIZI;

  halka(140, 122, 84, 71, oran, renk, C_KART);

  uint32_t sn = (kalanMs + 999) / 1000;
  char b[8];
  sprintf(b, "%02d:%02d", (int)(sn / 60), (int)(sn % 60));
  if (calisAktif) ortala(140, 98, "CALISIYOR", F9, 1, C_YESIL);
  else            ortala(140, 98, "DURAKLADI", F9B, 1, C_TURUNCU);
  ortala(140, 140, b, F24B, 1, calisAktif ? C_METIN : C_SOLUK);
  if (calisAktif) ortala(140, 164, "BAS: IPTAL", F9, 1, C_SOLUK);
  else            ortala(140, 164, "SWITCH'E BAS", F9B, 1, C_SARI);

  char f[16];
  if (FAN_AKTIF) sprintf(f, "FAN %d", fanRpm);
  else           sprintf(f, "FAN OFF");
  yazi(10, 232, f, F9, 1, C_ACIK);
  ortala(140, 232, BED_AD[bedHiz], F9, 1, C_SOLUK);
  const char* v = valfAyar ? "VALF ON" : "VALF OFF";
  yazi(270 - yaziGenislik(v, F9, 1), 232, v, F9, 1, valfAyar ? C_YESIL : C_SOLUK);
}

void cizBitti()
{
  kanvas.fillScreen(C_BG);
  kanvas.fillCircle(140, 78, 46, bittiRenk);
  if (bittiIkon == 0) {                       // tik
    for (int k = -2; k <= 2; k++) {
      kanvas.drawLine(120, 78 + k, 134, 92 + k, C_BG);
      kanvas.drawLine(134, 92 + k, 162, 62 + k, C_BG);
    }
  } else if (bittiIkon == 1) {                // duraklat
    kanvas.fillRoundRect(122, 58, 12, 40, 3, C_BG);
    kanvas.fillRoundRect(146, 58, 12, 40, 3, C_BG);
  } else {                                    // uyarı
    kanvas.fillRoundRect(136, 54, 8, 28, 3, C_BG);
    kanvas.fillCircle(140, 92, 5, C_BG);
  }
  ortala(140, 176, bittiMesaj, F24B, 1, C_METIN);
  ortala(140, 214, "BAS: TAMAM", F9, 1, C_SOLUK);
}

// ----- liste yapısı -----
struct Satir { const char* ad; char deger[16]; int8_t anahtar; };
Satir sat[6];

void satirAyarla(int i, const char* ad, const char* deger, int8_t anahtar)
{
  sat[i].ad = ad;
  strncpy(sat[i].deger, deger, 15);
  sat[i].deger[15] = 0;
  sat[i].anahtar = anahtar;
}

void listeCiz(int adet, int y0, int h, int pitch, int secili)
{
  for (int i = 0; i < adet; i++) {
    int y = y0 + i * pitch;
    bool s = (i == secili);
    kanvas.fillRoundRect(8, y, 264, h, 10, s ? C_ACIK : C_KART);
    yazi(20, y + h / 2 + 6, sat[i].ad, s ? F9B : F9, 1, C_METIN);

    if (sat[i].anahtar >= 0) {
      bool on = (sat[i].anahtar == 1);
      int sx = 272 - 12 - 40, sy = y + h / 2 - 10;
      kanvas.fillRoundRect(sx, sy, 40, 20, 10, on ? C_YESIL : C_CIZGI);
      kanvas.fillCircle(on ? sx + 30 : sx + 10, sy + 10, 7, C_METIN);
    } else if (sat[i].deger[0]) {
      int w = yaziGenislik(sat[i].deger, F9B, 1);
      yazi(272 - 14 - w, y + h / 2 + 6, sat[i].deger, F9B, 1, s ? C_SARI : C_SOLUK);
    }
  }
}

void cizMod()
{
  kanvas.fillScreen(C_BG);
  ustBar();
  char b[16];
  for (int i = 0; i < 3; i++) {
    sprintf(b, "FAN %d%%", MOD_FAN[i]);
    satirAyarla(i, MOD_AD[i], b, -1);
  }
  listeCiz(3, 40, 44, 52, imlec);
  altYol("MENU > MODE");
}

void cizBed()
{
  kanvas.fillScreen(C_BG);
  ustBar();
  char b[16];
  for (int i = 0; i < 3; i++) {
    sprintf(b, "%d/sn", BED_HIZ[i]);
    satirAyarla(i, BED_AD[i], b, -1);
  }
  listeCiz(3, 40, 44, 52, imlec);
  altYol("MENU > SETTINGS > BED SPEED");
}

void cizAyar()
{
  kanvas.fillScreen(C_BG);
  ustBar();
  satirAyarla(0, "BED SPEED",    BED_AD[bedHiz], -1);
  satirAyarla(1, "BUZZER",       "", buzzerAcik ? 1 : 0);
  satirAyarla(2, "VALF",         "", valfAyar ? 1 : 0);
  satirAyarla(3, "SYSTEM INFO",  ">", -1);
  satirAyarla(4, "ABOUT DEVICE", ">", -1);
  satirAyarla(5, "EXIT",         "", -1);
  listeCiz(6, 34, 28, 31, ayarImlec);
  altYol("MENU > SETTINGS");
}

void bilgiSatir(int i, const char* ad, const char* deger, uint16_t renk)
{
  int y = 58 + i * 28;
  yazi(14, y, ad, F9, 1, C_SOLUK);
  int w = yaziGenislik(deger, F9B, 1);
  yazi(266 - w, y, deger, F9B, 1, renk);
  kanvas.drawFastHLine(14, y + 9, 252, C_CIZGI);
}

void cizInfo()
{
  kanvas.fillScreen(C_BG);
  ustBar();
  char b[24];

  if (FAN_AKTIF) sprintf(b, "%d RPM", fanRpm); else strcpy(b, "KAPALI");
  bilgiSatir(0, "FAN", b, C_ACIK);

  bilgiSatir(1, "TMC2130", tmcOk ? "OK" : "YOK", tmcOk ? C_YESIL : C_KIRMIZI);
  bilgiSatir(2, "SWITCH", limitBasili ? "BASILI" : "BOSTA", limitBasili ? C_YESIL : C_METIN);

  uint16_t r = C_METIN;
  if (dhtHata == 0 && sicaklikGecerli) sprintf(b, "%dC  %%%d", sicaklik, nem);
  else if (dhtHata < 0) strcpy(b, "BEKLIYOR");
  else { sprintf(b, "HATA %d", dhtHata); r = C_KIRMIZI; }
  bilgiSatir(3, "SICAKLIK", b, r);

  unsigned long s = millis() / 1000;
  sprintf(b, "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
  bilgiSatir(4, "CALISMA", b, C_METIN);

  sprintf(b, "%u KB", (unsigned)(ESP.getFreeHeap() / 1024));
  bilgiSatir(5, "BOS RAM", b, C_METIN);

  altYol("MENU > SETTINGS > SYSTEM INFO");
}

void cizAbout()
{
  kanvas.fillScreen(C_BG);
  ustBar();
  ortala(140, 84, "KURLEME", F9B, 2, C_METIN);
  ortala(140, 116, "MAKINESI", F9B, 2, C_METIN);
  ortala(140, 148, "v0.0.2", F9B, 1, C_YESIL);
  ortala(140, 172, "Serialprint", F9, 1, C_ACIK);
  ortala(140, 194, "ESP32-S2 + TMC2130", F9, 1, C_SOLUK);
  altYol("MENU > SETTINGS > ABOUT DEVICE");
}

void ciz()
{
  switch (ekran) {
    case E_ANA:   cizAna();   break;
    case E_SURE:  cizSure();  break;
    case E_CALIS: cizCalis(); break;
    case E_BITTI: cizBitti(); break;
    case E_MOD:   cizMod();   break;
    case E_AYAR:  cizAyar();  break;
    case E_BED:   cizBed();   break;
    case E_INFO:  cizInfo();  break;
    case E_ABOUT: cizAbout(); break;
  }
  tft.drawRGBBitmap(0, 0, kanvas.getBuffer(), 280, 240);
}

// ================= KONTROL =================
void uyari(const char* m)
{
  uyariMetni = m;
  uyariBitis = millis() + 2000;
  cizimIstek = true;
}

void baslat()
{
  tmcBaslat();
  calisSureMs = (uint32_t)sureDk * 60000UL;
  calisGecenMs = 0;
  sonTick = millis();
  sonSaniye = -1;
  calisAktif = false;          // switch basılınca başlayacak
  hiz = 0;
  motorGuncelle = true;
  ekran = E_CALIS;
  bip(1, 200);
}

void bitir(const char* m, uint16_t renk, int ikon, int adet, int sure)
{
  calisAktif = false;
  hiz = 0;
  motorGuncelle = true;
  fanSogumaBitis = millis() + 15000;
  bittiMesaj = m;
  bittiRenk = renk;
  bittiIkon = ikon;
  bittiZaman = millis();
  ekran = E_BITTI;
  cizimIstek = true;
  bip(adet, sure);
}

void tikIsle(int tik)
{
  tik *= ENC_YON;
  switch (ekran) {
    case E_ANA:  anaImlec = ((anaImlec + tik) % 3 + 3) % 3; break;
    case E_SURE: sureDk = constrain(sureDk + tik, 1, 99);   break;
    case E_MOD:
    case E_BED:  imlec = ((imlec + tik) % 3 + 3) % 3;       break;
    case E_AYAR: ayarImlec = ((ayarImlec + tik) % 6 + 6) % 6; break;
    default: return;
  }
  cizimIstek = true;
}

void kisaBasis()
{
  bip(1, 40);
  switch (ekran) {
    case E_ANA:
      if (anaImlec == 0)      ekran = E_SURE;
      else if (anaImlec == 1) { ekran = E_AYAR; ayarImlec = 0; }
      else                    { ekran = E_MOD; imlec = mod; }
      break;
    case E_SURE:  baslat(); break;
    case E_CALIS: bitir("IPTAL", C_TURUNCU, 1, 1, 200); break;
    case E_BITTI: ekran = E_ANA; break;
    case E_MOD:   mod = imlec; ekran = E_ANA; anaImlec = 2; break;
    case E_AYAR:
      if (ayarImlec == 0)      { ekran = E_BED; imlec = bedHiz; }
      else if (ayarImlec == 1) { buzzerAcik = !buzzerAcik; bip(1, 120); }
      else if (ayarImlec == 2) { valfAyar = !valfAyar; }
      else if (ayarImlec == 3) ekran = E_INFO;
      else if (ayarImlec == 4) ekran = E_ABOUT;
      else                     { ekran = E_ANA; anaImlec = 1; }
      break;
    case E_BED:   bedHiz = imlec; ekran = E_AYAR; break;
    case E_INFO:
    case E_ABOUT: ekran = E_AYAR; break;
  }
  cizimIstek = true;
}

void uzunBasis()   // 1 sn basılı tut = geri
{
  bip(1, 40);
  switch (ekran) {
    case E_SURE:
    case E_MOD:
    case E_AYAR:  ekran = E_ANA; break;
    case E_BED:
    case E_INFO:
    case E_ABOUT: ekran = E_AYAR; break;
    case E_CALIS: bitir("IPTAL", C_TURUNCU, 1, 1, 200); break;
    case E_BITTI: ekran = E_ANA; break;
    default: break;
  }
  cizimIstek = true;
}

// ================= SETUP =================
void setup()
{
  pinMode(TFT_CS, OUTPUT);  digitalWrite(TFT_CS, HIGH);
  pinMode(TMC_CS, OUTPUT);  digitalWrite(TMC_CS, HIGH);

  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);

  pinMode(VALF_PIN, OUTPUT);
  digitalWrite(VALF_PIN, !VALF_ACIK);

  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT,  INPUT_PULLUP);
  pinMode(ENC_SW,  INPUT_PULLUP);
  pinMode(LIMIT_PIN, INPUT);

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN,  OUTPUT);
  pinMode(EN_PIN,   OUTPUT);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(EN_PIN, HIGH);

#if BUZZER_TIP == 1
  ledcAttach(BUZZER_PIN, 2700, 8);
  ledcWrite(BUZZER_PIN, 0);
#else
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#endif

  stepTimer = timerBegin(1000000);
  timerAttachInterrupt(stepTimer, &stepISR);
  timerStop(stepTimer);

  SPI.begin(TFT_SCLK, TMC_MISO, TFT_MOSI, TFT_CS);
  tft.init(240, 280, SPI_MODE3);       // gri/boş gelirse SPI_MODE0 dene
  tft.setSPISpeed(20000000);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  if (!kanvas.getBuffer()) {
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(20, 100);
    tft.print("RAM YETERSIZ");
    while (1) delay(1000);
  }

  tmcBaslat();

  led.begin();
  led.setBrightness(80);
  renkler[0]  = led.Color(255, 0, 0);
  renkler[1]  = led.Color(255, 90, 0);
  renkler[2]  = led.Color(255, 200, 0);
  renkler[3]  = led.Color(150, 255, 0);
  renkler[4]  = led.Color(0, 255, 0);
  renkler[5]  = led.Color(0, 255, 100);
  renkler[6]  = led.Color(0, 255, 220);
  renkler[7]  = led.Color(0, 150, 255);
  renkler[8]  = led.Color(0, 0, 255);
  renkler[9]  = led.Color(80, 0, 255);
  renkler[10] = led.Color(180, 0, 255);
  renkler[11] = led.Color(255, 0, 255);
  renkler[12] = led.Color(255, 0, 120);
  renkler[13] = led.Color(255, 105, 180);

  attachInterrupt(digitalPinToInterrupt(ENC_CLK), encISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DT),  encISR, CHANGE);

#if FAN_AKTIF
  ledcAttach(FAN_PWM_PIN, FAN_FREKANS, 8);
  ledcWrite(FAN_PWM_PIN, 0);
  pinMode(FAN_RPM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FAN_RPM_PIN), tachISR, FALLING);
#endif
  sonRpm = millis();

#if DHT_AKTIF
  delay(1200);              // DHT11 açıldıktan sonra hazır olsun
  sicaklikOku();
  sonDht = millis();
#endif

  bip(1, 120);              // açılış bip'i (buzzer testi)
}

// ================= LOOP =================
void loop()
{
  unsigned long simdi = millis();

  // --- Fan devri ---
#if FAN_AKTIF
  if (simdi - sonRpm >= 1000) {
    unsigned long gecen = simdi - sonRpm;
    sonRpm = simdi;
    noInterrupts();
    uint32_t n = tachSayac;
    tachSayac = 0;
    interrupts();
    int yeni = (int)((uint64_t)n * 60000ULL / gecen / FAN_DARBE);
    if (yeni != fanRpm) {
      fanRpm = yeni;
      if (ekran == E_CALIS || ekran == E_INFO) cizimIstek = true;
    }
  }
#endif

  // --- Sıcaklık (işlem sırasında okuma) ---
#if DHT_AKTIF
  if (ekran != E_CALIS && hiz == 0 && simdi - sonDht >= 3000) {
    sonDht = simdi;
    sicaklikOku();
  }
#endif

  // --- Switch (20 ms debounce) ---
  bool okunan = (digitalRead(LIMIT_PIN) == LIMIT_BASILI);
  if (okunan != limitOnceki) { limitZaman = simdi; limitOnceki = okunan; }
  if (simdi - limitZaman > 20 && okunan != limitBasili) {
    limitBasili = okunan;
    if (ekran == E_INFO) cizimIstek = true;
  }

  // --- Encoder ---
  noInterrupts();
  int32_t a = adim;
  interrupts();
  int tik = a / ADIM_BASINA;
  if (tik != 0) {
    noInterrupts();
    adim -= tik * ADIM_BASINA;
    interrupts();
    tikIsle(tik);
  }

  // --- Encoder butonu ---
  bool buton = digitalRead(ENC_SW);
  if (oncekiButon == HIGH && buton == LOW) { basmaZamani = simdi; uzunBasma = false; }
  if (buton == LOW && !uzunBasma && simdi - basmaZamani > 1000) {
    uzunBasma = true;
    uzunBasis();
  }
  if (oncekiButon == LOW && buton == HIGH) {
    if (!uzunBasma && simdi - basmaZamani > 30) kisaBasis();
  }
  oncekiButon = buton;

  // --- İşlem ekranı: switch basılıyken çalış, bırakınca duraklat ---
  if (ekran == E_CALIS) {
    unsigned long dt = simdi - sonTick;
    sonTick = simdi;

    if (limitBasili != calisAktif) {
      calisAktif = limitBasili;
      hiz = calisAktif ? BED_HIZ[bedHiz] : 0;
      motorGuncelle = true;
      if (!calisAktif) fanSogumaBitis = simdi + 15000;
      cizimIstek = true;
      bip(1, calisAktif ? 60 : 120);
    }

    if (calisAktif) {
      calisGecenMs += dt;
      if (calisGecenMs >= calisSureMs) {
        bitir("BITTI!", C_YESIL, 0, 3, 200);
      } else {
        int sn = (calisSureMs - calisGecenMs + 999) / 1000;
        if (sn != sonSaniye) { sonSaniye = sn; cizimIstek = true; }
      }
    }
  }

  // --- Bitiş ekranı zaman aşımı ---
  if (ekran == E_BITTI && simdi - bittiZaman > 6000) { ekran = E_ANA; cizimIstek = true; }

  // --- Uyarı süresi doldu ---
  if (uyariBitis && simdi > uyariBitis) { uyariBitis = 0; cizimIstek = true; }

  // --- Info sayfası saniyede bir yenilensin ---
  if (ekran == E_INFO && simdi / 1000 != sonInfoSn) { sonInfoSn = simdi / 1000; cizimIstek = true; }

  // --- Motor ---
  if (motorGuncelle) { motoruUygula(); motorGuncelle = false; }

  // --- Valf: işlem çalışırken (switch basılıyken) ---
  bool valfIstek = (ekran == E_CALIS) && calisAktif && valfAyar;
  if (valfIstek != valfAcik) {
    valfAcik = valfIstek;
    digitalWrite(VALF_PIN, valfAcik ? VALF_ACIK : !VALF_ACIK);
    cizimIstek = true;
  }

  // --- Fan gücü (kalkışta kısa süre %100) ---
  int fanHedef = 0;
  if (ekran == E_CALIS && calisAktif) fanHedef = MOD_FAN[mod];
  else if (simdi < fanSogumaBitis)    fanHedef = FAN_SOGUTMA;
  if (fanHedef != fanYuzde) {
    if (fanYuzde <= 0 && fanHedef > 0) fanKickBitis = simdi + FAN_KICK_MS;
    fanYuzde = fanHedef;
  }
  int fanUyg = (fanYuzde > 0 && simdi < fanKickBitis) ? 100 : fanYuzde;
  if (fanUyg != fanUygulanan) { fanUygulanan = fanUyg; fanUygula(fanUyg); }

  // --- Buzzer ---
  bipIsle(simdi);

  // --- LED animasyonu ---
#if LED_AKTIF
  if (simdi - sonLed >= 200) {
    sonLed = simdi;
    for (int i = 0; i < LED_SAYISI; i++)
      led.setPixelColor(i, renkler[(i + kaydir) % RENK_SAYISI]);
    led.show();
    kaydir++;
  }
#endif

  // --- Ekranı çiz ---
  if (cizimIstek && simdi - sonCizim >= 50) {
    cizimIstek = false;
    sonCizim = simdi;
    ciz();
  }
}vvv
