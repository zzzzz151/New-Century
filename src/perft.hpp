#pragma once

#include <chrono>
#include "board.hpp"

inline u64 perft(Board &board, int depth)
{
    if (depth == 0) return 1;

    std::vector<Move> moves = {};
    board.getMoves(moves);

    if (depth == 1) return moves.size();

    u64 nodes = 0;
    for (int i = 0; i < moves.size(); i++) 
    {
        board.makeMove(moves[i]);
        nodes += perft(board, depth - 1);
        board.undoMove();
    }

    return nodes;
}

inline void perftSplit(Board &board, int depth)
{
    std::cout << "Running split perft depth " << depth 
              << " on " << board.fen() << std::endl;

    std::vector<Move> moves = {};
    board.getMoves(moves);
    u64 totalNodes = 0;

    for (int i = 0; i < moves.size(); i++) 
    {
        Move move = moves[i];
        board.makeMove(move);
        u64 nodes = perft(board, depth - 1);
        board.undoMove();
        std::cout << move.toUci() << ": " << nodes << std::endl;
        totalNodes += nodes;
    }

    std::cout << "Total: " << totalNodes << std::endl;
}

inline u64 perftBench(Board &board, int depth)
{
    std::cout << "Running perft depth " << depth 
              << " on " << board.fen() << std::endl;

    std::chrono::steady_clock::time_point start =  std::chrono::steady_clock::now();
    u64 nodes = perft(board, depth);

    std::cout << "perft depth " << depth 
              << " nodes " << nodes 
              << " nps " << nodes * 1000 / max((u64)millisecondsElapsed(start), (u64)1)
              << " time " << millisecondsElapsed(start)
              << " fen " << board.fen()
              << std::endl;

    return nodes;
}
