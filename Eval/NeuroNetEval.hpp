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

		static float EvaluateMat(const Gigantua::Board& brd)
		{
			float eval = 0;

			eval += (136) * (Bitcount(brd.WPawn) - Bitcount(brd.BPawn));
			eval += (782) * (Bitcount(brd.WKnight) - Bitcount(brd.BKnight));
			eval += (830) * (Bitcount(brd.WBishop) - Bitcount(brd.BBishop));
			eval += (1289) * (Bitcount(brd.WRook) - Bitcount(brd.BRook));
			eval += (2529) * (Bitcount(brd.WQueen) - Bitcount(brd.BQueen));

			return eval;
		}

		float Evaluate(const Gigantua::Board& brd)
		{
			float eval = m_nn.Evaluate(brd);
			if (std::abs(eval) > 2400) {
				const float material = EvaluateMat(brd);
				eval += brd.status.WhiteMove() ? material : -material;
			}

			return eval;
		}
	};

}

