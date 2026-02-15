#pragma once

#include <array>
#include <vector>
#include <limits>

#include <../Gigantua/ChessBase.hpp>

namespace Search {

	struct TTable {
		const size_t HashTableSize;

		enum class Flag{
			Value = 0,
			Alpha,
			Beta
		};

		static constexpr int NAN_VAL = 0x7fffffff;

		struct Node {
			Gigantua::Board brd;
			uint64_t smpKey = 0ull;
			uint64_t smpData = NAN_VAL;

			int ExtractScore() const {
				const uint32_t scoreData = (smpData & 0x00000000FFFFFFFFull);
				return *(int*)(&scoreData);
			}

			uint16_t ExtractMove() const {
				return (uint16_t)((smpData & 0x0000FFFF00000000ull) >> 32);
			}

			uint8_t ExtractDepth() const {
				return (uint8_t)((smpData & 0x00FF000000000000ull) >> 48);
			}

			Flag ExtractFlag() const {
				return (Flag)((smpData & 0xFF00000000000000ull) >> 56);
			}

			static uint64_t PackData(int score, uint16_t move, uint8_t depth, Flag flag) {
				const int s = score;
				const uint64_t scoreData = *(uint32_t*)(&s);
				const uint64_t moveData = move;
				const uint64_t depthData = depth;
				const uint64_t flagData = (uint8_t)flag;

				uint64_t data = scoreData;
				data |= moveData << 32;
				data |= depthData << 48;
				data |= flagData << 56;

				return data;
			}

		};

		static constexpr uint8_t BucketSize = 2;

		typedef std::array<Node, BucketSize> Bucket;

		typedef std::vector<Bucket> HashTable;

		mutable HashTable hashTable;

		TTable(size_t size) : HashTableSize(size / BucketSize), hashTable(HashTableSize) {
		}

		void Clear() {
			static Bucket empty;
			std::fill(hashTable.begin(), hashTable.end(), empty);
		}

		uint16_t GetBestMove(const Gigantua::Board& brd) const {
			const Bucket& bucket = hashTable[brd.Hash % HashTableSize];
			for(size_t i = 0; i < BucketSize; i++) {
				const Node& node = bucket[i];
				const uint64_t testKey = brd.Hash ^ node.smpData;
				if (testKey == node.smpKey && node.ExtractFlag() == Flag::Value && node.brd == brd) {
					return node.ExtractMove();
				}
			}
			return 0;	
		}

		int Get(const Gigantua::Board& brd, int alpha, int beta, uint8_t depth, uint16_t& bestMove) const {
			Bucket& bucket = hashTable[brd.Hash % HashTableSize];
			for (size_t i = 0; i < BucketSize; i++) {
				Node& node = bucket[i];
				const uint64_t testKey = brd.Hash ^ node.smpData;
				if (testKey == node.smpKey && node.brd == brd) {
					bestMove = node.ExtractMove();

					if (node.ExtractDepth() >= depth) {
						int score = node.ExtractScore();

						switch (node.ExtractFlag()) {
						case Flag::Value:
							return score;
						case Flag::Alpha:
						{
							if (score <= alpha) return alpha;
							break;
						}
						case Flag::Beta:
						{
							if (score >= beta) return beta;
							break;
						}
						default:
							break;
						}
					}
				}
				break;
			}
			
			return NAN_VAL;
		}

		void Put(const Gigantua::Board& brd, int cost, uint16_t bestMove, uint8_t depth, Flag flag) {
			Bucket& bucket = hashTable[brd.Hash % HashTableSize];

			uint8_t minDepth = 255;
			uint8_t minIndex = 0;

			for (uint8_t i = 0; i < BucketSize; i++) {
				Node& node = bucket[i];

				if (node.smpKey != 0) {
					if (node.brd == brd && node.ExtractDepth() > depth)
						return;
				}

				if (node.ExtractDepth() == 0) {
					minIndex = i;
					break;
				}

				if (bucket[i].ExtractDepth() < minDepth) {
					minDepth = bucket[i].ExtractDepth();
					minIndex = i;
				}
			}

			bucket[minIndex].brd = brd;
			bucket[minIndex].smpData = Node::PackData(cost, bestMove, depth, flag);
			bucket[minIndex].smpKey = brd.Hash ^ bucket[minIndex].smpData;
		}
	};

}

