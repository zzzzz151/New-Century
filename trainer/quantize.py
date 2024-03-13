from train import *
import numpy as np

OUTPUT_FOLDER = "quantized"

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Invalid num of args")
        exit(1)

    netFile = sys.argv[1] # "nets/netEpoch40.pth"
    netFileRaw = netFile.split("/")[-1][:-4] # "netEpoch40"

    if netFile[-4:] != ".pth":
        print("Wrong extension, must be .pth")
        exit(1)

    net = Net().to(device)
    net.load_state_dict(torch.load(sys.argv[1]))

    for param_name, param in net.named_parameters():
        #param.data *= 255.0
        #param.data = param.data.round()
        pass

    if not os.path.exists(OUTPUT_FOLDER):
        os.makedirs(OUTPUT_FOLDER)

    # save .json
    torch.save(net.state_dict(), OUTPUT_FOLDER + "/" + netFileRaw + ".pth")

    # save .pth
    with open(OUTPUT_FOLDER + "/" + netFileRaw + ".json", 'w') as json_file:
        json.dump(net.state_dict(), json_file,cls=EncodeTensor)
        
    # save .bin
    with open(OUTPUT_FOLDER + "/" + netFileRaw + ".bin", 'wb') as binFile:
        # Write weights1
        for i in range(INPUT_SIZE):
            for j in range(HIDDEN_SIZE):
                weight = np.float32(net.conn1.weight[j, i].item())
                weight.tofile(binFile)

        # Write hidden biases
        for i in range(HIDDEN_SIZE):
            bias = np.float32(net.conn1.bias[i].cpu().detach().numpy())
            bias.tofile(binFile)

        # Write weights2
        for i in range(HIDDEN_SIZE):
            for j in range(OUTPUT_SIZE):
                weight = np.float32(net.conn2.weight[j, i].item())
                weight.tofile(binFile)
        
        # Write output biases
        for i in range(OUTPUT_SIZE):
            bias = np.float32(net.conn2.bias[i].cpu().detach().numpy())
            bias.tofile(binFile)
