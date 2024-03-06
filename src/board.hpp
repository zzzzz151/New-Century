#pragma once

// clang-format off
#include <array>
#include <vector>
#include <random>
#include <cassert>
#include <cstring> // for memset()
#include "types.hpp"
#include "utils.hpp"
#include "move.hpp"
#include "attacks.hpp"
#include "nnue.hpp"

u64 ZOBRIST_COLOR[2],         // [color]
    ZOBRIST_PIECES[2][6][64], // [color][pieceType][square]
    ZOBRIST_FILES[8];         // [file]

inline void initZobrist()
{
    std::mt19937_64 gen(12345); // 64 bit Mersenne Twister rng with seed 12345
    std::uniform_int_distribution<u64> distribution; // distribution(gen) returns random u64

    ZOBRIST_COLOR[0] = distribution(gen);
    ZOBRIST_COLOR[1] = distribution(gen);

    for (int pt = 0; pt < 6; pt++)
        for (int sq = 0; sq < 64; sq++)
        {
            ZOBRIST_PIECES[(int)Color::WHITE][pt][sq] = distribution(gen);
            ZOBRIST_PIECES[(int)Color::BLACK][pt][sq] = distribution(gen);
        }

    for (int file = 0; file < 8; file++)
        ZOBRIST_FILES[file] = distribution(gen);
}

struct BoardState
{
    private:
    Color mColorToMove;
    std::array<u64, 2> mColorBitboard;   // [color]
    std::array<u64, 6> mPiecesBitboards; // [pieceType]
    u64 mCastlingRights;
    Square mEnPassantSquare;
    u8 mPliesSincePawnOrCapture;
    u16 mMoveCounter;
    u64 mZobristHash;
    Move mLastMove;
    Accumulator mAccumulator;

    public:

    inline BoardState() = default;

    inline BoardState(std::string fen)
    {
        mAccumulator = Accumulator();

        trim(fen);
        std::vector<std::string> fenSplit = splitString(fen, ' ');

        // Parse color to move
        mColorToMove = fenSplit[1] == "b" ? Color::BLACK : Color::WHITE;
        mZobristHash = ZOBRIST_COLOR[(int)mColorToMove];

        // Parse pieces
        memset(mColorBitboard.data(), 0, sizeof(mColorBitboard));
        memset(mPiecesBitboards.data(), 0, sizeof(mPiecesBitboards));
        std::string fenRows = fenSplit[0];
        int currentRank = 7, currentFile = 0; // iterate ranks from top to bottom, files from left to right
        for (int i = 0; i < fenRows.length(); i++)
        {
            char thisChar = fenRows[i];
            if (thisChar == '/') {
                currentRank--;
                currentFile = 0;
            }
            else if (isdigit(thisChar))
                currentFile += charToInt(thisChar);
            else {
                Color color = isupper(thisChar) ? Color::WHITE : Color::BLACK;
                PieceType pt = CHAR_TO_PIECE_TYPE[thisChar];
                Square sq = currentRank * 8 + currentFile;
                placePiece(color, pt, sq);
                currentFile++;
            }
        }

        // Parse castling rights
        mCastlingRights = 0;
        std::string fenCastlingRights = fenSplit[2];
        if (fenCastlingRights != "-")  {
            for (int i = 0; i < fenCastlingRights.length(); i++)
            {
                char thisChar = fenCastlingRights[i];
                Color color = isupper(thisChar) ? Color::WHITE : Color::BLACK;
                int castlingRight = thisChar == 'K' || thisChar == 'k' 
                                    ? CASTLE_SHORT : CASTLE_LONG;
                mCastlingRights |= CASTLING_MASKS[(int)color][castlingRight];
            }
            mZobristHash ^= mCastlingRights;
        }

        // Parse en passant target square
        mEnPassantSquare = SQUARE_NONE;
        std::string strEnPassantSquare = fenSplit[3];
        if (strEnPassantSquare != "-") {
            mEnPassantSquare = strToSquare(strEnPassantSquare);
            int file = (int)squareFile(mEnPassantSquare);
            mZobristHash ^= ZOBRIST_FILES[file];
        }

        // Parse last 2 fen tokens
        mPliesSincePawnOrCapture = fenSplit.size() >= 5 ? stoi(fenSplit[4]) : 0;
        mMoveCounter = fenSplit.size() >= 6 ? stoi(fenSplit[5]) : 1;

        mLastMove = MOVE_NONE;
    }

