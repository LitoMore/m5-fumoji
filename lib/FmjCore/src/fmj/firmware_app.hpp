#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "fmj/bbk_font.hpp"
#include "fmj/byte_source.hpp"
#include "fmj/dat_lib.hpp"
#include "fmj/effect_resource.hpp"
#include "fmj/map_resource.hpp"
#include "fmj/mono_canvas.hpp"
#include "fmj/res_image.hpp"
#include "fmj/save_format.hpp"
#include "fmj/script_resource.hpp"

namespace fmj {

enum class InputKey : std::uint8_t {
  None,
  Up,
  Down,
  Left,
  Right,
  PageUp,
  PageDown,
  Confirm,
  Cancel,
  Menu,
};

class FirmwareApp {
 public:
  explicit FirmwareApp(SaveBackend& saves) : saves_(saves) {}

  void begin(const ByteSource* rom, const ByteSource* hzk16 = nullptr,
             const ByteSource* asc16 = nullptr);
  bool update(InputKey key);
  bool tick(std::uint32_t deltaMs);
  void render(MonoCanvas& canvas) const;

  bool romAvailable() const { return rom_ != nullptr && resources_.valid(); }
  std::size_t resourceCount() const { return resources_.entryCount(); }
  std::uint8_t chapterType() const { return scriptType_; }
  std::uint8_t chapterIndex() const { return scriptIndex_; }
  std::uint8_t currentMapType() const { return mapType_; }
  std::uint8_t currentMapIndex() const { return mapIndex_; }
  bool storyBusy() const {
    return scriptRunning_ || waitState_ != WaitState::None;
  }
  std::int16_t playerX() const { return playerActor_.x; }
  std::int16_t playerY() const { return playerActor_.y; }

 private:
  enum class Screen : std::uint8_t { Splash, Menu, Game, RomStatus };
  enum class WaitState : std::uint8_t {
    None,
    Dialogue,
    Movie,
    Move,
    Pose,
    SceneName,
  };

  enum class Direction : std::uint8_t { North, East, South, West };

  struct SceneActor {
    bool active = false;
    std::uint8_t id = 0;
    std::uint8_t actorType = 0;
    std::uint8_t actorIndex = 0;
    std::uint8_t spriteId = 0;
    std::int16_t x = 0;
    std::int16_t y = 0;
    Direction direction = Direction::South;
    std::uint8_t step = 1;
    ImageResource sprite;
  };

  struct MovieLayer {
    bool active = false;
    std::uint8_t frame = 0;
    std::int16_t show = 0;
    std::int16_t nextShow = 0;
  };

#pragma pack(push, 1)
  struct PlayerState {
    std::uint32_t magic = 0x314A4D46U;
    std::uint16_t version = 1;
    std::uint8_t x = 5;
    std::uint8_t y = 4;
    std::uint32_t steps = 0;
  };

  struct PersistedActor {
    std::uint8_t active = 0;
    std::uint8_t actorType = 0;
    std::uint8_t actorIndex = 0;
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::uint8_t direction = 0;
    std::uint8_t step = 0;
  };

  struct GameSaveState {
    std::uint32_t magic = 0x324A4D46U;
    std::uint16_t version = 2;
    std::uint8_t mapType = 0;
    std::uint8_t mapIndex = 0;
    std::int16_t mapLeft = 0;
    std::int16_t mapTop = 0;
    std::uint8_t scriptType = 0;
    std::uint8_t scriptIndex = 0;
    std::uint32_t steps = 0;
    PersistedActor player;
    std::array<PersistedActor, 41> npcs{};
    std::array<std::uint8_t, 256> events{};
    std::array<std::uint16_t, 2048> variables{};
    std::array<std::uint8_t, 32> sceneName{};
  };
#pragma pack(pop)

  void renderSplash(MonoCanvas& canvas) const;
  void renderMenu(MonoCanvas& canvas) const;
  void renderGame(MonoCanvas& canvas) const;
  void renderRomStatus(MonoCanvas& canvas) const;
  void renderScene(MonoCanvas& canvas) const;
  void renderActor(MonoCanvas& canvas, const SceneActor& actor) const;
  void renderDialogue(MonoCanvas& canvas) const;
  void renderSceneName(MonoCanvas& canvas) const;
  void renderMovie(MonoCanvas& canvas) const;

