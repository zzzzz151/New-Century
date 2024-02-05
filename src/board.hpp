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
#include "array218.hpp"
#include "attacks.hpp"

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

u64 IN_BETWEEN[64][64], LINE_THROUGH[64][64]; // [square][square]

inline void initInBetweenLineThrough()
{
    const int DIRECTIONS[8][2] = {{0,1}, {1,0}, {-1,0}, {0, -1}, 
                                  {1,1}, {1, -1}, {-1, 1}, {-1,-1}};

    for (int sq1 = 0; sq1 < 64; sq1++)
        for (int sq2 = 0; sq2 < 64; sq2++)
        {
            int rank = (int)squareRank(sq1);
            int file = (int)squareFile(sq1);
            bool found = false;
            LINE_THROUGH[sq1][sq2] = (1ULL << sq1) | (1ULL << sq2);
            
            for (auto [x, y] : DIRECTIONS) {
                u64 between = 0;
                int r = rank, f = file;

                while (true) {
                    r += x;
                    f += y;

                    if (r < 0 || r > 7 || f < 0 || f > 7)
                        break;

                    Square newSq = r * 8 + f;
                    if (found) {
                        LINE_THROUGH[sq1][sq2] |= 1ULL << newSq;
                        continue;
                    }

                    if (newSq == sq2) {
                        found = true;
                        IN_BETWEEN[sq1][sq2] = between;
                        LINE_THROUGH[sq1][sq2] |= between;
                    }

                    between |= 1ULL << newSq;
                }

                if (found) 
                {
                    while (true)
                    {
                        rank += x * -1;
                        file += y * -1;
                        if (rank < 0 || rank > 7 || file < 0 || file > 7)
                            break;
                        int sq = rank * 8 + file;
                        LINE_THROUGH[sq1][sq2] |= 1ULL << sq;
                    }
                    break;
                }
            }

        }
}

bool boardInitialized = false;

struct Board
{
    Color colorToMove;
    std::array<u64, 2> colorBitboard;   // [color]
    std::array<u64, 6> piecesBitboards; // [pieceType]
    u64 castlingRights;
    Square enPassantSquare;
    u8 pliesSincePawnOrCapture;
    u16 currentMoveCounter;
    u64 zobristHash;
    Move lastMove;
    std::vector<u64> repetitionHashes;

    inline Board() = default;

    inline Board(std::string fen, bool perft = false)
    {
        if (!boardInitialized) {
            initZobrist();
            initInBetweenLineThrough();
            boardInitialized = true;
        }

        lastMove = MOVE_NONE;
        repetitionHashes = {};

        trim(fen);
        std::vector<std::string> fenSplit = splitString(fen, ' ');

        // Parse color to move
        colorToMove = fenSplit[1] == "b" ? Color::BLACK : Color::WHITE;
        zobristHash = ZOBRIST_COLOR[(int)colorToMove];

        // Parse pieces
        memset(colorBitboard.data(), 0, sizeof(colorBitboard));
        memset(piecesBitboards.data(), 0, sizeof(piecesBitboards));
        std::string fenRows = fenSplit[0];
        int currentRank = 7, currentFile = 0; // iterate ranks from top to bottom, files from left to right
        for (int i = 0; i < fenRows.length(); i++)
        {
            char thisChar = fenRows[i];
            if (thisChar == '/')
            {
                currentRank--;
                currentFile = 0;
            }
            else if (isdigit(thisChar))
                currentFile += charToInt(thisChar);
            else
            {
                Color color = isupper(thisChar) ? Color::WHITE : Color::BLACK;
                PieceType pt = CHAR_TO_PIECE_TYPE[thisChar];
                Square sq = currentRank * 8 + currentFile;
                placePiece(color, pt, sq);
                currentFile++;
            }
        }

        // Parse castling rights
        castlingRights = 0;
        std::string fenCastlingRights = fenSplit[2];
        if (fenCastlingRights != "-") 
        {
            for (int i = 0; i < fenCastlingRights.length(); i++)
            {
                char thisChar = fenCastlingRights[i];
                Color color = isupper(thisChar) ? Color::WHITE : Color::BLACK;
                int castlingRight = thisChar == 'K' || thisChar == 'k' 
                                    ? CASTLE_SHORT : CASTLE_LONG;
                castlingRights |= CASTLING_MASKS[(int)color][castlingRight];
            }
            zobristHash ^= castlingRights;
        }

        // Parse en passant target square
        enPassantSquare = SQUARE_NONE;
        std::string strEnPassantSquare = fenSplit[3];
        if (strEnPassantSquare != "-")
        {
            enPassantSquare = strToSquare(strEnPassantSquare);
            int file = (int)squareFile(enPassantSquare);
            zobristHash ^= ZOBRIST_FILES[file];
        }

        // Parse last 2 fen tokens
        pliesSincePawnOrCapture = fenSplit.size() >= 5 ? stoi(fenSplit[4]) : 0;
        currentMoveCounter = fenSplit.size() >= 6 ? stoi(fenSplit[5]) : 1;
    }

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