    inline Color sideToMove() { return mColorToMove; }

    inline Color oppSide() { 
        return mColorToMove == Color::WHITE 
               ? Color::BLACK : Color::WHITE; 
    }

    inline u64 occupancy() { 
        return mColorBitboard[(int)Color::WHITE] 
               | mColorBitboard[(int)Color::BLACK]; 
    }

    inline bool isOccupied(Square square) {
        return occupancy() & (1ULL << square);
    }

    inline u64 us() { 
        return mColorBitboard[(int)mColorToMove]; 
    }

    inline u64 them() { 
        return mColorBitboard[(int)oppSide()]; 
    }

    inline u64 getBitboard(PieceType pieceType) {   
        return mPiecesBitboards[(int)pieceType];
    }

    inline u64 getBitboard(Color color) { 
        return mColorBitboard[(int)color]; 
    }

    inline u64 getBitboard(Color color, PieceType pieceType) {
        return mPiecesBitboards[(int)pieceType]
               & mColorBitboard[(int)color];
    }

    inline Color colorAt(Square square) {
        u64 sqBitboard = 1ULL << square;
        if (sqBitboard & mColorBitboard[(int)Color::WHITE])
            return Color::WHITE;
        if (sqBitboard & mColorBitboard[(int)Color::BLACK])
            return Color::BLACK;
        return Color::NONE;
    }

    inline PieceType pieceTypeAt(Square square) { 
        u64 sqBitboard = 1ULL << square;
        for (int i = 0; i < mPiecesBitboards.size(); i++)
            if (sqBitboard & mPiecesBitboards[i])
                return (PieceType)i;
        return PieceType::NONE;
    }

    inline Piece pieceAt(Square square) { 
        u64 sqBitboard = 1ULL << square;
        for (int i = 0; i < mPiecesBitboards.size(); i++)
            if (sqBitboard & mPiecesBitboards[i])
                {
                    Color color = sqBitboard & mColorBitboard[(int)Color::WHITE]
                                  ? Color::WHITE : Color::BLACK;
                    return makePiece((PieceType)i, color);
                }
        return Piece::NONE;
     }

    inline auto pliesSincePawnOrCapture() { return mPliesSincePawnOrCapture; }

    inline u64 zobristHash() { return mZobristHash; }

    inline Move lastMove() { return mLastMove; }

    inline Accumulator &accumulator() { return mAccumulator; }

    private:

    inline void placePiece(Color color, PieceType pieceType, Square square) {
        u64 sqBitboard = 1ULL << square;
        mColorBitboard[(int)color] |= sqBitboard;
        mPiecesBitboards[(int)pieceType] |= sqBitboard;
        mZobristHash ^= ZOBRIST_PIECES[(int)color][(int)pieceType][square];
        mAccumulator.activate(color, pieceType, square);
    }

    inline void removePiece(Square square) {
        Color color = colorAt(square);
        if (color != Color::NONE)
        {
            u64 sqBitboard = 1ULL << square;
            PieceType pt = pieceTypeAt(square);
            mColorBitboard[(int)color] ^= sqBitboard;
            mPiecesBitboards[(int)pt] ^= sqBitboard;
            mZobristHash ^= ZOBRIST_PIECES[(int)color][(int)pt][square];
            mAccumulator.deactivate(color, pt, square);
        }
    }

    public:

