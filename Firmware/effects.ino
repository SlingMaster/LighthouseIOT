/* effects.ino
   FastLED Colors
   http://fastled.io/docs/3.1/struct_c_r_g_b.html
   https://github.com/FastLED/FastLED/wiki/Pixel-reference
*/

// ============= ЭФФЕКТЫ ===============
// несколько общих переменных и буферов, которые могут использоваться в любом эффекте
#define SQRT_VARIANT sqrt3                         // выбор основной функции для вычисления квадратного корня sqrtf или sqrt3 для ускорения
uint8_t hue, hue2;                                 // постепенный сдвиг оттенка или какой-нибудь другой цикличный счётчик
uint8_t deltaHue, deltaHue2;                       // ещё пара таких же, когда нужно много
uint8_t step;                                      // какой-нибудь счётчик кадров или последовательностей операций
uint8_t pcnt;                                      // какой-то счётчик какого-то прогресса
uint8_t deltaValue;                                // просто повторно используемая переменная
float speedfactor;                                 // регулятор скорости в эффектах реального времени
float emitterX, emitterY;                          // какие-то динамичные координаты
CRGB ledsbuff[NUM_LEDS];                           // копия массива leds[] целиком
#define NUM_LAYERSMAX 2
uint8_t noise3d[NUM_LAYERSMAX][WIDTH][HEIGHT];     // двухслойная маска или хранилище свойств в размер всей матрицы
uint8_t line[WIDTH];                               // свойство пикселей в размер строки матрицы
uint8_t shiftHue[HEIGHT];                          // свойство пикселей в размер столбца матрицы
uint8_t shiftValue[HEIGHT];                        // свойство пикселей в размер столбца матрицы ещё одно
uint16_t ff_x, ff_y, ff_z;                         // большие счётчики

uint8_t noise2[2][WIDTH + 1][HEIGHT + 1];

//массивы состояния объектов, которые могут использоваться в любом эффекте
#define trackingOBJECT_MAX_COUNT                         (100U)  // максимальное количество отслеживаемых объектов (очень влияет на расход памяти)
float   trackingObjectPosX[trackingOBJECT_MAX_COUNT];
float   trackingObjectPosY[trackingOBJECT_MAX_COUNT];
float   trackingObjectSpeedX[trackingOBJECT_MAX_COUNT];
float   trackingObjectSpeedY[trackingOBJECT_MAX_COUNT];
float   trackingObjectShift[trackingOBJECT_MAX_COUNT];
uint8_t trackingObjectHue[trackingOBJECT_MAX_COUNT];
uint8_t trackingObjectState[trackingOBJECT_MAX_COUNT];
bool    trackingObjectIsShift[trackingOBJECT_MAX_COUNT];
#define enlargedOBJECT_MAX_COUNT                     (WIDTH * 2) // максимальное количество сложных отслеживаемых объектов (меньше, чем trackingOBJECT_MAX_COUNT)
uint8_t enlargedObjectNUM;                                       // используемое в эффекте количество объектов
long    enlargedObjectTime[enlargedOBJECT_MAX_COUNT];
float    liquidLampHot[enlargedOBJECT_MAX_COUNT];
float    liquidLampSpf[enlargedOBJECT_MAX_COUNT];
unsigned liquidLampMX[enlargedOBJECT_MAX_COUNT];
unsigned liquidLampSC[enlargedOBJECT_MAX_COUNT];
unsigned liquidLampTR[enlargedOBJECT_MAX_COUNT];


// стандартные функции библиотеки LEDraw от @Palpalych (для адаптаций его эффектов)
void blurScreen(fract8 blur_amount, CRGB *LEDarray = leds) {
  blur2d(LEDarray, WIDTH, HEIGHT, blur_amount);
}

void dimAll(uint8_t value, CRGB *LEDarray = leds) {
  //for (uint16_t i = 0; i < NUM_LEDS; i++) {
  //  leds[i].nscale8(value); //fadeToBlackBy
  //}
  // теперь короткий вариант
  nscale8(LEDarray, NUM_LEDS, value);
  //fadeToBlackBy(LEDarray, NUM_LEDS, 255U - value); // эквивалент
}

//константы размера матрицы вычисляется только здесь и не меняется в эффектах
const uint8_t CENTER_X_MINOR =  (WIDTH / 2) -  ((WIDTH - 1) & 0x01); // центр матрицы по ИКСУ, сдвинутый в меньшую сторону, если ширина чётная
const uint8_t CENTER_Y_MINOR = (HEIGHT / 2) - ((HEIGHT - 1) & 0x01); // центр матрицы по ИГРЕКУ, сдвинутый в меньшую сторону, если высота чётная
const uint8_t CENTER_X_MAJOR =   WIDTH / 2  + (WIDTH % 2);           // центр матрицы по ИКСУ, сдвинутый в большую сторону, если ширина чётная
const uint8_t CENTER_Y_MAJOR =  HEIGHT / 2  + (HEIGHT % 2);          // центр матрицы по ИГРЕКУ, сдвинутый в большую сторону, если высота чётная

#if defined(USE_RANDOM_SETS_IN_APP) || defined(RANDOM_SETTINGS_IN_CYCLE_MODE)
void setModeSettings(uint8_t Scale = 0U, uint8_t Speed = 0U) {
  modes[currentMode].Scale = Scale ? Scale : pgm_read_byte(&defaultSettings[currentMode][2]);
  modes[currentMode].Speed = Speed ? Speed : pgm_read_byte(&defaultSettings[currentMode][1]);
  selectedSettings = 0U;
#ifdef USE_BLYNK
  updateRemoteBlynkParams();
#endif
}
#endif //#if defined(USE_RANDOM_SETS_IN_APP) || defined(RANDOM_SETTINGS_IN_CYCLE_MODE)

// ======================================
//   Дополнительные функции рисования
// ======================================
void DrawLine(int x1, int y1, int x2, int y2, CRGB color) {
  int deltaX = abs(x2 - x1);
  int deltaY = abs(y2 - y1);
  int signX = x1 < x2 ? 1 : -1;
  int signY = y1 < y2 ? 1 : -1;
  int error = deltaX - deltaY;

  drawPixelXY(x2, y2, color);
  while (x1 != x2 || y1 != y2) {
    drawPixelXY(x1, y1, color);
    int error2 = error * 2;
    if (error2 > -deltaY) {
      error -= deltaY;
      x1 += signX;
    }
    if (error2 < deltaX) {
      error += deltaX;
      y1 += signY;
    }
  }
}

