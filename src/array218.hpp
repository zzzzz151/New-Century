// clang-format off

#pragma once

template <typename T>
struct Array218
{
    private:

    std::array<T, 218> elements;
    u8 numElements = 0;

    public:

    inline Array218() = default;

    inline void add(T elem) { 
        assert(numElements < 218);
        elements[numElements++] = elem;
    }

    inline T operator[](int i) {
        assert(i >= 0 && i < numElements);
        return elements[i];
    }

    inline u8 size() { return numElements; }

    inline void clear() { numElements = 0; }

    inline void swap(int i, int j)
    {
    	assert(i >= 0 && j >= 0 && i < numElements && j < numElements);
        T temp = elements[i];
        elements[i] = elements[j];
        elements[j] = temp;
    }

    inline void shuffle()
    {
        if (numElements <= 1) return;

        // Fisher-Yates shuffle algorithm 
        for (int i = 0; i < numElements; i++)
        {
            u8 randomIndex = randomU64() % numElements;
            swap(i, randomIndex);
        }
    }
};