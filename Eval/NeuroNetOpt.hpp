#pragma once

#include <../Gigantua/ChessBase.hpp>
#include <array>
#include <vector>
#include <immintrin.h>
#include <algorithm>


namespace NN
{
	namespace NeuroNetOpt
	{
		static constexpr size_t InputSize = 10 * 64;
		static constexpr std::array<uint32_t, 3> Architecture = { 512, 32, 32 };
		static constexpr size_t ActiveIndexSize = 32;
		static constexpr size_t HalfInputSize = Architecture[0] >> 1;
		static int16_t mFirstWeights alignas(64)[InputSize * HalfInputSize * 64];
		static int32_t mFirstBiases alignas(64)[HalfInputSize * 64];
		static int8_t mWeights1 alignas(64)[Architecture[0] * Architecture[1]];
		static int32_t mBiases1 alignas(64)[Architecture[1]];
		static int8_t mWeights2 alignas(64)[Architecture[1] * Architecture[2]];
		static int32_t mBiases2 alignas(64)[Architecture[2]];
		static int8_t mWeights3 alignas(64)[Architecture[2]];
		static int32_t mBiases3;

		struct ActiveIndex {
			uint16_t value[ActiveIndexSize];
			uint16_t size = 0;
		};

		static void fillFromBitBoard(uint16_t from, uint64_t bits, uint16_t*& index, uint16_t& size) {
			Bitloop(bits) {
				index[size++] = SquareOf(bits) + from;
			}
		}

		static uint32_t GetInput(const std::vector<uint64_t>& brd, uint16_t*& index) {
			uint16_t size = 0;

			for (size_t i = 0; i < brd.size(); i++)
				fillFromBitBoard(uint16_t(i * 64), brd[i], index, size);

			return size;
		}

		template <uint32_t inDims, uint32_t outDims>
		inline void affine_txfm(const int8_t* input, int8_t* output, const int32_t* biases, const int8_t* weights)
		{
			int32_t tmp[outDims];
			std::memcpy(tmp, biases, outDims * sizeof(int32_t));

			for (uint32_t idx = 0; idx < inDims; idx++) {
				const int8_t inp = input[idx];
				if (inp == 0) continue;
				const int8_t* w = weights + idx * outDims;

				for (uint32_t i = 0; i < outDims; i++) {
					tmp[i] += inp * w[i];
				}
			}

			for (uint32_t i = 0; i < outDims; i++)
				output[i] = std::clamp(tmp[i] >> 6, 0, 127);
		}

		template <uint32_t inDims>
		inline void affine_txfm_32_avx2(
			const int8_t* input,
			int8_t* output,
			const int32_t* biases,
			const int8_t* weights)
		{
			// 32 outputs → 4 vectors of 8 int32 each
			__m256i acc0 = _mm256_loadu_si256((__m256i*)(biases + 0));
			__m256i acc1 = _mm256_loadu_si256((__m256i*)(biases + 8));
			__m256i acc2 = _mm256_loadu_si256((__m256i*)(biases + 16));
			__m256i acc3 = _mm256_loadu_si256((__m256i*)(biases + 24));

			for (uint32_t idx = 0; idx < inDims; idx++) {
				int8_t inp = input[idx];
				if (!inp) continue;

				__m256i vinp = _mm256_set1_epi32(inp);
				const int8_t* w = weights + idx * 32;

				__m256i w0 = _mm256_cvtepi8_epi32(_mm_loadl_epi64((__m128i*)(w + 0)));
				__m256i w1 = _mm256_cvtepi8_epi32(_mm_loadl_epi64((__m128i*)(w + 8)));
				__m256i w2 = _mm256_cvtepi8_epi32(_mm_loadl_epi64((__m128i*)(w + 16)));
				__m256i w3 = _mm256_cvtepi8_epi32(_mm_loadl_epi64((__m128i*)(w + 24)));

				acc0 = _mm256_add_epi32(acc0, _mm256_mullo_epi32(vinp, w0));
				acc1 = _mm256_add_epi32(acc1, _mm256_mullo_epi32(vinp, w1));
				acc2 = _mm256_add_epi32(acc2, _mm256_mullo_epi32(vinp, w2));
				acc3 = _mm256_add_epi32(acc3, _mm256_mullo_epi32(vinp, w3));
			}

			// Store accumulators into a temporary array
			alignas(32) int32_t tmp[32];
			_mm256_storeu_si256((__m256i*)(tmp + 0), acc0);
			_mm256_storeu_si256((__m256i*)(tmp + 8), acc1);
			_mm256_storeu_si256((__m256i*)(tmp + 16), acc2);
			_mm256_storeu_si256((__m256i*)(tmp + 24), acc3);

			for (int i = 0; i < 32; i++) {
				output[i] = (int8_t)std::clamp(tmp[i] >> 6, 0, 127);
			}
		}