// ======================================
//по мотивам /https://gist.github.com/sutaburosu/32a203c2efa2bb584f4b846a91066583
void drawPixelXYF(float x, float y, CRGB color)  {
  //  if (x<0 || y<0) return; //не похоже, чтобы отрицательные значения хоть как-нибудь учитывались тут // зато с этой строчкой пропадает нижний ряд
  // extract the fractional parts and derive their inverses
  uint8_t xx = (x - (int)x) * 255, yy = (y - (int)y) * 255, ix = 255 - xx, iy = 255 - yy;
  // calculate the intensities for each affected pixel
#define WU_WEIGHT(a,b) ((uint8_t) (((a)*(b)+(a)+(b))>>8))
  uint8_t wu[4] = {WU_WEIGHT(ix, iy), WU_WEIGHT(xx, iy),
                   WU_WEIGHT(ix, yy), WU_WEIGHT(xx, yy)
                  };
  // multiply the intensities by the colour, and saturating-add them to the pixels
  for (uint8_t i = 0; i < 4; i++) {
    int16_t xn = x + (i & 1), yn = y + ((i >> 1) & 1);
    CRGB clr = getPixColorXY(xn, yn);
    clr.r = qadd8(clr.r, (color.r * wu[i]) >> 8);
    clr.g = qadd8(clr.g, (color.g * wu[i]) >> 8);
    clr.b = qadd8(clr.b, (color.b * wu[i]) >> 8);
    drawPixelXY(xn, yn, clr);
  }
}

// ======================================
void DrawLineF(float x1, float y1, float x2, float y2, CRGB color) {
  float deltaX = std::fabs(x2 - x1);
  float deltaY = std::fabs(y2 - y1);
  float error = deltaX - deltaY;

  float signX = x1 < x2 ? 0.5 : -0.5;
  float signY = y1 < y2 ? 0.5 : -0.5;

  while (x1 != x2 || y1 != y2) { // (true) - а я то думаю - "почему функция часто вызывает вылет по вачдогу?" А оно вон оно чё, Михалычь!
    if ((signX > 0 && x1 > x2 + signX) || (signX < 0 && x1 < x2 + signX)) break;
    if ((signY > 0 && y1 > y2 + signY) || (signY < 0 && y1 < y2 + signY)) break;
    drawPixelXYF(x1, y1, color); // интересно, почему тут было обычное drawPixelXY() ???
    float error2 = error;
    if (error2 > -deltaY) {
      error -= deltaY;
      x1 += signX;
    }
    if (error2 < deltaX) {
      error += deltaX;
      y1 += signY;
    }
  }
}

/* kostyamat добавил функция уменьшения яркости */
CRGB makeDarker( const CRGB& color, fract8 howMuchDarker) {
  CRGB newcolor = color;
  //newcolor.nscale8( 255 - howMuchDarker);
  newcolor.fadeToBlackBy(howMuchDarker);//эквивалент
  return newcolor;
}

// ======================================
void drawCircleF(float x0, float y0, float radius, CRGB color) {
  float x = 0, y = radius, error = 0;
  float delta = 1. - 2. * radius;
  while (y >= 0) {
    drawPixelXYF(fmod(x0 + x + WIDTH, WIDTH), y0 + y, color); // сделал, чтобы круги были бесшовными по оси х
    drawPixelXYF(fmod(x0 + x + WIDTH, WIDTH), y0 - y, color);
    drawPixelXYF(fmod(x0 - x + WIDTH, WIDTH), y0 + y, color);
    drawPixelXYF(fmod(x0 - x + WIDTH, WIDTH), y0 - y, color);
    error = 2. * (delta + y) - 1.;
    if (delta < 0 && error <= 0) {
      ++x;
      delta += 2. * x + 1.;
      continue;
    }
    error = 2. * (delta - x) - 1.;
    if (delta > 0 && error > 0) {
      --y;
      delta += 1. - 2. * y;
      continue;
    }
    ++x;
    delta += 2. * (x - y);
    --y;
  }
}


// палитра для типа реалистичного водопада (если ползунок Масштаб выставить на 100)
extern const TProgmemRGBPalette16 WaterfallColors_p FL_PROGMEM = {0x000000, 0x060707, 0x101110, 0x151717, 0x1C1D22, 0x242A28, 0x363B3A, 0x313634, 0x505552, 0x6B6C70, 0x98A4A1, 0xC1C2C1, 0xCACECF, 0xCDDEDD, 0xDEDFE0, 0xB2BAB9};

// добавлено изменение текущей палитры (используется во многих эффектах ниже для бегунка Масштаб)
const TProgmemRGBPalette16 *palette_arr[] = {
  &PartyColors_p,
  &OceanColors_p,
  &LavaColors_p,
  &HeatColors_p,
  &WaterfallColors_p,
  &CloudColors_p,
  &ForestColors_p,
  &RainbowColors_p,
  &RainbowStripeColors_p
};
const TProgmemRGBPalette16 *curPalette = palette_arr[0];
void setCurrentPalette() {
  if (modes[currentMode].Scale > 100U) modes[currentMode].Scale = 100U; // чтобы не было проблем при прошивке без очистки памяти
  curPalette = palette_arr[(uint8_t)(modes[currentMode].Scale / 100.0F * ((sizeof(palette_arr) / sizeof(TProgmemRGBPalette16 *)) - 0.01F))];
}
// при таком количестве палитр (9шт) каждый диапазон Масштаба (от 1 до 100) можно разбить на участки по 11 значений
// значения от 0 до 10 = ((modes[currentMode].Scale - 1U) % 11U)
// значения от 1 до 11 = ((modes[currentMode].Scale - 1U) % 11U + 1U)
// а 100е значение Масштаба можно использовать для белого цвета


