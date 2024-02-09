#include "nnue.hpp"
#include "see.hpp"

const double CPuct = 1.5;

const double QUIET_MOVE_SCORE = 100,
             GOOD_NOISY_SCORE = 300,
             GOOD_QUEEN_PROMO_SCORE = 500,
             BAD_NOISY_SCORE = -100;

struct Node {
    public:
    Board board;
    Node *parent;
    std::vector<Node> children;
    Array218<Move> moves;
    std::array<double, 218> policy;
    u32 visits;
    double value;
    u16 depth;

    inline Node() = default;

    inline Node(Board &board, Node *parent, u16 depth) {
        this->board = board;
        this->parent = parent;
        this->children = {};
        this->board.getMoves(moves, false);
        this->visits = this->value = 0;
        this->depth = depth;
        
        for (int i = 0; i < moves.size(); i++) {
            Move move = moves[i];
            PieceType captured = board.captured(move);
            PieceType promotion = move.promotion();

            if (captured != PieceType::NONE) {
                policy[i] = SEE(board, move)
                                ? (promotion == PieceType::QUEEN
                                   ? GOOD_QUEEN_PROMO_SCORE 
                                   : GOOD_NOISY_SCORE)
                                : BAD_NOISY_SCORE;

                policy[i] += 10 * (u16)captured - (u16)move.pieceType();
                continue;
            }
            else if (promotion != PieceType::NONE)
                policy[i] = SEE(board, move) ? GOOD_QUEEN_PROMO_SCORE
                                             : BAD_NOISY_SCORE;
            else
                policy[i] = QUIET_MOVE_SCORE;

            policy[i] += randomU64() % 10;
        }

        softmax<218>(policy, moves.size());
    }

    inline double puct(int moveIdx) {
        assert(moveIdx < children.size());
        assert(children[moveIdx].visits > 0);
        assert(children[moveIdx].board.getLastMove() == moves[moveIdx]);

        double U = CPuct * policy[moveIdx] * (double)visits;
        U /= 1.0 + (double)children[moveIdx].visits;

        return children[moveIdx].value + U;
    }

    inline bool isTerminal() {
        return moves.size() == 0 
               || board.isFiftyMovesDraw() 
               || board.isInsufficientMaterial()
               || board.isRepetition(parent == nullptr);
    }

    inline Node* expand() {
        assert(children.size() < moves.size());

        for (int i = children.size() + 1; i < moves.size(); i++)
            if (policy[i] > policy[children.size()]) 
            {
                std::swap(policy[i], policy[children.size()]);
                moves.swap(i, children.size());
            }

        Move move = moves[children.size()];
        Board childBoard = board;
        childBoard.makeMove(move);
        children.push_back(Node(childBoard, this, depth + 1));

        return &children.back();
    }

    inline double simulate() {
        assert(parent != nullptr);

        if (isTerminal())
            return moves.size() == 0
                   ? (board.inCheck() ? -1 : 0)
                   : 0;

        double eval = nnue::evaluate(board.accumulator, board.sideToMove());
        double wdl = 1.0 / (1.0 + exp(-eval / 200.0)); // [0, 1]
        wdl *= 2; // [0, 2]
        wdl -= 1; // [-1, 1]

        assert(wdl >= -1 && wdl <= 1);
        return wdl;
    }

    inline void backprop(double wdl) {
        assert(parent != nullptr);

        Node *current = this;
        while (current != nullptr) {
            current->visits++;
            wdl *= -1;
            current->value += wdl;
            current = current->parent;
        }
    }

    inline Node* mostVisitsChild() {
        assert(moves.size() > 0 && children.size() > 0);
        Node *highestVisitsChild = &children[0];

        for (Node &child : children)
            if (child.visits > highestVisitsChild->visits)
                highestVisitsChild = &child;

        return highestVisitsChild;
    }

    inline std::string nodeToString() 
    {
        return "(Node, last move " + board.getLastMove().toUci()
               + ", depth " + std::to_string(depth)
               + ", isTerminal " + std::to_string(isTerminal())
               + ", moves " + std::to_string(moves.size())
               + ", children " + std::to_string(children.size())
               + ", visits " + std::to_string(visits)
               + ", value "+ std::to_string(roundToDecimalPlaces(value, 4))
               + ")";
    }

    inline void printTree() {
        for (int i = 0; i < depth; i++)
            std::cout << "  ";

        std::cout << nodeToString() << std::endl;
            
        for (Node &child : children)
            child.printTree();
    }
};