#!/bin/bash
#
# Script to extract a frame from a clip and store it in a JPEG file
#
# Usage: extractFrame.sh [-l label] [-f frame] [-o out.jpg] [clip.mkv]
#   clip.mkv  clip to read (default: newest ~/at/clips/<label>_*.mkv)
#   -l        clip label to search for when no clip is given (default: test)
#   -f        frame number to extract, starting at 0 (default: 90, i.e. 1.5 s in at 60 fps)
#   -o        output JPEG (default: ~/at/<label>.jpg)

CLIPS_DIR="${HOME}/at/clips"
LABEL="test"
FRAME_NUM=90
JPEG_PATH=""

usage() {
    sed -n '5,9p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
}

while getopts "l:f:o:h" opt; do
    case ${opt} in
        l) LABEL="${OPTARG}" ;;
        f) FRAME_NUM="${OPTARG}" ;;
        o) JPEG_PATH="${OPTARG}" ;;
        *) usage ;;
    esac
done
shift $((OPTIND - 1))

[[ "${FRAME_NUM}" =~ ^[0-9]+$ ]] || { echo "frame number must be a non-negative integer" >&2; exit 1; }

CLIP="$1"
if [[ -z "${CLIP}" ]]; then
    CLIP=$(ls -t "${CLIPS_DIR}/${LABEL}"_*.mkv 2>/dev/null | head -1)
    if [[ -z "${CLIP}" ]]; then
        echo "no ${LABEL}_*.mkv clips in ${CLIPS_DIR}" >&2
        exit 1
    fi
fi
[[ -f "${CLIP}" ]] || { echo "clip not found: ${CLIP}" >&2; exit 1; }
JPEG_PATH="${JPEG_PATH:-${HOME}/at/${LABEL}.jpg}"

# same reader as calibrate.py: GStreamer demux + cv2.imdecode, skipping any data
# before SOI, so it works on misframed clips that cv2.VideoCapture can't read
SRC_DIR="$(cd "$(dirname "$0")/../src" && pwd)"

python3 - "${CLIP}" "${FRAME_NUM}" "${JPEG_PATH}" "${SRC_DIR}" <<'EOF'
import sys

clip, frame_num, out, src_dir = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
sys.path.insert(0, src_dir)
import cv2
from calibrate import decode, jpeg_frames

n_frames = 0
for idx, jpeg in jpeg_frames(clip):
    n_frames = idx + 1
    if idx == frame_num:
        frame, misframed = decode(jpeg, cv2.IMREAD_COLOR)
        if frame is None:
            sys.exit(f"can't decode frame {frame_num} of {clip}")
        if not cv2.imwrite(out, frame):
            sys.exit(f"can't write {out}")
        note = " (misframed: skipped data before SOI)" if misframed else ""
        print(f"wrote frame {frame_num} of {clip} to {out}{note}")
        break
else:
    sys.exit(f"can't read frame {frame_num} of {clip} ({n_frames} frames)")
EOF