    inline std::string fen()
    {
        std::string myFen = "";

        for (int rank = 7; rank >= 0; rank--)
        {
            int emptySoFar = 0;
            for (int file = 0; file < 8; file++)
            {
                Square square = rank * 8 + file;
                Piece piece = pieceAt(square);
                if (piece == Piece::NONE)
                {
                    emptySoFar++;
                    continue;
                }
                if (emptySoFar > 0) 
                    myFen += std::to_string(emptySoFar);
                myFen += std::string(1, PIECE_TO_CHAR[piece]);
                emptySoFar = 0;
            }
            if (emptySoFar > 0) 
                myFen += std::to_string(emptySoFar);
            myFen += "/";
        }

        myFen.pop_back(); // remove last '/'

        myFen += mColorToMove == Color::BLACK ? " b " : " w ";

        std::string strCastlingRights = "";
        if (mCastlingRights & CASTLING_MASKS[(int)Color::WHITE][CASTLE_SHORT]) 
            strCastlingRights += "K";
        if (mCastlingRights & CASTLING_MASKS[(int)Color::WHITE][CASTLE_LONG]) 
            strCastlingRights += "Q";
        if (mCastlingRights & CASTLING_MASKS[(int)Color::BLACK][CASTLE_SHORT]) 
            strCastlingRights += "k";
        if (mCastlingRights & CASTLING_MASKS[(int)Color::BLACK][CASTLE_LONG]) 
            strCastlingRights += "q";

        if (strCastlingRights.size() == 0) 
            strCastlingRights = "-";

        myFen += strCastlingRights;

        myFen += " ";
        myFen += mEnPassantSquare == SQUARE_NONE 
                 ? "-" : SQUARE_TO_STR[mEnPassantSquare];
        
        myFen += " " + std::to_string(mPliesSincePawnOrCapture);
        myFen += " " + std::to_string(mMoveCounter);

        return myFen;
    }

    inline void print() { 
        std::string str = "";

        for (int i = 7; i >= 0; i--) {
            for (Square j = 0; j < 8; j++) 
            {
                int square = i * 8 + j;
                str += pieceAt(square) == Piece::NONE 
                       ? "." 
                       : std::string(1, PIECE_TO_CHAR[pieceAt(square)]);
                str += " ";
            }
            str += "\n";
        }

        std::cout << str;
        std::cout << fen() << std::endl;
        std::cout << "Zobrist hash: " << mZobristHash << std::endl;
    }

    inline bool isCapture(Move move) 
    {
        assert(move != MOVE_NONE);

        return colorAt(move.to()) == oppSide() 
               || move.flag() == Move::EN_PASSANT_FLAG;
    }

    inline PieceType captured(Move move) 
    {
        assert(move != MOVE_NONE);
        auto flag = move.flag();

        return flag == Move::PAWN_TWO_UP_FLAG || flag == Move::CASTLING_FLAG
               ? PieceType::NONE
               : flag == Move::EN_PASSANT_FLAG
               ? PieceType::PAWN
               : pieceTypeAt(move.to());
    }

    inline bool isFiftyMovesDraw() {
        return mPliesSincePawnOrCapture >= 100;
    }

    inline bool isInsufficientMaterial()
    {
        int numPieces = std::popcount(occupancy());
        if (numPieces == 2) return true;

        // KB vs K
        // KN vs K
        return numPieces == 3 
               && (getBitboard(PieceType::KNIGHT) > 0 
               || getBitboard(PieceType::BISHOP) > 0);
    }

    inline bool isSquareAttacked(Square square, Color colorAttacking)
    {
        if (getBitboard(colorAttacking, PieceType::PAWN)
        & attacks::pawnAttacks(square, oppColor(colorAttacking)))
            return true;

        if (getBitboard(colorAttacking, PieceType::KNIGHT)
        & attacks::knightAttacks(square))
            return true;

        if ((getBitboard(colorAttacking, PieceType::BISHOP) 
        | getBitboard(colorAttacking, PieceType::QUEEN))
        & attacks::bishopAttacks(square, occupancy()))
            return true;
 
        if ((getBitboard(colorAttacking, PieceType::ROOK) 
        | getBitboard(colorAttacking, PieceType::QUEEN))
        & attacks::rookAttacks(square, occupancy()))
            return true;

        if (getBitboard(colorAttacking, PieceType::KING)
        & attacks::kingAttacks(square) )
            return true;

        return false;
    }   

    inline bool inCheck()
    {
        u64 ourKingBitboard = getBitboard(mColorToMove, PieceType::KING);
        Square ourKingSquare = lsb(ourKingBitboard);
        return isSquareAttacked(ourKingSquare, oppSide());
    }

