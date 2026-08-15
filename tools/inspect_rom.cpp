#include <fstream>
#include <iostream>
#include <iterator>
#include <array>
#include <vector>

#include "fmj/byte_source.hpp"
#include "fmj/dat_lib.hpp"
#include "fmj/effect_resource.hpp"
#include "fmj/map_resource.hpp"
#include "fmj/res_image.hpp"
#include "fmj/script_resource.hpp"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: inspect_rom <FMJ.LIB>\n";
    return 2;
  }
  std::ifstream file(argv[1], std::ios::binary);
  if (!file) {
    std::cerr << "cannot open " << argv[1] << '\n';
    return 2;
  }
  std::vector<std::uint8_t> bytes{
      std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  fmj::MemoryByteSource source(bytes.data(), bytes.size());
  fmj::DatLibIndex index;
  if (!index.open(source)) {
    std::cerr << "invalid FMJ.LIB\n";
    return 1;
  }
  std::cout << "resources: " << index.entryCount() << '\n';
  std::array<std::size_t, fmj::ScriptResource::kOpcodeCount> opcodeCounts{};
  std::size_t scriptCount = 0;
  std::size_t commandCount = 0;
  std::size_t invalidScripts = 0;
  for (std::size_t i = 0; i < index.entryCount(); ++i) {
    const auto* resource = index.entryAt(i);
    if (resource == nullptr ||
        (resource->key >> 16U) !=
            static_cast<std::uint32_t>(fmj::ResourceType::Script)) {
      continue;
    }
    ++scriptCount;
    fmj::ScriptResource script;
    if (!script.open(source, resource->offset)) {
      ++invalidScripts;
      continue;
    }
    std::uint16_t cursor = 0;
    while (cursor < script.codeSize()) {
      fmj::ScriptCommand command;
      if (!script.decode(cursor, command) || command.nextOffset <= cursor) {
        ++invalidScripts;
        std::cerr << "invalid script " << static_cast<unsigned>(script.type())
                  << '/' << static_cast<unsigned>(script.index())
                  << " at code offset " << cursor << '\n';
        break;
      }
      ++commandCount;
      ++opcodeCounts[command.opcode];
      cursor = command.nextOffset;
    }
  }
  std::cout << "scripts: " << scriptCount << ", commands=" << commandCount
            << ", invalid=" << invalidScripts << '\n';
  std::cout << "used opcodes:";
  for (std::size_t i = 0; i < opcodeCounts.size(); ++i) {
    if (opcodeCounts[i] != 0U) {
      std::cout << ' ' << i << ':' << fmj::ScriptResource::opcodeName(i)
                << '=' << opcodeCounts[i];
    }
  }
  std::cout << '\n';
  for (std::uint8_t id : {247U, 248U, 249U}) {
    const auto* entry = index.find(fmj::ResourceType::Effect, 1U, id);
    if (entry == nullptr) {
      std::cout << "effect 1/" << static_cast<unsigned>(id) << ": missing\n";
      continue;
    }
    fmj::EffectResource effect;
    if (!effect.open(source, entry->offset)) {
      std::uint8_t header[6]{};
      source.read(entry->offset, header, sizeof(header));
      std::cerr << "effect 1/" << static_cast<unsigned>(id)
                << ": invalid at " << entry->offset << " header=";
      for (const auto value : header) {
        std::cerr << static_cast<unsigned>(value) << ',';
      }
      std::cerr << '\n';
      return 1;
    }
    std::cout << "effect 1/" << static_cast<unsigned>(id) << ": frames="
              << static_cast<unsigned>(effect.frameCount()) << ", images="
              << static_cast<unsigned>(effect.imageCount()) << '\n';
  }
  const auto* mapEntry = index.first(fmj::ResourceType::Map);
  fmj::MapResource map;
  if (mapEntry == nullptr || !map.open(source, mapEntry->offset)) {
    std::cerr << "first map: invalid\n";
    return 1;
  }
  const auto* tileEntry =
      index.find(fmj::ResourceType::Tile, 1U, map.tileSetIndex());
  fmj::ImageResource tiles;
  if (tileEntry == nullptr || !tiles.open(source, tileEntry->offset)) {
    std::cerr << "first map tile set: invalid\n";
    return 1;
  }
  std::cout << "first map: " << static_cast<unsigned>(map.width()) << 'x'
            << static_cast<unsigned>(map.height()) << ", tile set="
            << static_cast<unsigned>(map.tileSetIndex()) << " ("
            << static_cast<unsigned>(tiles.width()) << 'x'
            << static_cast<unsigned>(tiles.height()) << ", "
            << static_cast<unsigned>(tiles.sliceCount()) << " tiles)\n";
  return 0;
}
