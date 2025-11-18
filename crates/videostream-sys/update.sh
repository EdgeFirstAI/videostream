#!/bin/sh

bindgen --dynamic-loading VideoStreamLibrary --allowlist-function 'vsl_.*' --allowlist-type 'vsl_.*' --allowlist-var 'VSL_.*' ../../include/videostream.h > src/ffi.rs