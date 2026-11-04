#pragma once

#include <../Gigantua/ChessBase.hpp>

#include "NeuroNetOpt.hpp"

namespace NN
{
	class NeuroNetEval
	{
	private:
		struct CacheEntry {
			static constexpr int NAN_VAL = 0x7fffffff;

			struct Node {
				uint64_t brdHash;
				uint64_t smpKey = 0ull;
				uint64_t smpData = NAN_VAL;

				int ExtractScore() const {
					const uint32_t scoreData = (smpData & 0x00000000FFFFFFFFull);
					return *(int*)(&scoreData);
				}

				static uint64_t PackData(int score) {
					const int s = score;
					const uint64_t scoreData = *(uint32_t*)(&s);
					uint64_t data = scoreData;
					return data;
				}

			};

			const size_t HashTableSize;

			typedef std::vector<Node> HashTable;

			mutable HashTable hashTable;

			CacheEntry() : HashTableSize(20000000), hashTable(HashTableSize) {}

			int Get(const Gigantua::Board& brd) {
				Node& node = hashTable[brd.Hash % HashTableSize];
				const uint64_t testKey = brd.Hash ^ node.smpData;

				if (testKey == node.smpKey && node.brdHash == brd.Hash) {
					return node.ExtractScore();
				}

				return NAN_VAL;
			}

			void Put(const Gigantua::Board& brd, int cost) {
				Node& node = hashTable[brd.Hash % HashTableSize];

				if (node.smpKey != 0) {
					if (node.brdHash == brd.Hash)
						return;
				}
						
				node.brdHash = brd.Hash;
				node.smpData = Node::PackData(cost);
				node.smpKey = brd.Hash ^ node.smpData;
			}
		};

		CacheEntry m_cacheEntry;
	public:
		NeuroNetOpt m_nn;

		void SetGenome(const std::vector<float>& genome) {
			m_nn.SetGenome(genome);
		}

		static float EvaluateMaterial(const Gigantua::Board& brd)
		{
			float eval = 0;

			eval += (136) * (Bitcount(brd.WPawn) - Bitcount(brd.BPawn));
			eval += (782) * (Bitcount(brd.WKnight) - Bitcount(brd.BKnight));
			eval += (830) * (Bitcount(brd.WBishop) - Bitcount(brd.BBishop));
			eval += (1289) * (Bitcount(brd.WRook) - Bitcount(brd.BRook));
			eval += (2529) * (Bitcount(brd.WQueen) - Bitcount(brd.BQueen));

			return eval;
		}

		static float EvaluateQueenKingMate(const Gigantua::Board& brd)
		{
			const int whitePieces = Bitcount(brd.WKnight | brd.WBishop | brd.WRook | brd.WQueen);
			const int blackPieces = Bitcount(brd.BKnight | brd.BBishop | brd.BRook | brd.BQueen);
			
			float eval = 0.0f;

			// White has queen vs lone king
			if ((brd.WQueen || brd.WRook) && blackPieces == 0) {
				const int bKingSquare = SquareOf(brd.BKing);
				const int bKingRank = bKingSquare / 8;
				const int bKingFile = bKingSquare % 8;
				
				// Push black king to edge
				const int distToEdgeRank = std::min(bKingRank, 7 - bKingRank);
				const int distToEdgeFile = std::min(bKingFile, 7 - bKingFile);
				const int minDistToEdge = std::min(distToEdgeRank, distToEdgeFile);
				eval += (3 - minDistToEdge) * 100.0f;
				
				// Bring white king closer for mating
				const int wKingSquare = SquareOf(brd.WKing);
				const int wKingRank = wKingSquare / 8;
				const int wKingFile = wKingSquare % 8;
				const int kingDistance = std::abs(wKingRank - bKingRank) + std::abs(wKingFile - bKingFile);
				eval += (14 - kingDistance) * 60.0f;
			}
			// Black has queen vs lone king
			else if ((brd.BQueen || brd.BRook) && whitePieces == 0) {
				const int wKingSquare = SquareOf(brd.WKing);
				const int wKingRank = wKingSquare / 8;
				const int wKingFile = wKingSquare % 8;
				
				// Push white king to edge
				const int distToEdgeRank = std::min(wKingRank, 7 - wKingRank);
				const int distToEdgeFile = std::min(wKingFile, 7 - wKingFile);
				const int minDistToEdge = std::min(distToEdgeRank, distToEdgeFile);
				eval -= (3 - minDistToEdge) * 100.0f;
				
				// Bring black king closer for mating
				const int bKingSquare = SquareOf(brd.BKing);
				const int bKingRank = bKingSquare / 8;
				const int bKingFile = bKingSquare % 8;
				const int kingDistance = std::abs(wKingRank - bKingRank) + std::abs(wKingFile - bKingFile);
				eval -= (14 - kingDistance) * 60.0f;
			}

			return eval;
		}

		int Evaluate(const Gigantua::Board& brd)
		{
			int cost = m_cacheEntry.Get(brd);
			if (cost != CacheEntry::NAN_VAL) {
				return cost;
			}

			int nnEval = m_nn.Evaluate(brd);
			if (abs(nnEval) > 2300) {
				const int matEval = (EvaluateMaterial(brd) + EvaluateQueenKingMate(brd));
				nnEval += brd.status.WhiteMove() ? matEval : -matEval;
			}

			m_cacheEntry.Put(brd, nnEval);
			
			return nnEval;
		}
	};

}