// дополнительные палитры для пламени
// для записи в PROGMEM преобразовывал из 4 цветов в 16 на сайте https://colordesigner.io/gradient-generator, но не уверен, что это эквивалент CRGBPalette16()
// значения цветовых констант тут: https://github.com/FastLED/FastLED/wiki/Pixel-reference
extern const TProgmemRGBPalette16 WoodFireColors_p FL_PROGMEM = {CRGB::Black, 0x330e00, 0x661c00, 0x992900, 0xcc3700, CRGB::OrangeRed, 0xff5800, 0xff6b00, 0xff7f00, 0xff9200, CRGB::Orange, 0xffaf00, 0xffb900, 0xffc300, 0xffcd00, CRGB::Gold};             //* Orange
extern const TProgmemRGBPalette16 NormalFire_p FL_PROGMEM = {CRGB::Black, 0x330000, 0x660000, 0x990000, 0xcc0000, CRGB::Red, 0xff0c00, 0xff1800, 0xff2400, 0xff3000, 0xff3c00, 0xff4800, 0xff5400, 0xff6000, 0xff6c00, 0xff7800};                             // пытаюсь сделать что-то более приличное
extern const TProgmemRGBPalette16 NormalFire2_p FL_PROGMEM = {CRGB::Black, 0x560000, 0x6b0000, 0x820000, 0x9a0011, CRGB::FireBrick, 0xc22520, 0xd12a1c, 0xe12f17, 0xf0350f, 0xff3c00, 0xff6400, 0xff8300, 0xffa000, 0xffba00, 0xffd400};                      // пытаюсь сделать что-то более приличное
extern const TProgmemRGBPalette16 LithiumFireColors_p FL_PROGMEM = {CRGB::Black, 0x240707, 0x470e0e, 0x6b1414, 0x8e1b1b, CRGB::FireBrick, 0xc14244, 0xd16166, 0xe08187, 0xf0a0a9, CRGB::Pink, 0xff9ec0, 0xff7bb5, 0xff59a9, 0xff369e, CRGB::DeepPink};        //* Red
extern const TProgmemRGBPalette16 SodiumFireColors_p FL_PROGMEM = {CRGB::Black, 0x332100, 0x664200, 0x996300, 0xcc8400, CRGB::Orange, 0xffaf00, 0xffb900, 0xffc300, 0xffcd00, CRGB::Gold, 0xf8cd06, 0xf0c30d, 0xe9b913, 0xe1af1a, CRGB::Goldenrod};           //* Yellow
extern const TProgmemRGBPalette16 CopperFireColors_p FL_PROGMEM = {CRGB::Black, 0x001a00, 0x003300, 0x004d00, 0x006600, CRGB::Green, 0x239909, 0x45b313, 0x68cc1c, 0x8ae626, CRGB::GreenYellow, 0x94f530, 0x7ceb30, 0x63e131, 0x4bd731, CRGB::LimeGreen};     //* Green
extern const TProgmemRGBPalette16 AlcoholFireColors_p FL_PROGMEM = {CRGB::Black, 0x000033, 0x000066, 0x000099, 0x0000cc, CRGB::Blue, 0x0026ff, 0x004cff, 0x0073ff, 0x0099ff, CRGB::DeepSkyBlue, 0x1bc2fe, 0x36c5fd, 0x51c8fc, 0x6ccbfb, CRGB::LightSkyBlue};  //* Blue
extern const TProgmemRGBPalette16 RubidiumFireColors_p FL_PROGMEM = {CRGB::Black, 0x0f001a, 0x1e0034, 0x2d004e, 0x3c0068, CRGB::Indigo, CRGB::Indigo, CRGB::Indigo, CRGB::Indigo, CRGB::Indigo, CRGB::Indigo, 0x3c0084, 0x2d0086, 0x1e0087, 0x0f0089, CRGB::DarkBlue};        //* Indigo
extern const TProgmemRGBPalette16 PotassiumFireColors_p FL_PROGMEM = {CRGB::Black, 0x0f001a, 0x1e0034, 0x2d004e, 0x3c0068, CRGB::Indigo, 0x591694, 0x682da6, 0x7643b7, 0x855ac9, CRGB::MediumPurple, 0xa95ecd, 0xbe4bbe, 0xd439b0, 0xe926a1, CRGB::DeepPink}; //* Violet
const TProgmemRGBPalette16 *firePalettes[] = {
  //    &HeatColors_p, // эта палитра уже есть в основном наборе. если в эффекте подключены оба набора палитр, тогда копия не нужна
  &WoodFireColors_p,
  &NormalFire_p,
  &NormalFire2_p,
  &LithiumFireColors_p,
  &SodiumFireColors_p,
  &CopperFireColors_p,
  &AlcoholFireColors_p,
  &RubidiumFireColors_p,
  &PotassiumFireColors_p
};

// ======================================
// espModeStat default lamp start effect
// ======================================
void  espModeState(uint8_t color) {
  fillAll(CRGB::Green);
  delay(2);
  FastLED.show();
  delay(2000);
  return;


  if (loadingFlag) {
    loadingFlag = false;
    fillAll(CRGB::Green);
    delay(2);
    FastLED.show();
    delay(2000);
    pcnt = 1;
  }
  dimAll(10);
  if (pcnt > 0 & pcnt < 200) {
    if (pcnt != 0) {
      pcnt++;
    }
  }

  // clear led lamp ---------------
  if ( pcnt >= 100) {
    pcnt = 0;
    FastLED.clear();
    delay(2);
    FastLED.show();
    loadingFlag = false;
  }
  return;


  uint8_t i = 0;
  if (loadingFlag) {
    loadingFlag = false;
    step = deltaValue;
    deltaValue = 1;
    hue2 = 0;
    deltaHue2 = 1;
    DrawLine(0, 0, WIDTH, 0, CHSV(color, 255, 255));
    //    DrawLine(CENTER_X_MINOR, CENTER_Y_MINOR, CENTER_X_MAJOR + 1, CENTER_Y_MINOR, CHSV(color, 255, 210));
    //    DrawLine(CENTER_X_MINOR, CENTER_Y_MINOR - 1, CENTER_X_MAJOR + 1, CENTER_Y_MINOR - 1, CHSV(color, 255, 210));
    // setModeSettings(128U, 128U);
    pcnt = 1;
    // FastLED.clear();
  }
  if (pcnt > 0 & pcnt < 200) {
    if (pcnt != 0) {
      pcnt++;
    }

    // animation esp state ===========
    dimAll(108);

    while (i > 10)   {
      delay(150);
      if (i % 2) {
        DrawLine(0, 0, WIDTH, 0, CHSV(color, 255, 255));
      } else {
        DrawLine(0, 1, WIDTH, 1, CHSV(color, 255, 255));
      }
      delay(2);
      FastLED.show();
      i++;

    }
    return;
    //    if (step % 2 == 0) {
    uint8_t w = validMinMax(hue2, 0, floor(WIDTH / 2) - 1);
    // float w = 0.5; // validMinMax(hue2, 0, CENTER_X_MINOR - (WIDTH % 2)*2);
    uint8_t posY = validMinMax(CENTER_Y_MINOR + deltaHue2, 0, HEIGHT - 1);
    DrawLine(CENTER_X_MINOR - w, posY, CENTER_X_MAJOR + w, posY, CHSV(color, 255, (210 - deltaHue2)));
    posY = validMinMax(CENTER_Y_MINOR - 1 - deltaHue2, 1, HEIGHT - 1);
    DrawLine(CENTER_X_MINOR - w, posY, CENTER_X_MAJOR + w, posY, CHSV(color, 255, (210 - deltaHue2)));

    if (deltaHue2 == 0) {
      deltaHue2 = 1;
    }
    hue2++;
    deltaHue2 = deltaHue2 << 1;
    if (deltaHue2 == 2) {
      deltaHue2 = deltaHue2 << 1;
    }
    if (CENTER_Y_MINOR + deltaHue2 > HEIGHT) {
      deltaHue2 = 0;
      hue2 = 0;
    }
    // LOG.printf_P(PSTR("espModeState | pcnt = %05d | deltaHue2 = %03d | step %03d | ONflag • %s\n"), pcnt, deltaHue2, step, (ONflag ? "TRUE" : "FALSE"));
  } else {

#ifdef USE_NTP
    // error ntp ------------------
    color = 255;        // если при включенном NTP время не получено, будем красным цветом мигать
#else
    color = 176U;       // иначе скромно синим - нормальная ситуация при отсутствии NTP
#endif //USE_NTP
    // animtion no time -----------
    leds[XY(CENTER_X_MINOR , 0U)] = CHSV( color, 255, (step % 4 == 0) ? 200 : 128);

  }
  // clear led lamp ---------------
  if ( pcnt >= 100) {
    pcnt = 0;
    //    FastLED.clear();
    //    FastLED.delay(2);
    FastLED.clear();
    delay(2);
    FastLED.show();
    loadingFlag = false;
  }
  step++;
}


