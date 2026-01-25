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
			static constexpr int Killer1MoveCost = 3000;

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
					memcpy(line.data() + 1, tail.line.data(), tail.size * sizeof(uint16_t));
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
				std::array<uint16_t, MaxSearchDepth> killerMove;
				// history[from][to] - history heuristic, per-thread to avoid mutexes
				std::array<std::array<int32_t, 64>, 64> history;

				void Clear() {
					ply = 0;
					pvTable.Clear();
					killerMove.fill(0);
					for (auto &row : history) row.fill(0);
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
			std::array<uint64_t, 8> history;

			void ClearSearch()
			{
				tTable.Clear();
				// per-thread histories are cleared when threads are (re)created
			}

			int Evaluate(const Gigantua::Board& brd) const {
				return m_costFunc(brd);
			}

			int SimpleEvaluate(const Gigantua::Board& brd) const {
				int result = 0;
				result += (Bitcount(brd.WPawn) - Bitcount(brd.BPawn)) * 136;
				result += (Bitcount(brd.WKnight) - Bitcount(brd.BKnight)) * 782;
				result += (Bitcount(brd.WBishop) - Bitcount(brd.BBishop)) * 830;
				result += (Bitcount(brd.WRook) - Bitcount(brd.BRook)) * 1289;
				result += (Bitcount(brd.WQueen) - Bitcount(brd.BQueen)) * 2529;

				return (brd.status.WhiteMove() ? result : -result);
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

				for (size_t i = 0; i < history.size(); i++)
					if (pos.Hash == history[i]) return 0;

				const int stand_pat = Evaluate(pos);

				if (stand_pat >= beta) return beta;

				MoveCollector<white> collector;
				Gigantua::MoveList::EnumerateMoves<MoveCollector<white>, white>(collector, pos);

				const bool inCheck = Gigantua::MoveList::InCheck<white>(pos);
				if (!inCheck) {
					for (uint8_t i = 0; i < collector.size; i++) {
						const Gigantua::Board::Move<white> mv(collector.moves[i]);
						collector.order[i] = SimpleSort(pos, mv, qply >= 4);

						if (collector.order[i] > 3000 || collector.order[i] < 100) {
							qply++;
						}
					}
				}
				else {
					for (uint8_t i = 0; i < collector.size; i++) {
						const Gigantua::Board::Move<white> move(collector.moves[i]);
						collector.order[i] = 5000;
					}
				}

				if (!inCheck && stand_pat > alpha) alpha = stand_pat;

				for (uint8_t i = 0; i < collector.size; i++) {
					if (!searchStarted) break;

					collector.SortMoves(i);
					const int order = collector.order[collector.index[i]];

					if (order < 60)
						break;

					const bool isCapture = order < 3000 && order > 100;
					// Delta-pruning: skip captures that cannot raise alpha sufficiently
					if (isCapture && !inCheck) {
						const int staticGain = order;
						if ((stand_pat + staticGain + 550) <= alpha) {
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
				int8_t depth, int alpha, int beta)
			{
				if (ctx.ply >= MaxSearchDepth) return 0;
				if (isDraw(pos)) return 0;

				if (ctx.ply) {
					for (size_t i = 0; i < history.size(); i++)
						if (pos.Hash == history[i]) return 0;
				}

				Search::GameTree::ConstNodePtr nodePtr = nullptr;
				if (antTreePtr) {
					nodePtr = antTreePtr->Get(pos);
					nodePtr.Unlock();
				}

				const bool pvNode = (beta - alpha) > 1;
				uint16_t bestMove = 0;

				int cost = tTable.Get(pos, alpha, beta, depth, ctx.ply, bestMove);
				if (!pvNode && ctx.ply && cost != TTable::NAN_VAL) {
					return cost;
				}

				const bool inCheck = Gigantua::MoveList::InCheck<white>(pos);

				ctx.pvTable.table[ctx.ply].Clear();

				if (depth < 1 && !inCheck) {
					return QuiescenceSearch<white>(pos, alpha, beta, ctx.ply, 0);
				}

				bool futilityPruning = false;

				if (!inCheck && !pvNode && std::abs(alpha) < (MatVal - 100)) {
					// static evaluation for pruning purposes
					const int staticEval = Evaluate(pos);

					const int margin = 250 * depth;
					if ((staticEval - margin) >= beta) {
						return (staticEval + beta) / 2;
					}
					// futility pruning condition
					if ((staticEval + 220 * depth) <= alpha)
						futilityPruning = true;
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
					uint16_t max_m = 0;
					for (uint8_t j = 0; j < nodePtr->edges.size(); j++) {
						const auto e = nodePtr->edges[j].getProbability<white>();
						const auto m = nodePtr->edges[j].Move();
						if (e > max_e) {
							max_e = e;
							max_m = m;
						}
					}

					for (uint8_t i = 0; i < collector.size; i++) {
						const auto mcode = collector.moves[i];
						const Gigantua::Board::Move<white> mv(mcode);
						collector.order[i] = bestMove == mcode ? 1000000 : SimpleSort(pos, mv);

						if (mcode == max_m) collector.order[i] += 2000000;
						if (mcode == ctx.killerMove[ctx.ply]) collector.order[i] += Killer1MoveCost;

						// History heuristic influence (per-thread table, mutex-free)
						collector.order[i] += ctx.history[ mv.from() ][ mv.to() ];
					}
				}
				else {
					for (uint8_t i = 0; i < collector.size; i++) {
						const auto mcode = collector.moves[i];
						const Gigantua::Board::Move<white> mv(mcode);
						collector.order[i] = bestMove == mcode ? 1000000 : SimpleSort(pos, mv);
						if (mcode == ctx.killerMove[ctx.ply]) collector.order[i] += Killer1MoveCost;

						// History heuristic influence (per-thread table, mutex-free)
						collector.order[i] += ctx.history[ mv.from() ][ mv.to() ];
					}
				}

				TTable::Flag flag = TTable::Flag::Alpha;
				uint8_t searchSize = collector.size;

				for (uint8_t m = 0; m < searchSize; m++) {
					if (!searchStarted) break;

					collector.SortMoves(m);

					const Gigantua::Board::Move<white> move(collector.moves[collector.index[m]]);
					const auto next = move.play(pos);

					if (futilityPruning && m > 4)
						continue;

					ctx.ply++;

					const bool reduce = m > 0 && !inCheck;

					int score = std::numeric_limits<int>::max();
					if (reduce) {
						// more conservative LMR formula
						int reduction = int(log2f(depth) / 2.0f + log2f(m) / 2.0f + 0.7f);
						if (reduction && pvNode) reduction--;
						if (reduction && collector.order[collector.index[m]] > 100) reduction--;

						// try a null-window search with reduction
						while ((score = -MiniMaxAB<!white>(ctx, next, depth - 1 - reduction, -alpha - 1, -alpha)) > alpha
							&& reduction > 0
							) reduction = 0;
					}

					if (score > alpha)
						score = -MiniMaxAB<!white>(ctx, next, depth - 1, -beta, -alpha);

					ctx.ply--;

					if (score > alpha) {
						flag = TTable::Flag::Value;
						bestMove = move.move;
						alpha = score;
						ctx.pvTable.table[ctx.ply].Compose(move.move, ctx.pvTable.table[ctx.ply + 1]);

						if (alpha >= beta) {
							// update per-thread history heuristic on beta cutoff (no mutex needed)
							{
								const int from = move.from();
								const int to = move.to();
								// add depth^2 to emphasize deeper cutoffs, cap to avoid overflow
								const int add = int(depth) * int(depth);
								const int32_t cap = std::numeric_limits<int32_t>::max() / 4;
								int64_t val = int64_t(ctx.history[from][to]) + add;
								if (val > cap) val = cap;
								ctx.history[from][to] = int32_t(val);
							}

							ctx.killerMove[ctx.ply] = move.move;
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

			void SetHistory(const std::array<uint64_t, 8>& h) { history = h; }

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
					// initialize per-thread context (including history) to zero
					st.ctx.Clear();
					searchThreads.push_back(std::move(st));
				}

				for (size_t i = 0; i < threadsNum; i++) {
					searchThreads[i].threadPtr.reset(new std::thread([this, current, milliseconds, i, onWin, threadsNum]() {
						uint8_t depth = 1;
						int64_t search_time_ms = milliseconds;
						int alpha = -1000000;
						int beta = 1000000;

						while (searchStarted && depth < MaxSearchDepth) {
							depth++;
							Gigantua::Board pos = current;
							const auto startTime = std::chrono::high_resolution_clock::now();

							int score = 0;

							// Aspiration windows for main thread (i == 0) to speed up successful searches
							if (i == 0 && currentBestLine.size > 0) {
								// dynamic window based on depth and previous score
								int window = std::max(50, 10 + int(depth) * 10);
								int aspAlpha = currentBestScore - window;
								int aspBeta = currentBestScore + window;

								score = MiniMaxAB<white>(searchThreads[i].ctx, pos, depth, aspAlpha, aspBeta);

								// if fails low/high, retry with full window
								if (score <= aspAlpha || score >= aspBeta) {
									score = MiniMaxAB<white>(searchThreads[i].ctx, pos, depth, -1000000, 1000000);
								}
							}
							else {
								score = MiniMaxAB<white>(searchThreads[i].ctx, pos, depth, alpha, beta);
							}

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
											onWin(0);//BestMove());
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
				return QuiescenceSearch<white>(brd, -1000000, 1000000);
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