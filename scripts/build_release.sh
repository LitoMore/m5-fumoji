#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
pio_home=${PLATFORMIO_CORE_DIR:-$(pio system info | sed -n 's/^PlatformIO Core Directory  *//p')}
pio_python=$(pio system info | sed -n 's/^Python Executable  *//p')
build_dir="$project_root/.pio/build/cardputer_adv"
output_dir="$project_root/dist"

if [ -z "$pio_home" ] || [ -z "$pio_python" ]; then
  echo "Could not locate PlatformIO's Python environment" >&2
  exit 1
fi

cd "$project_root"
pio run -e cardputer_adv
pio run -e cardputer_adv -t buildfs
mkdir -p "$output_dir"
install -m 0644 "$project_root/LICENSE" "$output_dir/LICENSE"
install -m 0644 "$project_root/THIRD_PARTY_NOTICES.md" \
  "$output_dir/THIRD_PARTY_NOTICES.md"

tar -czf "$output_dir/m5-fumoji-source.tar.gz" \
  --exclude='./.git' \
  --exclude='./.pio' \
  --exclude='./dist' \
  --exclude='./data/FMJ.LIB' \
  --exclude='./data/HZK16' \
  --exclude='./data/ASC16' \
  .

"$pio_python" "$pio_home/packages/tool-esptoolpy/esptool.py" \
  --chip esp32s3 merge_bin \
  -o "$output_dir/m5-fumoji-cardputer-adv.bin" \
  0x0000 "$build_dir/bootloader.bin" \
  0x8000 "$build_dir/partitions.bin" \
  0x10000 "$build_dir/firmware.bin" \
  0x310000 "$build_dir/littlefs.bin"

echo "Created $output_dir/m5-fumoji-cardputer-adv.bin"
echo "Created $output_dir/m5-fumoji-source.tar.gz without game assets"
echo "Copied LICENSE and THIRD_PARTY_NOTICES.md to $output_dir"
