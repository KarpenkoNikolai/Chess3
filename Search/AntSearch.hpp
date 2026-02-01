#pragma once

#include <functional>
#include <numeric>
#include <algorithm>
#include <unordered_set>
#include <thread>
#include <memory>
#include <sstream>
#include <set>
#include <map>
#include <atomic>
#include <random>
#include <chrono>

#include "../Gigantua/MoveList.hpp"
#include "GameTree.hpp"
#include "MoveCollector.hpp"
#include "AlphaBetaSearch.hpp"


namespace Search{
namespace Ant {

	class Engine {
	private:
		struct SearchContext {
		private:
			static constexpr size_t MaxRnd = 200000;
			size_t CurrRnd = 0;
			std::vector<float> RndNums;

			float getRnd() {
				size_t n = CurrRnd++;
				if (n >= MaxRnd) {
					n = 0;
					CurrRnd = 0;
				}

				return RndNums[n];
			}

			MoveCollector<true> collW;
			MoveCollector<false> collB;
		public:
			SearchContext() {
				unsigned seed = unsigned(std::chrono::system_clock::now().time_since_epoch().count());
				std::default_random_engine generator(seed);
				std::uniform_real_distribution distrib;

				RndNums.resize(MaxRnd);
				for (size_t i = 0; i < MaxRnd; i++) RndNums[i] = float(distrib(generator));
			}

			struct Step {
				Gigantua::Board board;
				GameTree::Edge* edgePtr = nullptr;
			};

			template <bool white>
			constexpr MoveCollector<white>& GetMoveCollector() {
				if constexpr (white) return collW;
				else return collB;
			}

			static constexpr uint8_t MaxPath = 64;
			std::array<Step, MaxPath> path;
			std::array<float, 256> probList;

			uint8_t peekRnd(std::array<float, 256>& list, size_t size)
			{
				memset(list.data() + size, 0, 4 * sizeof(float));

				float summ = 0;
				for (uint8_t i = 0; i < size; i += 4) {
					summ += list[i] + list[i + 1] + list[i + 2] + list[i + 3];
				}

				float rnd = getRnd() * summ;

				for (uint8_t i = 0; i < size; i++) {
					rnd -= list[i];
					if (rnd < 0) {
						return i;
					}
				}

				return 0;
			}
		};

		Search::GameTree m_searchTree;

		struct SearchThread
		{
			SearchThread() {}
			~SearchThread() {
				started = false;
				if (thread && thread->joinable()) thread->join();
			}
			std::shared_ptr<std::thread> thread;
			bool started = false;
			SearchContext ctx;
		};

		std::vector<SearchThread> m_threads;

		std::function<float(const Gigantua::Board&)> m_costFunc;

		static constexpr float MatVal = 200000.0f;

		Gigantua::Board m_current;
		AlphaBeta::SearchEngine m_abEngine;
		std::array<uint64_t, 16> history;
		static constexpr size_t AbAnt = 128;
		static constexpr size_t MaxAnt = 128;

		enum class AntStepResult
		{
			Sucess,
			inLoop,
			isMate,
			isPat,
			EndPath,
			Retry
		};