    inline u64 attackers(Square sq, Color colorAttacking) 
    {
        u64 attackers = getBitboard(PieceType::KNIGHT) & attacks::knightAttacks(sq);
        attackers |= getBitboard(PieceType::KING) & attacks::kingAttacks(sq);
        attackers |= getBitboard(PieceType::PAWN) & attacks::pawnAttacks(sq, oppColor(colorAttacking));

        u64 rooksQueens = getBitboard(PieceType::ROOK) ^ getBitboard(PieceType::QUEEN);
        attackers |= rooksQueens & attacks::rookAttacks(sq, occupancy());

        u64 bishopsQueens = getBitboard(PieceType::BISHOP) ^ getBitboard(PieceType::QUEEN);
        attackers |= bishopsQueens & attacks::bishopAttacks(sq, occupancy());

        return attackers & getBitboard(colorAttacking);
    }

    inline u64 checkers() {
        u8 kingSquare = lsb(getBitboard(mColorToMove, PieceType::KING));
        return attackers(kingSquare, oppSide());
    }

    // returns pinned non diagonal, pinned diagonal
    inline std::pair<u64, u64> pinned()
    {
        u64 kingSquare = lsb(getBitboard(mColorToMove, PieceType::KING));

        u64 pinnedNonDiagonal = 0;
        u64 pinnersNonDiagonal = (getBitboard(PieceType::ROOK) | getBitboard(PieceType::QUEEN))
                                 & attacks::xrayRook(kingSquare, occupancy(), us()) & them();

        
        while (pinnersNonDiagonal) {
            u8 pinnerSquare = poplsb(pinnersNonDiagonal);
            pinnedNonDiagonal |= IN_BETWEEN[pinnerSquare][kingSquare] & us();
        }

        u64 pinnedDiagonal = 0;
        u64 pinnersDiagonal = (getBitboard(PieceType::BISHOP) | getBitboard(PieceType::QUEEN))
                              & attacks::xrayBishop(kingSquare, occupancy(), us()) & them();

        while (pinnersDiagonal) {
            u8 pinnerSquare = poplsb(pinnersDiagonal);
            pinnedDiagonal |= IN_BETWEEN[pinnerSquare][kingSquare] & us();
        }

        return { pinnedNonDiagonal, pinnedDiagonal };
    }

    inline u64 threats()
    {
        u64 threats = 0;
        u64 kingBb = getBitboard(mColorToMove, PieceType::KING);
        u64 occ = occupancy() ^ kingBb;
        Color oppColor = oppSide();

        u64 enemyRooks = getBitboard(oppColor, PieceType::ROOK) | getBitboard(oppColor, PieceType::QUEEN);
        while (enemyRooks) {
            u8 sq = poplsb(enemyRooks);
            threats |= attacks::rookAttacks(sq, occ);
        }

        u64 enemyBishops = getBitboard(oppColor, PieceType::BISHOP) | getBitboard(oppColor, PieceType::QUEEN);
        while (enemyBishops) {
            u8 sq = poplsb(enemyBishops);
            threats |= attacks::bishopAttacks(sq, occ);
        }

        u64 enemyKnights = getBitboard(oppColor, PieceType::KNIGHT);
        while (enemyKnights) {
            u8 sq = poplsb(enemyKnights);
            threats |= attacks::knightAttacks(sq);
        }

        u64 enemyPawns = getBitboard(oppColor, PieceType::PAWN);
        while (enemyPawns) {
            u8 sq = poplsb(enemyPawns);
            threats |= attacks::pawnAttacks(sq, oppColor);
        }

        u8 enemyKingSquare = lsb(getBitboard(oppColor, PieceType::KING));
        threats |= attacks::kingAttacks(enemyKingSquare);

        return threats;
    }

    inline bool isSlider(Square sq)
    {
        u64 squareBb = 1ULL << sq;
        return (getBitboard(PieceType::BISHOP) & squareBb) > 0
               || (getBitboard(PieceType::ROOK) & squareBb) > 0
               || (getBitboard(PieceType::QUEEN) & squareBb) > 0;
    }

