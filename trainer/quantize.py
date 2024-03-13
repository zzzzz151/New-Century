from train import *

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
        param.data *= 255.0
        param.data = param.data.round()

    if not os.path.exists(OUTPUT_FOLDER):
        os.makedirs(OUTPUT_FOLDER)

    torch.save(net.state_dict(), OUTPUT_FOLDER + "/" + netFileRaw + ".pth")
    with open(OUTPUT_FOLDER + "/" + netFileRaw + ".json", 'w') as json_file:
        json.dump(net.state_dict(), json_file,cls=EncodeTensor)
        