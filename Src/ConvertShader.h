#pragma once
#include <cstdint>
#include <string>
#include <vector>

#if defined(__AVX__)
#include <immintrin.h>
#else
#include <emmintrin.h>
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "avisynth.h"

#pragma warning(disable: 4556)


enum arch_t {
    NO_SIMD,
    USE_SSE2,
    USE_SSSE3,
    USE_F16C,
};


using convert_shader_t = void(__stdcall*)(
    uint8_t** dstp, const uint8_t** srcp, const int dpitch, const int spitch, const int width, const int height, void*);



class ConvertShader : public GenericVideoFilter {
    std::string name;
    VideoInfo viSrc;
    int procWidth;
    int procHeight;
    int floatBufferPitch;
    float* buff;
    bool isPlusMt;
    std::vector<uint16_t> lut;
    bool useLut;

    void constructToShader(int precision, bool stack16, bool planar, arch_t arch);
    void constructFromShader(int precision, bool stack16, std::string& format, arch_t arch);
    convert_shader_t mainProc;

public:
    ConvertShader(PClip _child, int _precision, bool stack16, std::string& format, bool planar, int opt, IScriptEnvironment* env);
    ~ConvertShader();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	int __stdcall SetCacheHints(int cachehints, int frame_range);
};


convert_shader_t get_to_shader_packed(int precision, int pix_type, bool stack16, arch_t& arch);
convert_shader_t get_to_shader_planar(int precision, int pix_type, bool stack16, arch_t& arch);
convert_shader_t get_from_shader_packed(int precision, int pix_type, bool stack16, arch_t& arch);
convert_shader_t get_from_shader_planar(int precision, int pix_type, bool stack16, arch_t& arch);


static __forceinline __m128i loadl(const uint8_t* p)
{
    return _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p));
}


static __forceinline __m128i load(const uint8_t* p)
{
    return _mm_load_si128(reinterpret_cast<const __m128i*>(p));
}


static __forceinline void storel(uint8_t* p, const __m128i& x)
{
    _mm_storel_epi64(reinterpret_cast<__m128i*>(p), x);
}


static __forceinline void stream(uint8_t* p, const __m128i& x)
{
    _mm_stream_si128(reinterpret_cast<__m128i*>(p), x);
}


#if defined(__AVX__)
static __forceinline void
convert_float_to_half(uint8_t* dstp, const float* srcp, size_t count)
{
    for (size_t x = 0; x < count; x += 8) {
        __m256 s = _mm256_load_ps(srcp + x);
        __m128i d = _mm256_cvtps_ph(s, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
        stream(dstp + 2 * x, d);
    }
}


static __forceinline void
convert_half_to_float(float* dstp, const uint8_t* srcp, size_t count)
{
    for (size_t x = 0; x < count; x += 8) {
        __m128i s = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + 2 * x));
        __m256 d = _mm256_cvtph_ps(s);
        _mm256_store_ps(dstp + x, d);
    }
}
#endif

