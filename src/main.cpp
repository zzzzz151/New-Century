// clang-format off

#include "board.hpp"
#include "searcher.hpp"
#include "uci.hpp"

int main() {
    std::cout << "New Century by zzzzz" << std::endl;

    #if defined(__AVX512F__) && defined(__AVX512BW__)
        std::cout << "Using avx512" << std::endl;
    #elif defined(__AVX2__)
        std::cout << "Using avx2" << std::endl;
    #else
        std::cout << "Not using avx2 or avx512" << std::endl;
    #endif

    initUtils();
    initZobrist();
    attacks::init();
    Searcher searcher = Searcher(START_BOARD);
    uci::uciLoop(searcher);
    return 0;
}


