#include <iostream>
#include <chrono>
#include <cstring>
#include <fstream>
#include <algorithm>


#include "../Search/AlphaBetaSearch.hpp"
#include "../Search/AntSearch.hpp"

#include "../Eval/NeuroNetEval.hpp"
#include "../Eval/EvaluateGpt.hpp"

struct MoveStr
{
	uint8_t from;
	uint8_t to;
	int8_t type;
	MoveStr(const std::string& strMove) {
		Gigantua::Board::moveFromStr(strMove, from, to, type);
	}
};

bool importNet(std::vector<int>& genome, const std::string& fileName) {
	std::fstream file;
	file.open(fileName, std::ios::in);

	if (file.is_open()) {
		std::string line;
		size_t i = 0;
		while (std::getline(file, line)) {
			genome[i++] = std::stoi(line);
		}

		file.close();
		return true;
	}

	return false;
}

bool importNet(std::vector<float>& genome, const std::string& fileName) {
	std::fstream file;
	file.open(fileName, std::ios::in);

	if (file.is_open()) {
		std::string line;
		size_t i = 0;
		while (std::getline(file, line)) {
			genome[i++] = std::stof(line);
		}

		file.close();
		return true;
	}

	return false;
}

bool importNet(std::vector<double>& genome, const std::string& fileName) {
	std::fstream file;
	file.open(fileName, std::ios::in);

	if (file.is_open()) {
		std::string line;
		size_t i = 0;
		while (std::getline(file, line)) {
			genome[i++] = std::stod(line);
		}

		file.close();
		return true;
	}

	return false;
}

std::vector<float> importNet(const std::string& fileName) {
	std::vector<float> genome;
	std::fstream file;
	file.open(fileName, std::ios::in);

	if (file.is_open()) {
		std::string line;
		size_t i = 0;
		while (std::getline(file, line)) {
			genome.push_back(std::stod(line));
		}

		file.close();
	}

	return genome;
}

int main() {
	const std::vector<float> gen = importNet("genome0.txt");
	NN::NeuroNetEval nne;
	nne.SetGenome(gen);

	//Gigantua::Board p("8/8/7K/8/5Q1P/3k4/8/8 w - - 0 0");
	//Gigantua::Board p("8/8/8/6K1/5Q1P/2k5/8/8 w - - 2 2");
	//Gigantua::Board p("8/8/8/6K1/4Q2P/8/1k6/8 w - - 4 3");
	//Gigantua::Board p("8/8/8/8/4QK1P/2k5/8/8 w - - 6 4");
	//Gigantua::Board p("6k1/8/6P1/6K1/7P/8/8/8 w - - 3 67");

	Gigantua::Board p("rnbqr1k1/pp4b1/1n1p2p1/2pN2p1/5P2/1Q6/PP2B1PP/R1B2RK1 w - - 0 16");// d5f6
	//Gigantua::Board p("r5k1/b1p2pp1/p1n1b2p/P3p3/B3Pq2/2r1NN1P/1Q3PP1/R2R3K b - - 1 24"); //c6d4
	//Gigantua::Board p("r2q1rk1/pp3pn1/3pp3/8/3NP1p1/2N5/PPPQ4/2K4R w - - 0 20"); //d2h6

	//Gigantua::Board p("1r1q2k1/5pp1/B1p1b2p/P7/Qb1P4/5N2/5PPP/1R4K1 b - - 3 25"); //b4d6
	//Gigantua::Board p("1R1q2k1/5pp1/B1pbb2p/P7/Q2P4/5N2/5PPP/6K1 b - - 0 26"); //d6b8
	//Gigantua::Board p("1b1q2k1/5pp1/2p1b2p/P7/Q2P4/5N2/5PPP/5BK1 b - - 1 27"); //b8a7
	//Gigantua::Board p("3q2k1/b4pp1/2p1b2p/P3N3/Q2P4/8/5PPP/5BK1 b - - 3 28"); //d8d4
	//Gigantua::Board p("6k1/5pp1/2N1b2p/P7/3b4/8/5PPP/5BK1 b - - 0 30"); //d4c5
	//Gigantua::Board p("6k1/5pp1/P1N1b2p/2b5/8/8/5PPP/5BK1 b - - 0 31"); //e6d7
	//Gigantua::Board p("6k1/3b1pp1/P6p/2b1N3/8/8/5PPP/5BK1 b - - 2 32"); //d7e8
	//Gigantua::Board p("4b1k1/5pp1/P6p/2b1N3/2B5/8/5PPP/6K1 b - - 4 33"); //c5a7 c5d4
	//Gigantua::Board p("4b1k1/b4pp1/P6p/4N3/2B5/8/5PPP/5K2 b - - 6 34"); //g8f8
	//Gigantua::Board p("4bk2/b4pp1/P6p/8/2B3N1/8/5PPP/5K2 b - - 8 35"); //e8d7

	//Gigantua::Board p("3q1rk1/3nb1p1/6p1/3ppp1n/NQ2P3/1N2BP2/PP5P/1K5R w - - 0 23"); //e3c5


	std::cout << nne.Evaluate(p) << std::endl;

	std::unordered_set<Gigantua::Board, Gigantua::BoardHash> history;

	std::atomic<size_t> posNum = 0;

	std::atomic<size_t> kkk = 0;
	
	std::function<float(const Gigantua::Board&)> nsCostFunc = [&kkk, &nne](const Gigantua::Board& pos) {
		kkk++;
		return nne.Evaluate(pos);
		//return GPT::Evaluator::EvaluateMaterial(pos);
	};

	Search::Ant::Engine engine(nsCostFunc, 1000000);
	uint32_t timeMs = 1000 * 1000;

	uint16_t winMove = 0;
	std::function<void(uint16_t)> onDone = [&winMove](uint16_t move) {
		winMove = move;
	};

	engine.Set(p);

#ifndef NDEBUG
	engine.Start(1, 1, timeMs, onDone);
#else
	engine.Start(4, 4, timeMs, onDone);
#endif

	for (int t = 0; t < timeMs/1000; t++) {
		if (winMove) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		//std::cout << t + 1 << std::endl;
		//std::cout << engine.Statistic(6) << std::endl;
	}

	engine.Stop();


	const uint16_t bestMove = winMove ? winMove : engine.BestMove();

	if (bestMove) {
		std::cout << "bestmove " << Gigantua::Board::moveStr(bestMove) << " " << kkk << std::endl;
	}

	return 0;
}