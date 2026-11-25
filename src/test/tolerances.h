#pragma once

namespace lambda {
    namespace tolerances {
        inline constexpr Float FINE = Float(1e-6);
        inline constexpr Float ACCUMULATED = Float(1e-4);
        inline constexpr Float STANDARD = Float(1e-5);
        inline constexpr Float COARSE = Float(1e-3);
        inline constexpr Float STATISTICAL = Float(0.05);
    }
}