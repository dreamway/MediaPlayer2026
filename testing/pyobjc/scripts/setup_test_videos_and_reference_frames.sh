#!/usr/bin/env bash
set -euo pipefail

# One-click setup:
# - Extract reference frames from existing test videos in testing/video/
# - Reference frames are saved to testing/pyobjc/reference_frames/
#
# Prerequisites:
#   - Test videos should already exist in testing/video/
#   - ffmpeg must be installed
#
# Usage:
#   ./testing/pyobjc/scripts/setup_test_videos_and_reference_frames.sh
#

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
VIDEO_DIR="${ROOT_DIR}/testing/video"
REF_DIR="${ROOT_DIR}/testing/pyobjc/reference_frames"

mkdir -p "${REF_DIR}"

echo "=== Extracting Reference Frames from Test Videos ==="
echo "Video directory: ${VIDEO_DIR}"
echo "Reference frame directory: ${REF_DIR}"
echo ""

# 检查 ffmpeg 是否安装
if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ERROR: ffmpeg is required but not installed."
  echo "Install with: brew install ffmpeg"
  exit 1
fi

# 定义要提取关键帧的视频和时间点
# 格式: "视频文件名|提取时间点|参考帧名称"
declare -a EXTRACTIONS=(
  # SMPTE 色条测试视频 - 第1秒
  "test_smpte_640x480_5s.mp4|1|reference_smpte_640x480.png"
  # testsrc 测试视频 - 第1秒
  "test_testsrc_640x480_5s.mp4|1|reference_testsrc_640x480.png"
  # Big Buck Bunny 1080p - 第5秒 (经典向日葵场景)
  "bbb_sunflower_1080p_30fps_normal.mp4|5|reference_bbb_normal_5s.png"
  # Big Buck Bunny 1080p 立体版 - 第5秒
  "bbb_sunflower_1080p_30fps_stereo_abl.mp4|5|reference_bbb_stereo_abl_5s.png"
  # 黑神话悟空 2D - 第5秒
  "黑神话悟空2D3D对比宽屏-40S.mp4|5|reference_wukong_2d3d_5s.png"
  # 黑神话悟空 4K - 第5秒
  "黑神话悟空4K-40S.mp4|5|reference_wukong_4k_5s.png"
  # 医疗3D演示 - 第5秒
  "医疗3D演示4k5-2.mp4|5|reference_medical_3d_5s.png"
  # 通用测试视频 - 第2秒
  "test.mp4|2|reference_test_2s.png"
)

extracted_count=0
skipped_count=0

for entry in "${EXTRACTIONS[@]}"; do
  IFS='|' read -r video_file timestamp ref_name <<< "${entry}"
  video_path="${VIDEO_DIR}/${video_file}"
  ref_path="${REF_DIR}/${ref_name}"

  if [[ ! -f "${video_path}" ]]; then
    echo "  [SKIP] Video not found: ${video_file}"
    ((skipped_count++)) || true
    continue
  fi

  echo "  [EXTRACT] ${video_file} @ ${timestamp}s -> ${ref_name}"
  if ffmpeg -hide_banner -y -ss "00:00:${timestamp}" -i "${video_path}" -frames:v 1 \
    "${ref_path}" 2>/dev/null; then
    ((extracted_count++)) || true
  else
    echo "    [ERROR] Failed to extract frame"
    ((skipped_count++)) || true
  fi
done

echo ""
echo "=== Summary ==="
echo "  Extracted: ${extracted_count} reference frames"
echo "  Skipped:   ${skipped_count} (video not found or error)"
echo ""
echo "=== Available Test Videos ==="
if [[ -d "${VIDEO_DIR}" ]]; then
  ls -1 "${VIDEO_DIR}"/*.mp4 2>/dev/null | while read -r f; do
    basename "${f}"
  done | sed 's/^/  - /'
fi

echo ""
echo "=== Generated Reference Frames ==="
if [[ -d "${REF_DIR}" ]]; then
  ls -1 "${REF_DIR}"/*.png 2>/dev/null | while read -r f; do
    basename "${f}"
  done | sed 's/^/  - /'
fi

