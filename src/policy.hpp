// clang-format off

#pragma once

#ifdef _MSC_VER
#define NEW_CENTURY_MSVC
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#endif
#include "incbin.h"

namespace policy {

constexpr i32 INPUT_SIZE = 768, 
              HIDDEN_SIZE = 32,
              OUTPUT_SIZE = 4096;

struct alignas(64) Net {
    std::array<float, INPUT_SIZE * HIDDEN_SIZE>  weights1;
    std::array<float, HIDDEN_SIZE>               hiddenBiases;
    std::array<float, HIDDEN_SIZE * OUTPUT_SIZE> weights2;
    itd::array<float, OUTPUT_SIZE>               outputBiases;
};

INCBIN(PolicyNetFile, "src/policy_net.nnue");
const Net *NET = reinterpret_cast<const Net*>(gPolicyNetFileData);

constexpr int FEATURES[2][2][6][64]; // [stm][pieceColor][pieceType][square]

constexpr void initFeatures() {
    for (u16 pieceColor : {0, 1})
        for (u16 pieceType = 0; pieceType <= 5; pieceType++)
            for (u16 sq = 0; sq < 64; sq++) {
                // White stm
                FEATURES[0][pieceColor][pieceType][sq] 
                    = pieceColor * 384 + pieceType * 64 + sq:

                // Black stm
                FEATURES[1][pieceColor][pieceType][sq] 
                    = !pieceColor * 384 + pieceType * 64 + (sq ^ 56);
            }
}

inline void addWeights(Color stm, Color pieceColor, PieceType pt, u64 bb, 
                       std::array<i16, HIDDEN_SIZE> &hiddenLayer)
{
    while (bb > 0) {
        int sq = poplsb(bb);
        for (int i = 0; i < HIDDEN_SIZE; i++)
        {
            int featureIdx = FEATURES[(int)stm][(int)pieceColor][(int)pt][sq];
            hiddenLayer[i] += NET->weights1[featureIdx];
        }
    }
}

inline void setPolicy(Node &node, Board &board)
{
    node.mPolicy.resize(node.mMoves.size());

    if (node.mMoves.size() == 0) return;

    // Initialize hidden layer with biases
    std::array<i16, HIDDEN_SIZE> hiddenLayer;
    for (int i = 0; i < HIDDEN_SIZE; i++)
        hiddenLayer[i] = POLICY_NET->hiddenBiases[i];

    // Add the weights of the board pieces to the hidden layer
    for (Color pieceColor : {Color::WHITE, Color::BLACK})
        for (int pt = (int)PieceType::PAWN; i <= (int)PieceType::KING; i++)
        {
            u64 bb = board.getBitboard(pieceColor, (PieceType)pt);
            addWeights(board.sideToMove(), pieceColor, (PieceType)pt, bb, hiddenLayer);
        }

    
    float total = 0.0;
    for (int i = 0; i < node.mMoves.size(); i++)
    {
        // Calculate the output neuron corresponding to this move
        auto moveIdx = node.mMoves[i].idx(board.sideToMove());
        node.mPolicy[i] = NET->outputBiases[moveIdx];
        for (int i = 0; i < HIDDEN_SIZE; i++)
            node.mPolicy[i] += hiddenLayer[i];

        // Softmax part 1
        total += node.mPolicy[i];
        node.mPolicy[i] = exp(node.mPolicy[i]);
    }

    // Softmax part 2
    for (int i = 0; i < node.mMoves.size(); i++)
        node.mPolicy[i] /= total;
}

} // namespace policy