    inline void getMoves(std::vector<Move> &moves, bool underpromotions = true)
    {
        moves = {};
        u64 threats = this->threats();

        // King moves
        u8 kingSquare = lsb(getBitboard(mColorToMove, PieceType::KING));
        u64 targetSquares = attacks::kingAttacks(kingSquare) & ~us() & ~threats;
        while (targetSquares) {
            u8 targetSquare = poplsb(targetSquares);
            moves.push_back(Move(kingSquare, targetSquare, Move::KING_FLAG));
        }

        u64 checkers = this->checkers();
        int numCheckers = std::popcount(checkers);
        assert(numCheckers <= 2);

        // If in double check, only king moves are allowed
        if (numCheckers > 1) return;

        u64 movableBb = ONES;
        if (numCheckers == 1) {
            movableBb = checkers;
            u8 checkerSquare = lsb(checkers);
            if (isSlider(checkerSquare)) 
                movableBb |= IN_BETWEEN[kingSquare][checkerSquare];
        }

        Color enemyColor = oppSide();
        auto [pinnedNonDiagonal, pinnedDiagonal] = pinned();

        u64 ourPawns = getBitboard(mColorToMove, PieceType::PAWN),
            ourKnights = getBitboard(mColorToMove, PieceType::KNIGHT) 
                         & ~pinnedDiagonal & ~pinnedNonDiagonal,
            ourBishops = getBitboard(mColorToMove, PieceType::BISHOP) & ~pinnedNonDiagonal,
            ourRooks = getBitboard(mColorToMove, PieceType::ROOK) & ~pinnedDiagonal,
            ourQueens = getBitboard(mColorToMove, PieceType::QUEEN),
            ourKing = getBitboard(mColorToMove, PieceType::KING);

        // En passant
        if (mEnPassantSquare != SQUARE_NONE)
        {
            u64 ourNearbyPawns = ourPawns & attacks::pawnAttacks(mEnPassantSquare, enemyColor);
            while (ourNearbyPawns) {
                u8 ourPawnSquare = poplsb(ourNearbyPawns);
                auto _zobristHash = mZobristHash;
                auto _colorBitboard = mColorBitboard;
                auto _piecesBitboards = mPiecesBitboards;

                // Make the en passant move
                removePiece(ourPawnSquare);
                placePiece(mColorToMove, PieceType::PAWN, mEnPassantSquare);
                Square capturedPawnSquare = mColorToMove == Color::WHITE
                                            ? mEnPassantSquare - 8 : mEnPassantSquare + 8;
                removePiece(capturedPawnSquare);

                if (!inCheck())
                    // en passant is legal
                    moves.push_back(Move(ourPawnSquare, mEnPassantSquare, Move::EN_PASSANT_FLAG));

                // Undo the en passant move
                mZobristHash = _zobristHash;
                mColorBitboard = _colorBitboard;
                mPiecesBitboards = _piecesBitboards;
            }
        }

        // Castling
        if (numCheckers == 0)
        {
            if ((mCastlingRights & CASTLING_MASKS[(int)mColorToMove][CASTLE_SHORT])
            && !isOccupied(kingSquare + 1)
            && !isOccupied(kingSquare + 2)
            && !isSquareAttacked(kingSquare+1, enemyColor) 
            && !isSquareAttacked(kingSquare+2, enemyColor))
                moves.push_back(Move(kingSquare, kingSquare + 2, Move::CASTLING_FLAG));

            if ((mCastlingRights & CASTLING_MASKS[(int)mColorToMove][CASTLE_LONG])
            && !isOccupied(kingSquare - 1)
            && !isOccupied(kingSquare - 2)
            && !isOccupied(kingSquare - 3)
            && !isSquareAttacked(kingSquare-1, enemyColor) 
            && !isSquareAttacked(kingSquare-2, enemyColor))
                moves.push_back(Move(kingSquare, kingSquare - 2, Move::CASTLING_FLAG));
        }

        while (ourPawns > 0)
        {
            Square sq = poplsb(ourPawns);
            u64 sqBb = 1ULL << sq;
            bool pawnHasntMoved = false, willPromote = false;
            Rank rank = squareRank(sq);

            if (rank == Rank::RANK_2) {
                pawnHasntMoved = mColorToMove == Color::WHITE;
                willPromote = mColorToMove == Color::BLACK;
            } else if (rank == Rank::RANK_7) {
                pawnHasntMoved = mColorToMove == Color::BLACK;
                willPromote = mColorToMove == Color::WHITE;
            }

            // Generate this pawn's captures 

            u64 pawnAttacks = attacks::pawnAttacks(sq, mColorToMove) & them() & movableBb;
            if (sqBb & (pinnedDiagonal | pinnedNonDiagonal)) 
                pawnAttacks &= LINE_THROUGH[kingSquare][sq];

            while (pawnAttacks > 0) {
                Square targetSquare = poplsb(pawnAttacks);
                if (willPromote) 
                    addPromotions(moves, sq, targetSquare, underpromotions);
                else 
                    moves.push_back(Move(sq, targetSquare, Move::PAWN_FLAG));
            }

            // Generate this pawn's pushes 

            if (sqBb & pinnedDiagonal) continue;
            u64 pinRay = LINE_THROUGH[sq][kingSquare];
            bool pinnedHorizontally = (sqBb & pinnedNonDiagonal) && (pinRay & (pinRay << 1)) > 0;
            if (pinnedHorizontally) continue;

            Square squareOneUp = mColorToMove == Color::WHITE ? sq + 8 : sq - 8;
            if (isOccupied(squareOneUp)) continue;

            if (movableBb & (1ULL << squareOneUp))
            {
                if (willPromote) {
                    addPromotions(moves, sq, squareOneUp, underpromotions);
                    continue;
                }
                moves.push_back(Move(sq, squareOneUp, Move::PAWN_FLAG));
            }

            if (!pawnHasntMoved) continue;

            Square squareTwoUp = mColorToMove == Color::WHITE 
                                 ? sq + 16 : sq - 16;

            if ((movableBb & (1ULL << squareTwoUp)) && !isOccupied(squareTwoUp))
                moves.push_back(Move(sq, squareTwoUp, Move::PAWN_TWO_UP_FLAG));
        }

        while (ourKnights > 0) {
            Square sq = poplsb(ourKnights);
            u64 knightMoves = attacks::knightAttacks(sq) & ~us() & movableBb;
            while (knightMoves > 0) {
                Square targetSquare = poplsb(knightMoves);
                moves.push_back(Move(sq, targetSquare, Move::KNIGHT_FLAG));
            }
        }

        u64 occ = occupancy();
        
        while (ourBishops > 0) {
            Square sq = poplsb(ourBishops);
            u64 bishopMoves = attacks::bishopAttacks(sq, occ) & ~us() & movableBb;
            if ((1ULL << sq) & pinnedDiagonal)
                bishopMoves &= LINE_THROUGH[kingSquare][sq];
            while (bishopMoves > 0) {
                Square targetSquare = poplsb(bishopMoves);
                moves.push_back(Move(sq, targetSquare, Move::BISHOP_FLAG));
            }
        }

        while (ourRooks > 0) {
            Square sq = poplsb(ourRooks);
            u64 rookMoves = attacks::rookAttacks(sq, occ) & ~us() & movableBb;
            if ((1ULL << sq) & pinnedNonDiagonal)
                rookMoves &= LINE_THROUGH[kingSquare][sq];
            while (rookMoves > 0) {
                Square targetSquare = poplsb(rookMoves);
                moves.push_back(Move(sq, targetSquare, Move::ROOK_FLAG));
            }
        }

        while (ourQueens > 0) {
            Square sq = poplsb(ourQueens);
            u64 queenMoves = attacks::queenAttacks(sq, occ) & ~us() & movableBb;
            if ((1ULL << sq) & (pinnedDiagonal | pinnedNonDiagonal))
                queenMoves &= LINE_THROUGH[kingSquare][sq];
            while (queenMoves > 0) {
                Square targetSquare = poplsb(queenMoves);
                moves.push_back(Move(sq, targetSquare, Move::QUEEN_FLAG));
            }
        }
    }

