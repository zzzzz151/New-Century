#include "tree_node.hpp"

class Searcher {
    public:

    Board mBoard;
    Node mRoot;
    std::chrono::time_point<std::chrono::steady_clock> mStartTime;
    u64 mMilliseconds, mNodes, mMaxNodes;

    inline Searcher(Board board) {
        resetLimits();
        mBoard = board;
    }

    inline void resetLimits() {
        mStartTime = std::chrono::steady_clock::now();
        mNodes =  0;
        mMilliseconds = mMaxNodes = U64_MAX;
    }

    inline void setTimeLimits(i64 milliseconds, i64 incrementMilliseconds, i64 movesToGo, bool isMoveTime)
    {
        if (isMoveTime) movesToGo = 1;

        mMilliseconds = max(milliseconds / movesToGo - (i64)10, (i64)1);
    }

    inline bool isTimeUp() {
        if (mNodes >= mMaxNodes) return true;

        return (mNodes % 512) == 0 
               && millisecondsElapsed(mStartTime) >= mMilliseconds;
    }

    inline Move search(bool boolPrintInfo, u64 maxAvgDepth = U64_MAX) {
        mRoot = Node(mBoard, nullptr, 0);
        mNodes = 1;
        int boardStateIdx = (int)mBoard.numStates() - 1;
        u64 depthSum = 0;
        u64 printInfoDepth = 1;

        while (!isTimeUp() && depthSum / mNodes < maxAvgDepth) {
            Node *selected = mRoot.select(mBoard);

            Node *node = selected->mGameState != GameState::ONGOING
                         ? selected : selected->expand(mBoard);

            node->backprop(node->simulate(mBoard));
            mBoard.revertToState(boardStateIdx);

            mNodes++;
            depthSum += node->mDepth;
            if (depthSum / mNodes == printInfoDepth && boolPrintInfo)
                printInfo(printInfoDepth++);
        }

        if (boolPrintInfo)
            printInfo(round((double)depthSum / (double)mNodes));

        auto [bestRootChild, bestRootMove] = mRoot.mostVisits();
        return bestRootMove;
    }

    inline void printInfo(u64 avgDepth)
    {
        auto [bestRootChild, bestRootMove] = mRoot.mostVisits();
        u64 msElapsed = millisecondsElapsed(mStartTime);

        std::cout << "info depth " << avgDepth
                  << " nodes " << mNodes
                  << " time " << msElapsed
                  << " nps " << mNodes * 1000 / max(msElapsed, (u64)1)
                  << " wdl " << roundToDecimalPlaces(bestRootChild->Q(), 2)
                  << " pv " << bestRootMove.toUci()
                  << std::endl;
    }
};