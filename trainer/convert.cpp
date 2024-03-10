#include <fstream>
#include <bit>
#include "board.hpp"
#include "data_entry.hpp"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Invalid number of args" << std::endl;
        return 1;
    }

    // Parse and print file names
    std::string inputFileName(argv[1]);
    std::string outputFileName(argv[2]);
    std::cout << inputFileName << " to " << outputFileName << std::endl;

    // Open input file
    std::ifstream inFile(inputFileName);
    if (!inFile.is_open()) {
        std::cout << "Error opening input file" << std::endl;
        return 1;
    }

    // Open or clean output file
    std::ofstream outFile(outputFileName, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        std::cout << "Error opening output file" << std::endl;
        return 1;
    }

    initUtils();
    attacks::init();

    // Iterate lines/positions in input file
    u64 positionsSeen = 0, positionsConverted = 0;
    std::string line;
    while (std::getline(inFile, line))
    {
        if (positionsSeen != 0 && (positionsSeen % 10'000'000) == 0)
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

        if (moves.size() == 0 || board.isFiftyMovesDraw() || board.isInsufficientMaterial()) 
            continue;

        DataEntry entry = DataEntry();
        entry.stm = board.sideToMove();
        assert(entry.stm != Color::NONE);

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

            entry.activeInputs.push_back((i16)color * 384 + (i16)pt * 64 + (i16)sq);
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
            entry.movesIdxs.push_back((i16)from * 64 + (i16)to);
            assert(entry.movesIdxs.back() >= 0 && entry.movesIdxs.back() < 4096);
        }

        Square bestMoveFrom = bestMove.from();
        Square bestMoveTo = bestMove.to();
        if (board.sideToMove() == Color::BLACK) {
            bestMoveFrom ^= 56;
            bestMoveTo ^= 56;
        }

        entry.bestMoveIdx = (u16)bestMoveFrom * 64 + (u16)bestMoveTo;
        assert(entry.bestMoveIdx < 4096);

        outFile.write(reinterpret_cast<const char*>(&entry.stm), sizeof(entry.stm));
        outFile.write(reinterpret_cast<const char*>(&entry.numActiveInputs), sizeof(entry.numActiveInputs));
        outFile.write(reinterpret_cast<const char*>(entry.activeInputs.data()), 2 * entry.activeInputs.size());
        outFile.write(reinterpret_cast<const char*>(&entry.numMoves), sizeof(entry.numMoves));
        outFile.write(reinterpret_cast<const char*>(entry.movesIdxs.data()), 2 * entry.movesIdxs.size());
        outFile.write(reinterpret_cast<const char*>(&entry.bestMoveIdx), sizeof(entry.bestMoveIdx));
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
        return 1;
    }
    finalOutFile.seekg(0, std::ios::end);
    std::streampos outSizeBytes = finalOutFile.tellg();
    u64 outSizeMB = (double)outSizeBytes / (1024 * 1024);
    finalOutFile.close();

    std::cout << "Output bytes: " << outSizeBytes << std::endl;
    std::cout << "Output megabytes: " << outSizeMB << std::endl;

    return 1;
}