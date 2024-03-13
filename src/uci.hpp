#pragma once

// clang-format-off

#include "perft.hpp"
#include "bench.hpp"

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
            searcher.mBoard.print();
        else if (tokens[0] == "perft")
        {
            int depth = stoi(tokens[1]);
            perftBench(searcher.mBoard, depth);
        }
        else if (tokens[0] == "perftsplit" 
        || tokens[0] == "splitperft" 
        || tokens[0] == "perftdivide")
        {
            int depth = stoi(tokens[1]);
            perftSplit(searcher.mBoard, depth);
        }
        else if (tokens[0] == "bench")
        {
            if (tokens.size() > 1)
            {
                int depth = stoi(tokens[1]);
                bench(depth);
            }
            else
                bench();
        }
        else if (received == "eval") {
            std::cout << nnue::evaluate(searcher.mBoard.accumulator(), 
                                        searcher.mBoard.sideToMove()) 
                      << std::endl;
        }
        else if (tokens[0] == "makemove")
        {
            Move move = searcher.mBoard.uciToMove(tokens[1]);
            searcher.mBoard.makeMove(move);
        }
        else if (received == "policy") {
            Node root = Node(searcher.mBoard, nullptr, 0);
            root.printPolicy();
        }
        else if (received == "tree" && searcher.mNodes > 0)
            searcher.mRoot.printTree();
        else if (received == "tree 1" && searcher.mNodes > 0)
        {
            for (int i = 0; i < searcher.mRoot.mChildren.size(); i++)
            {
                Node *child = &searcher.mRoot.mChildren[i];
                Move move = searcher.mRoot.mMoves[i];
                std::cout << child->toString(move) << std::endl;
            }
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
        searcher.mBoard = START_BOARD;
        movesTokenIndex = 2;
    }
    else if (tokens[1] == "fen")
    {
        std::string fen = "";
        int i = 0;
        for (i = 2; i < tokens.size() && tokens[i] != "moves"; i++)
            fen += tokens[i] + " ";
        fen.pop_back(); // remove last whitespace
        searcher.mBoard = Board(fen);
        movesTokenIndex = i;
    }

    for (int i = movesTokenIndex + 1; i < tokens.size(); i++)
    {
        Move move = searcher.mBoard.uciToMove(tokens[i]);
        searcher.mBoard.makeMove(move);
    }
}

inline void go(Searcher &searcher, std::vector<std::string> &tokens)
{
    searcher.resetLimits();
    u64 milliseconds = U64_MAX;
    u64 incrementMilliseconds = 0;
    u16 movesToGo = 23;
    bool isMoveTime = false;

    for (int i = 1; i < (int)tokens.size() - 1; i += 2)
    {
        i64 value = stoi(tokens[i + 1]);

        if ((tokens[i] == "wtime" && searcher.mBoard.sideToMove() == Color::WHITE) 
        ||  (tokens[i] == "btime" && searcher.mBoard.sideToMove() == Color::BLACK))
            milliseconds = value;

        else if ((tokens[i] == "winc" && searcher.mBoard.sideToMove() == Color::WHITE) 
        ||       (tokens[i] == "binc" && searcher.mBoard.sideToMove() == Color::BLACK))
            incrementMilliseconds = value;

        else if (tokens[i] == "movestogo")
            movesToGo = value;
        else if (tokens[i] == "movetime")
        {
            milliseconds = value;
            isMoveTime = true;
        }
        else if (tokens[i] == "nodes")
            searcher.mMaxNodes = value;
    }

    searcher.setTimeLimits(milliseconds, incrementMilliseconds, movesToGo, isMoveTime);
    Move bestMove = searcher.search(true);
    assert(bestMove != MOVE_NONE);
    std::cout << "bestmove " + bestMove.toUci() << std::endl;
}

} // namespace uci


