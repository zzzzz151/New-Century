const i32 PIECE_VALUES[7] = {100, 300, 300, 500, 900, 20000, 0};

inline i32 evaluate(Board &board)
{
    Color stm = board.sideToMove();
    Color nstm = board.oppSide();
    i32 eval = i32(randomU64() % 101) - 50;

    for (u8 pt = (u8)PieceType::PAWN; pt <= (u8)PieceType::QUEEN; pt++)
    {
        i32 numOurs = std::popcount(board.getBitboard(stm, (PieceType)pt));
        i32 numTheirs = std::popcount(board.getBitboard(nstm, (PieceType)pt));
        eval += (numOurs - numTheirs) * PIECE_VALUES[pt];
    }

    return eval;
}