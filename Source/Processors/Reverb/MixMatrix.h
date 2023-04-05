/**
 * MixMatrix.h
 * Helper functions for using Hadamard or Householder matrices
*/

template <int size>
struct MixMatrix
{
    MixMatrix() = default;

    // Expects an array of N channels worth of samples
    template <typename T>
    static inline void processHadamardMatrix(T *ch)
    {
        recursive(ch);

        auto scale = std::sqrt(1.0 / (double)size);

        for (int i = 0; i < size; ++i)
            ch[i] *= scale;
    }

    // Expects an array of N channels worth of samples
    template <typename T>
    static void processHouseholder(T *ch)
    {
        const T h_mult = -2.0 / (T)size;

        T sum = 0.0;
        for (int j = 0; j < size; ++j)
            sum += ch[j];

        sum *= h_mult;

        for (int j = 0; j < size; ++j)
            ch[j] += sum;
    }

    template <typename T>
    static inline void recursive(T *data)
    {
        if (size <= 1)
            return;
        const int hSize = size / 2;

        MixMatrix<hSize>::recursive(data);
        MixMatrix<hSize>::recursive(data + hSize);

        for (int i = 0; i < hSize; ++i)
        {
            T a = data[i];
            T b = data[i + hSize];
            data[i] = a + b;
            data[i + hSize] = a - b;
        }
    }
};