#!/bin/bash
#
# Script to calibrate a camera using one or more calibration clips
#
# Usage: doCalibration.sh [-b board.json] [-n] [clip.mkv ...] [-- calibrate.py options]
#   clip.mkv  calibration clip(s), views pooled (default: newest ~/at/clips/calib_*.mkv)
#   -b        board description (default: ~/at/board_letter/board.json)
#   -n        print the command without running it
#   anything after -- is passed to calibrate.py (e.g., -- --out /tmp/calib --stride 5)

CLIPS_DIR="${HOME}/at/clips"
BOARD_PATH="${HOME}/at/board_letter/board.json"
DRY_RUN=0

usage() {
    sed -n '5,9p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
}

while getopts "b:nh" opt; do
    case ${opt} in
        b) BOARD_PATH="${OPTARG}" ;;
        n) DRY_RUN=1 ;;
        *) usage ;;
    esac
done
# getopts eats a "--" that comes before any clip path; note whether it did
ENDED_BY_DASHDASH=0
PREV=$((OPTIND - 1))
[[ ${PREV} -gt 0 && "${!PREV}" == "--" ]] && ENDED_BY_DASHDASH=1
shift $((OPTIND - 1))

# optional clip paths, then optional "--" and pass-through options
CLIPS=()
if [[ ${ENDED_BY_DASHDASH} -eq 0 ]]; then
    while [[ $# -gt 0 && "$1" != "--" ]]; do
        CLIPS+=("$1")
        shift
    done
fi
[[ "$1" == "--" ]] && shift

if [[ ${#CLIPS[@]} -eq 0 ]]; then
    NEWEST=$(ls -t "${CLIPS_DIR}"/calib_*.mkv 2>/dev/null | head -1)
    if [[ -z "${NEWEST}" ]]; then
        echo "no calib_*.mkv clips in ${CLIPS_DIR}" >&2
        exit 1
    fi
    echo "using newest calibration clip: ${NEWEST}"
    CLIPS=("${NEWEST}")
fi
for i in "${!CLIPS[@]}"; do
    [[ -f "${CLIPS[$i]}" ]] || { echo "clip not found: ${CLIPS[$i]}" >&2; exit 1; }
    CLIPS[$i]=$(realpath "${CLIPS[$i]}")  # absolute, since we cd below
done
[[ -f "${BOARD_PATH}" ]] || { echo "board file not found: ${BOARD_PATH}" >&2; exit 1; }
BOARD_PATH=$(realpath "${BOARD_PATH}")

# run from the repo root so src/ resolves wherever this is called from
cd "$(dirname "$0")/.." || exit 1

CMD=(python3 src/calibrate.py "${CLIPS[@]}" --board "${BOARD_PATH}" "$@")
if [[ ${DRY_RUN} -eq 1 ]]; then
    echo "${CMD[@]}"
else
    "${CMD[@]}"
fi
