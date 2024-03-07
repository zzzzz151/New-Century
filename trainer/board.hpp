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

struct Board
{
    public:
    Color mColorToMove;
    std::array<u64, 12> mPiecesBitboards;

    inline Board() = default;

    inline Board(std::string fen)
    {
        trim(fen);
        std::vector<std::string> fenSplit = splitString(fen, ' ');

        // Parse color to move
        mColorToMove = fenSplit[1] == "b" ? Color::BLACK : Color::WHITE;

        // Parse pieces
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
                Square sq = currentRank * 8 + currentFile;
                placePiece(CHAR_TO_PIECE[thisChar], sq);
                currentFile++;
            }
        }
    }

    inline Color oppSide() { 
        return mColorToMove == Color::WHITE 
               ? Color::BLACK : Color::WHITE; 
    }

    inline u64 occupancy() { 
        u64 occ = 0;
        for (int i = 0; i < mPiecesBitboards.size(); i++)
            occ |= mPiecesBitboards[i];
        return occ;
    }

    inline bool isOccupied(Square square) {
        for (int i = 0; i < mPiecesBitboards.size(); i++)
            if ((1uLL << square) & mPiecesBitboards[i])
                return true;
        return false;
    }

    inline u64 us() { 
        u64 bb = 0;
        for (int i = (mColorToMove == Color::WHITE ? 0 : 6);
        i < (mColorToMove == Color::WHITE ? 6 : 12);
        i++)
            bb |= mPiecesBitboards[i];
        return bb;
    }

    inline u64 them() { 
        u64 bb = 0;
        for (int i = (mColorToMove == Color::WHITE ? 6 : 0);
        i < (mColorToMove == Color::WHITE ? 12 : 6);
        i++)
            bb |= mPiecesBitboards[i];
        return bb;
    }

    inline PieceType pieceTypeAt(Square square) { 
        u64 sqBitboard = 1ULL << square;
        for (int i = 0; i < mPiecesBitboards.size(); i++)
            if (sqBitboard & mPiecesBitboards[i])
                return pieceToPieceType((Piece)i);
        return PieceType::NONE;
    }

    inline Piece pieceAt(Square square) { 
        u64 sqBitboard = 1ULL << square;
        for (int i = 0; i < mPiecesBitboards.size(); i++)
            if (sqBitboard & mPiecesBitboards[i])
                return (Piece)i;
        return Piece::NONE;
     }

    inline void placePiece(Piece piece, Square square) {
        assert(piece != Piece::NONE);
        mPiecesBitboards[(int)piece] |= 1ULL << square;
    }

    inline void removePiece(Square square) {
        Piece piece = pieceAt(square);
        assert(piece != Piece::NONE);
        mPiecesBitboards[(int)piece] ^= 1ULL << square;
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
        myFen += mColorToMove == Color::BLACK ? " b " : " w ";
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

