# SPDX-License-Identifier: Apache-2.0
# Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

"""
Hardware video decoder for H.264/H.265 streams.

This module provides Python bindings to the VideoStream library's
hardware decoder functionality, supporting both V4L2 and Hantro backends.

Example:
    >>> from videostream.decoder import Decoder, DecoderCodec
    >>> decoder = Decoder(DecoderCodec.H264, fps=30)
    >>> with open('video.h264', 'rb') as f:
    ...     data = f.read()
    >>> code, bytes_used, frame = decoder.decode_frame(data)
    >>> if frame is not None:
    ...     print(f"Decoded frame: {frame.width}x{frame.height}")
"""

from ctypes import c_size_t, c_void_p, byref
from enum import IntEnum, IntFlag
from typing import Optional, Tuple

from typeguard import typechecked

from .library import lib
from .frame import Frame


class DecoderCodec(IntEnum):
    """
    Video codec type for hardware decoder.

    Specifies which video compression standard to use for decoding.
    Both codecs are supported via hardware acceleration on i.MX8.
    """

    H264 = 0
    """
    H.264/AVC (Advanced Video Coding) codec.

    Widely supported standard with good compression and compatibility.
    Recommended for maximum device compatibility.
    """

    HEVC = 1
    """
    H.265/HEVC (High Efficiency Video Coding) codec.

    Next-generation standard providing approximately 50% better compression
    than H.264 at equivalent quality.
    """


class CodecBackend(IntEnum):
    """
    Codec backend selection for encoder/decoder.

    Allows selection between V4L2 kernel driver and Hantro user-space
    library (libcodec.so) backends.
    """

    AUTO = 0
    """
    Auto-detect best available backend (default).

    Selection priority:
    1. Check VSL_CODEC_BACKEND environment variable
    2. Prefer V4L2 if device available and has M2M capability
    3. Fall back to Hantro if V4L2 unavailable
    """

    HANTRO = 1
    """
    Force Hantro/libcodec.so backend.

    Uses the proprietary VPU wrapper library.
    """

    V4L2 = 2
    """
    Force V4L2 kernel driver backend.

    Uses the vsi_v4l2 mem2mem driver for better performance.
    """


class DecodeReturnCode(IntFlag):
    """
    Return codes from decode operations.

    These codes can be combined (bitfield) to indicate multiple conditions.
    """

    SUCCESS = 0x0
    """Decode succeeded but no frame or initialization info available yet."""

    ERROR = 0x1
    """Decoder encountered an error."""

    INIT_INFO = 0x2
    """Decoder has been initialized with stream parameters."""

    FRAME_DECODED = 0x4
    """A decoded frame is available."""


class Decoder:
    """
    Hardware video decoder for H.264/H.265 streams.

    The decoder processes H.264 or H.265 NAL units and produces decoded frames.
    It automatically selects the best available backend (V4L2 or Hantro) unless
    explicitly specified.

    Args:
        codec: The video codec type (H.264 or H.265)
        fps: Expected frame rate (used for buffer management)
        backend: Optional backend selection (Auto, Hantro, or V4L2)

    Example:
        >>> decoder = Decoder(DecoderCodec.H264, fps=30)
        >>> code, bytes_used, frame = decoder.decode_frame(h264_data)
        >>> if DecodeReturnCode.FRAME_DECODED in code:
        ...     print(f"Got frame: {decoder.width}x{decoder.height}")
    """

    @typechecked
    def __init__(
        self,
        codec: DecoderCodec,
        fps: int,
        backend: Optional[CodecBackend] = None
    ):
        """
        Creates a new decoder instance.

        Args:
            codec: The video codec type (H.264 or H.265)
            fps: Expected frame rate (used for buffer management)
            backend: Optional backend selection. If None, uses automatic
                     detection.

        Raises:
            RuntimeError: If decoder creation fails
        """
        if backend is not None:
            self._ptr = lib.vsl_decoder_create_ex(
                int(codec), fps, int(backend))
        else:
            self._ptr = lib.vsl_decoder_create(int(codec), fps)

        if not self._ptr:
            raise RuntimeError(
                f"Failed to create decoder for codec {codec.name}")

    def __del__(self):
        """Releases the decoder and associated resources."""
        if hasattr(self, '_ptr') and self._ptr:
            lib.vsl_decoder_release(self._ptr)
            self._ptr = None

    @property
    def width(self) -> int:
        """
        Width of decoded frames in pixels.

        Only valid after decoder initialization (after first decode_frame()).
        """
        return lib.vsl_decoder_width(self._ptr)

    @property
    def height(self) -> int:
        """
        Height of decoded frames in pixels.

        Only valid after decoder initialization (after first decode_frame()).
        """
        return lib.vsl_decoder_height(self._ptr)

    @typechecked
    def decode_frame(
        self,
        data: bytes
    ) -> Tuple[DecodeReturnCode, int, Optional[Frame]]:
        """
        Decodes a frame from compressed video data.

        Args:
            data: H.264/H.265 NAL unit data to decode

        Returns:
            A tuple containing:
            - DecodeReturnCode: Status flags from the decode operation
            - int: Number of bytes consumed from the input data
            - Optional[Frame]: Decoded frame if available, None otherwise

        Raises:
            RuntimeError: If decoder encounters an unrecoverable error

        Example:
            >>> code, bytes_used, frame = decoder.decode_frame(h264_data)
            >>> if DecodeReturnCode.FRAME_DECODED in code:
            ...     # Process the decoded frame
            ...     process(frame)
            >>> elif DecodeReturnCode.INIT_INFO in code:
            ...     print(f"Initialized: {decoder.width}x{decoder.height}")
            >>> # Continue with remaining data
            >>> remaining_data = h264_data[bytes_used:]
        """
        bytes_used = c_size_t(0)
        output_frame = c_void_p(None)

        ret_code = lib.vsl_decode_frame(
            self._ptr,
            data,
            len(data),
            byref(bytes_used),
            byref(output_frame)
        )

        code = DecodeReturnCode(ret_code)

        if DecodeReturnCode.ERROR in code:
            raise RuntimeError("Decoder error during frame decode")

        frame = None
        if output_frame.value:
            # Create a Frame from the raw pointer (Frame constructor accepts
            # ptr=)
            frame = Frame(0, 0, 0, ptr=output_frame.value)

        return code, bytes_used.value, frame


# Backwards compatibility alias
DecoderInputCodec = DecoderCodec