		template <bool MoveWhite>
		inline AntStepResult DoStep(SearchContext& ctx, bool abAnt, bool maxAnt,
			Gigantua::Board& position, uint8_t& ply,
			std::array<uint64_t, SearchContext::MaxPath>& repetition)
		{
			using NodePtr = Search::GameTree::NodePtr;

			NodePtr nodePtr = m_searchTree.Get(position);
			if (nodePtr.IsNull()) {
				MoveCollector<MoveWhite>& coll = ctx.GetMoveCollector<MoveWhite>();
				coll.Reset();
				Gigantua::MoveList::EnumerateMoves<MoveCollector<MoveWhite>, MoveWhite>(coll, position);

				for (uint8_t i = 0; i < coll.size; i++) {
					const Gigantua::Board::Move<MoveWhite> move(coll.moves[i]);
					coll.order[i] = SimpleSort(position, move);
				}

				coll.SortMoves();
				nodePtr = m_searchTree.Put(position, coll.moves.data(), coll.index.data(), coll.size);
			}

			if (nodePtr.IsNull()) {
				return AntStepResult::Retry;
			}

			Search::GameTree::EdgeList& edges = nodePtr->edges;

			if (edges.size() == 0) {
				if (Gigantua::MoveList::InCheck<MoveWhite>(position)) {
					return AntStepResult::isMate;
				}
				else {
					return AntStepResult::isPat;
				}
			}

			uint8_t moveIndex = 0;
			bool isRndAnt = true;

			if (maxAnt) {
				isRndAnt = false;
				float maxProb = 0;
				for (uint8_t k = 0; k < edges.size(); k++) {
					const float prob = edges[k].template getProbability<MoveWhite>();
					if (maxProb < prob) {
						maxProb = prob;
						moveIndex = k;
					}
				}
			}
			else if (abAnt) {
				const auto abline = m_abEngine.GetBestLine();
				if (ply < abline.size) {
					const auto abMove = abline.line[ply];
					for (uint8_t k = 0; k < edges.size(); k++) {
						if (edges[k].Move() == abMove) {
							isRndAnt = false;
							moveIndex = k;
							break;
						}
					}
				}
			}

			if (isRndAnt) {
				for (uint8_t k = 0; k < edges.size(); k++) {
					ctx.probList[k] = edges[k].template getProbability<MoveWhite>();
				}
				moveIndex = ctx.peekRnd(ctx.probList, edges.size());

				if (edges[moveIndex].Entries() == 0) {
					for (uint8_t k = 0; k < edges.size(); k++) {
						if (edges[k].Entries() == 0) {
							moveIndex = k;
							break;
						}
					}
				}
			}

			// record step (board before the move) and edge pointer
			ctx.path[ply].board = position;
			ctx.path[ply].edgePtr = &edges[moveIndex];
			ply++;

			if (nodePtr->board != position) {
				return AntStepResult::Retry;
			}

			const Gigantua::Board::Move<MoveWhite> currMove(edges[moveIndex].Move());
			position = currMove.play(position);
			nodePtr.Unlock();

			// common repetition / loop detection against global history
			for (size_t i = 0; i < history.size(); i++) {
				if (position.Hash == history[i]) {
					return AntStepResult::inLoop;
				}
			}

			repetition[ply] = position.Hash;

			if (ply > 2) {
				for (int i = ply - 2; i > -1; i -= 2) {
					if (position.Hash == repetition[i]) {
						return AntStepResult::inLoop;
					}
				}
			}

			if (edges[moveIndex].Entries() == 0) {
				return AntStepResult::EndPath;
			}

			return AntStepResult::Sucess;
		}

		template <bool white>
		void RunAnt(SearchContext& ctx, bool abAnt, bool maxAnt) {
			uint8_t ply = 0;
			const float currCost = float(m_abEngine.BestScore());
			float lastCost = currCost;
			Gigantua::Board position = m_current;
			AntStepResult stepResult = AntStepResult::EndPath;
			std::array<uint64_t, SearchContext::MaxPath> repetition = { 0 };
			repetition[ply] = position.Hash;
			while (ply < SearchContext::MaxPath - 2) {
				// first move: this is "my" move when RunAnt<white> and DoStep<white, true>
				{
					stepResult = DoStep<white>(ctx, abAnt, maxAnt, position, ply, repetition);
					if (stepResult != AntStepResult::Sucess) break;
				}

				// second move: opponent move
				{
					stepResult = DoStep<!white>(ctx, abAnt, maxAnt, position, ply, repetition);
					if (stepResult != AntStepResult::Sucess) break;
				}
			}// while path

			if (stepResult == AntStepResult::Retry) {
				return;
			}

			if(stepResult == AntStepResult::isMate) {
				if(white == position.status.WhiteMove())
					lastCost = std::max(10000.0f, -MatVal + 10000 * ply);
				else
					lastCost = std::max(10000.0f, MatVal - 10000 * ply);
			}
			else if (stepResult == AntStepResult::EndPath) {
				if (white == position.status.WhiteMove())
					lastCost = m_costFunc(position);
				else
					lastCost = -m_costFunc(position);
			}
			
			float cost = lastCost - currCost;

			if (stepResult == AntStepResult::isPat || stepResult == AntStepResult::inLoop) {
				if(currCost > 100) cost = -500.0f;
				else cost = -500;
			}

			if (cost > std::numeric_limits<float>::epsilon()) {
				for (int8_t i = 0; i < ply; i++) {
					const auto& p = ctx.path[i];
					if (p.edgePtr != nullptr) {
						p.edgePtr->AddSugar<white>(cost);
					}
				}
			}
			else if (cost < -std::numeric_limits<float>::epsilon()) {
				for (int8_t i = 0; i < ply; i++) {
					const auto& p = ctx.path[i];
					if (p.edgePtr != nullptr) {
						p.edgePtr->AddSugar<!white>(-cost);
					}
				}
			}

			for (int8_t i = 0; i < ply; i++) {
				const auto& p = ctx.path[i];
				if (p.edgePtr != nullptr) {
					p.edgePtr->AddEntries(cost);
				}
			}
		}

