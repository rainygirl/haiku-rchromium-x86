#!/usr/bin/env python3
"""Video that plays on a 1.33 GHz Atom with no video hardware.

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

2. **Scale video frames nearest-neighbour.** The software renderer is the whole
   display pipeline on Haiku -- no GPU, and the GMA500's display engine has no
   scaler (its sprite plane is RGB-only and 1:1; see the VAIO P patch set's
   notes) -- so every decoded frame is resampled to the player's size on the
   CPU, once per frame. Measured on the 114 package with a 320x176 H.264 video
   shown at 980x540: 5 of 2446 frames dropped, and VizCompositorThread at 46%
   of the core -- almost all of it that bilinear resample. Nearest is one read
   per output pixel instead of four taps and a blend. On a local video that is
   CPU nobody was waiting for; on youtube.com it is CPU the page's own script
   is. (On the older Chromium 87 build the same change took drops from a
   third of all frames to none and the compositor from 42% to 18%.)
   `RCH_VIDEO_BILINEAR=1` restores bilinear.

Not yet built or measured on 114: this script was written and checked against
pristine 114.0.5735.199 copies of both files (two edits, then "already
applied" on a second run), but the cross-build environment was not available.
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

# 2. Nearest-neighbour video frames in the software renderer.
path = "%s/components/viz/service/display/software_renderer.cc" % root
s = open(path).read()
if "HaikuVideoNearest" in s:
    print("  software_renderer.cc: 이미 적용")
else:
    old = '#include "components/viz/service/display/software_renderer.h"\n'
    assert s.count(old) == 1, ("PATTERN NOT FOUND", path, "include")
    s = s.replace(old, old + "\n#include <stdlib.h>\n", 1)

    old = "namespace viz {\nnamespace {\n"
    assert s.count(old) == 1, ("PATTERN NOT FOUND", path, "namespace")
    new = ("namespace viz {\nnamespace {\n"
           "\n"
           "#if BUILDFLAG(IS_HAIKU)\n"
           "// Video frames are drawn nearest-neighbour unless RCH_VIDEO_BILINEAR is\n"
           "// set. This renderer is the whole display pipeline on Haiku and the\n"
           "// display hardware has no scaler, so every decoded frame is resampled\n"
           "// here on the CPU. On the VAIO P, bilinear dropped a third of the frames\n"
           "// of a 320x176 video shown at 980x540 and nearest dropped none.\n"
           "bool HaikuVideoNearest() {\n"
           "  static const bool nearest = getenv(\"RCH_VIDEO_BILINEAR\") == nullptr;\n"
           "  return nearest;\n"
           "}\n"
           "#endif\n")
    s = s.replace(old, new, 1)

    old = ("  SkSamplingOptions sampling(quad->nearest_neighbor ? SkFilterMode::kNearest\n"
           "                                                    : SkFilterMode::kLinear);\n"
           "  current_canvas_->drawImageRect(image, sk_uv_rect, quad_rect, sampling,\n")
    assert s.count(old) == 1, ("PATTERN NOT FOUND", path, "DrawTextureQuad sampling")
    new = ("#if BUILDFLAG(IS_HAIKU)\n"
           "  const bool nearest_neighbor =\n"
           "      quad->nearest_neighbor || (quad->is_video_frame && HaikuVideoNearest());\n"
           "#else\n"
           "  const bool nearest_neighbor = quad->nearest_neighbor;\n"
           "#endif\n"
           "  SkSamplingOptions sampling(nearest_neighbor ? SkFilterMode::kNearest\n"
           "                                              : SkFilterMode::kLinear);\n"
           "  current_canvas_->drawImageRect(image, sk_uv_rect, quad_rect, sampling,\n")
    s = s.replace(old, new, 1)
    open(path, "w").write(s)
    print("  software_renderer.cc: 비디오는 최근접 보간")
    done += 1

print("port-video: %d edits" % done)
