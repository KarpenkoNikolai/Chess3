#pragma once

#include "../Gigantua/ChessBase.hpp"
#include "../Gigantua/MoveList.hpp"

#include "TTable.hpp"
#include "MoveCollector.hpp"
#include "GameTree.hpp"

#include <thread>
#include <functional>
#include <unordered_set>
#include <chrono>
#include <mutex>
#include <map>
#include <algorithm>
#include <limits>
#include <cstring>
#include <cmath>

namespace Search {
	namespace AlphaBeta {

		static constexpr uint8_t MaxSearchDepth = 32;

		struct Line {
			uint8_t size = 0;
			std::array<uint16_t, MaxSearchDepth> line;
		};

		class SearchEngine
		{
		private:
			static constexpr int MatVal = 500000;
			static constexpr int Killer1MoveCost = 5000;
			static constexpr int Killer2MoveCost = 200;

			Search::TTable tTable;

			struct PvLine {
				uint8_t size = 0;
				std::array<uint16_t, MaxSearchDepth> line;
				void Clear() {
					line.fill(0);
					size = 0;
				}

				void Compose(uint16_t mv, const PvLine& tail)
				{
					line[0] = mv;
					std::memcpy(line.data() + 1, tail.line.data(), tail.size * sizeof(uint16_t));
					size = 1 + tail.size;
				}

				std::string Print() const {
					std::string result;
					for (uint8_t i = 0; i < size; i++) {
						result += Gigantua::Board::moveStr(line[i]) + " ";
					}
					return result;
				}
			};

			struct PvTable {
				std::array<PvLine, MaxSearchDepth> table;
				void Clear() {
					for (uint8_t i = 0; i < MaxSearchDepth; i++) table[i].Clear();
				}
				PvLine GetBest() const { return table[0]; }
			};

			// Search context per thread: enthält jetzt die History-Tabelle (mutexfrei, pro-thread)
			struct SearchCtx {
				uint8_t ply = 0;
				PvTable pvTable;
				std::array<uint16_t, MaxSearchDepth> killerMove1 = { 0 };
				std::array<uint16_t, MaxSearchDepth> killerMove2 = { 0 };
				std::array<uint64_t, MaxSearchDepth> repetition = { 0 };

				void Clear() {
					ply = 0;
					pvTable.Clear();
					killerMove1.fill(0);
					killerMove2.fill(0);
					repetition.fill(0);
				}
			};

			struct SearchThread {
				~SearchThread() {
					Wait();
				}

				void Wait() {
					if (threadPtr && threadPtr->joinable())
						threadPtr->join();
				}
				std::shared_ptr<std::thread> threadPtr;
				SearchCtx ctx;
			};

			std::atomic<bool> searchStarted = false;
			PvLine currentBestLine;
			int currentBestScore = 0;
			std::function<float(const Gigantua::Board&)> m_costFunc;
			std::vector<SearchThread> searchThreads;
			const GameTree* antTreePtr = nullptr;
			std::array<uint64_t, 16> history;

			void ClearSearch()
			{
				tTable.Clear();
				// per-thread histories are cleared when threads are (re)created
			}

			int Evaluate(const Gigantua::Board& brd) const {
				return m_costFunc(brd);
			}

			static bool isDraw(const Gigantua::Board& brd) {
				if (brd.WPawn == 0ull && brd.BPawn == 0ull &&
					brd.WRook == 0ull && brd.BRook == 0ull &&
					brd.WQueen == 0ull && brd.BQueen == 0ull) {

					if (brd.WBishop == 0ull && brd.BBishop == 0ull && Bitcount(brd.WKnight) == 1ull && brd.BKnight == 0ull)
						return true;

					if (brd.WBishop == 0ull && brd.BBishop == 0ull && Bitcount(brd.WKnight) == 0ull && brd.BKnight == 1ull)
						return true;

					if (brd.WBishop == 0ull && brd.BBishop == 0ull && Bitcount(brd.WKnight) == 1ull && brd.BKnight == 1ull)
						return true;

					if (brd.WBishop == 1ull && brd.BBishop == 0ull && Bitcount(brd.WKnight) == 0ull && brd.BKnight == 0ull)
						return true;

					if (brd.WBishop == 0ull && brd.BBishop == 1ull && Bitcount(brd.WKnight) == 0ull && brd.BKnight == 0ull)
						return true;

					if (brd.WBishop == 1ull && brd.BBishop == 1ull && Bitcount(brd.WKnight) == 0ull && brd.BKnight == 0ull)
						return true;

					if (brd.WBishop == 1ull && brd.BBishop == 1ull && Bitcount(brd.WKnight) == 1ull && brd.BKnight == 0ull)
						return true;

					if (brd.WBishop == 1ull && brd.BBishop == 1ull && Bitcount(brd.WKnight) == 0ull && brd.BKnight == 1ull)
						return true;

				}

				return false;
			}

