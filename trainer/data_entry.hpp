// clang-format off

#pragma once

#include "types.hpp"
#include "utils.hpp"

#pragma pack(push, 1)
struct DataEntry {
    public:

    Color stm = Color::NONE;
    u8 numActiveInputs = 0;
    std::vector<i16> activeInputs = { };
    u8 numMoves = 0;
    std::vector<i16> movesIdxs = { };
    u16 bestMoveIdx = 4096;

    inline DataEntry() = default;

    inline std::string toString()
    {
        return "stm " + std::to_string((int)stm)
               + " numActiveInputs " + std::to_string((int)numActiveInputs)
               + "\nactiveInputs " + vecToString(activeInputs)
               + "numMoves " +  std::to_string((int)numMoves)
               + "\nmovesIdxs " + vecToString(movesIdxs)
               + "bestMoveIdx " + std::to_string(bestMoveIdx);
    }

    inline auto size()
    {
        return sizeof(stm) 
               + sizeof(numActiveInputs) 
               + 2 * activeInputs.size() 
               + sizeof(numMoves) 
               + 2 * movesIdxs.size() 
               + sizeof(bestMoveIdx);
    }
};
#pragma pack(pop)