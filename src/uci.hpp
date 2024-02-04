#pragma once

// clang-format-off

#include "perft.hpp"

namespace uci { // Universal chess interface

inline void uci();
inline void setoption(Searcher &searcher, std::vector<std::string> &tokens);
inline void ucinewgame(Searcher &searcher);
inline void position(Searcher &searcher, std::vector<std::string> &tokens);
inline void go(Searcher &searcher, std::vector<std::string> &tokens);

inline void uciLoop(Searcher &searcher)
{
    while (true)
    {
        std::string received = "";
        getline(std::cin, received);
        trim(received);
        std::vector<std::string> tokens = splitString(received, ' ');
        if (received == "" || tokens.size() == 0)
            continue;

        searcher.resetLimits();

        try {

        if (received == "quit" || !std::cin.good())
            break;
        else if (received == "uci")
            uci();
        else if (tokens[0] == "setoption") // e.g. "setoption name Hash value 32"
            setoption(searcher, tokens);
        else if (received == "ucinewgame")
            ucinewgame(searcher);
        else if (received == "isready")
            std::cout << "readyok\n";
        else if (tokens[0] == "position")
            position(searcher, tokens);
        else if (tokens[0] == "go")
            go(searcher, tokens);
        else if (tokens[0] == "print" || tokens[0] == "d"
        || tokens[0] == "display" || tokens[0] == "show")
            searcher.board.print();
        else if (tokens[0] == "perft")
        {
            int depth = stoi(tokens[1]);
            perft::perftBench(searcher.board, depth);
        }
        else if (tokens[0] == "perftsplit" || tokens[0] == "splitperft" 
        || tokens[0] == "perftdivide")
        {
            int depth = stoi(tokens[1]);
            perft::perftSplit(searcher.board, depth);
        }
        else if (tokens[0] == "makemove")
        {
            if (tokens[1] == "0000" || tokens[1] == "null" || tokens[1] == "none")
            {
                assert(!searcher.board.inCheck());
                searcher.board.makeMove(MOVE_NONE);
            }
            else
            {
                Move move = searcher.board.uciToMove(tokens[1]);
                searcher.board.makeMove(move);
            }
        }
        else if (received == "incheck")
            std::cout << searcher.board.inCheck() << std::endl;
        else if (received == "checkers")
            printBitboard(searcher.board.checkers());
        else if (received == "threats")
            printBitboard(searcher.board.threats());
        else if (received == "pinned")
        {
            auto [pinnedNonDiagonal, pinnedDiagonal] = searcher.board.pinned();
            std::cout << "Pinned non diagonal" << std::endl;
            printBitboard(pinnedNonDiagonal);
            std::cout << "Pinned diagonal" << std::endl;
            printBitboard(pinnedDiagonal);
        }
        else if (tokens[0] == "inbetween")
        {
            Square sq1 = stoi(tokens[1]);
            Square sq2 = stoi(tokens[2]);
            printBitboard(IN_BETWEEN[sq1][sq2]);
        }
        else if (tokens[0] == "linethrough")
        {
            Square sq1 = stoi(tokens[1]);
            Square sq2 = stoi(tokens[2]);
            printBitboard(LINE_THROUGH[sq1][sq2]);
        }
        } 
        catch (const char* errorMessage)
        {

        }
        
    }
}

inline void uci()
{
    std::cout << "id name New Century" << std::endl;
    std::cout << "id author zzzzz" << std::endl;
    std::cout << "option name Hash type spin default 32 min 1 max 1024" << std::endl;
    std::cout << "uciok" << std::endl;
}

inline void setoption(Searcher &searcher, std::vector<std::string> &tokens)
{
    std::string optionName = tokens[2];
    trim(optionName);
    std::string optionValue = tokens[4];
    trim(optionValue);

    if (optionName == "Hash" || optionName == "hash")
    {
    }
}

inline void ucinewgame(Searcher &searcher)
{

}

inline void position(Searcher &searcher, std::vector<std::string> &tokens)
{
    int movesTokenIndex = -1;

    if (tokens[1] == "startpos")
    {
        searcher.board = START_BOARD;
        movesTokenIndex = 2;
    }
    else if (tokens[1] == "fen")
    {
        std::string fen = "";
        int i = 0;
        for (i = 2; i < tokens.size() && tokens[i] != "moves"; i++)
            fen += tokens[i] + " ";
        fen.pop_back(); // remove last whitespace
        searcher.board = Board(fen);
        movesTokenIndex = i;
    }

    for (int i = movesTokenIndex + 1; i < tokens.size(); i++)
    {
        Move move = searcher.board.uciToMove(tokens[i]);
        searcher.board.makeMove(move);
    }
}

inline void go(Searcher &searcher, std::vector<std::string> &tokens)
{
    u64 milliseconds = U64_MAX;
    u64 incrementMilliseconds = 0;
    u16 movesToGo = 25;
    bool isMoveTime = false;

    for (int i = 1; i < tokens.size() - 1; i += 2)
    {
        i64 value = stoi(tokens[i + 1]);

        if ((tokens[i] == "wtime" && searcher.board.sideToMove() == Color::WHITE) 
        ||  (tokens[i] == "btime" && searcher.board.sideToMove() == Color::BLACK))
            milliseconds = value;

        else if ((tokens[i] == "winc" && searcher.board.sideToMove() == Color::WHITE) 
        ||       (tokens[i] == "binc" && searcher.board.sideToMove() == Color::BLACK))
            incrementMilliseconds = value;

        else if (tokens[i] == "movestogo")
            movesToGo = value;
        else if (tokens[i] == "movetime")
        {
            milliseconds = value;
            isMoveTime = true;
        }
        else if (tokens[i] == "nodes")
            searcher.maxNodes = value;
    }

    searcher.setTimeLimits(milliseconds, incrementMilliseconds, movesToGo, isMoveTime);
    Move bestMove = searcher.search();
    assert(bestMove != MOVE_NONE);
    std::cout << "bestmove " + bestMove.toUci() << std::endl;
}

}


