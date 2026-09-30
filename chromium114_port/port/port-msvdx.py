#!/usr/bin/env python3
"""Hardware H.264 on the GMA500's video decoder, wired into the renderer.

The decoder itself is files/media/gpu/haiku/ (HaikuMsvdxVideoDecoder, an
H264Decoder accelerator over psb_video's H.264 code and a userland MSVDX
driver). This script connects it:

1. media/gpu/BUILD.gn: //media/gpu/haiku becomes a public dependency of the
   media_gpu component on Haiku. media/gpu:common, which holds H264Decoder,
   is visible only inside //media/gpu/*, so the decoder has to live there.
2. media/renderers/default_decoder_factory.{h,cc}: a creator hook, tried
   before VPX, dav1d and FFmpeg. //media cannot depend on //media/gpu (the
   dependency runs the other way), so the renderer registers the creator.
   The external decoder factory is not an option: DefaultDecoderFactory
   only consults it when GPU video decoding is enabled, and this port runs
   with --disable-gpu.
3. content/renderer/media/media_factory.cc: registers the creator before
   the first DefaultDecoderFactory is made.

When the firmware is missing, RCH_MSVDX=0 is set or the hardware is taken,
the creator returns nothing or Initialize() fails, and FFmpeg decodes as
before.
"""
import sys

root = sys.argv[1]
done = 0


def edit(rel, marker, old, new, what):
    global done
    path = "%s/%s" % (root, rel)
    s = open(path).read()
    if marker in s:
        print("  %s: 이미 적용" % rel)
        return
    assert s.count(old) == 1, ("PATTERN NOT FOUND", rel, what, s.count(old))
    open(path, "w").write(s.replace(old, new, 1))
    print("  %s: %s" % (rel, what))
    done += 1


# 1. media/gpu depends on media/gpu/haiku
edit("media/gpu/BUILD.gn", '"//media/gpu/haiku"',
     "  if (use_v4l2_codec || use_vaapi) {\n    public_deps += [",
     "  if (is_haiku) {\n"
     "    # Hardware H.264 on the GMA500 (port-msvdx.py).\n"
     "    public_deps += [ \"//media/gpu/haiku\" ]\n"
     "  }\n\n"
     "  if (use_v4l2_codec || use_vaapi) {\n    public_deps += [",
     "media_gpu 가 media/gpu/haiku 에 의존")

# 2a. the hook's declaration
edit("media/renderers/default_decoder_factory.h", "SetHaikuVideoDecoderCreator",
     "};\n\n}  // namespace media",
     "};\n\n"
     "#if BUILDFLAG(IS_HAIKU)\n"
     "class MediaLog;\n"
     "class VideoDecoder;\n"
     "// A hardware video decoder that //media cannot depend on directly: the\n"
     "// renderer registers it (content/renderer/media/media_factory.cc) and\n"
     "// DefaultDecoderFactory tries it before the software decoders. It may\n"
     "// return nullptr when the hardware is not usable.\n"
     "using HaikuVideoDecoderCreator =\n"
     "    std::unique_ptr<VideoDecoder> (*)(MediaLog* media_log);\n"
     "MEDIA_EXPORT void SetHaikuVideoDecoderCreator(\n"
     "    HaikuVideoDecoderCreator creator);\n"
     "#endif\n\n"
     "}  // namespace media",
     "생성 훅 선언")
edit("media/renderers/default_decoder_factory.h", '#include "build/build_config.h"',
     '#include "base/memory/weak_ptr.h"\n',
     '#include "base/memory/weak_ptr.h"\n#include "build/build_config.h"\n',
     "build_config.h 포함")

# 2b. the hook and its use
edit("media/renderers/default_decoder_factory.cc", "g_haiku_video_decoder_creator",
     "namespace media {\n",
     "namespace media {\n\n"
     "#if BUILDFLAG(IS_HAIKU)\n"
     "namespace {\n"
     "HaikuVideoDecoderCreator g_haiku_video_decoder_creator = nullptr;\n"
     "}  // namespace\n\n"
     "void SetHaikuVideoDecoderCreator(HaikuVideoDecoderCreator creator) {\n"
     "  g_haiku_video_decoder_creator = creator;\n"
     "}\n"
     "#endif\n",
     "생성 훅 정의")
edit("media/renderers/default_decoder_factory.cc", "g_haiku_video_decoder_creator(media_log)",
     "#if BUILDFLAG(ENABLE_LIBVPX)\n"
     "  video_decoders->push_back(std::make_unique<OffloadingVpxVideoDecoder>());",
     "#if BUILDFLAG(IS_HAIKU)\n"
     "  // The GMA500's H.264 decoder, when the renderer registered it and the\n"
     "  // hardware is usable; FFmpeg below takes over otherwise.\n"
     "  if (g_haiku_video_decoder_creator) {\n"
     "    if (auto decoder = g_haiku_video_decoder_creator(media_log))\n"
     "      video_decoders->push_back(std::move(decoder));\n"
     "  }\n"
     "#endif\n\n"
     "#if BUILDFLAG(ENABLE_LIBVPX)\n"
     "  video_decoders->push_back(std::make_unique<OffloadingVpxVideoDecoder>());",
     "소프트웨어 디코더보다 먼저 하드웨어 디코더를 시도")

# 3. the renderer registers it
edit("content/renderer/media/media_factory.cc", "RegisterHaikuMsvdxVideoDecoder",
     "    decoder_factory_ = std::make_unique<media::DefaultDecoderFactory>(\n"
     "        std::move(external_decoder_factory));",
     "#if BUILDFLAG(IS_HAIKU)\n"
     "    media::RegisterHaikuMsvdxVideoDecoder();\n"
     "#endif\n"
     "    decoder_factory_ = std::make_unique<media::DefaultDecoderFactory>(\n"
     "        std::move(external_decoder_factory));",
     "MSVDX 디코더 등록")
edit("content/renderer/media/media_factory.cc", "media/gpu/haiku/haiku_msvdx_video_decoder.h",
     "namespace content {\n",
     "#if BUILDFLAG(IS_HAIKU)\n"
     "#include \"media/gpu/haiku/haiku_msvdx_video_decoder.h\"\n"
     "#endif\n\n"
     "namespace content {\n",
     "헤더 포함")

print("port-msvdx: %d edits" % done)
