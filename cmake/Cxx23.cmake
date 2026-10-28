include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
    [=[
    #include <optional>
    #include <utility>
    #include <type_traits>
    enum class Probe {
        value = 7
    };
    static_assert(std::to_underlying(Probe::value) == 7);
    constexpr bool probe()
    {
        return std::optional<int>(3)
                       .transform([](int value) {
                           return value + 4;
                       })
                       .value()
                == 7;
    }
    static_assert(probe());
    int main()
    {
        return probe() ? 0 : 1;
    }
]=]
    YALAHOO_HAS_CXX23
)
if(NOT YALAHOO_HAS_CXX23)
    message(FATAL_ERROR "C++23 language/library probe failed; no standard downgrade is allowed")
endif()
