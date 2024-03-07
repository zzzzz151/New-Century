#include <iostream>
#include <string>
#include <fstream>
#include <bit>
#include <cstdlib> // For _byteswap_uint64
#include "board.hpp"

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

    // Iterate lines/positions in input file
    u64 numPositions = 0;
    std::string line;
    while (std::getline(inFile, line))
    {
        std::vector<std::string> tokens = splitString(line, '|');
        std::string fen = tokens[0];
        std::string uciMove = tokens[1];
        Board board = Board(fen);
        Move move = board.uciToMove(uciMove);

        // Write stm
        outFile.write(reinterpret_cast<const char*>(&board.mColorToMove), sizeof(Color));

        // Write 12 pieces bitboards
        // If black stm, flip colors and vertically 
        for (int i = 0; i < board.mPiecesBitboards.size(); i++)
        {
            if (board.mColorToMove == Color::BLACK)
                board.mPiecesBitboards[i] = _byteswap_uint64(board.mPiecesBitboards[i]);
            outFile.write(reinterpret_cast<const char*>(&board.mPiecesBitboards[i]), sizeof(u64));
        }

        // Write move idx (0-4096), flipped vertically if black stm
        u16 from = board.mColorToMove == Color::WHITE ? move.from() : move.from() ^ 56;
        u16 to = board.mColorToMove == Color::WHITE ? move.to() : move.to() ^ 56;
        u16 moveIdx = from * 64 + to;
        outFile.write(reinterpret_cast<const char*>(&moveIdx), sizeof(moveIdx));

        // Print positions converted so far
        numPositions++;
        if ((numPositions % 10'000'000) == 0)
            std::cout << "Converted " << numPositions << " positions" << std::endl;
    }

    inFile.close();
    outFile.close();

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

    std::cout << "Total: converted " << numPositions << " positions" << std::endl;
    std::cout << "Output bytes: " << outSizeBytes << std::endl;
    std::cout << "Output megabytes: " << outSizeMB << std::endl;

    assert(outSizeBytes == numPositions * (sizeof(Color) + 8*12 + 2));
    return 0;
}