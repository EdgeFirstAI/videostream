# vpu_wrapper library

This is a modified version of imx-vpuwrap library
The base is the ccaf10a0dae7c0d7d204bd64282598bc0e3bd661 commit from https://github.com/NXP/imx-vpuwrap.git repository.
Thre are three key functional changes:
1. Patched how chroma and luma addresses are handled (allows simpler implementation without extended frame info)
2. Exposed xOffset and yOffset parameters in top level interface to allow frame cropping
3. Introduced hantro_vc8000e_enc library wrapper. Previously this was linked as shared library, now it's interface compatible but internally each function uses dlsym to invoke the method from the library opened via dlopen. This allows for fallback implementation on platfrorms where hantro vc8000e encoder is not available.

libhantro_vc8000e.so.1 (imx-vpu-hantro-vc-1.3.0) is required for the wrapper to work. Library will be loaded from default library location or from path specified in `LIBHANTRO_VC8000E_LOCATION` environment variable.

Currently this library only supports hantro VC8000e encoder, but originally it could support all hantro encoders and decoders. This support could be added, but requires additional work and testing.