// --------------------------------------
void Connect(byte color) {
  static byte y;
  static bool direct;
#ifdef GENERAL_DEBUG
  LOG.printf_P(PSTR("• Connect | espMode • %d | %d \n\r"), espMode, y);
#endif
  if (loadingFlag) {
    direct = 0;
    loadingFlag = false;
    y = 5;
    FastLED.clear();
    FPSdelay = 1; //LOW_DELAY;
    pcnt = 1;
  }

  if (pcnt > 0 & pcnt < 200) {
    if (pcnt != 0) {
      pcnt++;
    }

    if (direct == 0U) {
      y++;
    } else {
      y--;
    }
    if (y >= HEIGHT - 1) {
      direct = 1;
    }
    if (y == 255) {
      y = 0;
      direct = 0;
    }

    fadeToBlackBy(leds, NUM_LEDS, 50);

    for (uint16_t x = 0; x < WIDTH; x++) {
      leds[XY(x, y)] = CHSV(color, 255U, 255U); // CRGB::Green;
      if ((x == y / 2.0) & (y % 2U == 0U)) {
        drawPixelXYF(random(WIDTH) - 1.5, y / 0.9, CRGB::PaleGreen );
      }
    }
    FastLED.show();
  }
}





//---------------------------------------
bool isJavelinMode() {
  if (eff_valid == CMD_NEXT_EFF) {
    currentMode = MODE_AMOUNT - 1;
  }
  if (eff_valid == 0) {
    currentMode = 81;
  }
  return !dawnFlag;
}

//---------------------------------------
// Global Function
//---------------------------------------
void drawRec(uint8_t startX, uint8_t startY, uint8_t endX, uint8_t endY, uint32_t color) {
  for (uint8_t y = startY; y < endY; y++) {
    for (uint8_t x = startX; x < endX; x++) {
      drawPixelXY(x, y, color);
    }
  }
}

//---------------------------------------
void drawRecCHSV(uint8_t startX, uint8_t startY, uint8_t endX, uint8_t endY, CHSV color) {
  for (uint8_t y = startY; y < endY; y++) {
    for (uint8_t x = startX; x < endX; x++) {
      drawPixelXY(x, y, color);
    }
  }
}

//--------------------------------------
uint8_t validMinMax(float val, uint8_t minV, uint32_t maxV) {
  uint8_t result;
  if (val <= minV) {
    result = minV;
  } else if (val >= maxV) {
    result = maxV;
  } else {
    result = ceil(val);
  }
  //  LOG.printf_P(PSTR( "result: %f | val: %f \n\r"), result, val);
  return result;
}

//--------------------------------------
void gradientHorizontal(int startX, int startY, int endX, int endY, uint8_t start_color, uint8_t end_color, uint8_t start_br, uint8_t end_br, uint8_t saturate) {
  static float step_color = 1.;
  static float step_br = 1.;
  static int color = start_color;
  static int br = start_br;
  uint8_t temp_step;
  if ((startX < 0) | ( startY < 0)) {
    return;
  }
  if (startX == endX) {
    if (startX >= WIDTH) {
      startX = WIDTH - 1 ;
    } else {
      endX++;
    }
  }
  if (startY == endY) {
    if (startY >= HEIGHT) {
      startY = HEIGHT - 1 ;
    } else {
      endY++;
    }
  }
  temp_step = abs(startX - endX);
  if (temp_step == 0) temp_step = 1;
  step_color = float(abs(end_color - start_color) / temp_step);
  step_br = float(abs(end_br - start_br) / temp_step);

  if (start_color > end_color) {
    step_color = -1. * step_color;
    color = end_color;
  } else {
    color = start_color;
  }
  br = start_br;
  if (start_br > end_br) {
    step_br = -1. * step_br;
  }

  for (uint8_t x = startX; x < endX; x++) {
    CHSV thisColor = CHSV(color + floor((x - startX) * step_color), saturate, br + floor((x - startX) * step_br));
    for (uint8_t y = startY; y < endY; y++) {
      leds[XY(x, y)] = thisColor;
    }
  }
}

//--------------------------------------
void gradientVertical(int startX, int startY, int endX, int endY, uint8_t start_color, uint8_t end_color, uint8_t start_br, uint8_t end_br, uint8_t saturate) {
  static float step_color = 1.;
  static float step_br = 1.;
  static int color = start_color;
  static int br = start_br;
  uint8_t temp_step;

  if ( (startX < 0) | ( startY < 0) | (startX > (WIDTH - 1)) | ( startY > (HEIGHT - 1))) {
    return;
  }
  if (startX == endX) {
    if (startX >= WIDTH) {
      startX = WIDTH - 1 ;
    } else {
      endX++;
    }
  }
  if (startY == endY) {
    if (startY >= HEIGHT) {
      startY = HEIGHT - 1 ;
    } else {
      endY++;
    }
  }
  temp_step = abs(startY - endY);
  if (temp_step == 0) temp_step = 1;

  step_color = float(abs(end_color - start_color) / temp_step);
  step_br = float(abs(end_br - start_br) / temp_step);

  if (start_color > end_color) {
    step_color = -1. * step_color;
    color = end_color;
  } else {
    color = start_color;
  }
  br = start_br;
  if (start_br > end_br) {
    step_br = -1. * step_br;
  }

  for (uint8_t y = startY; y < endY; y++) {
    CHSV thisColor = CHSV(color + floor((y - startY) * step_color), saturate, br + floor((y - startY) * step_br));
    for (uint8_t x = startX; x < endX; x++) {
      leds[XY(x, y)] = thisColor;
    }
  }
}

CRGBPalette16 currentPalette(PartyColors_p);
bool br_down;
byte last_time;
// =====================================
void ShowModeESP(CRGB color) {
  for (uint16_t i = 0; i < HEIGHT; i++) {
    DrawLine(0, i, WIDTH, i, color);
    FastLED.delay(400);
  }
  delay(1000);
  for (uint16_t i = HEIGHT; i > 0; i--) {
    DrawLine(0, i - 1, WIDTH, i - 1, CRGB::Black);
    FastLED.delay(400);
  }
  delay(250);
}

