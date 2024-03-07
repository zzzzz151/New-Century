#include <iostream>
#include <fstream>
#include <string>
#include <typeinfo>
#include <vector>
#include <torch/torch.h>
#include "board.hpp"

constexpr int HIDDEN_SIZE = 16;
constexpr int OUTPUT_SIZE = 4096;
constexpr int EPOCHS = 3;
constexpr u64 BATCH_SIZE = 16384;
constexpr double LR = 0.001;
constexpr std::string DATA_FILE_NAME = "16M.bin";
constexpr int WORKERS = 6;

std::ifstream inputFile;

struct MyNet : torch::nn::Module {
    public:

    MyNet() {
        connection1 = register_module("connection1", torch::nn::Linear(768, HIDDEN_SIZE));
        connection2 = register_module("connection2", torch::nn::Linear(HIDDEN_SIZE, OUTPUT_SIZE));

        // Random weights and biases
        torch::manual_seed(123);
        for (auto &param : parameters()) 
            if (param.requires_grad()) 
                torch::nn::init::uniform_(param, -1.0, 1.0);
    }

    torch::nn::Linear connection1{nullptr}, connection2{nullptr};

    torch::Tensor forward(torch::Tensor x) {
        /*
        auto sizes = x.sizes();
        std::cout << "Dimensions of x (sizes): ";
        for (size_t i = 0; i < sizes.size(); ++i) 
            std::cout << sizes[i] << " ";
        std::cout << std::endl;
        */

        x = connection1->forward(x);
        //x = torch::relu(x);
        x = torch::clamp(x, 0, 1); // crelu
        x = connection2->forward(x);
        x = torch::softmax(x, /*dim=*/1);
        return x;
    }

    void printParams() {
        for (const auto &pair : named_parameters()) 
            std::cout << pair.key() << ":" << std::endl
                      << pair.value() << std::endl
                      << "End of " << pair.key() << std::endl;
    }

};

#pragma pack(push, 1)
struct DataEntry {
    public:
    Color stm;
    std::array<u64, 12> piecesBitboards;
    u16 moveIdx;
};
#pragma pack(pop)

class CustomDataset : public torch::data::Dataset<CustomDataset> {
    public:
    u64 numEntries, numBatches;
    std::vector<DataEntry> batch;

    CustomDataset() {
        inputFile.open(DATA_FILE_NAME, std::ios::binary);
        assert(inputFile.is_open());

        inputFile.seekg(0, std::ios::end);
        numEntries = inputFile.tellg() / sizeof(DataEntry);
        inputFile.seekg(0, std::ios::beg);
        
        std::cout << "Total positions: " << numEntries << std::endl;
        numBatches = numEntries / BATCH_SIZE + ((numEntries % BATCH_SIZE) != 0);

        assert(numEntries > 0);
        assert(numBatches > 0);
    }

    torch::optional<size_t> size() const override {
        return numEntries;
    }

    torch::data::Example<> get(size_t index) override {
        if ((index % BATCH_SIZE) == 0)
        {
            assert((i64)numEntries - (i64)index > 0);
            batch.resize(std::min(BATCH_SIZE, numEntries - (u64)index));
            assert(batch.size() > 0);
            inputFile.read(reinterpret_cast<char*>(batch.data()), sizeof(DataEntry) * batch.size());
        }

        assert(batch.size() > 0);
        DataEntry entry = batch[index % batch.size()];

        torch::Tensor inputTensor = torch::zeros({768}, torch::kFloat32);

        for (int i = 0; i < entry.piecesBitboards.size(); ++i) 
            while (entry.piecesBitboards[i] > 0)
            {
                int sq = poplsb(entry.piecesBitboards[i]);
                inputTensor[i * 64 + sq] = 1.0;
            }

        // Encode target tensor (one hot encoded moveIdx) - OUTPUT_SIZE elements
        torch::Tensor targetTensor = torch::zeros({OUTPUT_SIZE}, torch::kFloat32);
        assert(entry.moveIdx < OUTPUT_SIZE);
        targetTensor[entry.moveIdx] = 1.0;

        return {inputTensor, targetTensor};
    }
};

int main() {
    #if defined(__AVX512F__) && defined(__AVX512BW__)
        std::cout << "Using avx512" << std::endl;
    #elif defined(__AVX2__)
        std::cout << "Using avx2" << std::endl;
    #else
        std::cout << "Not using avx2 or avx512" << std::endl;
    #endif

    std::cout << "Epochs: " << EPOCHS << std::endl;
    std::cout << "Batch size: " << BATCH_SIZE << std::endl;
    std::cout << "LR: " << LR << std::endl;
    std::cout << "Dataloader workers: " << WORKERS << std::endl;
    std::cout << "Data file: " << DATA_FILE_NAME << std::endl;

    assert(HIDDEN_SIZE > 0);
    assert(OUTPUT_SIZE > 0);
    assert(EPOCHS > 0);
    assert(BATCH_SIZE > 0);
    assert(LR > 0);
    assert(WORKERS > 0);

    initUtils();
    attacks::init();

    auto device = torch::Device(torch::kCPU);   
    auto net = std::make_shared<MyNet>();
    net->to(device);

    /*
    torch::Tensor input = torch::rand({1, 768});
    input = (input >= 0.5).to(torch::kFloat32);
    torch::Tensor output = net.forward(input);
    std::cout << output << std::endl;
    */

    CustomDataset dataset = CustomDataset();
    auto datasetMapped = dataset.map(torch::data::transforms::Stack<>());

    auto dataLoader = torch::data::make_data_loader<torch::data::samplers::SequentialSampler>(
        std::move(datasetMapped), 
        torch::data::DataLoaderOptions().batch_size(BATCH_SIZE).workers(WORKERS));

    torch::optim::Adam optimizer(net->parameters(), torch::optim::AdamOptions(LR));
    auto lossFunction = torch::nn::CrossEntropyLoss();

    std::cout << std::endl;

    for (int epoch = 1; epoch <= EPOCHS; ++epoch)
    {
        std::cout << "Starting epoch " << epoch << "/" << EPOCHS << std::endl;
        inputFile.seekg(0, std::ios::beg);
        i64 batchIdx = -1;
        double epochLoss = 0;

        for (auto &batch : *dataLoader) {
            batchIdx++;
            
            auto data = batch.data.to(device);
            auto targets = batch.target.to(device);

            optimizer.zero_grad(); // Clear the optimizer parameters
            auto outputs = net->forward(data);
            auto loss = lossFunction(outputs, targets);

            loss.backward(); // Compute gradients
            optimizer.step(); // Update net params (weights and biases)
            epochLoss += loss.item<double>();

            std::cout << "Epoch " << epoch << "/" << EPOCHS
                      << ", finished batch " << batchIdx+1 << "/" << dataset.numBatches 
                      << ", epoch train loss " << epochLoss / (batchIdx+1)
                      << (batchIdx == dataset.numBatches - 1 ? "\n" : "\r");
        }

        assert(batchIdx == dataset.numBatches - 1);
    }

    std::cout << "Training finished" << std::endl;

    Board board = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    torch::Tensor inputTensor = torch::zeros({768}, torch::kFloat32);
    for (int i = 0; i < board.mPiecesBitboards.size(); i++) {
        u64 bb = board.mPiecesBitboards[i];
        while (bb > 0) {
            int sq = poplsb(bb); 
            inputTensor[i * 64 + sq] = 1.0;
        }
    }
    auto output = net->forward(inputTensor);

    return 0;
}