#include <stdexcept>
#include "tree_node.hpp"

class Searcher {
    public:

    Node root;
    std::chrono::time_point<std::chrono::steady_clock> startTime;
    u64 milliseconds, nodes, maxNodes, iterations;

    const i64 OVERHEAD_MILLISECONDS = 10;

    inline Searcher(Board board) {
        resetLimits();
        this->root = Node(board, nullptr, 0);
        assert(!root.isTerminal());
    }

    inline void resetLimits() {
        startTime = std::chrono::steady_clock::now();
        nodes = iterations = 0;
        milliseconds = maxNodes = U64_MAX;
    }

    inline void setTimeLimits(i64 milliseconds, i64 incrementMilliseconds, i64 movesToGo, bool isMoveTime)
    {
        if (isMoveTime) 
            this->milliseconds = max(milliseconds - OVERHEAD_MILLISECONDS, (i64)1);
        else
            this->milliseconds = max(milliseconds / movesToGo - OVERHEAD_MILLISECONDS, (i64)1);
    }

    inline bool stop() {
        if (nodes >= maxNodes) return true;

        if ((iterations % 512) != 0) return false;

        return millisecondsElapsed(startTime) >= milliseconds;
    }

    inline Move search() {
        resetRng();
        root = Node(root.board, nullptr, 0);
        assert(!root.isTerminal());
        nodes = iterations = 0;

        while (!stop()) {
            Node *selected = select();

            if (selected->isTerminal())
            {
                double wdl = selected->moves.size() == 0
                             ? (selected->board.inCheck() ? -1 : 0)
                             : 0;
                selected->backprop(wdl);
            }
            else {
                Node *newNode = selected->expand();
                nodes++;
                double wdl = newNode->simulate();
                newNode->backprop(wdl);
            }

            iterations++;
            if ((iterations % (1ULL << 18)) == 0) 
                printInfo(root.mostVisitsChild());
        }

        Node *bestRootChild = root.mostVisitsChild();
        printInfo(bestRootChild);
        return bestRootChild->board.getLastMove();
    }

    inline Node* select() {
        Node* current = &root;
        while (true)
        {
            if (current->children.size() == 0
            || current->children.size() != current->moves.size())
                return current;

            assert(current->children.size() > 0);
            Node *bestChild = nullptr;
            double bestPuct = -INF;

            for (int moveIdx = 0; moveIdx < current->moves.size(); moveIdx++)
            {
                assert(moveIdx < current->children.size());
                assert(current->children[moveIdx].board.getLastMove() == current->moves[moveIdx]);

                double movePuct = current->puct(moveIdx);
                if (movePuct > bestPuct)
                {
                    bestChild = &(current->children[moveIdx]);
                    bestPuct = movePuct;
                }
            }

            assert(bestChild != nullptr);
            current = bestChild;
        }
    }

    inline void printInfo(Node *bestRootChild)
    {
        u64 msElapsed = millisecondsElapsed(startTime);
        u64 nps = nodes * 1000 / max(msElapsed, 1ULL);
        u64 ips = iterations * 1000 / max(msElapsed, 1ULL);
        double wdl = bestRootChild->value / (double)bestRootChild->visits;

        std::cout << "info time " << msElapsed
                  << " nodes " << nodes
                  << " nps " << nps
                  << " iterations " << iterations
                  << " ips " << ips
                  << " wdl " << round(wdl * 100.0)
                  << " pv " << bestRootChild->board.getLastMove().toUci()
                  << std::endl;
    }

};