#include <iostream>
#include <chrono>
#include <random>
#include <cstring>

#include <../Gigantua/MoveList.hpp>
#include <../Gigantua/ChessTest.hpp>

static inline uint64_t nodes;

template<bool white>
static void PerfT(const Gigantua::Board& brd, int depth);

template<bool white>
class PerfTCollector : public Gigantua::MoveList::MoveCollectorBase<PerfTCollector<white>, white>
{
private:
	const Gigantua::Board& m_brd;
	const int m_depth;
public:
	PerfTCollector(const Gigantua::Board& brd, int depth) : m_brd(brd), m_depth(depth) {}

	void CollectImpl(const Gigantua::Board::Move<white>& move) const
	{
		PerfT<!white>(move.play(m_brd), m_depth - 1);
	}
};

template<bool white>
static void PerfT(const Gigantua::Board& brd, int depth)
{
	if(depth == 1){
		nodes += Gigantua::MoveList::MovesCount<white>(brd);
		return;
	}

	PerfTCollector<white> collector(brd, depth);
	Gigantua::MoveList::EnumerateMoves<PerfTCollector<white>, white>(collector, brd);
}


static inline void _PerfT(std::string_view fen, int depth) {
	Gigantua::Board brd(fen);
	nodes = 0;
	
	const std::string f = brd.Fen();
	for (size_t i = 0; i < f.size(); i++)
		if(f[i] != fen[i]){
			std::cout << "fen error" << std::endl;
			break;
		}

	if (brd.status.WhiteMove()) {
		PerfT<true>(brd, depth);
	}
	else {
		PerfT<false>(brd, depth);
	}
}

void Chess_Test() {
	for (auto pos : Test::Positions)
	{
		auto v = Test::GetElements(pos, ';');
		std::string fen = v[0];

		std::cout << fen << "\n";
		size_t to = v.size();
		for (int i = 1; i < to; i++) {
			auto perftvals = Test::GetElements(v[i], ' ');
			uint64_t expected = static_cast<uint64_t>(std::strtol(perftvals[1].c_str(), NULL, 10));
			_PerfT(fen, i);
			uint64_t result = nodes;
			std::string status = expected == result ? "OK" : "ERROR";
			if (expected == result)  std::cout << "   " << i << ": " << result << " " << status << "\n";
			else  std::cout << "xxx -> " << i << ": " << result <<" vs " << expected << " " << status << "\n";
			
		}
	}
}

