#ifndef QUAN_P4ML_H
#define QUAN_P4ML_H
#include <immintrin.h>
#include <stdint.h>

// scale up float then translate it to int
// without any further optimization
inline static void quantizeNaive(char *data_ptr, uint32_t size)
{
    int factor = 1000000;
    int *int_data_ptr = (int *)data_ptr;
    float *float_data_ptr = (float *)data_ptr;
    for (uint32_t i = 0; i < size; i++)
    {
        int_data_ptr[i] = (int)(float_data_ptr[i] * factor);
    }
}

// translate back to float and scale down
// without any further optimization
inline static void dequantizeNaive(char *data_ptr, uint32_t size)
{
    float factor = 1000000.0;
    int *int_data_ptr = (int *)data_ptr;
    float *float_data_ptr = (float *)data_ptr;
    for (uint32_t i = 0; i < size; i++)
    {
        float_data_ptr[i] = (float)(int_data_ptr[i] / factor);
    }
}

// functioned the same as quantizeNaive
// boost with avx 256 instructions
inline static void quantizeAVX2(char *data_ptr, uint32_t size)
{
    // check alignment

    __m256 input;
    __m256i output;

    int unaligned_size = size % 8;
    int aligned_size = size / 8;

    const float factor = 1000000.0;
    float *float_data_ptr = (float *)data_ptr;
    int *int_data_ptr = (int *)data_ptr;

    // 0xF4240 is 1000000 in hex
    __m256 factor_in_avx = _mm256_broadcast_ss(&factor);

    for (uint32_t i = 0; i < aligned_size; i++)
    {
        float *current_pos = float_data_ptr + i * 8;
        input = _mm256_loadu_ps(current_pos);
        input = _mm256_mul_ps(input, factor_in_avx);
        output = _mm256_cvtps_epi32(input);
        _mm256_storeu_si256((__m256i *)current_pos, output);
    }

    for (uint32_t i = 0; i < unaligned_size; i++)
    {
        int_data_ptr[aligned_size * 8 + i] =
            (int)(float_data_ptr[aligned_size * 8 + i] * factor);
    }
}

// functioned the same as dequantizeNaive
// boost with avx 256 instructions
inline static void dequantizeAVX2(char *data_ptr, uint32_t size)
{
    __m256i input;
    __m256 output;

    int unaligned_size = size % 8;
    int aligned_size = size / 8;

    const float factor = 1000000.0;
    int *int_data_ptr = (int *)data_ptr;
    float *float_data_ptr = (float *)data_ptr;

    // __m256i* input_avx = (__m256i*) data_ptr;
    __m256 factor_in_avx = _mm256_broadcast_ss(&factor);

    for (uint32_t i = 0; i < aligned_size; i++)
    {
        float *current_pos = float_data_ptr + i * 8;
        input = _mm256_loadu_si256((__m256i *)current_pos);
        output = _mm256_cvtepi32_ps(input);
        output = _mm256_div_ps(output, factor_in_avx);
        _mm256_storeu_ps(current_pos, output);
    }

    for (uint32_t i = 0; i < unaligned_size; i++)
    {
        float_data_ptr[aligned_size * 8 + i] =
            (float)(int_data_ptr[aligned_size * 8 + i] / factor);
    }
}

// functioned the same as quantizeNaive
// boost with avx 256 instructions
inline static void quantizeAVX2to(char *dst_ptr, char *src_ptr, uint32_t size)
{
    // check alignment

    __m256 input;
    __m256i output;

    int unaligned_size = size % 8;
    int aligned_size = size / 8;

    const float factor = 1000000.0;
    float *float_data_ptr = (float *)src_ptr;
    int *int_data_ptr = (int *)src_ptr;

    float *dst_float_data_ptr = (float *)dst_ptr;
    int *dst_int_data_ptr = (int *)dst_ptr;

    // 0xF4240 is 1000000 in hex
    __m256 factor_in_avx = _mm256_broadcast_ss(&factor);

    for (uint32_t i = 0; i < aligned_size; i++)
    {
        float *current_pos = float_data_ptr + i * 8;
        float *current_dst_pos = dst_float_data_ptr + i * 8;

        input = _mm256_loadu_ps(current_pos);
        input = _mm256_mul_ps(input, factor_in_avx);
        output = _mm256_cvtps_epi32(input);
        _mm256_storeu_si256((__m256i *)current_dst_pos, output);
    }

    for (uint32_t i = 0; i < unaligned_size; i++)
    {
        dst_int_data_ptr[aligned_size * 8 + i] =
            (int)(float_data_ptr[aligned_size * 8 + i] * factor);
    }
}

// functioned the same as dequantizeNaive
// boost with avx 256 instructions
inline static void dequantizeAVX2to(char *dst_ptr, char *src_ptr,
                                    uint32_t size)
{
    __m256i input;
    __m256 output;

    int unaligned_size = size % 8;
    int aligned_size = size / 8;

    const float factor = 1000000.0;
    int *int_data_ptr = (int *)src_ptr;
    float *float_data_ptr = (float *)src_ptr;

    int *dst_int_data_ptr = (int *)dst_ptr;
    float *dst_float_data_ptr = (float *)dst_ptr;

    // __m256i* input_avx = (__m256i*) src_ptr;
    __m256 factor_in_avx = _mm256_broadcast_ss(&factor);

    for (uint32_t i = 0; i < aligned_size; i++)
    {
        float *current_pos = float_data_ptr + i * 8;
        float *current_dst_pos = dst_float_data_ptr + i * 8;

        input = _mm256_loadu_si256((__m256i *)current_pos);
        output = _mm256_cvtepi32_ps(input);
        output = _mm256_div_ps(output, factor_in_avx);
        _mm256_storeu_ps(current_dst_pos, output);
    }

    for (uint32_t i = 0; i < unaligned_size; i++)
    {
        dst_float_data_ptr[aligned_size * 8 + i] =
            (float)(int_data_ptr[aligned_size * 8 + i] / factor);
    }
}

#endif