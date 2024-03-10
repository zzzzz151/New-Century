from train import *
import chess
import numpy as np

def printPolicy(net, fen):
    print("Fen:", fen)
    fen_parts = fen.split(' ')
    fen_board = fen_parts[0]
    inputs = torch.zeros(768)

    for rank_idx, rank in enumerate(fen_board.split('/')):
        file_idx = 0
        for char in rank:
            if char.isdigit():
                file_idx += int(char)
            else:
                piece_type = char.lower()
                piece_color = char.islower()  # True for black, False for white
                piece_value = {'p': 0, 'n': 1, 'b': 2, 'r': 3, 'q': 4, 'k': 5}[piece_type]
                sq = 8 * (7 - rank_idx) + file_idx

                if piece_color:  # Black piece
                    inputs[384 + 64 * piece_value + sq] = 1
                else:  # White piece
                    inputs[64 * piece_value + sq] = 1

                file_idx += 1

    @dataclass
    class MoveWithIdx:
        move: chess.Move
        moveIdx: int

    board = chess.Board(fen)
    movesWithIdx = [MoveWithIdx(move=legalMove, moveIdx=legalMove.from_square*64+legalMove.to_square) 
        for legalMove in list(board.legal_moves)]

    illegals = torch.ones(OUTPUT_SIZE)
    for moveWithIdx in movesWithIdx:
        illegals[moveWithIdx.moveIdx] = 0

    output = net(inputs, illegals)
    output = torch.nn.functional.softmax(output, dim=0)

    movesWithIdx.sort(key=lambda moveWithIdx: output[moveWithIdx.moveIdx], reverse=True)
    for moveWithIdx in movesWithIdx:
        print("{} ({}): {:.4f}".format(
            moveWithIdx.move.uci(), moveWithIdx.moveIdx, output[moveWithIdx.moveIdx]))

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Invalid num of args")
        exit(1)

    net = Net()
    net.load_state_dict(torch.load(sys.argv[1]))
    printPolicy(net, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
    print()
    printPolicy(net, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1")