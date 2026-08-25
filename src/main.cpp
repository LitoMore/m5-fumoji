#include <Arduino.h>
#include <M5Cardputer.h>

#include "c_engine_runtime.hpp"
#include "cardputer_display.hpp"
#include "cardputer_input.hpp"
#include "fmj/mono_canvas.hpp"
#include "littlefs_byte_source.hpp"
#include "littlefs_save_backend.hpp"
#include "screen_streamer.hpp"

namespace {

LittleFsSaveBackend saves;
fmj::MonoCanvas canvas;
LittleFsByteSource rom;
LittleFsByteSource hzk16;
LittleFsByteSource asc16;
CardputerDisplay display;
CardputerInput input;
CEngineRuntime engine;
ScreenStreamer streamer;
bool engineReady = false;
std::uint32_t reportedMelodyRevision = 0;

void reportMelodyState() {
  if (!engineReady || streamer.active()) return;
  const auto state = engine.melodyState();
  if (state.revision == reportedMelodyRevision) return;
  reportedMelodyRevision = state.revision;
  if (state.playing) {
    Serial.printf("FMJAUDIO PLAY %u\n", state.number);
  } else {
    Serial.println("FMJAUDIO STOP");
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  auto config = M5.config();
  M5Cardputer.begin(config, true);
  display.begin();

  const bool filesystemReady = saves.begin();
  const bool romReady = filesystemReady && rom.open("/FMJ.LIB");
  const bool fontsReady = filesystemReady && hzk16.open("/HZK16") &&
                          asc16.open("/ASC16");
  engineReady = romReady && fontsReady && engine.begin(rom, hzk16, asc16);
  if (!engineReady) {
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(8, 12);
    M5Cardputer.Display.println("Fumoji C engine could not start");
    M5Cardputer.Display.println();
    M5Cardputer.Display.printf("FMJ.LIB: %s\n", romReady ? "ok" : "missing");
    M5Cardputer.Display.printf("Fonts: %s\n", fontsReady ? "ok" : "missing");
    if (engine.error() != nullptr) M5Cardputer.Display.println(engine.error());
  }

  const auto board = M5.getBoard();
  const char* boardName =
      board == m5::board_t::board_M5CardputerADV ? "Cardputer ADV" :
      board == m5::board_t::board_M5Cardputer ? "Cardputer" : "unknown";
  Serial.printf("FMJ C engine: board=%s fs=%s rom=%s fonts=%s engine=%s\n",
                boardName,
                filesystemReady ? "ok" : "failed", romReady ? "ok" : "missing",
                fontsReady ? "ok" : "missing",
                engineReady ? "running" : "failed");
}

void loop() {
  const bool frameChanged =
      engineReady && engine.update(input, display, canvas);
  const auto now = millis();
  const auto brightnessDelta = input.takeBrightnessDelta();
  if (brightnessDelta != 0) {
    display.adjustBrightness(brightnessDelta, now);
  }
  display.update(now);
  streamer.update(display.frame(), frameChanged);
  reportMelodyState();
  delay(5);
}