int main(int argc, char** argv)
{
	Gigantua::Board noCheckBrd1("rn1qkbnr/p2b1ppp/1p1p4/1Bp1p3/P2P2P1/2N1P3/1PP2P1P/R1BQK1NR b KQkq a3 0 6");
	Gigantua::Board noCheckBrd2("rn1qk1nr/p2b1ppp/1p6/bB1pp1N1/P2p2P1/4P3/1PPB1P1P/R2QK1NR w KQkq - 4 10");
	Gigantua::Board noCheckBrd3("rn4nr/p2b1p2/1p3kpp/bB4N1/P2p2P1/3K4/1PPB3P/R5NR w - - 0 18");
	Gigantua::Board noCheckBrd4("rn4nr/p2b1p2/Bpk3p1/b5p1/P1P1K1P1/1P6/3B3P/R5NR b - - 0 22");
	Gigantua::Board checkBrd1("rnbqkbnr/p4ppp/1p1p4/1Bp1p3/3P2P1/2N1P3/PPP2P1P/R1BQK1NR b KQkq - 1 5");
	Gigantua::Board checkBrd2("rn1qk1nr/p2b1ppp/1p6/1B1pp1N1/Pb1p2P1/4P3/1PP2P1P/R1BQK1NR w KQkq - 2 9");
	Gigantua::Board checkBrd3("rn4nr/p2bkp2/1p3Ppp/bB1p2N1/P2p2P1/8/1PPB3P/R3K1NR b KQ - 0 15");
	Gigantua::Board checkBrd4("rn4nr/p2b1p2/1p3kpp/bB1p2N1/P5P1/3p4/1PPBK2P/R5NR w - - 0 17");
	Gigantua::Board dummyMatBrd("rnbqkbnr/ppppp2p/5p2/6pQ/3PP3/8/PPP2PPP/RNB1KBNR b KQkq - 1 3");

	bool checkTest = 
		(Gigantua::MoveList::InCheck<false>(noCheckBrd1) == false) &&
		(Gigantua::MoveList::InCheck<true>(noCheckBrd2) == false) &&
		(Gigantua::MoveList::InCheck<true>(noCheckBrd3) == false) &&
		(Gigantua::MoveList::InCheck<false>(noCheckBrd4) == false) &&
		Gigantua::MoveList::InCheck<false>(checkBrd1) &&
		Gigantua::MoveList::InCheck<true>(checkBrd2) &&
		Gigantua::MoveList::InCheck<false>(checkBrd3) &&
		Gigantua::MoveList::InCheck<true>(checkBrd4) &&
		Gigantua::MoveList::InCheck<false>(dummyMatBrd);

	if (!checkTest) {
		std::cout << "check test ERROR!" << std::endl;
		return 0;
	}

	std::cout << "check test OK" << std::endl;

	{
		Gigantua::Board ttt("rnbqkbnr/ppppp1pp/5p2/7Q/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 1 2");

		const auto moves = Gigantua::MoveList::MoveList<false>(ttt);

		uint8_t from;
		uint8_t to;
		int8_t type;
		Gigantua::Board::moveFromStr("g7g6", from, to, type);

		for (const auto move : moves) {
			std::cout << Gigantua::Board::moveStr(move.move) << " " << (from == move.from() && to == move.to()) << std::endl;
		}
	}

	Chess_Test();

	std::string_view def = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
	std::string_view kiwi = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
	std::string_view midgame = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";
	std::string_view endgame = "5nk1/pp3pp1/2p4p/q7/2PPB2P/P5P1/1P5K/3Q4 w - - 1 28";

	auto ts = std::chrono::steady_clock::now();
	for (int i = 1; i <= 7; i++)
	{
		auto start = std::chrono::steady_clock::now();
		_PerfT(def, i);
		auto end = std::chrono::steady_clock::now();
		long long delta = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
		std::cout << "Perft Start " <<i<< ": "<< nodes << " " << delta / 1000 <<"ms " << nodes * 1.0 / delta << " MNodes/s\n";
	}
	if (nodes == 3195901860ull) std::cout << "OK\n\n";
	else std::cout << "ERROR!\n\n";

	for (int i = 1; i <= 6; i++)
	{
		auto start = std::chrono::steady_clock::now();
		_PerfT(kiwi, i);
		auto end = std::chrono::steady_clock::now();
		long long delta = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
		std::cout << "Perft Kiwi " << i << ": " << nodes << " " << delta / 1000 << "ms " << nodes * 1.0 / delta << " MNodes/s\n";
	}
	if (nodes == 8031647685ull) std::cout << "OK\n\n";
	else std::cout << "ERROR!\n\n";

	for (int i = 1; i <= 6; i++)
	{
		auto start = std::chrono::steady_clock::now();
		_PerfT(midgame, i);
		auto end = std::chrono::steady_clock::now();
		long long delta = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
		std::cout << "Perft Midgame " << i << ": " << nodes << " " << delta / 1000 << "ms " << nodes * 1.0 / delta << " MNodes/s\n";
	}
	if (nodes == 6923051137ull) std::cout << "OK\n\n";
	else std::cout << "ERROR!\n\n";

	for (int i = 1; i <= 6; i++)
	{
		auto start = std::chrono::steady_clock::now();
		_PerfT(endgame, i);
		auto end = std::chrono::steady_clock::now();
		long long delta = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
		std::cout << "Perft Endgame " << i << ": " << nodes << " " << delta / 1000 << "ms " << nodes * 1.0 / delta << " MNodes/s\n";
	}
	if (nodes == 849167880ull) std::cout << "OK\n\n";
	else std::cout << "ERROR!\n\n";

	//Total nodes
	nodes = (3195901860ull + 8031647685ull + 6923051137ull + 849167880ull);
	auto te = std::chrono::steady_clock::now();
	long long total = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count();
	std::cout << "Perft aggregate: " << nodes
		<< " " << total / 1000 << "ms " << nodes * 1.0 / total << " MNodes/s\n";

}
