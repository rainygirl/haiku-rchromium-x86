#!/usr/bin/env python3
"""Video that plays on a 1.33 GHz Atom: no AV1, VP9 opt-in, frames drawn nearest.

youtube.com on the 114 package, measured on the VAIO P: it picks AV1
(`av01.0.04M.08`, then `av01.0.00M.08` twice), and in 150 seconds decodes
**zero** frames. The player sits in buffering with the poster and a play
button on screen. Nothing throws and no media error is logged, so the only
visible symptom is a video that never starts.

Two things, both in the software paths this machine is limited to:

1. **Do not offer AV1, and make VP9 opt-in.** A site picks the best codec the
   browser claims to support. AV1 in software costs two to three times VP9,
   and VP9 about twice H.264 (libvpx against ffmpeg's SSSE3 paths). Every site
   that serves AV1 or VP9 also serves H.264, so answering "no" to the first two
   is what makes the site send the one this core can keep up with.
   `RCH_VP9=1` in the environment turns VP9 back on for a site that has
   nothing else. There is no switch for AV1: it cannot play here either way.

Measured on the VAIO with this change built into 114: youtube.com picks
H.264 (`avc1.4d4015`, 426x240) and plays -- 20 s of video in 140 s of
wall time, 434 of 602 frames dropped, with a Haiku kernel build taking 40% of
the same core the whole time. Before it: AV1, zero frames.

2. **Draw video frames nearest-neighbour.** An earlier note here said this
   changed nothing on 114 (VizCompositorThread at 43.6% nearest against 45.1%
   bilinear). That measurement was taken while Skia's raster pipeline was
   scalar -- see port/clang-x86 -- and the scalar pipeline's cost hid the
   filter's. With SkOpts.cpp built by clang, on the ABC News Australia live
   stream, same binary: VizCompositorThread 31.5% bilinear, 5.4% nearest, and
   YouTube stepped up from 144p@15 to 240p@30. RCH_VIDEO_BILINEAR=1 restores
   bilinear.
"""
import sys

root = sys.argv[1]
done = 0

# 1. Codec advertisement.
path = "%s/media/base/supported_types.cc" % root
s = open(path).read()
if "HaikuAdvertisesVp9" in s:
    print("  supported_types.cc: 이미 적용")
else:
    old = '#include "media/base/supported_types.h"\n'
    assert s.count(old) == 1, ("PATTERN NOT FOUND", path, "include")
    s = s.replace(old, old + "\n#include <stdlib.h>\n", 1)

    old = ("bool IsAV1Supported(const VideoType& type) {\n"
           "  // If the AV1 decoder is enabled, or if we're on Q or later, yes.\n"
           "#if BUILDFLAG(ENABLE_AV1_DECODER)\n")
    assert s.count(old) == 1, ("PATTERN NOT FOUND", path, "IsAV1Supported")
    new = ("#if BUILDFLAG(IS_HAIKU)\n"
           "// VP9 is advertised only when RCH_VP9 is set. See IsAV1Supported().\n"
           "bool HaikuAdvertisesVp9() {\n"
           "  static const bool advertise = getenv(\"RCH_VP9\") != nullptr;\n"
           "  return advertise;\n"
           "}\n"
           "#endif\n"
           "\n"
           "bool IsAV1Supported(const VideoType& type) {\n"
           "#if BUILDFLAG(IS_HAIKU)\n"
           "  // Not on Haiku. On the VAIO P (one 1.33 GHz Atom core, no video\n"
           "  // decode hardware) youtube.com picked AV1 and decoded zero frames in\n"
           "  // 150 s. AV1 in software costs two to three times VP9 and VP9 twice\n"
           "  // H.264, so saying no to both is what makes a site send H.264, the\n"
           "  // one format this core can keep up with. RCH_VP9=1 brings VP9 back.\n"
           "  return false;\n"
           "#endif\n"
           "  // If the AV1 decoder is enabled, or if we're on Q or later, yes.\n"
           "#if BUILDFLAG(ENABLE_AV1_DECODER)\n")
    s = s.replace(old, new, 1)

    old = ("    case VideoCodec::kVP9:\n"
           "      return IsVp9ProfileSupported(type);\n")
    assert s.count(old) == 1, ("PATTERN NOT FOUND", path, "kVP9")
    new = ("    case VideoCodec::kVP9:\n"
           "#if BUILDFLAG(IS_HAIKU)\n"
           "      if (!HaikuAdvertisesVp9())\n"
           "        return false;\n"
           "#endif\n"
           "      return IsVp9ProfileSupported(type);\n")
    s = s.replace(old, new, 1)
    open(path, "w").write(s)
    print("  supported_types.cc: AV1 끔, VP9는 RCH_VP9=1일 때만")
    done += 1

# 2. Video frames drawn nearest-neighbour by the software renderer.
path = "%s/components/viz/service/display/software_renderer.cc" % root
s = open(path).read()
if "HaikuVideoNearest" in s:
    print("  software_renderer.cc: 이미 적용")
else:
    old = '#include "build/build_config.h"\n'
    assert s.count(old) == 1, ("PATTERN NOT FOUND", path, "include")
    s = s.replace(old, old + "\n#if BUILDFLAG(IS_HAIKU)\n#include <stdlib.h>\n#endif\n", 1)
    old = "void SoftwareRenderer::DrawTextureQuad(const TextureDrawQuad* quad) {\n"
    assert s.count(old) == 1, ("PATTERN NOT FOUND", path, "DrawTextureQuad")
    new = ("#if BUILDFLAG(IS_HAIKU)\n"
           "// Video frames are resampled to the player's size here, on the CPU,\n"
           "// once per frame: there is no GPU and no scaler in the display\n"
           "// hardware. Bilinear is four taps and a blend per output pixel,\n"
           "// nearest is one read. RCH_VIDEO_BILINEAR=1 restores bilinear.\n"
           "static bool HaikuVideoNearest() {\n"
           "  static const bool nearest = getenv(\"RCH_VIDEO_BILINEAR\") == nullptr;\n"
           "  return nearest;\n"
           "}\n"
           "#endif\n"
           "\n" + old)
    s = s.replace(old, new, 1)
    old = ("  SkSamplingOptions sampling(quad->nearest_neighbor ? SkFilterMode::kNearest\n"
           "                                                    : SkFilterMode::kLinear);\n"
           "  current_canvas_->drawImageRect(image, sk_uv_rect, quad_rect, sampling,\n")
    assert s.count(old) == 1, ("PATTERN NOT FOUND", path, "texture sampling")
    new = ("  bool nearest = quad->nearest_neighbor;\n"
           "#if BUILDFLAG(IS_HAIKU)\n"
           "  nearest = nearest || (quad->is_video_frame && HaikuVideoNearest());\n"
           "#endif\n"
           "  SkSamplingOptions sampling(nearest ? SkFilterMode::kNearest\n"
           "                                     : SkFilterMode::kLinear);\n"
           "  current_canvas_->drawImageRect(image, sk_uv_rect, quad_rect, sampling,\n")
    s = s.replace(old, new, 1)
    open(path, "w").write(s)
    print("  software_renderer.cc: 동영상 프레임 nearest (RCH_VIDEO_BILINEAR=1로 끔)")
    done += 1

print("port-video: %d edits" % done)