// =====================================
//            Light Effects
// =====================================

// ======= Lighthouse Projector ========
void LighthouseProjector(byte light) {
  const float STEP = 0.1;
  const float LW = 11 - STEP;
  const float MAX_BR = 254;
  const byte LIGHT_DEGRESS = 4;
  byte sat = 240U;
  byte delta;
  byte br;
  static byte level;

  switch (light) {
    case 0:   /* light off */
      FastLED.clear();
      return;
    case 1:  /* light on */
      delta = MAX_BR * (1 - (emitterX - floor(emitterX)));
      break;
    case 2:/* light on fog*/
      sat = 80U;
      delta = MAX_BR / LIGHT_DEGRESS * (1 - (emitterX - floor(emitterX)));
      break;

    case 5: /* meter */
      //      sat = 80U;
      //      delta = MAX_BR / LIGHT_DEGRESS * (1 - (emitterX - floor(emitterX)));

      level = random(7);
      if (step % 8) hue++;

      for (uint8_t y = 0U; y < HEIGHT; y++) {
        for (uint8_t x = 0U; x < WIDTH; x++) {
          /* draw */
          if (isSoundPlay) {
            // delta = abs(level - x) * 16;
            br = ((x < level) | (x > WIDTH - level))  ? 0 : 235;
          } else {
            br = 0;
          }
          drawPixelXY(x, y, CHSV(hue, 255, br));
          drawPixelXY(x, y, CHSV(hue, 255, br));
        }
      }
      return;
  }

  // LOG.printf_P(PSTR("X • %d | delta: %02d \n\r"), (byte) emitterX, delta);
  br = (light == 2U) ? MAX_BR / LIGHT_DEGRESS : MAX_BR;
  for (uint8_t y = 0U; y < HEIGHT; y++) {
    /* draw */
    drawPixelXY(emitterX, y, CHSV(40, sat, delta));
    drawPixelXY(emitterX + 1, y, CHSV(40, (light == 2U) ? 32U : 192U, br));
    /* clear */
    if (light == 1) {
      drawPixelXY(emitterX + 4, y, CHSV(40, sat, MAX_BR - delta));
    } else {
      drawPixelXY(emitterX + 4, y, CHSV(40, sat, MAX_BR / LIGHT_DEGRESS - delta));
    }

    drawPixelXY(emitterX + 5, y, CRGB::Black);

    if (emitterX > 6) {
      drawPixelXY(emitterX - 6, y,  CRGB::Black);
    }
  }
  /* ------------------------------ */
  emitterX -= STEP;
  if (emitterX < 0) emitterX = LW;
}

// =====================================
void TowerLight(bool tower_light) {
  PlaySoundAdvert(2, 80);
  if (tower_light)  delay(10);
  digitalWrite(TOWER_LIGHT_PIN, (tower_light & ONflag));
}

// ======================================
void Sky(byte sky_type) {
  static uint8_t br;
  byte max_br;
  CRGB sky_color;

  if (!ONflag) sky_type = 10;

  if (br_down) {
    if (br > 0) br--;
    if (getPixColor(25) > 0) {
      for (uint8_t i = 0U; i < 8; i++)  {
        leds[24 + i].nscale8(br); // fade to black
      }
    } else {
      br = 0;
      br_down = false;
    }
    return;
  } else {
    if (currentMode == MODE_AMOUNT - 1) {
      byte id_group = floor(weather_id / 100);
      max_br = ((clouds < 5) & (clouds != 0)) ? 250 / clouds : 250U;
      if (tod == 0) max_br = max_br / 2;
      if (sky_type != 8) sky_type = tod;

    } else {
      max_br = (sky_type > 0U) ? 250 : 160;
    }
    if (br < max_br) br++;
  }

  switch (sky_type) {
    case 0:   /* night */
      for (uint8_t i = 0U; i < 8; i++) {
        leds[24 + i] = CHSV(150 + i * 2, 255U, br);
      }
      return;
    case 1:   /* sunrise */
      for (uint8_t i = 0U; i < 8; i++) {
        leds[24 + i] = CHSV(56 - i * 8, 255U, br);
      }
      return;
    case 2:   /* day */
      for (uint8_t i = 0U; i < 8; i++) {
        /* test */ // clouds = 4;
        // leds[24 + i] = CHSV(128 + i * 6, 255U, br);
        leds[24 + i] = CHSV(140 + i * 3, 180 + i * (clouds + 1) * 2, br);
      }
      return;
    case 3:   /* sunset */
      for (uint8_t i = 0U; i < 8; i++) {
        leds[24 + i] = CHSV(140 + i * 10, 255U, br);
      }
      return;

    case 5:   /* fog*/
      sky_color = CHSV(136, 15U, br / 3);
      break;

    case 7:   /* rainbow */
      leds[24] = CRGB::Black;
      for (uint8_t i = 0U; i < 7; i++) {
        leds[25 + i] = CHSV(i * 32, 255U, br);
      }
      return;

    case 8:   /* thunderstorm */
      sky_color =  CRGB::White;
      break;
    default:
      sky_color = CRGB::Black;
      break;
  }
  for (uint8_t i = 0U; i < 8; i++) {
    leds[24 + i] = sky_color;
  }
}

// ======================================
void HomeLight(byte led_type) {
  static bool flag;
  const byte BR = 128U;
  CRGB tv_color;
  CRGB light_color;
  if (!ONflag) led_type = 250;
  // if (temp < 20) id = (temp < 16) ? 14U : 15U;
  light_color = CHSV(30, 250U, light_sensor ? BR : 0U);
  switch (led_type) {
    case 1:   /* light on */
      tv_color = CRGB::Black;
      light_color = CHSV(30, 250U, BR);
      break;
    case 2:   /* tv on */
      tv_color = CHSV(160, random8(180, 255), BR);
      break;
    case 3:   /* Hearth */
      tv_color = CRGB::Black;
      break;
    case 4:   /* light & tv */
      tv_color = CHSV(160, random8(180, 255), BR);
      break;
    case 5:   /* light & Hearth */
      tv_color = CHSV(30, 250U, BR);
      break;
    case 6:   /* tv & Hearth */
      tv_color =  CHSV(160, random8(180, 255), BR);
      break;
    case 7:   /* tv & only */
      tv_color =  CHSV(160, random8(180, 255), BR);
      light_color = CRGB::Black;
      break;
    case 9:   /* ota */
      if (step % 10 == 0U) flag = !flag;
      dimAll(108);
      light_color = CHSV(240, 255U, flag ? BR : 0U);
      break;
    case 10:   /* auto */
      tv_color = CRGB::Black;
      // light_color = CHSV(45, 220U, (getSkyColorIndex() != 2 & getSkyColorIndex() != 0) ? 255 : 0);
      // light_color = CHSV(45, 220U, (getSkyColorIndex() != 2) ? 255 : 0);
      break;
    default:
      tv_color = CRGB::Black;
      light_color = CRGB::Black;
      break;
  }
  /* light or priority Hearth */
  leds[22] = (s_temp < 18) ? CHSV(random8(0, 40), 255U, 160U) : light_color;
  // leds[22] = light_color;
  /* tv */
  leds[23] = tv_color;
}