    private:

    inline void addPromotions(std::vector<Move> &moves, Square sq, Square targetSquare, bool underpromotions)
    {
        moves.push_back(Move(sq, targetSquare, Move::QUEEN_PROMOTION_FLAG));
        if (underpromotions) {
            moves.push_back(Move(sq, targetSquare, Move::ROOK_PROMOTION_FLAG));
            moves.push_back(Move(sq, targetSquare, Move::BISHOP_PROMOTION_FLAG));
            moves.push_back(Move(sq, targetSquare, Move::KNIGHT_PROMOTION_FLAG));
        }
    }

    public:

    inline void makeMove(Move move)
    {
        assert(move != MOVE_NONE);
        Square from = move.from();
        Square to = move.to();
        auto moveFlag = move.flag();
        PieceType pieceType = move.pieceType();
        PieceType promotion = move.promotion();
        bool isCapture = this->isCapture(move);
        Color oppSide = this->oppSide();

        removePiece(from);

        if (moveFlag == Move::CASTLING_FLAG)
        {
            placePiece(mColorToMove, PieceType::KING, to);
            auto [rookFrom, rookTo] = CASTLING_ROOK_FROM_TO[to];
            removePiece(rookFrom);
            placePiece(mColorToMove, PieceType::ROOK, rookTo);
        }
        else if (moveFlag == Move::EN_PASSANT_FLAG)
        {
            Square capturedPawnSquare = mColorToMove == Color::WHITE
                                        ? to - 8 : to + 8;
            removePiece(capturedPawnSquare);
            placePiece(mColorToMove, PieceType::PAWN, to);
        }
        else
        {
            if (isCapture)
                removePiece(to);
            PieceType place = promotion != PieceType::NONE ? promotion : pieceType;
            placePiece(mColorToMove, place, to);
        }

        mZobristHash ^= mCastlingRights; // XOR old castling rights out

        // Update castling rights
        if (pieceType == PieceType::KING)
        {
            mCastlingRights &= ~CASTLING_MASKS[(int)mColorToMove][CASTLE_SHORT]; 
            mCastlingRights &= ~CASTLING_MASKS[(int)mColorToMove][CASTLE_LONG]; 
        }
        else if ((1ULL << from) & mCastlingRights)
            mCastlingRights &= ~(1ULL << from);
        if ((1ULL << to) & mCastlingRights)
            mCastlingRights &= ~(1ULL << to);

        mZobristHash ^= mCastlingRights; // XOR new castling rights in

        // Update en passant square
        if (mEnPassantSquare != SQUARE_NONE)
        {
            mZobristHash ^= ZOBRIST_FILES[(int)squareFile(mEnPassantSquare)];
            mEnPassantSquare = SQUARE_NONE;
        }
        if (moveFlag == Move::PAWN_TWO_UP_FLAG)
        { 
            mEnPassantSquare = mColorToMove == Color::WHITE
                              ? to - 8 : to + 8;
            mZobristHash ^= ZOBRIST_FILES[(int)squareFile(mEnPassantSquare)];
        }

        mZobristHash ^= ZOBRIST_COLOR[(int)mColorToMove];
        mColorToMove = oppSide;
        mZobristHash ^= ZOBRIST_COLOR[(int)mColorToMove];

        if (pieceType == PieceType::PAWN || isCapture) {
            mPliesSincePawnOrCapture = 0;
        }
        else {
            mPliesSincePawnOrCapture++;
        }

        if (mColorToMove == Color::WHITE)
            mMoveCounter++;

        mLastMove = move;
    }