			template<bool white>
			int QuiescenceSearch(const Gigantua::Board& pos, int alpha, int beta, int ply, int qply) {

				if (ply >= MaxSearchDepth) return 0;
				if (isDraw(pos)) return 0;

				// global history repetition
				for (size_t i = 0; i < history.size(); i++)
					if (pos.Hash == history[i]) return 0;

				int stand_pat = 0;
				const bool inCheck = Gigantua::MoveList::InCheck<white>(pos);

				if (!inCheck) {
					stand_pat = Evaluate(pos);
					if (stand_pat >= beta) return beta;
				}

				MoveCollector<white> collector;
				Gigantua::MoveList::EnumerateMoves<MoveCollector<white>, white>(collector, pos);

				if (!inCheck) {
					for (uint8_t i = 0; i < collector.size; i++) {
						const Gigantua::Board::Move<white> mv(collector.moves[i]);
						collector.order[i] = SimpleSort(pos, mv, qply >= 2);

						if (collector.order[i] > 9000 || collector.order[i] < 100) {
							qply++;
						}
					}
				}
				else {
					for (uint8_t i = 0; i < collector.size; i++) {
						const Gigantua::Board::Move<white> mv(collector.moves[i]);
						collector.order[i] = 10000 + SimpleSort(pos, mv, true);
					}
				}

				if (!inCheck && stand_pat > alpha) alpha = stand_pat;

				for (uint8_t i = 0; i < collector.size; i++) {
					if (!searchStarted) break;

					collector.SortMoves(i);
					const int order = collector.order[collector.index[i]];

					if (order < 60)
						break;

					if (!inCheck && order < 9000) {
						const int staticGain = order;
						if ((stand_pat + staticGain + 750) <= alpha) {
							continue;
						}
					}

					const Gigantua::Board::Move<white> move(collector.moves[collector.index[i]]);

					const auto next = move.play(pos);

					int score = -QuiescenceSearch<!white>(next, -beta, -alpha, ply + 1, qply);
					if (score > alpha) {
						alpha = score;
						if (alpha >= beta)
							break;
					}
				}

				if (inCheck && collector.size == 0)
					alpha = ply - MatVal;

				return alpha;
			}

