/* effectTicker.ino

  ====================================================================================================================================================
  основная причина изменения этого файла и перехода на 3 версию прошивки это изменение принципа вызова эффектов,
  чтобы существенно сократить код и максимально исключить ручной труд переложив эти функции на процессор
  массив эффектов представляет собой указатели на функции еффектов, таким способом не нужно присваивать эффектам номер,
  чтобы потом к ним обращаться в масиве они нумеруются автоматом
  [ !!! ] есть несколько ограничений :
  1. никогда не трогайте в списке эффекты «Бeлый cвeт», «Maтpицa», «Oгoнь» у них фиксированные места
    дальше можете раставлять эффекты на свое усмотрение до бегущей строки она всегда должна быть последней
  2. нужно соблюдать такую же последовательность в константе defaultSettings[][3] файл Constants.h
   и файлах effects1.json, effects2.json, effects3.json, effects4.json файлы предусматриваю поддержку 30 эффектов из списка общим числом 120
   формат записи:
   поля | n         | v
   "Название эффекта,min_скорость,max_скорость,min_масштаб,max_масштаб,выбор_ли_цвета_это(0-нет,1-да 2-совмещённый);"
   проще добавлять новые эффекты в файл effects4.json там есть свободное место это упростит добавление новых эффектов
   главное соблюдать последовательность во весех файлах поэтому чтобы не путаться должно обязательно указываться название эффекта
   теперь общее количество эффектов MODE_AMOUNT высчисляется автоматически на основе defaultSettings

   • для поклонников программы FireLamp нужно будет распределить содержимое четырех файлов равномерно в effects1.json, effects2.json, effects3.json
   список достигает предела и нужно эксперементировать пройдет весь список или обрежется сейчас загружаются только эффекты из 3 первых файлов
   «Акварель» последний на текущий момент котейка программу не обновляет, а это мешает двигаться дальше,
   темболее что в архиве есть альтернатива для всех платформ
  ====================================================================================================================================================
*/

uint32_t effTimer;

// -------------------------------------
// массив указателей на функции эффектов
// -------------------------------------
void (*FuncEff[MODE_AMOUNT])(void) = {
  Calm,                   // Штиль на морі
  Rain,                   // Дощ на морі
  Storm,                  // Шторм на морі
  Thunderstorm,           // Шторм з грозою
  Fog,                    // Туман в Одесі
  EarlyMorning,           // Ранній ранок
  Siesta,                 // Сієста
  QuietEvening,           // Тихий вечір
  EveningWatch,           // Вечірня вахта
  NightWatch,             // Нічна вахта
  ChangeOfWatch,          // Зміна з вахти
  NostalgicCaretaker,     // Ностальгуючий Доглядач
  KeeperOnTip,            // Доглядач на підпитку
  AbandonedLighthouse,    // AbandonedLighthouse
  RealTime                // RealTime ........... (MODE_AMOUNT - 1)
};

// -------------------------------------
void effectsTick() {
  if (isJavelinMode()) {
    if (ONflag && (millis() - effTimer >= FPSdelay)) {
      effTimer = millis();
      if (lastScenario == 129) {
        // SetEnvironmentState(false, false, 9, 10, false);
      } else {
        if (extCtrl == 0U) (*FuncEff[currentMode])();
      }
#ifdef WARNING_IF_NO_TIME_ON_EFFECTS_TOO
      if (!timeSynched) {
        noTimeWarning();
      } else {
        FastLED.clear();
      }
#endif
    }

#ifdef WARNING_IF_NO_TIME
    else if (!timeSynched && !ONflag && !((uint8_t)millis())) {
      noTimeWarningShow();
    }
#endif
  }
  FastLED.delay(1);
}

// --------------------------------------
void changePower() {
  if (ONflag) {
    loadingFlag = true;
    effectsTick();
    FastLED.setBrightness(0);
    for (uint8_t i = 0U; i < modes[currentMode].Brightness; i = constrain(i + 8, 0, modes[currentMode].Brightness)) {
      FastLED.setBrightness(i);
      FastLED.delay(1);
    }
    FastLED.setBrightness(modes[currentMode].Brightness);
    FastLED.delay(1);
  } else {
    effectsTick();
    FPSdelay = HIGH_DELAY;
    for (uint8_t i = modes[currentMode].Brightness; i > 0; i = constrain(i - 8, 0, modes[currentMode].Brightness)) {
      FastLED.setBrightness(i);
      FastLED.delay(1);
    }


    digitalWrite(TOWER_LIGHT_PIN, LOW);
    // FastLED.setBrightness(getBrightnessForPrintTime());
    SetEnvironmentState(false, 0, 255, false);
    FastLED.clear();
    FastLED.delay(2);
    delay(1000);
    SoundMute();
  }

#ifdef BACKLIGHT_PIN
  digitalWrite(BACKLIGHT_PIN, (ONflag ? HIGH : LOW));
#endif
#if defined(MOSFET_PIN) && defined(MOSFET_LEVEL)         // установка сигнала в пин, управляющий MOSFET транзистором, соответственно состоянию вкл/выкл матрицы
  digitalWrite(MOSFET_PIN, ONflag ? MOSFET_LEVEL : !MOSFET_LEVEL);
#endif

  TimerManager::TimerRunning = false;
  TimerManager::TimerHasFired = false;
  TimerManager::TimeToFire = 0ULL;
#ifdef AUTOMATIC_OFF_TIME
  if (ONflag) {
    TimerManager::TimerRunning = true;
    TimerManager::TimeToFire = millis() + AUTOMATIC_OFF_TIME;
  }
#endif

  if (FavoritesManager::UseSavedFavoritesRunning == 0U) {    // если выбрана опция Сохранять состояние (вкл/выкл) "избранного", то ни выключение модуля, ни выключение матрицы не сбрасывают текущее состояние (вкл/выкл) "избранного"
    FavoritesManager::TurnFavoritesOff();
  }

#if (USE_MQTT)
  if (espMode == 1U) {
    MqttManager::needToPublish = true;
  }
#endif
}

#ifdef WARNING_IF_NO_TIME

// --------------------------------------
void noTimeWarning() {
  // зеленая анимация режим работы с роутером
  // синяя анимация режим работы в режиме точки доступа
  // espModeState(espMode ? 112U : 176U);
  // Connect(espMode ? 112U : 176U);
}

// --------------------------------------
void noTimeWarningShow() {
  noTimeWarning();
  FastLED.setBrightness(WARNING_IF_NO_TIME);
  FastLED.delay(1);
}

// --------------------------------------
void noTimeClear() {
  if (!timeSynched) {
#ifdef JAVELIN
    DrawLevel(0, ROUND_MATRIX - 1, ROUND_MATRIX, CHSV {200, 255, 250});
#else
    FastLED.clear();
    FastLED.delay(2);
#endif
  }
}
#endif //WARNING_IF_NO_TIME