    inline Move uciToMove(std::string uciMove)
    {
        Move move = MOVE_NONE;
        Square from = strToSquare(uciMove.substr(0,2));
        Square to = strToSquare(uciMove.substr(2,4));
        PieceType pieceType = pieceTypeAt(from);
        u16 moveFlag = (u16)pieceType + 1;

        if (uciMove.size() == 5) // promotion
        {
            char promotionLowerCase = uciMove.back(); // last char of string
            if (promotionLowerCase == 'n') 
                moveFlag = Move::KNIGHT_PROMOTION_FLAG;
            else if (promotionLowerCase == 'b') 
                moveFlag = Move::BISHOP_PROMOTION_FLAG;
            else if (promotionLowerCase == 'r') 
                moveFlag = Move::ROOK_PROMOTION_FLAG;
            else
                moveFlag = Move::QUEEN_PROMOTION_FLAG;
        }
        else if (pieceType == PieceType::KING)
        {
            if (abs((int)to - (int)from) == 2)
                moveFlag = Move::CASTLING_FLAG;
        }
        else if (pieceType == PieceType::PAWN)
        { 
            int bitboardSquaresTraveled = abs((int)to - (int)from);
            if (bitboardSquaresTraveled == 16)
                moveFlag = Move::PAWN_TWO_UP_FLAG;
            else if (bitboardSquaresTraveled != 8 && !isOccupied(to))
                moveFlag = Move::EN_PASSANT_FLAG;
        }

        return Move(from, to, moveFlag);
    }
};

