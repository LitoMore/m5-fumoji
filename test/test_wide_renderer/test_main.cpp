#include <unity.h>

#include <array>
#include <cstdint>

#include "fmj/byte_source.hpp"
#include "wide_battle_renderer.hpp"
#include "wide_map_renderer.hpp"

namespace {

constexpr std::size_t kMapOffset = 0x0100U;
constexpr std::size_t kTileOffset = 0x4100U;
constexpr std::size_t kActorOffset = 0x8100U;

void putImageHeader(std::array<std::uint8_t, 0x10000>& game,
                    std::size_t offset, std::uint8_t width,
                    std::uint8_t height, std::uint8_t slices,
                    std::uint8_t mode) {
  game[offset + 2U] = width;
  game[offset + 3U] = height;
  game[offset + 4U] = slices;
  game[offset + 5U] = mode;
}

void putDirectoryEntry(std::array<std::uint8_t, 0x10000>& game,
                       std::size_t entry, std::uint8_t type,
                       std::uint8_t group, std::uint8_t index,
                       std::size_t offset) {
  game[0x10U + entry * 3U] = type;
  game[0x11U + entry * 3U] = group;
  game[0x12U + entry * 3U] = index;
  game[0x2000U + entry * 3U] = static_cast<std::uint8_t>(offset / 0x4000U);
  const auto withinBank = offset % 0x4000U;
  game[0x2001U + entry * 3U] = static_cast<std::uint8_t>(withinBank);
  game[0x2002U + entry * 3U] =
      static_cast<std::uint8_t>(withinBank >> 8U);
}

WideMapState makeState() {
  WideMapState state{};
  state.mapWidth = 20;
  state.mapHeight = 12;
  state.tileWidth = 16;
  state.tileHeight = 16;
  state.mapOffset = static_cast<std::uint16_t>(kMapOffset);
  state.tileBank = 1;
  state.tileOffset = 0x0100;
  state.actorCount = 1;
  state.actors[0].type = 1;
  state.actors[0].worldX = 7;
  state.actors[0].worldY = 4;
  state.actors[0].direction = 1;
  state.actors[0].imageBank = 2;
  state.actors[0].imageOffset = 0x0100;
  return state;
}

void setOverlayPixel(WideBattleState& state, int x, int y) {
  state.overlayMask[static_cast<std::size_t>(y) * 20U +
                    static_cast<std::size_t>(x / 8)] |=
      static_cast<std::uint8_t>(0x80U >> (x & 7));
}

void setOverlayPixel(WideMapState& state, int x, int y) {
  state.overlayMask[static_cast<std::size_t>(y) * 20U +
                    static_cast<std::size_t>(x / 8)] |=
      static_cast<std::uint8_t>(0x80U >> (x & 7));
}

void test_wide_map_composites_black_and_white_ui_pixels() {
  WideMapState state{};
  fmj::MonoCanvas source;
  WideCanvas base;
  WideCanvas output;
  source.clear(false);
  base.clear(false);
  base.setPixel(51, 29, true);
  source.setPixel(10, 10, true);
  setOverlayPixel(state, 10, 10);
  setOverlayPixel(state, 11, 10);

  WideMapRenderer::compositeOverlay(state, source, base, output);

  TEST_ASSERT_TRUE(output.pixel(50, 29));
  TEST_ASSERT_FALSE(output.pixel(51, 29));
  TEST_ASSERT_FALSE(output.pixel(52, 29));
}

void test_wide_renderer_draws_native_tiles_and_player() {
  std::array<std::uint8_t, 0x10000> game{};
  putImageHeader(game, kTileOffset, 16, 16, 1, 1);
  game[kTileOffset + 6U] = 0x80U;
  putImageHeader(game, kActorOffset, 8, 8, 1, 1);
  for (std::size_t index = 0; index < 8U; ++index) {
    game[kActorOffset + 6U + index] = 0xFFU;
  }

  fmj::MemoryByteSource source(game.data(), game.size());
  WideMapRenderer renderer;
  WideCanvas canvas;
  const auto state = makeState();

  TEST_ASSERT_TRUE(renderer.render(source, state, canvas));
  TEST_ASSERT_TRUE(canvas.pixel(0, 0));
  TEST_ASSERT_FALSE(canvas.pixel(1, 0));
  TEST_ASSERT_TRUE(canvas.pixel(112, 72));
  TEST_ASSERT_TRUE(canvas.pixel(119, 79));
  TEST_ASSERT_FALSE(canvas.pixel(120, 79));
}

void test_wide_renderer_clamps_view_at_map_edge() {
  std::array<std::uint8_t, 0x10000> game{};
  putImageHeader(game, kTileOffset, 16, 16, 1, 1);
  putImageHeader(game, kActorOffset, 8, 8, 1, 1);
  game[kActorOffset + 6U] = 0x80U;

  auto state = makeState();
  state.actors[0].worldX = 19;
  state.actors[0].worldY = 10;
  fmj::MemoryByteSource source(game.data(), game.size());
  WideMapRenderer renderer;
  WideCanvas canvas;

  TEST_ASSERT_TRUE(renderer.render(source, state, canvas));
  TEST_ASSERT_TRUE(canvas.pixel(224, 120));
}

void test_wide_battle_moves_walls_and_centers_dynamic_layer() {
  std::array<std::uint8_t, 0x10000> game{};
  game[0] = 'L';
  game[1] = 'I';
  game[2] = 'B';
  constexpr std::size_t base = 0x4100U;
  constexpr std::size_t top = 0x4200U;
  constexpr std::size_t bottom = 0x4300U;
  putDirectoryEntry(game, 0, 11, 4, 1, base);
  putDirectoryEntry(game, 1, 11, 4, 31, top);
  putDirectoryEntry(game, 2, 11, 4, 111, bottom);
  game[0x10U + 3U * 3U] = 0xFFU;
  putImageHeader(game, base, 8, 8, 1, 1);
  putImageHeader(game, top, 2, 2, 1, 1);
  putImageHeader(game, bottom, 2, 2, 1, 1);
  game[top + 6U] = 0xC0U;
  game[top + 7U] = 0xC0U;
  game[bottom + 6U] = 0xC0U;
  game[bottom + 7U] = 0xC0U;

  fmj::MemoryByteSource source(game.data(), game.size());
  WideBattleRenderer renderer;
  fmj::MonoCanvas original;
  WideCanvas wide;
  original.clear(false);
  original.setPixel(0x42, 0, true);
  original.setPixel(0x43, 0, true);
  original.setPixel(0x42, 1, true);
  original.setPixel(0x43, 1, true);
  original.setPixel(0, 0x3D, true);
  original.setPixel(1, 0x3D, true);
  original.setPixel(0, 0x3E, true);
  original.setPixel(1, 0x3E, true);
  original.setPixel(80, 48, true);

  TEST_ASSERT_TRUE(renderer.begin(source));
  WideBattleState state{1, 31, 111};
  setOverlayPixel(state, 80, 48);
  TEST_ASSERT_TRUE(renderer.render(source, state, original, wide));
  TEST_ASSERT_TRUE(wide.pixel(238, 0));
  TEST_ASSERT_TRUE(wide.pixel(0, 133));
  TEST_ASSERT_FALSE(wide.pixel(0x42 + 40, 19));
  TEST_ASSERT_FALSE(wide.pixel(40, 0x3D + 19));
  TEST_ASSERT_TRUE(wide.pixel(120, 67));
  TEST_ASSERT_FALSE(wide.pixel(121, 67));
}

void test_wide_battle_repeats_periodic_background_past_original_edges() {
  std::array<std::uint8_t, 0x10000> game{};
  game[0] = 'L';
  game[1] = 'I';
  game[2] = 'B';
  constexpr std::size_t base = 0x4100U;
  constexpr std::size_t top = 0x5000U;
  constexpr std::size_t bottom = 0x5100U;
  putDirectoryEntry(game, 0, 11, 4, 1, base);
  putDirectoryEntry(game, 1, 11, 4, 31, top);
  putDirectoryEntry(game, 2, 11, 4, 111, bottom);
  game[0x10U + 3U * 3U] = 0xFFU;
  putImageHeader(game, base, 159, 96, 1, 1);
  putImageHeader(game, top, 1, 1, 1, 1);
  putImageHeader(game, bottom, 1, 1, 1, 1);
  for (std::size_t row = 0; row < 96U; ++row) {
    for (std::size_t byte = 0; byte < 20U; ++byte) {
      game[base + 6U + row * 20U + byte] =
          row % 2U == 0U ? 0xAAU : 0x55U;
    }
  }

  fmj::MemoryByteSource source(game.data(), game.size());
  WideBattleRenderer renderer;
  fmj::MonoCanvas original;
  WideCanvas wide;
  original.clear(false);
  for (int y = 0; y < fmj::MonoCanvas::kHeight; ++y) {
    for (int x = 0; x < fmj::MonoCanvas::kWidth - 1; ++x) {
      original.setPixel(x, y, ((x + y) & 1) == 0);
    }
  }

  TEST_ASSERT_TRUE(renderer.begin(source));
  TEST_ASSERT_TRUE(renderer.render(
      source, WideBattleState{1, 31, 111}, original, wide));
  TEST_ASSERT_FALSE(wide.pixel(39, 19));
  TEST_ASSERT_TRUE(wide.pixel(38, 19));
  TEST_ASSERT_TRUE(wide.pixel(199, 20));
}

void test_wide_battle_preserves_original_selection() {
  std::array<std::uint8_t, 0x10000> game{};
  game[0] = 'L';
  game[1] = 'I';
  game[2] = 'B';
  constexpr std::size_t base = 0x4100U;
  constexpr std::size_t top = 0x4200U;
  constexpr std::size_t bottom = 0x4300U;
  putDirectoryEntry(game, 0, 11, 4, 1, base);
  putDirectoryEntry(game, 1, 11, 4, 31, top);
  putDirectoryEntry(game, 2, 11, 4, 111, bottom);
  game[0x10U + 3U * 3U] = 0xFFU;
  putImageHeader(game, base, 8, 8, 1, 1);
  putImageHeader(game, top, 1, 1, 1, 2);
  putImageHeader(game, bottom, 1, 1, 1, 2);
  for (std::size_t row = 0; row < 8U; ++row) {
    game[base + 6U + row] = row % 2U == 0U ? 0xAAU : 0x55U;
  }
  game[top + 6U] = 0xC0U;

  fmj::MemoryByteSource source(game.data(), game.size());
  WideBattleRenderer renderer;
  fmj::MonoCanvas original;
  WideCanvas wide;
  original.clear(false);
  original.setPixel(0x42, 0, true);
  WideBattleState state{1, 31, 111};
  setOverlayPixel(state, 0x42, 0);

  TEST_ASSERT_TRUE(renderer.begin(source));
  TEST_ASSERT_TRUE(renderer.render(source, state, original, wide));
  TEST_ASSERT_TRUE(wide.pixel(0x42 + 40, 19));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_wide_renderer_draws_native_tiles_and_player);
  RUN_TEST(test_wide_renderer_clamps_view_at_map_edge);
  RUN_TEST(test_wide_map_composites_black_and_white_ui_pixels);
  RUN_TEST(test_wide_battle_moves_walls_and_centers_dynamic_layer);
  RUN_TEST(test_wide_battle_repeats_periodic_background_past_original_edges);
  RUN_TEST(test_wide_battle_preserves_original_selection);
  return UNITY_END();
}
