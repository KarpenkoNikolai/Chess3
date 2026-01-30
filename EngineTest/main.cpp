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
	NN::NeuroNetEval::SetGenome(gen);

	//Gigantua::Board p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	
	//Gigantua::Board p("rnbqr1k1/pp4b1/1n1p2p1/2pN2p1/5P2/1Q6/PP2B1PP/R1B2RK1 w - - 0 16");// d5f6
	Gigantua::Board p("r7/p3p1k1/1p1p1bBp/8/5P1P/1Rn4K/P1P3P1/4R3 w - - 4 29"); //g6d3
	//Gigantua::Board p("1r5k/5p2/3Q1n1b/3Pp2n/2Pq4/5PB1/1r1N2RP/3RKB2 b - - 3 28");// h6d2

	std::cout << NN::NeuroNetEval::Evaluate(p) << std::endl;

	std::unordered_set<Gigantua::Board, Gigantua::BoardHash> history;

	std::atomic<size_t> posNum = 0;

	std::atomic<size_t> kkk = 0;
	
	std::function<float(const Gigantua::Board&)> nsCostFunc = [&kkk](const Gigantua::Board& pos) {
		kkk++;
		return NN::NeuroNetEval::Evaluate(pos);
	};

	Search::Ant::Engine engine(nsCostFunc, 2000000);
	uint32_t timeMs = 30 * 1000;

	uint16_t winMove = 0;
	std::function<void(uint16_t)> onDone = [&winMove](uint16_t move) {
		winMove = move;
	};

	engine.Set(p);
	engine.Start(4, 4, timeMs, onDone);

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