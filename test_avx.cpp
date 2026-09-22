#include <immintrin.h>
#include <iostream>

int main() {
    __m256 v = _mm256_set1_ps(1.0f);
    float out[8];
    _mm256_storeu_ps(out, v);
    std::cout << "AVX2 instruction executed! Value: " << out[0] << "\n";
    return 0;
}