// =====================================
void SetEnvironmentState(byte light, byte h_environment, byte sky_state, bool rainbow) {
  if (projector_light == 9) {
    /* ota mode */
    for (uint8_t i = 0U; i < 24; i++) {
      leds[i] = CHSV(240U, 255U, 48 + (step % 16) * 8U );
    }
    step++;
    return;
  }

  projector_light = light;
  if (step % 4U == 0U) {
    LighthouseProjector(projector_light);

    if (light == 2) {
      Sky(5); /* fog */
    } else {
      Sky(rainbow ? 7 : (sky_state == 10U) ? getSkyColorIndex() : sky_state);
    }
  }
  // home_environment = 0;
  HomeLight(home_environment);

  if (isMusicPlay) {
    step++;
    return;
  }

  // -----------------------------------
  EVERY_N_SECONDS(1) {
    if (!advert_flag) progress++;

    if (quickIntro) {
      /* for youtube conditions */
      deltaValue++;
      if (deltaValue == 15) {
        deltaValue++;
        SetPlayerVolume(0);
      }
      if (deltaValue > 25) {
        quickIntro = false;
        scenario++;
        advert_flag = false;
        LOG.print("Quick Intro • " + String(quickIntro));
        LOG.printf(" | next scenario • %02d\n\r", scenario);
        deltaValue = 0;
      }
    }
  }

  EVERY_N_SECONDS(10) {
#ifdef GENERAL_DEBUG
    LOG.print(getTimeStr());
    LOG.printf_P(PSTR(" | SC %2d •"), scenario);
    LOG.printf_P(PSTR("%4d | AS%2d | light%2d | home %2d | sky %2d | T %2d°C | LS%2d | folder %2d | step %3d\n\r"), progress, advert_flag, projector_light, h_environment, sky_state, s_temp, light_sensor, cur_folder, step);
#endif
  }
  step++;
}

// =====================================
byte GetSoundSource() {
  byte folder = 99;
  byte id_group = floor(weather_id / 100);
  /* | 2: Thunderstorm | 3: Drizzle
     | 5: Rain         | 6: Snow
     | 7: Fog            */

  /*                         | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | */
  uint8_t const folders[9] = { 99, 99,  3, 99, 99,  2,  2,  5, 1 };
  folder = folders[id_group];
  return folder;
}

// =====================================
void InitEffect(byte volume) {
  // volume • percent 0-100% of eff_volume
  const byte MAX_BR = 200;
  if (loadingFlag) {
    loadingFlag = false;
    br_down = true;
    advert_flag = false;
    isSoundPlay = false;
    // isMusicPlay = false;
    scenario = 0;
    FPSdelay = 10;
    progress = 0;
    last_time = 255;
    cur_folder = (currentMode == (MODE_AMOUNT - 1)) ? GetSoundSource() : currentMode + 1;
    volume_cycle =  GetVolume(volume);
    digitalWrite(TOWER_LIGHT_PIN, LOW);

    if (advert_flag) {
      send_command(0x16, FEEDBACK, 0, 0); // stop adver
      delay(mp3_delay);
    }
    if (isSoundPlay) {
      send_command(0x16, FEEDBACK, 0, 0); // stop
      delay(mp3_delay);
    }
    //    if (currentMode == 12) {
    //    } else {
    PlayCycledSoundEffect(cur_folder);
    //    }
    getForecast();
    delay(500);
    LOG.printf_P(PSTR("\n\r InitEffect | %2d | advert%2d | folder%3d | step %3d\n\r"), progress, advert_flag, cur_folder, step);
  }
}

// ======================================
void RunFire(uint8_t val) {
  if (val == 2) PlaySoundAdvert(98, 20);
}

// =====================================
//            E F F E C T S
// =====================================
//               Calm
// =====================================
void Calm() {
  InitEffect(40);
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(1, 10, 10, false);

  if (progress % 100 == 0U) {
    PlaySoundAdvert(20 + random8(2), random8(1, 4)); /* Horn */
  }
}

// =====================================
//               Rain
// =====================================
void Rain() {
  InitEffect(40);
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(1, 2, 10, advert_flag);
  if (progress % 50 == 0U) {
    PlaySoundAdvert(18, random8(50, 65)); /* Windvane */
  }
  if (progress == 160) {
    PlaySoundAdvert(17, random8(25, 40)); /* AfterRain */
  }
}

// =====================================
//               Storm
// =====================================
void Storm() {
  InitEffect(40);
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(projector_light, home_environment, 10, false);

  /* scenario Storm */
  if (!advert_flag) {
    switch (scenario) {
      case 0:
        if (progress % 10U == 0U) {
          home_environment++;
          if (home_environment > 11) {
            home_environment = 0;
            scenario++;
          }
          if (home_environment == 0U) {
            projector_light = 0;
          } else {
            projector_light = 1;
          }
        }
        break;
      case 1: /* Draver */
        PlaySoundAdvert(20, 20);
        break;
      case 2: /* Steamboat Horn */
        PlaySoundAdvert(22 + random8(1), random8(10, 35));
        break;
      case 3:/* Steamboat Horn */
        PlaySoundAdvert(22 + random8(1), random8(30, 50));
        break;
      case 4: /* sos */
        if (progress == 100) PlaySoundAdvert(25, 15);
        break;
      case 5: /* morze */
        PlaySoundAdvert(24, 20);
        break;
      default: /* return */
        scenario = 0;
        break;
    }
  }
}

// =====================================
//            Thunderstorm
// =====================================
void Thunderstorm() {
  InitEffect(40);
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(1, 1, 10, false);
  //  EVERY_N_SECONDS(1) {
  //    LOG.printf_P(PSTR(" | Progress %4d | scenarios %3d | isSoundPlay %2d\n\r"), progress, scenarios, isSoundPlay);
  //  }

  switch (progress) {
    case 10:   /* light on */
      TowerLight(true);
      break;
    case 50:
      Sky(8); /* flash thunderstorm */
      PlaySoundAdvert(random8(5, 9), random8(25, 50));
      break;
    case 150:
      Sky(8); /* flash thunderstorm */
      PlaySoundAdvert(random8(5, 9), random8(30, 65));
      break;
  }
}

