// Builds with the standalone tool policy: shared header-only helpers must not require
// linking the application's business libraries to make their include paths available.
#include "Require.h"
#include <iostream>
#include <string_view>

int main()
{
    try {
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
