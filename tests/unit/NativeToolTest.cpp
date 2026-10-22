// Builds with the standalone tool policy: shared header-only helpers must not require
// linking the application's business libraries to make their include paths available.
#include "Require.h"
#include "NumericChecks.h"
#include <iostream>
#include <limits>
#include <string_view>

int main()
{
    try {
        using yalahoo::test::errorMagnitude;
        yalahoo::require(errorMagnitude(-.25) == .25 && errorMagnitude(0) == 0
                                 && errorMagnitude(.5) == .5,
                         "Numerical comparison changed finite differences");
        for (const double invalid :
             { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
               -std::numeric_limits<double>::infinity() }) {
            bool rejected = false;
            try {
                static_cast<void>(errorMagnitude(invalid));
            } catch (const std::runtime_error &error) {
                rejected = std::string_view(error.what()) == "Nonfinite numerical comparison";
            }
            yalahoo::require(rejected, "Nonfinite differences must fail before error reduction");
        }
        yalahoo::require(true, "unexpected rejection");
        try {
            yalahoo::require(false, "asset fixture error");
        } catch (const std::runtime_error &error) {
            if (std::string_view(error.what()) == "asset fixture error")
                return 0;
            std::cerr << "Resource failure lost its diagnostic\n";
            return 1;
        }
        std::cerr << "Invalid resources must fail at the catchable boundary\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
