#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "fmj/byte_source.hpp"
#include "fmj/mono_canvas.hpp"
#include "wide_canvas.hpp"
#include "wide_map_state.hpp"

class WideMapRenderer {
 public:
  enum class Error : std::uint8_t {
    None,
    InvalidState,
    MapOutOfRange,
    TileSet,
    TileGeometry,
    MapRead,
    PlayerImage,
  };

  bool render(const fmj::ByteSource& game,
              const WideMapState& state, WideCanvas& canvas);
  static void compositeOverlay(const WideMapState& state,
                               const fmj::MonoCanvas& source,
                               const WideCanvas& base,
                               WideCanvas& destination);
  Error error() const { return error_; }

 private:
  struct ImageHeader {
    std::uint8_t width = 0;
    std::uint8_t height = 0;
    std::uint8_t slices = 0;
    std::uint8_t mode = 0;
    std::size_t rowStride = 0;
    std::size_t sliceBytes = 0;
  };

  bool loadTileSet(const fmj::ByteSource& game, std::size_t offset);
  static bool readImageHeader(const fmj::ByteSource& game,
                              std::size_t offset, ImageHeader& header);
  static void drawSlice(WideCanvas& canvas, const std::uint8_t* pixels,
                        const ImageHeader& header, int x, int y);
  bool drawActor(const fmj::ByteSource& game,
                 const WideMapState& state, const WideMapActor& actor,
                 int originX, int originY,
                 WideCanvas& canvas);
  static std::size_t resourceOffset(const WideMapState& state,
                                    std::uint8_t bank,
                                    std::uint16_t offset);

  std::size_t tileSetOffset_ = static_cast<std::size_t>(-1);
  ImageHeader tileHeader_{};
  std::vector<std::uint8_t> tilePixels_;
  std::vector<std::uint8_t> actorPixels_;
  Error error_ = Error::None;
};
