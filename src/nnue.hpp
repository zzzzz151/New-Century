// clang-format off

#pragma once

#ifdef _MSC_VER
#define NC_MSVC
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#endif

#include "incbin.h"

#ifdef NC_MSVC
#pragma pop_macro("_MSC_VER")
#undef NC_MSVC
#endif

#include "simd.hpp"
using namespace SIMD;

namespace nnue {

const u16 HIDDEN_LAYER_SIZE = 128;
const i32 SCALE = 400, QA = 181, QB = 64;
constexpr int WEIGHTS_PER_VEC = sizeof(Vec) / sizeof(i16);

struct alignas(ALIGNMENT) Net {
    i16 featureWeights[768 * HIDDEN_LAYER_SIZE];
    i16 featureBiases[HIDDEN_LAYER_SIZE];
    i16 outputWeights[2][HIDDEN_LAYER_SIZE];
    i16 outputBias;
};

INCBIN(ValueNetFile, ValueNetFileName);
const Net *NET = reinterpret_cast<const Net*>(gValueNetFileData);

struct alignas(ALIGNMENT) Accumulator
{
    std::array<i16, HIDDEN_LAYER_SIZE> white, black;

    inline Accumulator()
    {
        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
            white[i] = black[i] = NET->featureBiases[i];
    }

    inline void activate(Color color, PieceType pieceType, Square sq)
    {
        int whiteIdx = (int)color * 384 + (int)pieceType * 64 + sq;
        int blackIdx = !(int)color * 384 + (int)pieceType * 64 + (sq ^ 56);

        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) {
            white[i] += NET->featureWeights[whiteIdx * HIDDEN_LAYER_SIZE + i];
            black[i] += NET->featureWeights[blackIdx * HIDDEN_LAYER_SIZE + i];
        }
    }

    inline void deactivate(Color color, PieceType pieceType, Square sq)
    {
        int whiteIdx = (int)color * 384 + (int)pieceType * 64 + sq;
        int blackIdx = !(int)color * 384 + (int)pieceType * 64 + (sq ^ 56);

        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) {
            white[i] -= NET->featureWeights[whiteIdx * HIDDEN_LAYER_SIZE + i];
            black[i] -= NET->featureWeights[blackIdx * HIDDEN_LAYER_SIZE + i];
        }
    }
};

inline i32 crelu(i16 x) {
    return x < 0 ? 0 : x > QA ? QA : x;
}

inline i32 screlu(i32 x) {
    i32 clamped = std::clamp(x, 0, QA);
    return clamped * clamped;
}

inline i32 evaluate(Accumulator &accumulator, Color color)
{
    Vec *stmAccumulator, *oppAccumulator;
    if (color == Color::WHITE) {
        stmAccumulator = (Vec*)&accumulator.white;
        oppAccumulator = (Vec*)&accumulator.black;
    }
    else {
        stmAccumulator = (Vec*)&accumulator.black;
        oppAccumulator = (Vec*)&accumulator.white;
    }

    Vec *stmWeights = (Vec*) &(NET->outputWeights[0]);
    Vec *oppWeights = (Vec*) &(NET->outputWeights[1]);
    const Vec vecZero = vecSetZero();
    const Vec vecQA = vecSet1Epi16(QA);
    Vec sum = vecSetZero();
    Vec reg;

    for (int i = 0; i < HIDDEN_LAYER_SIZE / WEIGHTS_PER_VEC; ++i) 
    {
        // Side to move
        reg = maxEpi16(stmAccumulator[i], vecZero); // clip
        reg = minEpi16(reg, vecQA); // clip
        reg = mulloEpi16(reg, reg); // square
        reg = maddEpi16(reg, stmWeights[i]); // multiply with output layer
        sum = addEpi32(sum, reg); // collect the result

        // Non side to move
        reg = maxEpi16(oppAccumulator[i], vecZero);
        reg = minEpi16(reg, vecQA);
        reg = mulloEpi16(reg, reg);
        reg = maddEpi16(reg, oppWeights[i]);
        sum = addEpi32(sum, reg);
    }

    return (vecHaddEpi32(sum) / QA + NET->outputBias) * SCALE / (QA * QB);
}

}

using Accumulator = nnue::Accumulator;
