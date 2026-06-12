#pragma once

#include <../Gigantua/ChessBase.hpp>

#include "NeuroNetOpt.hpp"
#include "EvaluateGpt.hpp"

namespace NN
{
	class NeuroNetEval
	{
	public:
		NeuroNetOpt m_nn;

		void SetGenome(const std::vector<float>& genome) {
			m_nn.SetGenome(genome);
		}

		float Evaluate(const Gigantua::Board& brd)
		{
			float nnEval = m_nn.Evaluate(brd);
			if (abs(nnEval) > 2500) {
				nnEval += GPT::Evaluator::Evaluate(brd) + GPT::Evaluator::EvaluateMaterial(brd);
			}
			
			return nnEval;
		}
	};

}

