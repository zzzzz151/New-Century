#include <fstream>
#include <bit>
#include "board.hpp"

#pragma pack(push, 1)
struct DataEntry {
    public:

    Color stm = Color::NONE;
    u8 numActiveInputs = 0;
    std::vector<i16> activeInputs = { };
    u8 numMoves = 0;
    std::vector<i16> moves4096 = { };
    u16 bestMove4096 = 4096;

    inline DataEntry() = default;

    inline std::string toString()
    {
        return "stm " + std::to_string((int)stm)
               + " numActiveInputs " + std::to_string((int)numActiveInputs)
               + "\nactiveInputs " + vecToString(activeInputs)
               + "numMoves " +  std::to_string((int)numMoves)
               + "\nmoves4096 " + vecToString(moves4096)
               + "bestmove4096 " + std::to_string(bestMove4096);
    }

    inline auto size()
    {
        return sizeof(stm) 
               + sizeof(numActiveInputs) 
               + 2 * activeInputs.size() 
               + sizeof(numMoves) 
               + 2 * moves4096.size() 
               + sizeof(bestMove4096);
    }
};
#pragma pack(pop)

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
        std::sort(entry.activeInputs.begin(), entry.activeInputs.end());

        entry.numMoves = moves.size();
        for (Move move : moves) {
            auto move4096 = move.to4096(board.sideToMove());
            entry.moves4096.push_back((i16)move4096);
            assert(entry.moves4096.back() >= 0 && entry.moves4096.back() < 4096);
        }
        std::sort(entry.moves4096.begin(), entry.moves4096.end());

        entry.bestMove4096 = bestMove.to4096(board.sideToMove());
        assert(entry.bestMove4096 < 4096);

        outFile.write(reinterpret_cast<const char*>(&entry.stm), sizeof(entry.stm));
        outFile.write(reinterpret_cast<const char*>(&entry.numActiveInputs), sizeof(entry.numActiveInputs));
        outFile.write(reinterpret_cast<const char*>(entry.activeInputs.data()), 2 * entry.activeInputs.size());
        outFile.write(reinterpret_cast<const char*>(&entry.numMoves), sizeof(entry.numMoves));
        outFile.write(reinterpret_cast<const char*>(entry.moves4096.data()), 2 * entry.moves4096.size());
        outFile.write(reinterpret_cast<const char*>(&entry.bestMove4096), sizeof(entry.bestMove4096));
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