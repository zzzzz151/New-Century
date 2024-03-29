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
import time

INPUT_SIZE = 768
HIDDEN_SIZE = 32
OUTPUT_SIZE = 4096
CHECKPOINT = None # Set to "nets/netEpoch15.pth" if resuming training, else None
START_EPOCH = 1 # Set to 1 if not resuming training
EPOCHS = 40
BATCH_SIZE = 16384
LR = 0.001
LR_DROP_EPOCH = 20
LR_DROP_MULTIPLIER = 0.2
DATALOADER_WORKERS = 11
DATA_FILE = "1M.bin"
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
        x = x.to(device)
        illegals = illegals.to(device)

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
    moves4096: list[ctypes.c_int16()] = field(default_factory=list)
    bestMove4096: ctypes.c_int16() = ctypes.c_int16()

class MyDataset(Dataset):
    def __init__(self, fileName, batchSize):
        self.fileName = fileName
        self.batchSize = batchSize
        self.numEntries = 0
        self.entries = [i for i in range(self.batchSize)]
        self.batchesPosInFile = [0]

        file = open(fileName, "rb")
        entry = None
        while (entry := self.readEntry(file)):
            self.numEntries += 1
            if self.numEntries % self.batchSize == 0:
                self.batchesPosInFile.append(file.tell())
        file.close()

        assert self.numEntries >= self.batchSize
        self.numEntries -= self.numEntries % self.batchSize
        self.batchesPosInFile.pop()
        print("Total positions:", self.numEntries)

    def __len__(self):
        return self.numEntries

    def __getitem__(self, idx):
        if idx % self.batchSize == 0:
            # Load new batch from file
            file = open(self.fileName, "rb")
            file.seek(self.batchesPosInFile[int(idx / self.batchSize)])
            for i in range(self.batchSize):
                self.entries[i] = self.readEntry(file)
                assert self.entries[i]
            file.close()

        entry = self.entries[idx % self.batchSize]

        inputs = torch.zeros(INPUT_SIZE)
        for activeInput in entry.activeInputs:
            inputs[activeInput] = 1

        illegals = torch.ones(OUTPUT_SIZE)
        for move4096 in entry.moves4096:
            illegals[move4096] = 0

        return (inputs, illegals, torch.tensor(entry.bestMove4096))

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

        moves4096 = file.read(numMoves * 2)
        moves4096 = struct.unpack("<{}h".format(numMoves), moves4096) # read as i16 list
        assert(len(moves4096) == numMoves)

        bestMove4096 = file.read(2)
        bestMove4096 = struct.unpack('<H', bestMove4096)[0] # read as u16
        assert bestMove4096 < 4096

        return DataEntry(stm=stm, activeInputs=activeInputs, 
            moves4096=moves4096, bestMove4096=bestMove4096)

class EncodeTensor(JSONEncoder,Dataset):
    def default(self, obj):
        if isinstance(obj, torch.Tensor):
            return obj.cpu().detach().numpy().tolist()
        return super(EncodeTensor, self).default(obj)

if __name__ == "__main__":
    if device != torch.device("cpu"):
        print("Device: {} {}".format(device, torch.cuda.get_device_name(0)))
    else:
        print("Device: cpu")
    print("Net arch: {}->{}->{}".format(INPUT_SIZE, HIDDEN_SIZE, OUTPUT_SIZE))
    print("Resuming from:", CHECKPOINT)
    print("Epochs:", EPOCHS)
    print("Batch size:", BATCH_SIZE)
    print("Learning rate:", LR)
    print("LR drop epoch:", LR_DROP_EPOCH)
    print("LR drop multiplier:", LR_DROP_MULTIPLIER)
    print("Nets folder:", NETS_FOLDER)
    print("Dataloader workers:", DATALOADER_WORKERS)
    print("Data file:", DATA_FILE)

    net = Net().to(device)
    if CHECKPOINT != None and CHECKPOINT is not None and CHECKPOINT != "":
        assert os.path.exists(CHECKPOINT)
        net.load_state_dict(torch.load(CHECKPOINT))

    lossFunction = torch.nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(net.parameters(), lr=LR)
    dataset = MyDataset(DATA_FILE, BATCH_SIZE)

    dataLoader = torch.utils.data.DataLoader(
        dataset, 
        batch_size=BATCH_SIZE, 
        shuffle=False, 
        num_workers=DATALOADER_WORKERS, 
        pin_memory = device != torch.device("cpu"))

    numBatches = len(dataLoader)
    assert numBatches == len(dataset.batchesPosInFile)

    if not os.path.exists(NETS_FOLDER):
        os.makedirs(NETS_FOLDER)

    print()

    for epoch in range(START_EPOCH, EPOCHS+1):
        epochStartTime = time.time()
        totalEpochLoss = 0.0

        if epoch-1 == LR_DROP_EPOCH:
            for param_group in optimizer.param_groups:
                param_group['lr'] = LR * LR_DROP_MULTIPLIER
            print("LR dropped to {:.8f}".format(LR * LR_DROP_MULTIPLIER))

        for batchIdx, (inputs, illegals, target) in enumerate(dataLoader):
            target = target.to(device)
            optimizer.zero_grad(set_to_none=True)
            outputs = net(inputs, illegals)
            loss = lossFunction(outputs, target)
            loss.backward()
            optimizer.step()
            totalEpochLoss += loss.item()

            if batchIdx == 0 or batchIdx == numBatches-1 or (batchIdx+1) % 8 == 0:
                avgEpochLoss = float(totalEpochLoss) / float(batchIdx+1)
                epochPosPerSec = BATCH_SIZE * (batchIdx+1) / (time.time() - epochStartTime) 

                sys.stdout.write("\rEpoch {}/{}, batch {}/{}, epoch train loss {:.4f}, {} positions/s".format(
                    epoch, EPOCHS, batchIdx+1, numBatches, avgEpochLoss, int(epochPosPerSec)))
                sys.stdout.flush()  

        netPthFileName = NETS_FOLDER + "/netEpoch" + str(epoch) + ".pth"
        netJsonFileName = NETS_FOLDER + "/netEpoch" + str(epoch) + ".json"
        print("\nSaving net to {} and {}".format(netPthFileName, netJsonFileName))

        torch.save(net.state_dict(), netPthFileName)
        with open(netJsonFileName, 'w') as json_file:
            json.dump(net.state_dict(), json_file,cls=EncodeTensor)