// =====================================
//     Fog in Odessa | Nautophone
// =====================================
void Fog() {
  // cycle volume
  InitEffect(90);
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(2, 2, 10, false);

  switch (progress) {
    case 50:
      PlaySoundAdvert(21 + random8(2), random8(30, 50));
      break;
    case 128: /* Steamboat Horn */
      PlaySoundAdvert(22, random8(20, 35));
      break;
    case 130: /* Steamboat Horn */
      PlaySoundAdvert(23, random8(30, 50));
      break;
    case 200: /* Steamboat Horn */
      PlaySoundAdvert(21 + random8(2), random8(30, 50));
      break;
  }
}

// =====================================
//             Early Morning
// =====================================
void EarlyMorning() {
  InitEffect(40);
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(1, 1, 1, false);
  if (progress == 250) {
    PlaySoundAdvert(22 + random8(1), random8(10, 50)); /* Steamboat Horn */
  }
}


// =====================================
//             Siesta
// =====================================
void Siesta() {
  InitEffect(40);
  const byte MSG_VOLUME = 30;
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(0, 255, 2, false);

  /* scenario Siesta */
  if (!advert_flag) {
    time_t currentLocalTime = getCurrentLocalTime();
    uint8_t curDay = dayOfWeek(currentLocalTime);
    switch (scenario) {
      case 0:
        projector_light = 0;
        home_environment = 0;
        scenario++;
        break;
      case 1: /* Set Radio */
        if (progress == 20) {
          PlaySoundAdvert(15, 40);
          isSoundPlay = false;
          Equalizer = 0;
        }
        break;
      case 2: /* radio msg 0 */
        PlaySoundAdvert(60, MSG_VOLUME);
        Equalizer = 1;
        break;
      case 3:  /* BoomBox*/
        PlaySoundAdvert(59, 40);
        Equalizer = 0;
        break;
      case 4: /* radio msg 1 */
        PlaySoundAdvert(61, MSG_VOLUME);
        Equalizer = 1;
        break;
      case 5:  /* StatusQuo */
        PlaySoundAdvert(58, 40);
        Equalizer = 0;
        break;
      case 6: /* radio msg 2 */
        if (curDay % 2U) {
          PlaySoundAdvert(62, MSG_VOLUME);
        } else {
          PlaySoundAdvert(64, MSG_VOLUME);
        }
        Equalizer = 1;
        break;
      case 7:  /* Nostalgie */
        if ((random8(10, 30) % 2U) ) {
          PlaySoundAdvert(50 + curDay, 40);
        } else {
          PlaySoundAdvert(random8(50, 57), 40);
        }
        Equalizer = 0;
        break;
      case 8: /* radio msg 3 */
        PlaySoundAdvert(63, MSG_VOLUME);
        isSoundPlay = true;
        Equalizer = 0;
        break;
      default:
        // PlayMusic(50);
        break;
    }
  }
}

// =====================================
//             Quiet Evening
// =====================================
void QuietEvening() {
  InitEffect(40);
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(0, 1, 3, false);
}

// =====================================
//             Evening Watch
// =====================================
void EveningWatch() {
  InitEffect(40);
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(projector_light, home_environment, 3, false);

  /* scenario Evening Watch */
  if (!advert_flag) {

    switch (scenario) {
      case 0:
        projector_light = 0;
        home_environment = 1;
        if (progress == 50U) {
          TowerLight(true);
        }
        break;
      case 1: /* Step Up */
        PlaySoundAdvert(13, 60);
        break;
      case 2: /* OnOff Lighthouse */
        PlaySoundAdvert(3, 50);
        projector_light = 1;
        // SetEnvironmentState(1, 5, 0, false);
        break;
      case 3: /* 0014_StepDown */
        PlaySoundAdvert(14, 60);
        break;
      case 4:
        TowerLight(false);
        break;
      case 5:
        PlaySoundAdvert(2, 50);
        break;
      case 6:
        home_environment = 7;
        scenario++;
        break;
      case 7:
        prevEffect();
        break;
    }
  }
}

// =====================================
//             Night Watch
// =====================================
void NightWatch() {
  InitEffect(30);
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(projector_light, home_environment, 0, false);

  /* scenario Night Watch */
  if (!advert_flag) {
    switch (scenario) {
      case 0:
        projector_light = 1;
        home_environment = 1;
        scenario++;
        break;
      case 1: /* Morze */
        if (progress == 20) {
          PlaySoundAdvert(24, 40);
        }
        break;
      case 2: /* Windvane */
        if (progress == 250) {
          PlaySoundAdvert(18, 40);
        }
        break;
      case 3: /* 0020 SteamboatHorn */
        if (progress == 150) {
          PlaySoundAdvert(random8(20, 23), random8(5, 15));
        }
        break;
      default:
        scenario = 0;
        break;
    }
  }
}

// =====================================
//            Change Of Watch
// =====================================
void ChangeOfWatch() {
  InitEffect(50);
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(projector_light, home_environment, 1, false);

  /* scenario Change Of Watch */
  if (!advert_flag) {
    switch (scenario) {
      case 0:
        projector_light = 1;
        home_environment = 1;
        if (progress == 50U) {
          TowerLight(true);
        }
        break;
      case 1: /* Step Down */
        PlaySoundAdvert(13, 60);
        break;
      case 2: /* OnOff Lighthouse */
        PlaySoundAdvert(3, 50);
        projector_light = 0;
        break;
      case 3: /* Step Down */
        PlaySoundAdvert(14, 60);
        break;
      case 4:
        TowerLight(false);
        break;
      case 5:
        PlaySoundAdvert(2, 50);
        break;
      case 6:
        home_environment = 10;
        break;
    }
  }
}

// =====================================
//         Nostalgic Caretaker
// =====================================
void NostalgicCaretaker() {
  InitEffect(20);
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(projector_light, home_environment, 3, true);

  /* scenario Nostalgic Caretaker */
  if (!advert_flag) {
    switch (scenario) {
      case 0: /* StepsOnBeach */
        GetMaxSongs(50);
        projector_light = isNight();
        pcnt = 1;
        home_environment = 250;
        PlaySoundAdvert(16, 60);
        break;
      case 1: /* On Light */
        home_environment = 1;
        PlaySoundAdvert(2, 60);
        break;
      case 2: /* Guitar */
        if (progress == 10U) {
          PlaySoundAdvert(70, 50);
        }
        break;
      case 3: /* Guitar */
        // PlaySoundAdvert(14, 60);
        PlaySoundAdvert(71, 40);
        break;
      case 4: /* Guitar */
        PlaySoundAdvert(72, 40);
        break;
      case 5: /* Guitar */
        PlaySoundAdvert(73, 40);
        break;
      case 6: /* Tape */
        if (progress == 20U) {
          PlaySoundAdvert(27, 40);
        }
        break;
      case 7: /* Play Song */
        /* folder, snd */
        PlayCurSound(50, pcnt, 40);
        scenario++;
        break;
      default:  /* Next Song */
        if (!isSoundPlay) {
          pcnt++;
          if (pcnt > songs) pcnt = 1;
          scenario = 7;
          // LOG.printf_P(PSTR(" | Nostalgic Caretaker | Next Scenario •%2d | Progress • %3d | track • %3d\n\r"), scenario, progress, pcnt);
        }
        break;
    }
  }
}

