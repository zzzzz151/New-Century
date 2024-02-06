#include <stdexcept>
#include "nnue.hpp"
#include "see.hpp"

const double UCT_C = 1.5;

struct Node {
    public:
    Board board;
    Array218<Move> moves;
    Node *parent;
    std::vector<Node> children;
    i32 visits;
    double value;

    inline Node(Board &board, Node *parent)
    {
        this->board = board;
        this->board.getMoves(moves);
        moves.shuffle();
        this->parent = parent;
        children = {};
        visits = value = 0;
    }

    inline double uct() 
    {
        assert(parent != nullptr && parent->visits > 0);
        if (visits == 0) return INF;

        return value / (double)visits +
               UCT_C * sqrt(ln(parent->visits) / (double)visits);
    }

    inline bool isTerminal() 
    {
        return moves.size() == 0 
               || board.isFiftyMovesDraw() 
               || board.isInsufficientMaterial()
               || board.isRepetition(parent == nullptr);
    }

    inline Node* expand() 
    {
        assert(children.size() < moves.size());
        Move move = moves[children.size()];
        Board childBoard = board;
        childBoard.makeMove(move);
        children.push_back(Node(childBoard, this));
        return &children.back();
    }

    inline double simulate()
    {
        assert(parent != nullptr);

        if (isTerminal())
            return moves.size() == 0
                   ? (board.inCheck() ? -1 : 0)
                   : 0;

        i32 eval = nnue::evaluate(board.accumulator, board.sideToMove());
        double wdl = 1.0 / (1.0 + exp(-eval / 200.0)); // [0, 1]
        wdl *= 2; // [0, 2]
        wdl -= 1; // [-1, 1]
        assert(wdl >= -1 && wdl <= 1);
        return wdl;
    }

    inline void backprop(double wdl)
    {
        assert(parent != nullptr);

        Node *current = this;
        while (current != nullptr)
        {
            current->visits++;
            wdl *= -1;
            current->value += wdl;
            current = current->parent;
        }
    }

    inline Node* mostVisitsChild()
    {
        assert(children.size() > 0);
        Node *highestVisitsChild = &children[0];

        for (Node &child : children)
            if (child.visits > highestVisitsChild->visits)
                highestVisitsChild = &child;

        return highestVisitsChild;
    }

};

class Searcher {
    public:

    Board board;
    std::chrono::time_point<std::chrono::steady_clock> startTime;
    u64 milliseconds, nodes, maxNodes;

    const i64 OVERHEAD_MILLISECONDS = 10;

    inline Searcher(Board board)
    {
        resetLimits();
        this->board = board;
    }

    inline void resetLimits()
    {
        startTime = std::chrono::steady_clock::now();
        nodes = 0;
        milliseconds = maxNodes = U64_MAX;
    }

    inline void setTimeLimits(i64 milliseconds, i64 incrementMilliseconds, i64 movesToGo, bool isMoveTime)
    {
        if (isMoveTime) 
            this->milliseconds = max(milliseconds - OVERHEAD_MILLISECONDS, (i64)1);
        else
            this->milliseconds = max(milliseconds / movesToGo - OVERHEAD_MILLISECONDS, (i64)1);
    }

    inline bool stop()
    {
        if (nodes >= maxNodes) return true;

        if ((nodes % 128) != 0) return false;

        return millisecondsElapsed(startTime) >= milliseconds;
    }

    inline Move search()
    {
        resetRng();
        nodes = 0;
        Node root = Node(board, nullptr);
        assert(!root.isTerminal());

        while (!stop()) {
            Node *selected = select(&root);

            if (selected->isTerminal())
            {
                double wdl = selected->moves.size() == 0
                             ? (selected->board.inCheck() ? -1 : 0)
                             : 0;
                selected->backprop(wdl);
            }
            else {
                Node *newNode = selected->expand();
                double wdl = newNode->simulate();
                newNode->backprop(wdl);
            }

            nodes++;
            if ((nodes % 32768) == 0) 
                printInfo(root.mostVisitsChild());
        }

        Node *bestRootChild = root.mostVisitsChild();
        printInfo(bestRootChild);
        return bestRootChild->board.getLastMove();
    }

    inline Node* select(Node *root)
    {
        Node* current = root;
        while (true)
        {
            if (current->children.size() == 0
            || current->children.size() != current->moves.size())
                return current;

            assert(current->children.size() > 0);
            Node *bestChild = nullptr;
            double bestUct = -INF;

            for (Node &child : current->children)
            {
                assert(child.visits > 0);
                double childUct = child.uct();
                if (childUct > bestUct)
                {
                    bestChild = &child;
                    bestUct = childUct;
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
        i32 score = round((double)bestRootChild->value / (double)bestRootChild->visits);

        std::cout << "info nodes " << nodes 
                  << " time " << msElapsed
                  << " nps " << nps
                  << " score " << score
                  << " pv " << bestRootChild->board.getLastMove().toUci()
                  << std::endl;
    }

};