#include <iostream>
#include <chrono>
#include <cstring>
#include <fstream>
#include <algorithm>


#include "../Search/AlphaBetaSearch.hpp"
#include "../Search/AntSearch.hpp"

#include "../Eval/NeuroNetEval.hpp"

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

	std::cout << nne.Evaluate(p) << std::endl;

	std::unordered_set<Gigantua::Board, Gigantua::BoardHash> history;

	std::atomic<size_t> posNum = 0;

	std::atomic<size_t> kkk = 0;
	
	std::function<float(const Gigantua::Board&)> nsCostFunc = [&kkk, &nne](const Gigantua::Board& pos) {
		kkk++;
		return nne.Evaluate(pos);
	};

	Search::Ant::Engine engine(nsCostFunc, 2000000);
	uint32_t timeMs = 600 * 1000;

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