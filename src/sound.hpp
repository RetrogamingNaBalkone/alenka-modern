#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace alenka {
// Small synthesized cues, independent of SDL and original DOS resources.
inline std::vector<std::int16_t> sound_samples(bool error) {
    constexpr unsigned rate=48000;
    const unsigned count=error?3840:720; // 80 ms error / 15 ms metronome.
    const double frequency=error?220.0:1800.0;
    std::vector<std::int16_t> samples(count);
    for(unsigned i=0;i<count;++i) {
        const double attack=std::min(1.0,double(i)/96.0);
        const double decay=1.0-double(i)/double(count-1);
        samples[i]=std::int16_t(6000.0*attack*decay*
            std::sin(6.283185307179586*frequency*double(i)/rate));
    }
    return samples;
}
}
