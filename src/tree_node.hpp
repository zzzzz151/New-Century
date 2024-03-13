#include "nnue.hpp"

const double UCT_C = 1.75; // Higher => more exploration

struct Node {
    public:
    Node *mParent;
    GameState mGameState;
    std::vector<Node> mChildren;
    std::vector<Move> mMoves;
    std::vector<float> mPolicy;
    u32 mVisits;
    double mResultsSum;
    u16 mDepth;

    inline Node() = default;

    inline Node(Board &board, Node *parent, u16 depth) {
        mParent = parent;
        mChildren = {};
        board.getMoves(mMoves);
        policy::setPolicy(this, board);
        mVisits = mResultsSum = 0;
        mDepth = depth;

        mGameState = mMoves.size() == 0 
                     ? (board.inCheck() ? GameState::LOST : GameState::DRAW)
                     : board.isFiftyMovesDraw() 
                       || board.isInsufficientMaterial() 
                       || board.isRepetition(parent == nullptr) 
                     ? GameState::DRAW
                     : GameState::ONGOING;

        if (parent == nullptr) 
            assert(mGameState == GameState::ONGOING);
    }

    // Q = avg result
    inline double Q() {
        assert(mVisits > 0);
        return (double)mResultsSum / (double)mVisits;
    }

    inline double puct(int moveIdx) {
        assert(mMoves.size() == mPolicy.size());
        assert(mPolicy.size() > 0 && moveIdx < mPolicy.size());
        assert(mChildren.size() > 0 && moveIdx < mChildren.size());
        
        Node *child = &mChildren[moveIdx];
        assert(child->mVisits > 0);

        double U = CPuct * mPolicy[moveIdx] * (double)mVisits;
        U /= 1.0 + (double)child->mVisits;
        return child->Q() + U;
    }

    inline Node* select(Board &board) 
    {
        if (mGameState != GameState::ONGOING
        || mChildren.size() == 0
        || mChildren.size() != mMoves.size())
            return this;

        assert(mMoves.size() > 0);
        double bestUct = -INF;
        int bestChildIdx = 0;

        for (int i = 0; i < mChildren.size(); i++) {
            double childUct = mChildren[i].uct();
            if (childUct > bestUct) {
                bestUct = childUct;
                bestChildIdx = i;
            }
        }

        board.makeMove(mMoves[bestChildIdx]);
        return mChildren[bestChildIdx].select(board);
    }

    inline Node* expand(Board &board) {
        assert(mMoves.size() > 0);
        assert(mChildren.size() < mMoves.size());

        Move move = mMoves[mChildren.size()];
        board.makeMove(move);
        mChildren.push_back(Node(board, this, mDepth + 1));

        return &mChildren.back();
    }

    inline double simulate(Board &board) {
        if (mGameState != GameState::ONGOING)
            return (double)mGameState;

        double eval = nnue::evaluate(board.accumulator(), board.sideToMove());
        double wdl = 1.0 / (1.0 + exp(-eval / 200.0)); // [0, 1]
        wdl *= 2; // [0, 2]
        wdl -= 1; // [-1, 1]

        assert(wdl >= -1 && wdl <= 1);
        return wdl;
    }

    inline void backprop(double wdl) {
        assert(mParent != nullptr);
        assert(wdl >= -1 && wdl <= 1);

        Node *current = this;
        while (current != nullptr) {
            current->mVisits++;
            wdl *= -1;
            current->mResultsSum += wdl;
            current = current->mParent;
        }
    }

    inline std::pair<Node*, Move> mostVisits() {
        assert(mMoves.size() > 0 && mChildren.size() > 0);

        u32 mostVisits = mChildren[0].mVisits;
        int mostVisitsIdx = 0;

        for (int i = 1; i < mChildren.size(); i++)
            if (mChildren[i].mVisits > mostVisits)
            {
                mostVisits = mChildren[i].mVisits;
                mostVisitsIdx = i;
            }

        return { &mChildren[mostVisitsIdx], mMoves[mostVisitsIdx] };
    }

    inline std::string toString(Move move) 
    {
        assert(mVisits > 0);

        return "(Node, move " + move.toUci()
               + ", depth " + std::to_string(mDepth)
               + ", " + gameStateToString(mGameState)
               + ", moves " + std::to_string(mMoves.size())
               + ", children " + std::to_string(mChildren.size())
               + ", visits " + std::to_string(mVisits)
               + ", Q (avg result) " + roundToDecimalPlaces(Q(), 4)
               + ")";
    }

    inline void printTree(Move move = MOVE_NONE) {
        for (int i = 0; i < mDepth; i++)
            std::cout << "  ";

        std::cout << toString(move) << std::endl;
            
        for (int i = 0; i < mChildren.size(); i++)
            mChildren[i].printTree(mMoves[i]);
    }

    inline void sortMovesAndPolicy()
    {
        assert(mMoves.size() == mPolicy.size());

        for (int i = 0; i < mMoves.size(); i++)
            for (int j = i + 1; j < mMoves.size(); j++)
                if (mPolicy[j] > mPolicy[i])
                {
                    std::swap(mPolicy[i], mPolicy[j]);
                    std::swap(mMoves[i], mMoves[j]);
                }
    }

    inline void printPolicy()
    {
        assert(mMoves.size() == mPolicy.size());

        if (mMoves.size() == 0)
        {
            std::cout << "No moves" << std::endl;
            return;
        }

        sortMovesAndPolicy();

        for (int i = 0; i < mMoves.size(); i++)
            std::cout << mMoves[i].toUci() << ": " 
                      << roundToDecimalPlaces(mPolicy[i], 4) 
                      << std::endl;
    }
};