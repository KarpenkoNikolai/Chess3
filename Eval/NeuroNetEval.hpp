#pragma once

#include <../Gigantua/ChessBase.hpp>

#include "NeuroNetOpt.hpp"

namespace NN
{
	class NeuroNetEval
	{
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

		float Evaluate(const Gigantua::Board& brd)
		{
			float nnEval = m_nn.Evaluate(brd);
			if(abs(nnEval) > 2200) {
				const float matEval = 2.0f*(EvaluateMaterial(brd) + EvaluateQueenKingMate(brd));
				nnEval += brd.status.WhiteMove() ? matEval : -matEval;
			}
			
			return nnEval;
		}
	};

}

