from train import *
import chess
import numpy as np

def printPolicyFromFen(net: Net, fen: str):
    print(fen)
    fen_parts = fen.split(' ')
    fen_board = fen_parts[0]
    isBlackStm = fen_parts[1] == "b"
    inputs = torch.zeros(INPUT_SIZE)

    for rank_idx, rank in enumerate(fen_board.split('/')):
        file_idx = 0
        for char in rank:
            if char.isdigit():
                file_idx += int(char)
            else:
                sq = 8 * (7 - rank_idx) + file_idx
                isBlackPiece = char.islower() 
                pieceType = {'p': 0, 'n': 1, 'b': 2, 'r': 3, 'q': 4, 'k': 5}[char.lower()]

                if isBlackStm:
                    idx = 64 * pieceType + (sq ^ 56)
                    if not isBlackPiece:
                        idx += 384
                else:
                    idx = 64 * pieceType + sq
                    if isBlackPiece:
                        idx += 384
                    
                inputs[idx] = 1
                file_idx += 1

    def moveTo4096(mov):
        if isBlackStm:
            return (mov.from_square ^ 56) * 64 + (mov.to_square ^ 56)
        return mov.from_square * 64 + mov.to_square

    moves = list(chess.Board(fen).legal_moves)
    illegals = torch.ones(OUTPUT_SIZE)

    for move in moves:
        illegals[moveTo4096(move)] = 0

    output = net(inputs, illegals)
    output = torch.nn.functional.softmax(output, dim=0)

    moves.sort(key=lambda move: output[moveTo4096(move)], reverse=True)
    for move in moves:
        print("{} ({}): {:.4f}".format(
            move.uci(), moveTo4096(move), output[moveTo4096(move)]))

def printPolicyFromDataEntry(net: Net, dataset: MyDataset, i: int):
    print("DataEntry index", i)
    inputs, illegals, target = dataset.__getitem__(i)

    output = net(inputs, illegals)
    output = torch.nn.functional.softmax(output, dim=0)

    @dataclass
    class PoliciedMove:
        move4096: int
        score: float

    policiedMoves = []
    for move4096 in dataset.entries[i].moves4096:
        policiedMoves.append(PoliciedMove(move4096=move4096, score=output[move4096]))

    policiedMoves.sort(key = lambda policiedMove: policiedMove.score, reverse=True)
    for policiedMove in policiedMoves:
        print("{}: {:.4f}".format(policiedMove.move4096, policiedMove.score))

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Invalid num of args")
        exit(1)

    dataset = MyDataset("printPolicy.bin", 3)

    net = Net().to(device)
    net.load_state_dict(torch.load(sys.argv[1]))

    # start pos
    printPolicyFromFen(net, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
    printPolicyFromDataEntry(net, dataset, 0)
    print() 

    # kiwipete (best move e2a6)
    printPolicyFromFen(net, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1")
    printPolicyFromDataEntry(net, dataset, 1)
    print()

    # endgame black stm (best move a3c3)
    printPolicyFromFen(net, "8/4r3/3k4/8/3K4/r1Q5/8/8 b - - 0 1") 
    printPolicyFromDataEntry(net, dataset, 2)