// =====================================
//            Keeper on Tipping
// =====================================
void KeeperOnTip() {
  InitEffect(15);
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(projector_light, 0, 10, true);

  /* scenario Keeper on a Tip */
  if (!advert_flag) {
    switch (scenario) {
      case 0: /* StepsOnBeach */
        GetMaxSongs(75);
        pcnt = 1;
        projector_light = 0;
        deltaHue2 = 2;
        PlaySoundAdvert(16, 60);
        break;

      case 1: /* Play Song */
        /* folder, snd */
        PlayCurSound(75, pcnt, 40);
        scenario++;
        projector_light = 5;
        digitalWrite(TOWER_LIGHT_PIN, HIGH);
        break;
      case 2:   /* Next Song */
        if (!isSoundPlay) {
          delay(250);
          digitalWrite(TOWER_LIGHT_PIN, LOW);
          pcnt++;
          if (pcnt > songs) pcnt = 1;
          scenario = 1;
          // LOG.printf_P(PSTR(" | Keeper on Tipping | Next Scenario •%2d | Progress • %3d | track • %3d\n\r"), scenario, progress, pcnt);
        }
        break;
    }
  }
}

// =====================================
//         Abandoned Lighthouse
// =====================================
void AbandonedLighthouse() {
  InitEffect(40);
  // light |  h_environment | sky_state | rainbow
  SetEnvironmentState(0, 0, 10, true);
  /* scenario */
  if (progress == 25) {
    PlaySoundAdvert(18, random8(50, 65)); /* Windvane */
  }
  if (progress == 150) {
    PlaySoundAdvert(30, random8(30, 50)); /* Cricket */
  }
  if (progress == 250) {
    // PlaySoundAdvert(16, random8(10, 40)); /* Steps */
    PlaySoundAdvert(22 + random8(1), random8(10, 40)); /* Steamboat Horn */
  }
}

// =====================================
void EffThunderstorm() {
  byte id_group = floor(weather_id / 100);
  if (!advert_flag) {
    if (id_group == 2) {
      Sky(8); /* flash thunderstorm */
      // LOG.printf("Flash Thunderstorm | weather_id %4d\n\r", weather_id);
      PlaySoundAdvert(random8(5, 9), random8(25, 50));
    } else {
      scenario++;
      // LOG.printf("Next Scenario | TOD%2d | scenario%4d\n\r", tod, scenario );
    }
  }
}

// =====================================
//              Real Time
// =====================================
void RealTime() {
  //  static byte last_time;
  InitEffect(20);
  // light | h_environment | sky_state | rainbow
  SetEnvironmentState(projector_light, home_environment, tod, false);

  if (!advert_flag) {
    /* determining the time of day*/
    if (tod != last_time) {
      last_time = tod;
      scenario = last_time * 10;

#ifdef GENERAL_DEBUG
      LOG.println("=======================");
      LOG.printf_P(PSTR("• RealTime • TOD%2d | Weather ID %3d | Next Scenario •%2d\n\r"), tod, weather_id, scenario);
#endif //GENERAL_DEBUG

    }
    // return;

    switch (scenario) {
      // • N I G H T •
      case 0:
        projector_light = 1;
        home_environment = 0;
        scenario++;
        break;
      case 1: /* Thunderstorm scenario */
        break;
      case 2: /* Morze */
        if (progress == 120) PlaySoundAdvert(24 + random8(1), random8(10, 40));
        break;
      case 3:
        if (progress == 50) PlaySoundAdvert(18, 80);
        break;
      case 4: /* return */
        scenario = 0U;
        break;

      // • S U N R I S E •
      case 10:
        projector_light = 1;
        home_environment = 1;
        if (progress == 50U) {
          TowerLight(true);
        }
        break;
      case 11: /* Step Down */
        PlaySoundAdvert(13, 60);
        break;
      case 12: /* OnOff Lighthouse */
        PlaySoundAdvert(3, 50);
        projector_light = 0;
        break;
      case 13: /* Step Down */
        PlaySoundAdvert(14, 60);
        break;
      case 14:
        TowerLight(false);
        break;
      case 15:
        home_environment = 10;
        PlaySoundAdvert(2, 50);
        break;
      case 16:
        if (progress % 25 == 0) PlaySoundAdvert(21 + random8(2), random8(10, 20));
        break;
      case 17: /* Windvane */
        if (progress % 32 == 0) PlaySoundAdvert(18, random8(20, 60));
        break;
      case 18:
        scenario = 16;
        break;

      // • D A Y •
      case 20:
        projector_light = 0;
        progress = 0;
        scenario++;
        break;
      case 21: /* Thunderstorm scenario */
        break;
      case 22: /* Steamboat Horn */
        if (progress % 120 == 0U ) PlaySoundAdvert(20 + random8(2), random8(5, 20));
        break;
      case 23:
        if (progress == 128) scenario = 20;
        break;
      case 24:
        scenario = 20;
        break;

      // • S U N S E T •
      case 30:
        projector_light = 0;
        home_environment = 1;
        if (progress == 50U) {
          TowerLight(true);
        }
        break;
      case 31: /* Step Up */
        PlaySoundAdvert(13, 60);
        break;
      case 32: /* OnOff Lighthouse */
        PlaySoundAdvert(3, 50);
        projector_light = 1;
        // SetEnvironmentState(1, 5, 0, false);
        break;
      case 33: /* 0014_StepDown */
        PlaySoundAdvert(14, 60);
        break;
      case 34:
        TowerLight(false);
        break;
      case 35:
        PlaySoundAdvert(2, 50);
        break;
      case 36:
        break;
      case 37:
        if (progress > 50U) {
          if (step % 2U) {  /* Steamboat Horn */
            home_environment = 5;
            PlaySoundAdvert(21 + random8(2), random8(10, 20));
          } else {          /* Morze or SOS */
            home_environment = 7;
            PlaySoundAdvert(24 + random8(1), random8(5, 20));
          }
        }
        break;
      case 38:
        break;
      default:
        scenario = 36;
        break;
    }
  }
  /* Thunderstorm for all scenarios */
  if (progress % 50 == 0) {
    EffThunderstorm();
  }
}

// =====================================
// END
// =====================================
