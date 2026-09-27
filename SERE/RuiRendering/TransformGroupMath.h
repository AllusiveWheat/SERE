#pragma once
#include "RenderManager.h"
#include <limits>

// engine.dll F79A0: bounds stay in transform coordinate space (no pixel
// conversion). Rebase each record's translation by the minimum corner.
inline __m128 RebaseTransformGroup(std::vector<TransformResult>& records) {
    if (records.empty()) return _mm_setzero_ps();
    __m128 lo = _mm_set1_ps(std::numeric_limits<float>::max());
    __m128 hi = _mm_set1_ps(std::numeric_limits<float>::lowest());
    for (const auto& record : records) {
        const __m128 a = _mm_shuffle_ps(record.directionVector, record.directionVector, 0x44);
        const __m128 b = _mm_shuffle_ps(record.directionVector, record.directionVector, 0xEE);
        lo = _mm_min_ps(lo, _mm_add_ps(_mm_add_ps(_mm_min_ps(a, _mm_setzero_ps()), record.position), _mm_min_ps(b, _mm_setzero_ps())));
        hi = _mm_max_ps(hi, _mm_add_ps(_mm_add_ps(_mm_max_ps(a, _mm_setzero_ps()), record.position), _mm_max_ps(b, _mm_setzero_ps())));
    }
    for (auto& record : records) record.position = _mm_sub_ps(record.position, lo);
    const __m128 extent = _mm_sub_ps(hi, lo);
    return _mm_shuffle_ps(extent, extent, 0xD8);
}

// engine.dll F4090. In particular, the cross term used for translation is
// derived from the original basis. Do not replace this with a generic affine
// matrix product: the engine's instruction sequence is the export contract.
inline void ApplyTransform12(std::vector<TransformResult>& records, const TransformResult& frame) {
    const __m128 normalized = _mm_mul_ps(frame.directionVector,
        NRReciprocal(_mm_max_ps(_mm_set1_ps(std::numeric_limits<float>::min()), frame.inputSize)));
    const __m128 diagonal = _mm_shuffle_ps(normalized, normalized, 0xCC);
    const __m128 cross = _mm_shuffle_ps(normalized, normalized, 0x66);
    for (auto& record : records) {
        const __m128 term = _mm_mul_ps(_mm_shuffle_ps(record.directionVector, record.directionVector, 0xB1), cross);
        record.directionVector = _mm_add_ps(_mm_mul_ps(record.directionVector, diagonal), term);
        record.position = _mm_add_ps(_mm_add_ps(_mm_mul_ps(record.position, diagonal), term), frame.position);
    }
}