		template <bool white>
		inline void FillAcc(int32_t* acc, const Gigantua::Board& brd)
		{
			std::vector<uint64_t> bitBoards(10);
			const auto mirr = brd.Mirror();

			bitBoards[0] = brd.WPawn;
			bitBoards[1] = brd.BPawn;
			bitBoards[2] = brd.WKnight;
			bitBoards[3] = brd.BKnight;
			bitBoards[4] = brd.WBishop;
			bitBoards[5] = brd.BBishop;
			bitBoards[6] = brd.WRook;
			bitBoards[7] = brd.BRook;
			bitBoards[8] = brd.WQueen;
			bitBoards[9] = brd.BQueen;

			ActiveIndex mIndexW;

			{
				uint16_t* iPtr = mIndexW.value;
				mIndexW.size = GetInput(bitBoards, iPtr);
			}

			bitBoards[0] = mirr.WPawn;
			bitBoards[1] = mirr.BPawn;
			bitBoards[2] = mirr.WKnight;
			bitBoards[3] = mirr.BKnight;
			bitBoards[4] = mirr.WBishop;
			bitBoards[5] = mirr.BBishop;
			bitBoards[6] = mirr.WRook;
			bitBoards[7] = mirr.BRook;
			bitBoards[8] = mirr.WQueen;
			bitBoards[9] = mirr.BQueen;

			ActiveIndex mIndexB;

			{
				uint16_t* iPtr = mIndexB.value;
				mIndexB.size = GetInput(bitBoards, iPtr);
			}

			const uint8_t wKingIndex = SquareOf(brd.WKing);
			const uint8_t bKingIndex = SquareOf(mirr.WKing);

			std::memcpy(acc, mFirstBiases + wKingIndex * HalfInputSize, HalfInputSize * sizeof(int32_t));
			std::memcpy(acc + HalfInputSize, mFirstBiases + bKingIndex * HalfInputSize, HalfInputSize * sizeof(int32_t));

			if constexpr (white) {
				int32_t* accPtr = acc;
				{
					const int16_t* wWeights = mFirstWeights + wKingIndex * InputSize * HalfInputSize;
					for (uint16_t j = 0; j < mIndexW.size; j++) {
						const int16_t* w = wWeights + HalfInputSize * mIndexW.value[j];
						for (uint32_t i = 0; i < HalfInputSize; i++) {
							accPtr[i] += w[i];
						}
					}
				}

				accPtr = acc + HalfInputSize;
				{
					const auto& bWeights = mFirstWeights + bKingIndex * InputSize * HalfInputSize;
					for (uint16_t j = 0; j < mIndexB.size; j++) {
						const int16_t* w = bWeights + HalfInputSize * (mIndexB.value[j]);
						for (uint32_t i = 0; i < HalfInputSize; i++) {
							accPtr[i] += w[i];
						}
					}
				}
			}
			else {
				int32_t* accPtr = acc;
				{
					const int16_t* bWeights = mFirstWeights + bKingIndex * InputSize * HalfInputSize;
					for (uint16_t j = 0; j < mIndexB.size; j++) {
						const int16_t* w = bWeights + HalfInputSize * mIndexB.value[j];
						for (uint32_t i = 0; i < HalfInputSize; i++) {
							accPtr[i] += w[i];
						}
					}
				}

				accPtr = acc + HalfInputSize;
				{
					const auto& wWeights = mFirstWeights + wKingIndex * InputSize * HalfInputSize;
					for (uint16_t j = 0; j < mIndexW.size; j++) {
						const int16_t* w = wWeights + HalfInputSize * (mIndexW.value[j]);
						for (uint32_t i = 0; i < HalfInputSize; i++) {
							accPtr[i] += w[i];
						}
					}
				}
			}

		}

