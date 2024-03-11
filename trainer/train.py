import sys
import os
import math
from dataclasses import dataclass, field
import ctypes
import struct
import torch
from torch.utils.data import Dataset, DataLoader
import json
from json import JSONEncoder
import copy

INPUT_SIZE = 768
HIDDEN_SIZE = 32
OUTPUT_SIZE = 4096
EPOCHS = 40
BATCH_SIZE = 16384
LR = 0.001
LR_DROP_EPOCH = 100
LR_DROP_MULTIPLIER = 0.1
DATALOADER_WORKERS = 11
DATA_FILE = "65M.bin"
NETS_FOLDER = "nets"

if torch.cuda.is_available():
    device = torch.device("cuda:0")
else:
    device = torch.device("cpu")

class Net(torch.nn.Module):
    def __init__(self):
        super().__init__()
        self.conn1 = torch.nn.Linear(INPUT_SIZE, HIDDEN_SIZE)
        self.conn2 = torch.nn.Linear(HIDDEN_SIZE, OUTPUT_SIZE)

        # Random weights and biases
        torch.manual_seed(42)
        with torch.no_grad():
            self.conn1.weight.uniform_(-1, 1)
            self.conn2.weight.uniform_(-1, 1)
            self.conn1.bias.uniform_(-1, 1)
            self.conn2.bias.uniform_(-1, 1)

    def forward(self, x, illegals):
        x = self.conn1(x)
        x = torch.relu(x)
        x = self.conn2(x)
        x[illegals == 1] = -1000000 # Set illegals to -1M

        """
        for policy in x:
            numLegals = torch.sum(policy != -1000000).item()
            assert numLegals > 0 and numLegals <= 218
        """

        return x

@dataclass
class DataEntry:
    stm: ctypes.c_int8() = ctypes.c_int8()
    activeInputs: list[ctypes.c_int16()] = field(default_factory=list)
    movesIdxs: list[ctypes.c_int16()] = field(default_factory=list)
    bestMoveIdx: ctypes.c_int16() = ctypes.c_int16()

class MyDataset(Dataset):
    def __init__(self):
        self.numEntries = 0
        self.entries = [i for i in range(BATCH_SIZE)]
        self.batchesPosInFile = [0]

        file = open(DATA_FILE, "rb")
        entry = None
        while (entry := self.readEntry(file)):
            self.numEntries += 1
            if self.numEntries % BATCH_SIZE == 0:
                self.batchesPosInFile.append(file.tell())

        file.close()
        assert self.numEntries >= BATCH_SIZE
        self.numEntries -= self.numEntries % BATCH_SIZE
        self.batchesPosInFile.pop()

        print("Total positions:", self.numEntries)

    def __len__(self):
        return self.numEntries

    def __getitem__(self, idx):
        if idx % BATCH_SIZE == 0:
            file = open(DATA_FILE, "rb")
            file.seek(self.batchesPosInFile[int(idx / BATCH_SIZE)])

            for i in range(BATCH_SIZE):
                self.entries[i] = self.readEntry(file)
                assert self.entries[i]

            file.close()

        entry = self.entries[idx % BATCH_SIZE]

        inputs = torch.sparse_coo_tensor(
            torch.LongTensor(entry.activeInputs).unsqueeze(0), 
            torch.ones(len(entry.activeInputs)),
            (INPUT_SIZE,))

        target = torch.tensor([entry.bestMoveIdx])

        illegals = torch.ones(OUTPUT_SIZE)
        for moveIdx in entry.movesIdxs:
            illegals[moveIdx] = 0

        return (inputs, illegals, target)

    def readEntry(self, file):
        stm = file.read(1)
    
        if not stm:
            # End of file
            return False

        stm = struct.unpack('<b', stm)[0] # Read as i8
        assert stm == 0 or stm == 1

        numActiveInputs = file.read(1)
        numActiveInputs = struct.unpack('<B', numActiveInputs)[0] # Read as u8
        assert numActiveInputs >= 3 and numActiveInputs <= 32

        activeInputs = file.read(numActiveInputs * 2)
        activeInputs = struct.unpack("<{}h".format(numActiveInputs), activeInputs) # read as i16 list
        assert(len(activeInputs) == numActiveInputs)

        numMoves = file.read(1)
        numMoves = struct.unpack('<B', numMoves)[0] # Read as u8
        assert numMoves > 0 and numMoves <= 218

        movesIdxs = file.read(numMoves * 2)
        movesIdxs = struct.unpack("<{}h".format(numMoves), movesIdxs) # read as i16 list
        assert(len(movesIdxs) == numMoves)

        bestMoveIdx = file.read(2)
        bestMoveIdx = struct.unpack('<H', bestMoveIdx)[0] # read as u16
        assert bestMoveIdx < 4096

        return DataEntry(stm=stm, activeInputs=activeInputs, 
            movesIdxs=movesIdxs, bestMoveIdx=bestMoveIdx)