	public:
		Engine(std::function<float(const Gigantua::Board&)> costFunc, size_t treeSize = 1000000, size_t ab_tt_size = 4000000)
			: m_costFunc(costFunc), m_searchTree(treeSize), m_abEngine(costFunc, ab_tt_size)
		{
			Set(Gigantua::Board::StartPosition());
		}

		~Engine() {
			Stop();
		}

		AlphaBeta::SearchEngine& AbEngine() { return m_abEngine; }

		void Set(const Gigantua::Board& brd) {
			Stop();
			m_current = brd;
		}

		void SetHistory(const std::array<uint64_t, 16>& h) {
			m_abEngine.SetHistory(h);
			history = h;
		}

		void Start(uint8_t threadNumber, uint8_t abThreadNumber, uint32_t search_ms, std::function<void(uint16_t)> onWin) {
			Stop();

			if (onWin) {
				m_abEngine.SetAntTree(&m_searchTree);
				bool abStarted = false;
				if (m_current.status.WhiteMove()) {
					abStarted = m_abEngine.StartSearch<true>(m_current, search_ms, abThreadNumber, onWin);
				}
				else {
					abStarted = m_abEngine.StartSearch<false>(m_current, search_ms, abThreadNumber, onWin);
				}

				if (!abStarted) {
					onWin(m_abEngine.BestMove());
					return;
				}
			}

			m_threads.resize(threadNumber);

			for (uint8_t i = 0; i < threadNumber; i++) {
				m_threads[i].started = true;
				m_threads[i].thread.reset(new std::thread([this, i]() {
					size_t abAnt = 0;
					size_t maxAnt = 0;
					while (m_threads[i].started) {
						abAnt++;
						maxAnt++;

						if (abAnt > AbAnt) {
							if (m_current.status.WhiteMove())
								RunAnt<true>(m_threads[i].ctx, true, false);
							else {
								RunAnt<false>(m_threads[i].ctx, true, false);
							}
							abAnt = 0;
						}
						else if (maxAnt > MaxAnt) {
							if (m_current.status.WhiteMove())
								RunAnt<true>(m_threads[i].ctx, false, true);
							else {
								RunAnt<false>(m_threads[i].ctx, false, true);
							}
							maxAnt = 0;
						}
						else {
							if (m_current.status.WhiteMove())
								RunAnt<true>(m_threads[i].ctx, false, false);
							else {
								RunAnt<false>(m_threads[i].ctx, false, false);
							}
						}
					}
				}));
			}
		}

		void Stop() {
			m_abEngine.Stop();
			for (uint8_t i = 0; i < m_threads.size(); i++) {
				m_threads[i].started = false;
			}

			for (uint8_t i = 0; i < m_threads.size(); i++) {
				if (m_threads[i].thread && m_threads[i].thread->joinable())
					m_threads[i].thread->join();
			}
		}

		uint16_t BestMove() const {
			uint16_t result = m_abEngine.BestMove();
			const int score = m_abEngine.BestScore();
			if (result != 0) return result;

			Search::GameTree::ConstNodePtr currentNodePtr = m_searchTree.Get(m_current);
			if (currentNodePtr.IsNull()) {
				return result;
			}

			const Search::GameTree::EdgeList& currentNodeEdges = currentNodePtr->edges;

			if (currentNodeEdges.size() == 0) {
				return result;
			}

			uint32_t maxEntries = 0;
			for (uint8_t i = 0; i < currentNodeEdges.size(); i++) {
				if (currentNodeEdges[i].Entries() > maxEntries) {
					result = currentNodeEdges[i].Move();
					maxEntries = currentNodeEdges[i].Entries();
				}
			}

			return result;
		}

