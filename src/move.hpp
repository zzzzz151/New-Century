#pragma once

// clang-format off
#include <cassert>
#include "types.hpp"
#include "utils.hpp"

struct Move
{
    private:

    // 16 bits: ffffff tttttt FFFF (f = from, t = to, F = flag)  
    u16 moveEncoded = 0;

    public:

    const static u16 NULL_FLAG = 0x0000,
                     PAWN_FLAG = 0x0001,
                     KNIGHT_FLAG = 0x0002,
                     BISHOP_FLAG = 0x0003,
                     ROOK_FLAG = 0x0004,
                     QUEEN_FLAG = 0x0005,
                     KING_FLAG = 0x0006,
                     CASTLING_FLAG = 0x0007,
                     KNIGHT_PROMOTION_FLAG = 0x000A,
                     BISHOP_PROMOTION_FLAG = 0x000B,
                     ROOK_PROMOTION_FLAG = 0x000C,
                     QUEEN_PROMOTION_FLAG = 0x000D,
                     EN_PASSANT_FLAG = 0x000E,
                     PAWN_TWO_UP_FLAG = 0x000F;

    inline Move() = default;

    inline bool operator==(const Move &other) const {
        return moveEncoded == other.moveEncoded;
    }

    inline bool operator!=(const Move &other) const {
        return moveEncoded != other.moveEncoded;
    }

    inline Move(Square from, Square to, u16 flag)
    {
        // from/to: 00100101
        // (u16)from/to: 00000000 00100101

        moveEncoded = ((u16)from << 10);
        moveEncoded |= ((u16)to << 4);
        moveEncoded |= flag;
    }

    inline u16 getMoveEncoded() { return moveEncoded; }

    inline Square from() { return (moveEncoded >> 10) & 0b111111; }

    inline Square to() { return (moveEncoded >> 4) & 0b111111; }

    inline u16 flag() { return moveEncoded & 0x000F; }

    inline PieceType pieceType()
    {
        auto flag = this->flag();
        if (flag == NULL_FLAG)
            return PieceType::NONE;
        if (flag <= KING_FLAG)
            return (PieceType)(flag - 1);
        if (flag == CASTLING_FLAG)
            return PieceType::KING;
        return PieceType::PAWN;
    }

    inline PieceType promotion()
    {
        u16 flag = this->flag();
        if (flag >= KNIGHT_PROMOTION_FLAG && flag <= QUEEN_PROMOTION_FLAG)
            return (PieceType)(flag - KNIGHT_PROMOTION_FLAG + 1);
        return PieceType::NONE;
    }

    inline std::string toUci()
    {
        std::string str = SQUARE_TO_STR[from()] + SQUARE_TO_STR[to()];
        u16 flag = this->flag();

        if (flag == QUEEN_PROMOTION_FLAG) 
            str += "q";
        else if (flag == KNIGHT_PROMOTION_FLAG) 
            str += "n";
        else if (flag == BISHOP_PROMOTION_FLAG) 
            str += "b";
        else if (flag == ROOK_PROMOTION_FLAG) 
            str += "r";

        return str;
    }

    inline u16 idx(Color stm)
    {
        u16 from = move.from();
        u16 to = move.to();

        return stm == Color::WHITE
               ? from * 64 + to
               : (from ^ 56) * 64 + (to ^ 56);
    }
    
};

Move MOVE_NONE = Move();

