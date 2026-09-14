#!/usr/bin/env bash
set -euo pipefail

# Build You Have Got Pizza as a standalone third-party PRG32 cartridge.
#
# This script is meant to live in the game repository, not inside PRG32.
# Point it to a separate PRG32 checkout with either:
#
#   PRG32_ROOT=/path/to/PRG32 tools/build_cartridges.sh qemu
#   tools/build_cartridges.sh qemu --prg32-root /path/to/PRG32
#
# It builds the resident PRG32 firmware if needed, then asks PRG32's own
# cartridge builder to compile this repository's C and RISC-V assembly sources.

usage() {
  cat >&2 <<'USAGE'
usage: tools/build_cartridges.sh [qemu|esp32c6] [options]

Options:
  --prg32-root DIR     Path to a cloned PRG32 repository. Alternatively set PRG32_ROOT.
  --out-dir DIR        Output directory for .prg32 files. Default: ./dist/<target>.
  --skip-firmware     Do not run idf.py set-target/build for the resident firmware.
  --upload-qemu       After building qemu cartridges, stage the assembly cartridge in qemu_flash.bin.
  --upload-hardware   After building esp32c6 cartridges, upload the assembly cartridge over Wi-Fi.
  --url URL           PRG32 hardware upload URL. Default: http://192.168.4.1.
  -h, --help          Show this help.

Examples:
  PRG32_ROOT=$HOME/src/PRG32 tools/build_cartridges.sh qemu
  tools/build_cartridges.sh esp32c6 --prg32-root ../PRG32 --upload-hardware
USAGE
}

TARGET_MODE="${1:-qemu}"
if [[ "$TARGET_MODE" == "-h" || "$TARGET_MODE" == "--help" ]]; then
  usage
  exit 0
fi
shift || true

PRG32_ROOT="${PRG32_ROOT:-}"
OUT_DIR=""
SKIP_FIRMWARE=0
UPLOAD_QEMU=0
UPLOAD_HARDWARE=0
UPLOAD_URL="http://192.168.4.1"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prg32-root)
      PRG32_ROOT="${2:-}"
      shift 2
      ;;
    --out-dir)
      OUT_DIR="${2:-}"
      shift 2
      ;;
    --skip-firmware)
      SKIP_FIRMWARE=1
      shift
      ;;
    --upload-qemu)
      UPLOAD_QEMU=1
      shift
      ;;
    --upload-hardware)
      UPLOAD_HARDWARE=1
      shift
      ;;
    --url)
      UPLOAD_URL="${2:-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 2
      ;;
  esac
done

case "$TARGET_MODE" in
  qemu)
    BUILD_DIR_NAME="build-qemu"
    DEFAULTS="sdkconfig.defaults.qemu"
    IDF_TARGET="esp32c3"
    ;;
  esp32c6|hardware)
    TARGET_MODE="esp32c6"
    BUILD_DIR_NAME="build-esp32c6"
    DEFAULTS="sdkconfig.defaults"
    IDF_TARGET="esp32c6"
    ;;
  *)
    echo "Unsupported target: $TARGET_MODE" >&2
    usage
    exit 2
    ;;
esac

if [[ -z "$PRG32_ROOT" ]]; then
  echo "Missing PRG32 checkout. Set PRG32_ROOT or pass --prg32-root DIR." >&2
  exit 2
fi

PRG32_ROOT="$(cd "$PRG32_ROOT" && pwd -P)"
GAME_ROOT="$(cd "$(dirname "$0")/.." && pwd -P)"
BUILD_DIR="$PRG32_ROOT/$BUILD_DIR_NAME"
if [[ -z "$OUT_DIR" ]]; then
  OUT_DIR="$GAME_ROOT/dist/$TARGET_MODE"
fi
mkdir -p "$OUT_DIR"

BUILDER="$PRG32_ROOT/prg32/__main__.py"

if [[ ! -f "$BUILDER" ]]; then
  echo "Cannot find PRG32 module: $BUILDER" >&2
  exit 1
fi

if [[ "$SKIP_FIRMWARE" -eq 0 ]]; then
  echo "Building resident PRG32 firmware for $IDF_TARGET in $BUILD_DIR"
  (cd "$PRG32_ROOT" && idf.py -B "$BUILD_DIR" -D SDKCONFIG="$BUILD_DIR/sdkconfig" -D SDKCONFIG_DEFAULTS="$DEFAULTS" set-target "$IDF_TARGET")
  (cd "$PRG32_ROOT" && idf.py -B "$BUILD_DIR" -D SDKCONFIG="$BUILD_DIR/sdkconfig" -D SDKCONFIG_DEFAULTS="$DEFAULTS" build)
fi

ASM_CART="$OUT_DIR/you-have-got-pizza-asm.prg32"
C_CART="$OUT_DIR/you-have-got-pizza-c.prg32"

(cd "$PRG32_ROOT" && python3 -m prg32 cartridge build \
  "$GAME_ROOT/assembly/game.S" \
  --portable --architecture "$TARGET_MODE" \
  --entry-prefix you_have_got_pizza \
  --name "You Have Got Pizza ASM" \
  --out "$ASM_CART")

(cd "$PRG32_ROOT" && python3 -m prg32 cartridge build \
  "$GAME_ROOT/c/game.c" \
  --portable --architecture "$TARGET_MODE" \
  --entry-prefix you_have_got_pizza_c \
  --name "You Have Got Pizza C" \
  --out "$C_CART")

echo "Built cartridges:"
echo "  $ASM_CART"
echo "  $C_CART"

if [[ "$UPLOAD_QEMU" -eq 1 ]]; then
  if [[ "$TARGET_MODE" != "qemu" ]]; then
    echo "--upload-qemu can only be used with target qemu" >&2
    exit 2
  fi
  FLASH_BIN="$BUILD_DIR/qemu_flash.bin"
  if [[ ! -f "$FLASH_BIN" ]]; then
    echo "Missing $FLASH_BIN. Run PRG32 QEMU once to create the flash image:" >&2
    echo "  cd $PRG32_ROOT && idf.py -B $BUILD_DIR -D SDKCONFIG=$BUILD_DIR/sdkconfig -D SDKCONFIG_DEFAULTS=$DEFAULTS qemu --graphics monitor" >&2
    exit 1
  fi
  (cd "$PRG32_ROOT" && python3 -m prg32 qemu upload "$ASM_CART" --flash "$FLASH_BIN")
  echo "Staged assembly cartridge into $FLASH_BIN"
fi

if [[ "$UPLOAD_HARDWARE" -eq 1 ]]; then
  if [[ "$TARGET_MODE" != "esp32c6" ]]; then
    echo "--upload-hardware can only be used with target esp32c6" >&2
    exit 2
  fi
  (cd "$PRG32_ROOT" && python3 -m prg32 esp32c6 upload "$ASM_CART" --url "$UPLOAD_URL")
  echo "Uploaded assembly cartridge to $UPLOAD_URL"
fi