def collate(batch):
    # batch has BATCH_SIZE tuples
    # batch[0][0].size() = torch.Size([768])
    # batch[0][1].size() = torch.Size([4096])
    # batch[0][2].size() = torch.Size([1])
    stacked_inputs = torch.stack([item[0] for item in batch], dim=0)
    stacked_illegals = torch.stack([item[1] for item in batch], dim=0)
    stacked_targets = torch.cat([item[2] for item in batch], dim=0)
    return stacked_inputs, stacked_illegals, stacked_targets

class EncodeTensor(JSONEncoder,Dataset):
    def default(self, obj):
        if isinstance(obj, torch.Tensor):
            return obj.cpu().detach().numpy().tolist()
        return super(EncodeTensor, self).default(obj)
    
if __name__ == "__main__":
    print("Device:", device)
    print("Input size:", INPUT_SIZE)
    print("Hidden size:", HIDDEN_SIZE)
    print("Output size:", OUTPUT_SIZE)
    print("Epochs:", EPOCHS)
    print("Batch size:", BATCH_SIZE)
    print("Learning rate:", LR)
    print("LR drop epoch:", LR_DROP_EPOCH)
    print("LR drop multiplier:", LR_DROP_MULTIPLIER)
    print("Nets folder:", NETS_FOLDER)
    print("Dataloader workers:", DATALOADER_WORKERS)
    print("Data file:", DATA_FILE)

    net = Net()
    lossFunction = torch.nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(net.parameters(), lr=LR)

    dataset = MyDataset()
    dataLoader = torch.utils.data.DataLoader(
        dataset, batch_size=BATCH_SIZE, shuffle=False, num_workers=DATALOADER_WORKERS, collate_fn=collate)
    numBatches = len(dataLoader)
    assert numBatches == len(dataset.batchesPosInFile)

    if not os.path.exists(NETS_FOLDER):
        os.makedirs(NETS_FOLDER)

    print()

    for epoch in range(1, EPOCHS+1):
        totalEpochLoss = 0.0

        if epoch-1 == LR_DROP_EPOCH:
            for param_group in optimizer.param_groups:
                param_group['lr'] = LR * LR_DROP_MULTIPLIER
            print("LR dropped to {:.8f}".format(LR * LR_DROP_MULTIPLIER))

        for batchIdx, (inputs, illegals, target) in enumerate(dataLoader):
            assert inputs.dim() == 2
            assert inputs.size(0) == BATCH_SIZE
            assert inputs.size(1) == INPUT_SIZE
            assert illegals.dim() == 2
            assert illegals.size(0) == BATCH_SIZE
            assert illegals.size(1) == OUTPUT_SIZE
            assert target.dim() == 1
            assert target.size(0) == BATCH_SIZE

            optimizer.zero_grad()
            outputs = net(inputs, illegals)
            loss = lossFunction(outputs, target)
            loss.backward()
            optimizer.step()

            totalEpochLoss += loss.item()
            avgEpochLoss = float(totalEpochLoss) / float(batchIdx+1)

            sys.stdout.write("\rEpoch {}/{}, batch {}/{}, epoch train loss {:.4f}".format(
                epoch, EPOCHS, batchIdx+1, numBatches, avgEpochLoss))
            sys.stdout.flush()  

        netPthFileName = NETS_FOLDER + "/netEpoch" + str(epoch) + ".pth"
        netJsonFileName = NETS_FOLDER + "/netEpoch" + str(epoch) + ".json"
        print("\nSaving net to {} and {}".format(netPthFileName, netJsonFileName))

        torch.save(net.state_dict(), netPthFileName)
        with open(netJsonFileName, 'w') as json_file:
            json.dump(net.state_dict(), json_file,cls=EncodeTensor)