			template<bool white> int MiniMaxAB(
				SearchCtx& ctx,
				const Gigantua::Board& pos,
				int8_t depth, int alpha, int beta, int moveOrder = 0)
			{
				if (ctx.ply >= MaxSearchDepth) return 0;
				if (isDraw(pos)) return 0;

				if (ctx.ply) {
					for (size_t i = 0; i < history.size(); i++)
						if (pos.Hash == history[i]) return 0;

					// per-thread repetition check (same side positions only)
					for (int i = int(ctx.ply) - 2; i >= 0; i--) {
						if (pos.Hash == ctx.repetition[i])
							return 0;
					}
				}

				Search::GameTree::ConstNodePtr nodePtr = nullptr;
				if (antTreePtr) {
					nodePtr = antTreePtr->Get(pos);
					nodePtr.Unlock();
				}

				const bool pvNode = (beta - alpha) > 1;
				uint16_t bestMove = 0;

				if (!pvNode && ctx.ply) {
					int cost = tTable.Get(pos, alpha, beta, depth, ctx.ply, bestMove);
					if (cost != TTable::NAN_VAL) {
						return cost;
					}
				}

				const bool inCheck = Gigantua::MoveList::InCheck<white>(pos);

				ctx.pvTable.table[ctx.ply].Clear();

				if (depth < 1 && !inCheck) {
					return QuiescenceSearch<white>(pos, alpha, beta, ctx.ply, 0);
				}

				bool futilityPruning = false;

				if (moveOrder < 100 && !pvNode && nodePtr.IsNull() && std::abs(alpha) < (MatVal - 100)) {
					const int staticEval = Evaluate(pos);

					if (moveOrder < 70) {
						int margin = 120 * depth;
						if ((staticEval - margin) >= beta) {
							return (staticEval + beta) / 2;
						}
					}

					{
						int margin = 220 * depth;
						if ((staticEval + margin) <= alpha)
							futilityPruning = true;
					}
				}

				MoveCollector<white> collector;
				Gigantua::MoveList::EnumerateMoves<MoveCollector<white>, white>(collector, pos);

				if (collector.size == 0) {
					if (inCheck) {
						return ctx.ply - MatVal;
					}
					return 0;
				}

				if (!nodePtr.IsNull()) {
					float max_e = 0;
					uint16_t max_m1 = 0;
					for (uint8_t j = 0; j < nodePtr->edges.size(); j++) {
						const auto e = nodePtr->edges[j].getProbability<white>();
						const auto m = nodePtr->edges[j].Move();
						if (e > max_e) {
							max_e = e;
							max_m1 = m;
						}
					}

					for (uint8_t i = 0; i < collector.size; i++) {
						const auto mcode = collector.moves[i];
						const Gigantua::Board::Move<white> mv(mcode);
						collector.order[i] = bestMove == mcode ? 1000000 : SimpleSort(pos, mv);

						if (mcode == max_m1) collector.order[i] += 2000000;
						if (mcode == ctx.killerMove1[ctx.ply]) collector.order[i] += Killer1MoveCost;
						if (mcode == ctx.killerMove2[ctx.ply]) collector.order[i] += Killer2MoveCost;
					}
				}
				else {
					for (uint8_t i = 0; i < collector.size; i++) {
						const auto mcode = collector.moves[i];
						const Gigantua::Board::Move<white> mv(mcode);
						collector.order[i] = bestMove == mcode ? 1000000 : SimpleSort(pos, mv);
						if (mcode == ctx.killerMove1[ctx.ply]) collector.order[i] += Killer1MoveCost;
						if (mcode == ctx.killerMove2[ctx.ply]) collector.order[i] += Killer2MoveCost;
					}
				}

				TTable::Flag flag = TTable::Flag::Alpha;
				uint8_t searchSize = collector.size;

				const int movePruneThreshold = 0;
				const int depthPruneThreshold = 0;

				for (uint8_t m = 0; m < searchSize; m++) {
					if (!searchStarted) break;

					collector.SortMoves(m);

					const Gigantua::Board::Move<white> move(collector.moves[collector.index[m]]);
					const auto order = collector.order[collector.index[m]];
					const auto next = move.play(pos);

					if (futilityPruning && m > 5)
						break;

					ctx.ply++;

					// record repetition for next ply
					if (ctx.ply < MaxSearchDepth) {
						ctx.repetition[ctx.ply] = next.Hash;
					}

					const bool reduce = m > movePruneThreshold && depth > depthPruneThreshold && !inCheck;

					int score = std::numeric_limits<int>::max();
					if (reduce) {
						// more conservative LMR formula
						int reduction = int(std::log2(depth) * 0.5f + std::log2(m) * 0.5f + 0.7f);
						if (reduction && pvNode) reduction--;
						if (reduction && order > 100) reduction--;
						if (reduction && order > 2000) reduction--;
						if (reduction && order > 3000) reduction--;

						// try a null-window search with reduction
						while ((score = -MiniMaxAB<!white>(ctx, next, depth - 1 - reduction, -alpha - 1, -alpha, order)) > alpha
							&& reduction > 0
							) reduction = 0;
					}

					if (score > alpha)
						score = -MiniMaxAB<!white>(ctx, next, depth - 1, -beta, -alpha, order);

					ctx.ply--;

					if (score > alpha) {
						flag = TTable::Flag::Value;
						bestMove = move.move;
						alpha = score;
						ctx.pvTable.table[ctx.ply].Compose(move.move, ctx.pvTable.table[ctx.ply + 1]);

						if (alpha >= beta) {
							ctx.killerMove2[ctx.ply] = ctx.killerMove1[ctx.ply];
							ctx.killerMove1[ctx.ply] = move.move;
							flag = TTable::Flag::Beta;
							break;
						}
					}
				}

				tTable.Put(pos, alpha, bestMove, depth, ctx.ply, flag);
				return alpha;
			}

		public:

			SearchEngine(std::function<float(const Gigantua::Board&)> costFunc, size_t ttSize = 2000000)
				: tTable(ttSize), m_costFunc(costFunc)
			{
			}

			~SearchEngine()
			{
				Stop();
			}

			constexpr int MatValue() const { return MatVal; }

			void SetHistory(const std::array<uint64_t, 16>& h) { history = h; }

			void SetAntTree(const GameTree* treePtr)
			{
				antTreePtr = treePtr;
			}