		std::vector<Gigantua::Board> GetBestPath(size_t minEntries) const
		{
			std::vector<Gigantua::Board> result;
			result.push_back(m_current);
			Gigantua::Board position = m_current;

			while (result.size() < SearchContext::MaxPath) {
				Search::GameTree::ConstNodePtr nodePtr = m_searchTree.Get(position);
				if (nodePtr.IsNull()) {
					break;
				}

				const Search::GameTree::EdgeList& edges = nodePtr->edges;

				if (edges.size() == 0) {
					break;
				}

				uint8_t moveIndex = 0;
				uint32_t maxEntries = 0;
				for (uint8_t k = 0; k < edges.size(); k++) {
					if (maxEntries < edges[k].Entries()) {
						maxEntries = edges[k].Entries();
						moveIndex = k;
					}
				}

				if (maxEntries < minEntries) {
					break;
				}

				if (nodePtr->board != position) {
					break;
				}
				
				if (position.status.WhiteMove()) {
					const Gigantua::Board::Move<true> currMove(edges[moveIndex].Move());
					position = currMove.play(position);
				}
				else {
					const Gigantua::Board::Move<false> currMove(edges[moveIndex].Move());
					position = currMove.play(position);
				}

				result.push_back(position);
			}// while path

			return result;
		}

		
		std::map<uint16_t, uint32_t> GetEntries(const Gigantua::Board& brd) const {
			std::map<uint16_t, uint32_t> result;
			Search::GameTree::ConstNodePtr currentNodePtr = m_searchTree.Get(brd);
			if (currentNodePtr.IsNull()) {
				return result;
			}

			const Search::GameTree::EdgeList& currentNodeEdges = currentNodePtr->edges;

			for (uint8_t i = 0; i < currentNodeEdges.size(); i++) {
				result[currentNodeEdges[i].Move()] = currentNodeEdges[i].Entries();
			}

			return result;
		}

		std::string Statistic(uint8_t maxMoves, uint32_t minEntries = 100) const {
			Search::GameTree::ConstNodePtr currentNodePtr = m_searchTree.Get(m_current);
			if (currentNodePtr.IsNull()) {
				return "";
			}

			const Search::GameTree::EdgeList& currentNodeEdges = currentNodePtr->edges;

			if (currentNodeEdges.size() == 0) {
				return "";
			}

			struct Info
			{
				float prob;
				uint32_t entries;
				uint16_t move;
			};

			std::vector<Info> info;
			float summ = 0;

			for (uint8_t i = 0; i < currentNodeEdges.size(); i++) {
				Info inf;
				inf.move = currentNodeEdges[i].Move();
				inf.prob = m_current.status.WhiteMove() ? currentNodeEdges[i].getProbability<true>() : currentNodeEdges[i].getProbability<false>();
				summ += inf.prob;
				inf.entries = currentNodeEdges[i].Entries();
				info.push_back(std::move(inf));
			}

			std::sort(info.begin(), info.end(), [](const Info& a, const Info& b) { return a.entries > b.entries; });

			std::stringstream result;

			const auto ab_line = m_abEngine.GetBestLine();

			for (uint8_t i = 0; i < std::min(uint8_t(info.size()), maxMoves); i++) {
				result << info[i].entries << ";" << int(info[i].prob * 1000.0f / summ) * 0.001f << ";";

				auto position = m_current;
				uint16_t m = info[i].move;
				bool w = m_current.status.WhiteMove();
				int d = 0;
				while (d < std::max(SearchContext::MaxPath, uint8_t(16)))
				{
					if (d < 16) {
						result << " " << Gigantua::Board::moveStr(m);

						if (d < ab_line.size && m == ab_line.line[d]) {
							result << "*";
						}
					}

					d++;

					if (w) {
						const Gigantua::Board::Move<true> currMove(m);
						position = currMove.play(position);
					}
					else {
						const Gigantua::Board::Move<false> currMove(m);
						position = currMove.play(position);
					}

					{
						Search::GameTree::ConstNodePtr currentNodePtr = m_searchTree.Get(position);
						if (currentNodePtr.IsNull()) {
							break;
						}

						const Search::GameTree::EdgeList& currentNodeEdges = currentNodePtr->edges;

						if (currentNodeEdges.size() == 0) {
							break;
						}

						if (currentNodePtr->board != position) break;

						uint32_t maxEntries = 0;
						for (uint8_t i = 0; i < currentNodeEdges.size(); i++) {
							if (maxEntries < currentNodeEdges[i].Entries()) {
								maxEntries = currentNodeEdges[i].Entries();
								m = currentNodeEdges[i].Move();
							}
						}

						if(maxEntries < minEntries) {
							break;
						}

						w = !w;
					}
				}

				result << " D" << d;
				result << std::endl;
			}

			return result.str();
		}

	};

}//namespace Ant
}//namespace Serch