		static int32_t Evaluate(const Gigantua::Board& brd)
		{
			int8_t input[Architecture[0]];

			{//NNUE first layer evaluation
				int32_t acc[Architecture[0]];
				if (brd.status.WhiteMove())
					FillAcc<true>(acc, brd);
				else
					FillAcc<false>(acc, brd);

				for (uint32_t i = 0; i < Architecture[0]; i++) {
					input[i] = int8_t(std::clamp(acc[i], 0, 127));
				}
			}

			int8_t output[Architecture[1]];
			//affine_txfm<Architecture[0], Architecture[1]>(input, output, mBiases1, mWeights1);
			//affine_txfm<Architecture[1], Architecture[2]>(output, output, mBiases2, mWeights2);

			affine_txfm_32_avx2<512>(input, output, mBiases1, mWeights1);
			affine_txfm_32_avx2<32>(output, output, mBiases2, mWeights2);

			int32_t result = mBiases3;
			for (uint32_t i = 0; i < Architecture[2]; i++) {
				result += output[i] * mWeights3[i];
			}

			return result / 16;
		}

		static void SetGenome(const std::vector<float>& genome)
		{
			size_t k = 0;
			float maxVal = 0;
			for(size_t b = 0; b < 64; b++)
			{// first layer
				for (size_t i = 0; i < InputSize; ++i) {
					for (size_t j = 0; j < HalfInputSize; ++j) {
						if (abs(genome[k]) > maxVal) maxVal = abs(genome[k]);
						mFirstWeights[b * InputSize * HalfInputSize + i * HalfInputSize + j] = int16_t(genome[k++]);
					}
				}

				std::cout << "mFirstWeights " << maxVal << std::endl;

				maxVal = 0;
				for (size_t j = 0; j < HalfInputSize; ++j) {
					if (abs(genome[k]) > maxVal) maxVal = abs(genome[k]);
					mFirstBiases[b * HalfInputSize + j] = int16_t(genome[k++]);
				}

				std::cout << "mFirstBiases " << maxVal << std::endl;
			}

			{
				maxVal = 0;
				for (size_t i = 0; i < Architecture[0]; ++i) {
					for (size_t j = 0; j < Architecture[1]; ++j) {
						if (abs(genome[k]) > maxVal) maxVal = abs(genome[k]);
						mWeights1[i* Architecture[1]  + j] = int8_t(genome[k++] * 64.0f);
					}
				}

				std::cout << "mWeights1 " << maxVal << std::endl;

				maxVal = 0;
				for (size_t j = 0; j < Architecture[1]; ++j) {
					if (abs(genome[k]) > maxVal) maxVal = abs(genome[k]);
					mBiases1[j] = int32_t(genome[k++] * 64.0f);
				}

				std::cout << "mBiases1 " << maxVal << std::endl;
			}

			{
				maxVal = 0;
				for (size_t i = 0; i < Architecture[1]; ++i) {
					for (size_t j = 0; j < Architecture[2]; ++j) {
						if (abs(genome[k]) > maxVal) maxVal = abs(genome[k]);
						mWeights2[i * Architecture[2] + j] = int8_t(genome[k++] * 64.0f);
					}
				}

				std::cout << "mWeights2 " << maxVal << std::endl;

				maxVal = 0;
				for (size_t j = 0; j < Architecture[2]; ++j) {
					if (abs(genome[k]) > maxVal) maxVal = abs(genome[k]);
					mBiases2[j] = int32_t(genome[k++] * 64.0f);
				}

				std::cout << "mBiases2 " << maxVal << std::endl;
			}

			{
				maxVal = 0;
				for (size_t i = 0; i < Architecture[2]; ++i) {
					if (abs(genome[k]) > maxVal) maxVal = abs(genome[k]);
					mWeights3[i] = int16_t(genome[k++] * 16.0f);
				}

				std::cout << "mWeights3 " << maxVal << std::endl;

				maxVal = 0;
				if (abs(genome[k]) > maxVal) maxVal = abs(genome[k]);
				mBiases3 = int32_t(genome[k++] * 16.0f);

				std::cout << "mBiases3 " << maxVal << std::endl;
			}
		}

	}
}