class Board
{
    private:
    std::vector<BoardState> mStates;
    BoardState *mState;

    public:

    inline Board() = default;

    inline Board(std::string fen)
    {
        mStates = {};
        mStates.reserve(256);
        mStates.push_back(BoardState(fen));
        mState = &mStates.back();
    }

    // Copy constructor
    Board(const Board& other) : mStates(other.mStates) {
        mState = mStates.empty() ? nullptr : &mStates.back();
    }

    // Copy assignment operator
    Board& operator=(const Board& other) {
        if (this != &other) {
            mStates = other.mStates;
            mState = mStates.empty() ? nullptr : &mStates.back();
        }
        return *this;
    }

    inline Color sideToMove() { return mState->sideToMove(); }

    inline Color oppSide() { return mState->oppSide(); }

    inline u64 getBitboard(PieceType pieceType) {
        return mState->getBitboard(pieceType);
    }

    inline u64 getBitboard(Color color) {
        return mState->getBitboard(color);
    }

    inline u64 getBitboard(Color color, PieceType pieceType) {
        return mState->getBitboard(color, pieceType);
    }

    inline u64 occupancy() { return mState->occupancy(); }

    inline u64 us() { return mState->us(); }

    inline u64 them() { return mState->them(); }

    inline u64 zobristHash() { return mState->zobristHash(); }

    inline Accumulator &accumulator() { return mState->accumulator(); }

    inline Move lastMove() { return mState->lastMove(); }

    inline std::string fen() { return mState->fen(); }

    inline void print() { mState->print(); }

    inline bool isCapture(Move move) { return mState->isCapture(move); }

    inline PieceType captured(Move move) { return mState->captured(move); }

    inline bool isFiftyMovesDraw() { return mState->isFiftyMovesDraw(); }

    inline bool isInsufficientMaterial() { return mState->isInsufficientMaterial(); }

    inline bool isRepetition(bool threeFold) {
        if (mStates.size() <= 4) return false;

        int count = 0;

        for (int i = (int)mStates.size() - 3;
        i >= 0 && i >= (int)mStates.size() - (int)mState->pliesSincePawnOrCapture() - 2;
        i -= 2)
            if (mState->zobristHash() == mStates[i].zobristHash() 
            && ++count == (threeFold ? 2 : 1))
                return true;

        return false;
    }
    
    inline bool inCheck() { return mState->inCheck(); }

    inline void getMoves(std::vector<Move> &moves, bool underpromotions = true) {
        mState->getMoves(moves, underpromotions);
    }

    inline Move uciToMove(std::string uciMove) {
        return mState->uciToMove(uciMove);
    }

    inline void makeMove(Move move) {
        assert(move != MOVE_NONE);
        assert(mState == &mStates.back());

        mStates.push_back(*mState);
        mState = &mStates.back();
        mState->makeMove(move);
    }

    inline void undoMove() {
        assert(mStates.size() > 1);
        mState = &mStates[mStates.size() - 2];
        mStates.pop_back();
    }

    inline auto numStates() { return mStates.size(); }

    inline void revertToState(int stateIdx) {
        assert(mState == &mStates.back());
        assert(stateIdx >= 0 && stateIdx < (int)mStates.size());

        mState = &mStates[stateIdx];
        mStates.resize(stateIdx + 1);

        assert(mState == &mStates.back());
        assert(mStates.size() >= 1 && stateIdx == (int)mStates.size() - 1);
    }
};

const Board START_BOARD = Board(START_FEN);

