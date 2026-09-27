#include "RuiRendering/TransformGroupMath.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

static __m128 lanes(float a, float b, float c, float d) { return _mm_set_ps(d, c, b, a); }
static void expect(__m128 actual, __m128 expected) {
    float a[4], b[4]; _mm_storeu_ps(a, actual); _mm_storeu_ps(b, expected);
    for (int i = 0; i < 4; ++i)
        if (!std::isfinite(a[i]) || std::abs(a[i] - b[i]) > 0.0001f) throw std::runtime_error("Transform math mismatch");
}

int main() {
    std::vector<TransformResult> records {
        {lanes(2,0,0,3), lanes(-4,2,-4,2), lanes(2,2,3,3), 1},
        {lanes(-1,0,0,2), lanes(5,-1,5,-1), lanes(1,1,2,2), 2}
    };
    const auto bounds = RebaseTransformGroup(records);
    expect(bounds, lanes(9,9,6,6));
    expect(records[0].position, lanes(0,3,0,3));
    expect(records[1].position, lanes(9,0,9,0));

    // A later placement holds the measured size when type 12 consumes it.
    // Its basis represents a parent with x/y scale 2/3 respectively.
    TransformResult placement(lanes(18,0,0,18), lanes(.25f,.5f,.25f,.5f), bounds, 4);
    ApplyTransform12(records, placement);
    expect(records[0].directionVector, lanes(4,0,0,9));
    expect(records[0].position, lanes(.25f,9.5f,.25f,9.5f));
    expect(records[1].position, lanes(18.25f,.5f,18.25f,.5f));
    expect(records[0].inputSize, lanes(2,2,3,3));

    std::vector<TransformResult> cross {{lanes(2,0,0,3), lanes(5,7,5,7), _mm_set1_ps(1), 5}};
    TransformResult rotated(lanes(0,2,-3,0), lanes(.25f,.5f,.25f,.5f), _mm_set1_ps(1), 6);
    ApplyTransform12(cross, rotated);
    expect(cross[0].directionVector, lanes(0,4,-9,0));
    expect(cross[0].position, lanes(.25f,4.5f,-8.75f,.5f));
    std::vector<TransformResult> empty;
    expect(RebaseTransformGroup(empty), _mm_setzero_ps());
    std::cout << "Transform-group math tests passed.\n";
}
