#include <unity.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "fmj/byte_source.hpp"
#include "fmj/crc32.hpp"
#include "fmj/dat_lib.hpp"
#include "fmj/effect_resource.hpp"
#include "fmj/map_resource.hpp"
#include "fmj/mono_canvas.hpp"
#include "fmj/res_image.hpp"
#include "fmj/save_format.hpp"
#include "fmj/script_resource.hpp"

void setUp() {}
void tearDown() {}

void test_canvas_clips_and_draws() {
  fmj::MonoCanvas canvas;
  canvas.clear(false);
  canvas.fillRect(-2, -2, 5, 5, true);
  TEST_ASSERT_TRUE(canvas.pixel(0, 0));
  TEST_ASSERT_TRUE(canvas.pixel(2, 2));
  TEST_ASSERT_FALSE(canvas.pixel(3, 3));
  canvas.rect(10, 10, 4, 3, true);
  TEST_ASSERT_TRUE(canvas.pixel(10, 10));
  TEST_ASSERT_TRUE(canvas.pixel(13, 12));
  TEST_ASSERT_FALSE(canvas.pixel(11, 11));
}

void test_crc_and_save_header() {
  constexpr char payload[] = "123456789";
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926U,
                          fmj::crc32(payload, sizeof(payload) - 1U));
  auto header = fmj::makeSaveHeader(payload, sizeof(payload) - 1U);
  TEST_ASSERT_TRUE(
      fmj::validateSaveHeader(header, payload, sizeof(payload) - 1U));
  header.payloadCrc32 ^= 1U;
  TEST_ASSERT_FALSE(
      fmj::validateSaveHeader(header, payload, sizeof(payload) - 1U));
}

void test_dat_lib_index_and_opaque_image() {
  std::vector<std::uint8_t> bytes(0x2100U, 0U);
  bytes[0] = 'L';
  bytes[1] = 'I';
  bytes[2] = 'B';
  bytes[0x10U] = static_cast<std::uint8_t>(fmj::ResourceType::Picture);
  bytes[0x11U] = 1U;
  bytes[0x12U] = 7U;
  bytes[0x13U] = 0xFFU;
  bytes[0x2000U] = 0U;
  bytes[0x2001U] = 0x50U;
  bytes[0x2002U] = 0x20U;

  constexpr std::size_t resourceOffset = 0x2050U;
  const std::uint8_t image[] = {1U, 7U, 8U, 2U, 1U, 1U, 0xAAU, 0x55U};
  std::memcpy(bytes.data() + resourceOffset, image, sizeof(image));
  fmj::MemoryByteSource source(bytes.data(), bytes.size());
  fmj::DatLibIndex index;
  TEST_ASSERT_TRUE(index.open(source));
  TEST_ASSERT_EQUAL_UINT32(1U, index.entryCount());
  const auto* entry = index.find(fmj::ResourceType::Picture, 1U, 7U);
  TEST_ASSERT_NOT_NULL(entry);
  TEST_ASSERT_EQUAL_UINT32(resourceOffset, entry->offset);

  fmj::ImageResource resource;
  TEST_ASSERT_TRUE(resource.open(source, entry->offset));
  fmj::MonoCanvas canvas;
  canvas.clear(false);
  TEST_ASSERT_TRUE(resource.drawSlice(canvas, 0U, 3, 4));
  TEST_ASSERT_TRUE(canvas.pixel(3, 4));
  TEST_ASSERT_FALSE(canvas.pixel(4, 4));
  TEST_ASSERT_FALSE(canvas.pixel(3, 5));
  TEST_ASSERT_TRUE(canvas.pixel(4, 5));
}

void test_transparent_image_keeps_background() {
  // Four pixels: transparent, black, white, transparent.
  const std::uint8_t bytes[] = {1U, 1U, 4U, 1U, 1U, 2U, 0xB2U, 0x00U};
  fmj::MemoryByteSource source(bytes, sizeof(bytes));
  fmj::ImageResource resource;
  TEST_ASSERT_TRUE(resource.open(source, 0U));
  fmj::MonoCanvas canvas;
  canvas.clear(true);
  TEST_ASSERT_TRUE(resource.drawSlice(canvas, 0U, 10, 10));
  TEST_ASSERT_TRUE(canvas.pixel(10, 10));
  TEST_ASSERT_TRUE(canvas.pixel(11, 10));
  TEST_ASSERT_FALSE(canvas.pixel(12, 10));
  TEST_ASSERT_TRUE(canvas.pixel(13, 10));
}