        myFen += colorToMove == Color::BLACK ? " b " : " w ";

        std::string strCastlingRights = "";
        if (castlingRights & CASTLING_MASKS[(int)Color::WHITE][CASTLE_SHORT]) 
            strCastlingRights += "K";
        if (castlingRights & CASTLING_MASKS[(int)Color::WHITE][CASTLE_LONG]) 
            strCastlingRights += "Q";
        if (castlingRights & CASTLING_MASKS[(int)Color::BLACK][CASTLE_SHORT]) 
            strCastlingRights += "k";
        if (castlingRights & CASTLING_MASKS[(int)Color::BLACK][CASTLE_LONG]) 
            strCastlingRights += "q";

        if (strCastlingRights.size() == 0) 
            strCastlingRights = "-";

        myFen += strCastlingRights;

        myFen += " ";
        myFen += enPassantSquare == SQUARE_NONE 
                 ? "-" : SQUARE_TO_STR[enPassantSquare];
        
        myFen += " " + std::to_string(pliesSincePawnOrCapture);
        myFen += " " + std::to_string(currentMoveCounter);

        return myFen;
    }

    inline void placePiece(Color color, PieceType pieceType, Square square) {
        u64 sqBitboard = 1ULL << square;
        colorBitboard[(int)color] |= sqBitboard;
        piecesBitboards[(int)pieceType] |= sqBitboard;
        zobristHash ^= ZOBRIST_PIECES[(int)color][(int)pieceType][square];
    }

    inline void removePiece(Square square) {
        Color color = colorAt(square);
        if (color != Color::NONE)
        {
            u64 sqBitboard = 1ULL << square;
            PieceType pt = pieceTypeAt(square);
            colorBitboard[(int)color] ^= sqBitboard;
            piecesBitboards[(int)pt] ^= sqBitboard;
            zobristHash ^= ZOBRIST_PIECES[(int)color][(int)pt][square];
        }
    }

    inline Color sideToMove() { return colorToMove; }

    inline Color oppSide() { 
        return colorToMove == Color::WHITE 
               ? Color::BLACK : Color::WHITE; 
    }

    inline u64 getZobristHash() { return zobristHash; }

    inline Move getLastMove() { return lastMove; }

    inline u64 occupancy() { 
        return colorBitboard[(int)Color::WHITE] 
               | colorBitboard[(int)Color::BLACK]; 
    }

    inline bool isOccupied(Square square) {
        return occupancy() & (1ULL << square);
    }

    inline u64 us() { 
        return colorBitboard[(int)colorToMove]; 
    }

    inline u64 them() { 
        return colorBitboard[(int)oppSide()]; 
    }

    inline u64 getBitboard(PieceType pieceType) {   
        return piecesBitboards[(int)pieceType];
    }

    inline u64 getBitboard(Color color) { 
        return colorBitboard[(int)color]; 
    }

    inline u64 getBitboard(Color color, PieceType pieceType) {
        return piecesBitboards[(int)pieceType]
               & colorBitboard[(int)color];
    }

    inline Color colorAt(Square square) {
        u64 sqBitboard = 1ULL << square;
        if (sqBitboard & colorBitboard[(int)Color::WHITE])
            return Color::WHITE;
        if (sqBitboard & colorBitboard[(int)Color::BLACK])
            return Color::BLACK;
        return Color::NONE;
    }

    inline PieceType pieceTypeAt(Square square) { 
        u64 sqBitboard = 1ULL << square;
        for (int i = 0; i < piecesBitboards.size(); i++)
            if (sqBitboard & piecesBitboards[i])
                return (PieceType)i;
        return PieceType::NONE;
    }

    inline Piece pieceAt(Square square) { 
        u64 sqBitboard = 1ULL << square;
        for (int i = 0; i < piecesBitboards.size(); i++)
            if (sqBitboard & piecesBitboards[i])
                {
                    Color color = sqBitboard & colorBitboard[(int)Color::WHITE]
                                  ? Color::WHITE : Color::BLACK;
                    return makePiece((PieceType)i, color);
                }
        return Piece::NONE;
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
        std::cout << "Zobrist hash: " << zobristHash << std::endl;
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
        return pliesSincePawnOrCapture >= 100;
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

    inline bool isRepetition(bool threeFold)
    {
        int count = 0;

        for (int i = (int)repetitionHashes.size() - 2; i >= 0; i -= 2)
            if (zobristHash == repetitionHashes[i] && ++count == (threeFold ? 2 : 1))
                return true;

        return false;
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
        u64 ourKingBitboard = getBitboard(colorToMove, PieceType::KING);
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
        u8 kingSquare = lsb(getBitboard(colorToMove, PieceType::KING));
        return attackers(kingSquare, oppSide());
    }

    // returns pinned non diagonal, pinned diagonal
    inline std::pair<u64, u64> pinned()
    {
        u64 kingSquare = lsb(getBitboard(colorToMove, PieceType::KING));

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
        u64 kingBb = getBitboard(colorToMove, PieceType::KING);
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

    inline void getMoves(Array218<Move> &moves, bool underpromotions = true)
    {
        moves.clear();
        u64 threats = this->threats();

        // King moves
        u8 kingSquare = lsb(getBitboard(colorToMove, PieceType::KING));
        u64 targetSquares = attacks::kingAttacks(kingSquare) & ~us() & ~threats;
        while (targetSquares) {
            u8 targetSquare = poplsb(targetSquares);
            moves.add(Move(kingSquare, targetSquare, Move::KING_FLAG));
        }

        u64 checkers = this->checkers();
        u8 numCheckers = std::popcount(checkers);
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

        u64 ourPawns = getBitboard(colorToMove, PieceType::PAWN),
            ourKnights = getBitboard(colorToMove, PieceType::KNIGHT) 
                         & ~pinnedDiagonal & ~pinnedNonDiagonal,
            ourBishops = getBitboard(colorToMove, PieceType::BISHOP) & ~pinnedNonDiagonal,
            ourRooks = getBitboard(colorToMove, PieceType::ROOK) & ~pinnedDiagonal,
            ourQueens = getBitboard(colorToMove, PieceType::QUEEN),
            ourKing = getBitboard(colorToMove, PieceType::KING);

        // En passant
        if (enPassantSquare != SQUARE_NONE)
        {
            u64 ourNearbyPawns = ourPawns & attacks::pawnAttacks(enPassantSquare, enemyColor);
            while (ourNearbyPawns) {
                u8 ourPawnSquare = poplsb(ourNearbyPawns);
                auto _zobristHash = zobristHash;
                auto _colorBitboard = colorBitboard;
                auto _piecesBitboards = piecesBitboards;

                // Make the en passant move
                removePiece(ourPawnSquare);
                placePiece(colorToMove, PieceType::PAWN, enPassantSquare);
                Square capturedPawnSquare = colorToMove == Color::WHITE
                                            ? enPassantSquare - 8 : enPassantSquare + 8;
                removePiece(capturedPawnSquare);

                if (!inCheck())
                    // en passant is legal
                    moves.add(Move(ourPawnSquare, enPassantSquare, Move::EN_PASSANT_FLAG));

                // Undo the en passant move
                zobristHash = _zobristHash;
                colorBitboard = _colorBitboard;
                piecesBitboards = _piecesBitboards;
            }
        }

        // Castling
        if (numCheckers == 0)
        {
            if ((castlingRights & CASTLING_MASKS[(int)colorToMove][CASTLE_SHORT])
            && !isOccupied(kingSquare + 1)
            && !isOccupied(kingSquare + 2)
            && !isSquareAttacked(kingSquare+1, enemyColor) 
            && !isSquareAttacked(kingSquare+2, enemyColor))
                moves.add(Move(kingSquare, kingSquare + 2, Move::CASTLING_FLAG));

            if ((castlingRights & CASTLING_MASKS[(int)colorToMove][CASTLE_LONG])
            && !isOccupied(kingSquare - 1)
            && !isOccupied(kingSquare - 2)
            && !isOccupied(kingSquare - 3)
            && !isSquareAttacked(kingSquare-1, enemyColor) 
            && !isSquareAttacked(kingSquare-2, enemyColor))
                moves.add(Move(kingSquare, kingSquare - 2, Move::CASTLING_FLAG));
        }

        while (ourPawns > 0)
        {
            Square sq = poplsb(ourPawns);
            u64 sqBb = 1ULL << sq;
            bool pawnHasntMoved = false, willPromote = false;
            Rank rank = squareRank(sq);

            if (rank == Rank::RANK_2) {
                pawnHasntMoved = colorToMove == Color::WHITE;
                willPromote = colorToMove == Color::BLACK;
            } else if (rank == Rank::RANK_7) {
                pawnHasntMoved = colorToMove == Color::BLACK;
                willPromote = colorToMove == Color::WHITE;
            }

            // Generate this pawn's captures 

            u64 pawnAttacks = attacks::pawnAttacks(sq, colorToMove) & them() & movableBb;
            if (sqBb & (pinnedDiagonal | pinnedNonDiagonal)) 
                pawnAttacks &= LINE_THROUGH[kingSquare][sq];

            while (pawnAttacks > 0) {
                Square targetSquare = poplsb(pawnAttacks);
                if (willPromote) 
                    addPromotions(moves, sq, targetSquare, underpromotions);
                else 
                    moves.add(Move(sq, targetSquare, Move::PAWN_FLAG));
            }

            // Generate this pawn's pushes 

            if (sqBb & pinnedDiagonal) continue;
            u64 pinRay = LINE_THROUGH[sq][kingSquare];
            bool pinnedHorizontally = (sqBb & pinnedNonDiagonal) && (pinRay & (pinRay << 1)) > 0;
            if (pinnedHorizontally) continue;

            Square squareOneUp = colorToMove == Color::WHITE ? sq + 8 : sq - 8;
            if (isOccupied(squareOneUp)) continue;

            if (movableBb & (1ULL << squareOneUp))
            {
                if (willPromote) {
                    addPromotions(moves, sq, squareOneUp, underpromotions);
                    continue;
                }
                moves.add(Move(sq, squareOneUp, Move::PAWN_FLAG));
            }

            if (!pawnHasntMoved) continue;

            Square squareTwoUp = colorToMove == Color::WHITE 
                                 ? sq + 16 : sq - 16;

            if ((movableBb & (1ULL << squareTwoUp)) && !isOccupied(squareTwoUp))
                moves.add(Move(sq, squareTwoUp, Move::PAWN_TWO_UP_FLAG));
        }

        while (ourKnights > 0) {
            Square sq = poplsb(ourKnights);
            u64 knightMoves = attacks::knightAttacks(sq) & ~us() & movableBb;
            while (knightMoves > 0) {
                Square targetSquare = poplsb(knightMoves);
                moves.add(Move(sq, targetSquare, Move::KNIGHT_FLAG));
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
                moves.add(Move(sq, targetSquare, Move::BISHOP_FLAG));
            }
        }

        while (ourRooks > 0) {
            Square sq = poplsb(ourRooks);
            u64 rookMoves = attacks::rookAttacks(sq, occ) & ~us() & movableBb;
            if ((1ULL << sq) & pinnedNonDiagonal)
                rookMoves &= LINE_THROUGH[kingSquare][sq];
            while (rookMoves > 0) {
                Square targetSquare = poplsb(rookMoves);
                moves.add(Move(sq, targetSquare, Move::ROOK_FLAG));
            }
        }

        while (ourQueens > 0) {
            Square sq = poplsb(ourQueens);
            u64 queenMoves = attacks::queenAttacks(sq, occ) & ~us() & movableBb;
            if ((1ULL << sq) & (pinnedDiagonal | pinnedNonDiagonal))
                queenMoves &= LINE_THROUGH[kingSquare][sq];
            while (queenMoves > 0) {
                Square targetSquare = poplsb(queenMoves);
                moves.add(Move(sq, targetSquare, Move::QUEEN_FLAG));
            }
        }
    }

    private:

    inline void addPromotions(Array218<Move> &moves, Square sq, Square targetSquare, bool underpromotions)
    {
        moves.add(Move(sq, targetSquare, Move::QUEEN_PROMOTION_FLAG));
        if (underpromotions)
        {
            moves.add(Move(sq, targetSquare, Move::ROOK_PROMOTION_FLAG));
            moves.add(Move(sq, targetSquare, Move::BISHOP_PROMOTION_FLAG));
            moves.add(Move(sq, targetSquare, Move::KNIGHT_PROMOTION_FLAG));
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

        if (pieceType == PieceType::PAWN || isCapture) {
            pliesSincePawnOrCapture = 0;
            repetitionHashes = {};
        }
        else {
            pliesSincePawnOrCapture++;
            repetitionHashes.push_back(zobristHash);
        }

        removePiece(from);

        if (moveFlag == Move::CASTLING_FLAG)
        {
            placePiece(colorToMove, PieceType::KING, to);
            auto [rookFrom, rookTo] = CASTLING_ROOK_FROM_TO[to];
            removePiece(rookFrom);
            placePiece(colorToMove, PieceType::ROOK, rookTo);
        }
        else if (moveFlag == Move::EN_PASSANT_FLAG)
        {
            Square capturedPawnSquare = colorToMove == Color::WHITE
                                        ? to - 8 : to + 8;
            removePiece(capturedPawnSquare);
            placePiece(colorToMove, PieceType::PAWN, to);
        }
        else
        {
            if (isCapture)
                removePiece(to);
            PieceType place = promotion != PieceType::NONE ? promotion : pieceType;
            placePiece(colorToMove, place, to);
        }

        zobristHash ^= castlingRights; // XOR old castling rights out

        // Update castling rights
        if (pieceType == PieceType::KING)
        {
            castlingRights &= ~CASTLING_MASKS[(int)colorToMove][CASTLE_SHORT]; 
            castlingRights &= ~CASTLING_MASKS[(int)colorToMove][CASTLE_LONG]; 
        }
        else if ((1ULL << from) & castlingRights)
            castlingRights &= ~(1ULL << from);
        if ((1ULL << to) & castlingRights)
            castlingRights &= ~(1ULL << to);

        zobristHash ^= castlingRights; // XOR new castling rights in

        // Update en passant square
        if (enPassantSquare != SQUARE_NONE)
        {
            zobristHash ^= ZOBRIST_FILES[(int)squareFile(enPassantSquare)];
            enPassantSquare = SQUARE_NONE;
        }
        if (moveFlag == Move::PAWN_TWO_UP_FLAG)
        { 
            enPassantSquare = colorToMove == Color::WHITE
                              ? to - 8 : to + 8;
            zobristHash ^= ZOBRIST_FILES[(int)squareFile(enPassantSquare)];
        }

        zobristHash ^= ZOBRIST_COLOR[(int)colorToMove];
        colorToMove = oppSide;
        zobristHash ^= ZOBRIST_COLOR[(int)colorToMove];

        if (colorToMove == Color::WHITE)
            currentMoveCounter++;

        lastMove = move;
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

    inline bool hasNonPawnMaterial(Color color = Color::NONE)
    {
        if (color == Color::NONE)
            return getBitboard(PieceType::KNIGHT) > 0
                   || getBitboard(PieceType::BISHOP) > 0
                   || getBitboard(PieceType::ROOK) > 0
                   || getBitboard(PieceType::QUEEN) > 0;

        return getBitboard(color, PieceType::KNIGHT) > 0
               || getBitboard(color, PieceType::BISHOP) > 0
               || getBitboard(color, PieceType::ROOK) > 0
               || getBitboard(color, PieceType::QUEEN) > 0;
    }
    
};

const Board START_BOARD = Board(START_FEN);