			template<bool white> int Search(const Gigantua::Board& current, uint8_t depth, uint16_t& bestMove)
			{
				{
					const auto mv = Gigantua::MoveList::MoveList<white>(current);
					if (mv.size() == 0) {
						currentBestLine.size = 0;

						return Evaluate(current);
					}

					if (mv.size() == 1) {
						currentBestLine.size = 1;
						bestMove = mv[0].move;
						currentBestLine.line[0] = mv[0].move;

						return Evaluate(current);
					}
				}

				SearchCtx ctx;
				ctx.Clear();
				// record root hash for repetition checks
				ctx.repetition[0] = current.Hash;

				searchStarted = true;
				int score = MiniMaxAB<white>(ctx, current, depth, -100000, 100000);
				searchStarted = false;
				bestMove = ctx.pvTable.GetBest().line[0];
				return score;
			}

			template<bool white> bool StartSearch(
				const Gigantua::Board& current,
				uint32_t milliseconds = 0,
				uint16_t threadsNum = 1,
				std::function<void(uint16_t)> onWin = nullptr)
			{
				Stop();
				ClearSearch();

				{
					const auto mv = Gigantua::MoveList::MoveList<white>(current);
					if (mv.size() == 0) {
						currentBestLine.size = 0;

						return false;
					}

					if (mv.size() == 1) {
						currentBestLine.size = 1;
						currentBestLine.line[0] = mv[0].move;

						return false;
					}
				}

				currentBestLine.size = 0;
				searchStarted = true;

				searchThreads.clear();
				//threadsNum = 1;
				for (uint16_t i = 0; i < threadsNum; i++) {
					SearchThread st;
					st.ctx.Clear();
					searchThreads.push_back(std::move(st));
				}

				for (size_t i = 0; i < threadsNum; i++) {
					searchThreads[i].threadPtr.reset(new std::thread([this, current, milliseconds, i, onWin, threadsNum]() {
						uint8_t depth = 1;
						int64_t search_time_ms = milliseconds;
						// aspiration window variables will be set per-depth
						while (searchStarted && depth < MaxSearchDepth) {
							depth++;
							Gigantua::Board pos = current;
							// ensure per-thread ctx has root repetition set
							searchThreads[i].ctx.repetition[0] = pos.Hash;
							const auto startTime = std::chrono::high_resolution_clock::now();

							// Aspiration window logic:
							int window = 100; // initial aspiration window in centipawns
							int score = 0;
							int attempts = 0;
							int prevScore = currentBestScore; // shared best score from previous iteration (may be 0)
							bool useAspiration = (prevScore != 0);

							// Try aspiration and widen if necessary (limited retries)
							do {
								int alpha = useAspiration && attempts == 0 ? prevScore - window : -1000000;
								int beta  = useAspiration && attempts == 0 ? prevScore + window : 1000000;

								score = MiniMaxAB<white>(searchThreads[i].ctx, pos, depth, alpha, beta);

								// if fail-low or fail-high, widen and retry
								if (score <= alpha) {
									window = std::max(window * 3, 200);
									attempts++;
									useAspiration = true;
								}
								else if (score >= beta) {
									window = std::max(window * 3, 200);
									attempts++;
									useAspiration = true;
								}
								else {
									break;
								}
							} while (attempts < 4 && searchStarted);

							const auto stopTime = std::chrono::high_resolution_clock::now();
							const auto dur_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime);

							if (searchStarted && i == 0) {
								{
									if (searchThreads[i].ctx.pvTable.GetBest().size > 0) {
										currentBestScore = score;
										currentBestLine = searchThreads[i].ctx.pvTable.GetBest();
										std::cout << "d" << int(depth) << "s" << score << "ms" << dur_ms.count() << "(" << Gigantua::Board::moveStr(currentBestLine.line[0]) << ")" << std::endl;
									}
								}

								if (abs(score) > MatVal - 100) {
									if (onWin) {
										onWin(BestMove());
									}

									searchStarted = false;
									break;
								}

								if (search_time_ms > 0) {
									const auto minimax_ms = dur_ms.count();
									if (minimax_ms < search_time_ms) {
										search_time_ms -= minimax_ms;
									}

									if (search_time_ms < minimax_ms) {
										if (onWin) {
											onWin(0);
										}

										searchStarted = false;
										break;
									}
								}
							}
						}
					}));
				}

				return true;
			}

			template<bool white> int qSearch(const Gigantua::Board& brd) {
				return QuiescenceSearch<white>(brd, -1000000, 1000000, 0, 0);
			}

			void Stop() {
				searchStarted = false;
				for (auto& t : searchThreads) {
					t.Wait();
				}
			}

			uint16_t GetBestMoveTT(const Gigantua::Board& brd) const { return tTable.GetBestMove(brd); }
			PvLine GetBestLine() const { return currentBestLine; }
			uint16_t BestMove() const { return currentBestLine.size > 0 ? currentBestLine.line[0] : 0; }
			int BestScore() const { return currentBestScore; }
		};

	}//namespace AlphaBeta
}//namespace Search