void test_map_resource_reads_tile_flags() {
  std::array<std::uint8_t, 0x12U + 8U> bytes{};
  bytes[0] = 1U;
  bytes[1] = 2U;
  bytes[2] = 9U;
  bytes[3] = 'M';
  bytes[4] = 'A';
  bytes[5] = 'P';
  bytes[0x10U] = 2U;
  bytes[0x11U] = 2U;
  bytes[0x12U] = 0x85U;
  bytes[0x13U] = 12U;
  fmj::MemoryByteSource source(bytes.data(), bytes.size());
  fmj::MapResource map;
  TEST_ASSERT_TRUE(map.open(source, 0U));
  std::uint8_t tile = 0;
  std::uint8_t event = 0;
  bool walkable = false;
  TEST_ASSERT_TRUE(map.tile(0, 0, tile, walkable, event));
  TEST_ASSERT_EQUAL_UINT8(5U, tile);
  TEST_ASSERT_TRUE(walkable);
  TEST_ASSERT_EQUAL_UINT8(12U, event);
  TEST_ASSERT_FALSE(map.tile(2, 0, tile, walkable, event));
}

void test_effect_resource_opens_embedded_image() {
  const std::uint8_t bytes[] = {
      1U, 9U, 1U, 1U, 0U, 0U,  // effect header
      2U, 3U, 1U, 1U, 0U,      // one frame
      1U, 4U, 8U, 1U, 1U, 1U,  // one image
      0x81U,
  };
  fmj::MemoryByteSource source(bytes, sizeof(bytes));
  fmj::EffectResource effect;
  TEST_ASSERT_TRUE(effect.open(source, 0U));
  TEST_ASSERT_EQUAL_UINT8(1U, effect.frameCount());
  fmj::MonoCanvas canvas;
  canvas.clear(false);
  TEST_ASSERT_TRUE(effect.drawFrame(canvas, 0U));
  TEST_ASSERT_TRUE(canvas.pixel(2, 3));
  TEST_ASSERT_TRUE(canvas.pixel(9, 3));
  TEST_ASSERT_FALSE(canvas.pixel(3, 3));
  TEST_ASSERT_EQUAL_UINT8(1U, effect.frameShow(0U));
  TEST_ASSERT_EQUAL_UINT8(1U, effect.frameNextShow(0U));
}

void test_script_resource_decodes_events_and_strings() {
  // Resource length includes the event table, its 3-byte trailer, and code.
  // Event 1 points to code offset 0 (header size is 5).
  const std::uint8_t bytes[] = {
      1U, 1U, 'T', 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
      0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
      17U, 0U, 1U,  // length=17, one event
      5U, 0U,       // event address = header size, thus code offset 0
      13U, 1U, 0U, 0xD0U, 0xA1U, 0U,  // SAY portrait 1, one GBK char
      14U, 2U, 0U, 12U, 0U,            // STARTCHAPTER 2:12
      9U,                               // CALLBACK
  };
  fmj::MemoryByteSource source(bytes, sizeof(bytes));
  fmj::ScriptResource script;
  TEST_ASSERT_TRUE(script.open(source, 0U));
  TEST_ASSERT_EQUAL_UINT8(1U, script.eventCount());
  TEST_ASSERT_EQUAL_UINT16(12U, script.codeSize());
  std::uint16_t eventOffset = 0xFFFFU;
  TEST_ASSERT_TRUE(script.eventCodeOffset(1U, eventOffset));
  TEST_ASSERT_EQUAL_UINT16(0U, eventOffset);

  fmj::ScriptCommand command;
  TEST_ASSERT_TRUE(script.decode(0U, command));
  TEST_ASSERT_EQUAL_UINT8(13U, command.opcode);
  TEST_ASSERT_EQUAL_UINT16(5U, command.operandLength);
  TEST_ASSERT_EQUAL_UINT16(6U, command.nextOffset);
  TEST_ASSERT_TRUE(script.decode(command.nextOffset, command));
  TEST_ASSERT_EQUAL_UINT8(14U, command.opcode);
  TEST_ASSERT_EQUAL_UINT16(11U, command.nextOffset);
  TEST_ASSERT_TRUE(script.decode(command.nextOffset, command));
  TEST_ASSERT_EQUAL_UINT8(9U, command.opcode);
  TEST_ASSERT_EQUAL_UINT16(12U, command.nextOffset);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_canvas_clips_and_draws);
  RUN_TEST(test_crc_and_save_header);
  RUN_TEST(test_dat_lib_index_and_opaque_image);
  RUN_TEST(test_transparent_image_keeps_background);
  RUN_TEST(test_map_resource_reads_tile_flags);
  RUN_TEST(test_effect_resource_opens_embedded_image);
  RUN_TEST(test_script_resource_decodes_events_and_strings);
  return UNITY_END();
}
