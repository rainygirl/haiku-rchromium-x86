// Hardware H.264 decoding on the Intel GMA500 (Poulsbo) video decoder,
// an Imagination VXD370 ("MSVDX"), for Haiku.
//
// Chromium's own H264Decoder parses the stream and manages the DPB; this
// file supplies its H264Accelerator, which turns the parsed structures into
// the VA-API parameter structures psb_video's H.264 code expects and drives
// the hardware from userland (msvdx/). Pictures are copied out as I420.
//
// Used only when the firmware is present and /dev/misc/poke opens; otherwise
// Initialize() fails and the decoder stream falls back to FFmpeg.
#ifndef MEDIA_GPU_HAIKU_HAIKU_MSVDX_VIDEO_DECODER_H_
#define MEDIA_GPU_HAIKU_HAIKU_MSVDX_VIDEO_DECODER_H_

#include <memory>

#include "media/base/media_log.h"
#include "media/base/video_decoder.h"

namespace media {

// Returns nullptr if hardware decoding is switched off (RCH_MSVDX=0) or the
// firmware cannot be found. Otherwise a decoder whose Initialize() may still
// fail, for instance when another video already holds the hardware.
std::unique_ptr<VideoDecoder> CreateHaikuMsvdxVideoDecoder(MediaLog* media_log);

// Hooks the decoder into DefaultDecoderFactory (see default_decoder_factory.h).
void RegisterHaikuMsvdxVideoDecoder();

}  // namespace media

#endif  // MEDIA_GPU_HAIKU_HAIKU_MSVDX_VIDEO_DECODER_H_
