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
    // [inputIdx][hiddenNeuronIdx]
    std::array<std::array<float, HIDDEN_SIZE>, INPUT_SIZE> weights1;

    // [hiddenNeuronIdx]
    std::array<float, HIDDEN_SIZE> hiddenBiases;

    // [hiddenNeuronIdx][outputNeuronIdx]
    std::array<std::array<float, OUTPUT_SIZE>, HIDDEN_SIZE> weights2;

    // [outputNeuronIdx]
    std::array<float, OUTPUT_SIZE> outputBiases; 
};

INCBIN(PolicyNetFile, "src/policy_net.bin");
const Net *NET = reinterpret_cast<const Net*>(gPolicyNetFileData);

int INPUTS_IDXS[2][2][6][64]; // [stm][pieceColor][pieceType][square]

constexpr void initInputsIdxs() {
    for (u16 pieceColor : {0, 1})
        for (int pt = (int)PieceType::PAWN; pt <= (int)PieceType::KING; pt++)
            for (u16 sq = 0; sq < 64; sq++) {
                // White stm
                INPUTS_IDXS[0][pieceColor][pt][sq] 
                    = pieceColor * 384 + pt * 64 + sq;

                // Black stm
                INPUTS_IDXS[1][pieceColor][pt][sq] 
                    = (!pieceColor) * 384 + pt * 64 + (sq ^ 56);
            }
}

inline void addWeights(std::array<float, HIDDEN_SIZE> &hiddenLayer,
                       Board &board, Color pieceColor, PieceType pt)
{
    int stm = (int)board.sideToMove();
    u64 bb = board.getBitboard(pieceColor, pt);

    while (bb > 0) {
        int sq = poplsb(bb);
        for (int i = 0; i < HIDDEN_SIZE; i++)
        {
            int inputIdx = INPUTS_IDXS[stm][(int)pieceColor][(int)pt][sq];
            hiddenLayer[i] += NET->weights1[inputIdx][i];
        }
    }
}

inline void getPolicy(std::vector<float> &policy, std::vector<Move> &moves, Board &board)
{
    policy.resize(moves.size());

    if (moves.size() == 0) return;

    if (moves.size() == 1) {
        policy[0] = 1;
        return;
    }

    // Initialize hidden layer with biases
    std::array<float, HIDDEN_SIZE> hiddenLayer;
    for (int i = 0; i < HIDDEN_SIZE; i++)
        hiddenLayer[i] = NET->hiddenBiases[i];

    // Add the weights of the board pieces to the hidden layer
    for (Color pieceColor : {Color::WHITE, Color::BLACK})
        for (int pt = (int)PieceType::PAWN; pt <= (int)PieceType::KING; pt++)
            addWeights(hiddenLayer, board, pieceColor, (PieceType)pt);

    // ReLU the hidden layer
    for (int i = 0; i < HIDDEN_SIZE; i++)
        hiddenLayer[i] = max((float)0, hiddenLayer[i]);

    float total = 0.0;
    for (int i = 0; i < moves.size(); i++)
    {
        // Calculate the output neuron corresponding to this move
        auto move4096 = moves[i].to4096(board.sideToMove());
        policy[i] = NET->outputBiases[move4096];
        for (int j = 0; j < HIDDEN_SIZE; j++)
            policy[i] += hiddenLayer[j] * NET->weights2[j][move4096];

        // Softmax part 1
        policy[i] = exp(policy[i]); // e^policy[i]
        total += policy[i];
    }

    // Softmax part 2
    for (int i = 0; i < moves.size(); i++)
        policy[i] /= total;
}

} // namespace policy