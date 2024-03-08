#include <fstream>
#include <bit>

#pragma pack(push, 1)
struct DataEntry {
    public:

    Color stm = Color::NONE;
    u8 numActiveInputs = 0;
    std::vector<u16> activeInputs = { };
    u8 numMoves = 0;
    std::vector<u16> movesIdxs = { };
    u16 bestMoveIdx = 4096;

    inline DataEntry() = default;
};
#pragma pack(pop)

inline void convert(std::string inputFileName, std::string outputFileName) {
    std::cout << inputFileName << " to " << outputFileName << std::endl;

    // Open input file
    std::ifstream inFile(inputFileName);
    if (!inFile.is_open()) {
        std::cout << "Error opening input file" << std::endl;
        return;
    }

    // Open or clean output file
    std::ofstream outFile(outputFileName, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        std::cout << "Error opening output file" << std::endl;
        return;
    }

    // Iterate lines/positions in input file
    u64 positionsSeen = 0, positionsConverted = 0;
    std::string line;
    while (std::getline(inFile, line))
    {
        if ((positionsSeen % 1'000'000) == 0)
            std::cout << "Positions seen: " << positionsSeen << std::endl
                      << "Positions converted: " << positionsConverted << std::endl;

        positionsSeen++;
        std::vector<std::string> tokens = splitString(line, '|');
        std::string fen = tokens[0];
        std::string uciMove = tokens[1];
        BoardState board = BoardState(fen);
        Move bestMove = board.uciToMove(uciMove);

        if (bestMove.promotion() != PieceType::NONE && bestMove.promotion() != PieceType::QUEEN)
            continue;

        std::vector<Move> moves = {};
        board.getMoves(moves, false);
        assert(moves.size() <= 218);

        if (moves.size() == 0) continue;

        DataEntry entry = DataEntry();
        entry.stm = board.sideToMove();

        u64 occ = board.occupancy();
        entry.numActiveInputs = std::popcount(occ);
        assert(entry.numActiveInputs >= 2 && entry.numActiveInputs <= 32);

        while (occ > 0) {
            Square sq = poplsb(occ);
            Color color = board.colorAt(sq);
            PieceType pt = board.pieceTypeAt(sq);
            assert(color != Color::NONE && pt != PieceType::NONE);

            if (board.sideToMove() == Color::BLACK) {
                color = oppColor(color);
                sq ^= 56;
            }

            entry.activeInputs.push_back((u16)color * 384 + (u16)pt * 64 + (u16)sq);
            assert(entry.activeInputs.back() >= 0 && entry.activeInputs.back() < 768);
        }

        assert(entry.numActiveInputs == entry.activeInputs.size());

        entry.numMoves = moves.size();
        for (Move move : moves) {
            Square from = move.from();
            Square to = move.to();
            if (board.sideToMove() == Color::BLACK) {
                from ^= 56;
                to ^= 56;
            }
            entry.movesIdxs.push_back((u16)from * 64 + (u16)to);
        }

        Square bestMoveFrom = bestMove.from();
        Square bestMoveTo = bestMove.to();
        if (board.sideToMove() == Color::BLACK) {
            bestMoveFrom ^= 56;
            bestMoveTo ^= 56;
        }

        entry.bestMoveIdx = (u16)bestMoveFrom * 64 + (u16)bestMoveTo;
        assert(entry.bestMoveIdx < 4096);

        positionsConverted++;
    }

    inFile.close();
    outFile.close();
    std::cout << "Conversion finished" << std::endl;
    std::cout << "Positions seen: " << positionsSeen << std::endl
              << "Positions converted: " << positionsConverted << std::endl;

    // Final output file size
    std::ifstream finalOutFile(outputFileName, std::ios::binary);
    if (!finalOutFile.is_open()) {
        std::cout << "Error opening final output file" << std::endl;
        return;
    }
    finalOutFile.seekg(0, std::ios::end);
    std::streampos outSizeBytes = finalOutFile.tellg();
    u64 outSizeMB = (double)outSizeBytes / (1024 * 1024);
    finalOutFile.close();

    std::cout << "Output bytes: " << outSizeBytes << std::endl;
    std::cout << "Output megabytes: " << outSizeMB << std::endl;

    assert(outSizeBytes == positionsConverted * sizeof(DataEntry));
}