  void startNewGame();
  void loadGame();
  void saveGame();
  static PersistedActor persistActor(const SceneActor& actor);
  bool restoreActor(SceneActor& actor, const PersistedActor& persisted,
                    std::uint8_t id);
  void initializeFallbackScene();
  void clearSceneActors();
  bool loadMap(std::uint8_t type, std::uint8_t index, std::uint16_t left,
               std::uint16_t top);
  bool loadActor(SceneActor& actor, std::uint8_t actorType,
                 std::uint8_t actorIndex, std::uint8_t id, std::int16_t x,
                 std::int16_t y);
  bool createPlayer(std::uint8_t actorIndex, std::uint16_t x,
                    std::uint16_t y);
  bool createNpc(std::uint8_t id, std::uint8_t actorType,
                 std::uint8_t actorIndex, std::uint16_t x, std::uint16_t y);
  void deleteNpc(std::uint8_t id);
  SceneActor* actorById(std::uint8_t id);
  const SceneActor* actorAt(std::int16_t x, std::int16_t y) const;
  bool movePlayer(Direction direction);
  void keepPlayerInViewport();

  bool startChapter(std::uint8_t type, std::uint8_t index);
  bool triggerEvent(std::uint8_t eventId);
  bool processScript();
  bool jumpAddress(std::uint16_t address);
  bool beginDialogue(const ScriptCommand& command, std::uint16_t prefix,
                     std::uint16_t portrait);
  bool nextDialoguePage();
  std::size_t dialoguePageEnd(std::size_t start, std::uint8_t firstLineUnits,
                              std::uint8_t secondLineUnits) const;
  bool beginMovie(std::uint16_t type, std::uint16_t index, std::uint16_t x,
                  std::uint16_t y, std::uint16_t control);
  bool advanceMovie(std::uint32_t deltaMs);
  bool advanceMove(std::uint32_t deltaMs);

  bool eventValue(std::uint16_t id) const;
  void setEventValue(std::uint16_t id, bool value);
  std::uint16_t variable(std::uint16_t id) const;
  void setVariable(std::uint16_t id, std::uint16_t value);

  SaveBackend& saves_;
  const ByteSource* rom_ = nullptr;
  DatLibIndex resources_;
  BbkFont font_;
  MapResource map_;
  ImageResource tiles_;
  bool sceneReady_ = false;
  std::int16_t mapLeft_ = 0;
  std::int16_t mapTop_ = 0;
  std::uint8_t mapType_ = 0;
  std::uint8_t mapIndex_ = 0;

  SceneActor playerActor_;
  std::array<SceneActor, 41> npcs_{};
  std::array<std::uint8_t, 256> events_{};
  std::array<std::uint16_t, 2048> variables_{};

  ScriptResource script_;
  std::uint16_t scriptPc_ = 0;
  std::uint8_t scriptType_ = 0;
  std::uint8_t scriptIndex_ = 0;
  bool scriptRunning_ = false;
  WaitState waitState_ = WaitState::None;
  std::uint32_t waitElapsedMs_ = 0;

  std::uint8_t moveActorId_ = 0;
  std::int16_t moveTargetX_ = 0;
  std::int16_t moveTargetY_ = 0;

  EffectResource movie_;
  std::array<MovieLayer, 16> movieLayers_{};
  std::uint32_t movieAccumulatorMs_ = 0;
  std::int16_t movieX_ = 0;
  std::int16_t movieY_ = 0;
  std::uint8_t movieControl_ = 0;

  std::array<std::uint8_t, 512> dialogue_{};
  std::size_t dialogueLength_ = 0;
  std::size_t dialoguePageOffset_ = 0;
  std::uint16_t dialoguePortrait_ = 0;
  ImageResource portrait_;
  std::array<std::uint8_t, 32> sceneName_{};

  Screen screen_ = Screen::Menu;
  std::uint8_t menuIndex_ = 0;
  PlayerState player_{};
  GameSaveState saveBuffer_{};
};

}  // namespace fmj
