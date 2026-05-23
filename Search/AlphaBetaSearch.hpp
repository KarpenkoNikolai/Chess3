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
#include <map>
#include <algorithm>
#include <limits>
#include <cstring>
#include <cmath>

namespace Search {
	namespace AlphaBeta {

		static constexpr uint8_t MaxSearchDepth = 64;

		struct Line {
			uint8_t size = 0;
			std::array<uint16_t, MaxSearchDepth> line;
		};

		class SearchEngine
		{
		private:
			static constexpr int MatVal = 500000;
			static constexpr int Killer1MoveCost = 60;
			static constexpr int Killer2MoveCost = 50;

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

			// Search context per thread
			struct SearchCtx {
				uint8_t ply = 0;
				PvTable pvTable;
				std::array<int, MaxSearchDepth> evalStack = {};
				std::array<uint16_t, MaxSearchDepth> killerMove1 = {};
				std::array<uint16_t, MaxSearchDepth> killerMove2 = {};
				std::array<uint64_t, MaxSearchDepth> repetition = {};
				std::vector<MoveCollector<true>> moveCollectorsWhite = std::vector<MoveCollector<true>>(MaxSearchDepth);
				std::vector<MoveCollector<false>> moveCollectorsBlack = std::vector<MoveCollector<false>>(MaxSearchDepth);

				template<bool white>
				MoveCollector<white>& GetMoveCollector() {
					if constexpr (white) return moveCollectorsWhite[ply];
					else return moveCollectorsBlack[ply];
				}

				void Clear() {
					ply = 0;
					pvTable.Clear();
					evalStack.fill(0);
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
			uint16_t currentBestMove = 0;
			std::function<float(const Gigantua::Board&)> m_costFunc;
			std::vector<SearchThread> searchThreads;
			const GameTree* antTreePtr = nullptr;
			std::array<uint64_t, 16> history;
			std::array<std::array<int, 64>, 64> historyTable; // [from][to]

			void ClearSearch()
			{
				tTable.Clear();
				for (auto& row : historyTable) row.fill(0);
			}

			int Evaluate(const Gigantua::Board& brd) const {
				return m_costFunc(brd);
			}

			bool IsMateScore(int score) const {
				return std::abs(score) > (MatVal - MaxSearchDepth*2);
			}

			int ScoreFromTT(int score, int ply) const {
				if (score >= (MatVal - MaxSearchDepth)) {
					return MatVal - ply;
				}
				if (score <= -(MatVal - MaxSearchDepth)) {
					return -MatVal + ply;
				}
				return score;
			}

			static bool isDraw(const Gigantua::Board& brd) {
				if (brd.WPawn == 0ull && brd.BPawn == 0ull &&
					brd.WRook == 0ull && brd.BRook == 0ull &&
					brd.WQueen == 0ull && brd.BQueen == 0ull) {

					const auto wKn = Bitcount(brd.WKnight);
					const auto bKn = Bitcount(brd.BKnight);
					const auto wBi = Bitcount(brd.WBishop);
					const auto bBi = Bitcount(brd.BBishop);

					if (wKn == 0 && bKn == 0 && wBi == 0 && bBi == 0) return true;
					if (wBi == 0 && bBi == 0 && wKn == 1 && bKn == 0) return true;
					if (wBi == 0 && bBi == 0 && wKn == 0 && bKn == 1) return true;
					if (wBi == 1 && bBi == 0 && wKn == 0 && bKn == 0) return true;
					if (wBi == 0 && bBi == 1 && wKn == 0 && bKn == 0) return true;
					if (wBi == 0 && bBi == 0 && wKn == 1 && bKn == 1) return true;

					if (wBi == 1 && bBi == 1 && wKn == 0 && bKn == 0) {
						const bool wBishopLight = (brd.WBishop & 0xAA55AA55AA55AA55ull) != 0;
						const bool bBishopLight = (brd.BBishop & 0xAA55AA55AA55AA55ull) != 0;
						if (wBishopLight == bBishopLight) return true;
					}

					if (wBi == 1 && bBi == 0 && wKn == 1 && bKn == 0) return false;
					if (wBi == 0 && bBi == 1 && wKn == 0 && bKn == 1) return false;
					if (wBi == 0 && bBi == 0 && wKn == 2 && bKn == 0) return true;
					if (wBi == 0 && bBi == 0 && wKn == 0 && bKn == 2) return true;
				}
				return false;
			}

			static bool isEndgame(const Gigantua::Board& brd) {
				const int totalPieces = Bitcount(brd.WPawn | brd.BPawn | brd.WKnight | brd.BKnight |
					brd.WBishop | brd.BBishop | brd.WRook | brd.BRook | brd.WQueen | brd.BQueen);
				return totalPieces <= 10;
			}

			template<bool white>
			int QuiescenceSearch(SearchCtx& ctx, const Gigantua::Board& pos, int alpha, int beta, int qply) {
				if (ctx.ply >= MaxSearchDepth) return 0;
				if (isDraw(pos)) return 0;

				for (int i = int(ctx.ply) - 2; i >= 0; i -= 2) {
					if (pos.Hash == ctx.repetition[i])
						return 0;
				}

				for (size_t i = 0; i < history.size(); i++)
					if (pos.Hash == history[i]) return 0;

				int stand_pat = 0;
				const bool inCheck = Gigantua::MoveList::InCheck<white>(pos);

				if (!inCheck) {
					stand_pat = Evaluate(pos);
					if (stand_pat >= beta) return beta;

					if (stand_pat + 2500 < alpha) {
						return alpha;
					}
				}

				MoveCollector<white>& collector = ctx.GetMoveCollector<white>();
				collector.Reset();
				Gigantua::MoveList::EnumerateMoves<MoveCollector<white>, white>(collector, pos);

				if (!inCheck) {
					for (uint8_t i = 0; i < collector.size; i++) {
						const Gigantua::Board::Move<white> mv(collector.moves[i]);
						collector.order[i] = SimpleSort(pos, mv, qply >= 2);
						collector.order[i] += historyTable[mv.from()][mv.to()] / 100;
					}
				}
				else {
					for (uint8_t i = 0; i < collector.size; i++) {
						const Gigantua::Board::Move<white> mv(collector.moves[i]);
						collector.order[i] = 10000 + SimpleSort(pos, mv, false);
					}
				}

				if (!inCheck && stand_pat > alpha) alpha = stand_pat;

				for (uint8_t i = 0; i < collector.size; i++) {
					if (!searchStarted) break;

					collector.SortMoves(i);
					const int order = collector.order[collector.index[i]];

					if (order < 10)
						break;

					if (!inCheck && (order > 9000 || order < 100)) {
						qply++;
					}

					const Gigantua::Board::Move<white> move(collector.moves[collector.index[i]]);
					const auto next = move.play(pos);

					ctx.ply++;
					if (ctx.ply < MaxSearchDepth) {
						ctx.repetition[ctx.ply] = next.Hash;
					}

					int score = -QuiescenceSearch<!white>(ctx, next, -beta, -alpha, qply);

					ctx.ply--;

					if (score > alpha) {
						alpha = score;

						if(order < 100) {
							historyTable[move.from()][move.to()] += 2;
						}

						if (score >= beta) {
							return beta;
						}

						if (alpha > MatVal - MaxSearchDepth) {
							return alpha;
						}
					}
				}

				if (inCheck && collector.size == 0) {
					alpha = -MatVal + ctx.ply;
				}

				return alpha;
			}

			template<bool white> int MiniMaxAB(
				SearchCtx& ctx,
				const Gigantua::Board& pos,
				int8_t depth, int alpha, int beta, bool pvNode = false)
			{
				if (ctx.ply >= MaxSearchDepth)
					return 0;

				if (depth < 0) depth = 0;

				const bool rootNode = (ctx.ply == 0);

				if (!rootNode) {
					alpha = std::max(alpha, -MatVal + ctx.ply);
					beta = std::min(beta, MatVal - ctx.ply + 1);
					if(alpha >= beta) {
						return alpha;
					}
				}

				const bool inCheck = Gigantua::MoveList::InCheck<white>(pos);

				if (depth < 1 && !inCheck) {
					return QuiescenceSearch<white>(ctx, pos, alpha, beta, 0);
				}

				if (!rootNode) {
					if (isDraw(pos)) return 0;

					for (size_t i = 0; i < history.size(); i++)
						if (pos.Hash == history[i]) return 0;

					for (int i = int(ctx.ply) - 2; i >= 0; i -= 2) {
						if (pos.Hash == ctx.repetition[i])
							return 0;
					}
				}

				Search::GameTree::ConstNodePtr nodePtr = nullptr;
				if (antTreePtr) {
					nodePtr = antTreePtr->Get(pos);
					nodePtr.Unlock();
				}

				uint16_t bestMove = 0;

				// TT probe with improved cutoff
				int ttCost = TTable::NAN_VAL;
				if (!rootNode) {
					ttCost = tTable.Get(pos, alpha, beta, depth, bestMove);
					if (!pvNode && ttCost != TTable::NAN_VAL) {
						return ScoreFromTT(ttCost, ctx.ply);
					}
				}

				ctx.pvTable.table[ctx.ply].Clear();
				ctx.evalStack[ctx.ply] = -MatVal;

				MoveCollector<white>& collector = ctx.GetMoveCollector<white>();
				collector.Reset();
				Gigantua::MoveList::EnumerateMoves<MoveCollector<white>, white>(collector, pos);

				if (collector.size == 0) {
					if (inCheck) {
						return -MatVal + ctx.ply;
					}
					return 0;
				}

				int staticEval = 0;
				if(!inCheck) {
					staticEval = Evaluate(pos);
					ctx.evalStack[ctx.ply] = staticEval;
				}

				const bool improving = ctx.ply >= 2 && !inCheck
					&& ctx.evalStack[ctx.ply - 2] != -MatVal
					&& staticEval > ctx.evalStack[ctx.ply - 2];

				if (alpha == beta - 1 && !inCheck && !rootNode && depth > 0 && depth < 6) {
					const int margine = improving ? 270 : 220;
					if (beta < 10000 && (staticEval + margine * depth) < beta) {
						const int value = QuiescenceSearch<white>(ctx, pos, alpha, beta, 0);
						if(value < beta) {
							return value;
						}
					}				
				}

				if (beta > -10000 && staticEval >= beta && !pvNode && depth > 4 && !inCheck) {
						const int R = depth > 6 ? 2 : 1;
						const auto nullPos = pos.SkipMove();
						
						ctx.ply++;
						int score = -MiniMaxAB<!white>(ctx, nullPos, depth - 1 - R, -beta, -beta + 1, true);
						ctx.ply--;
						
						if (score >= beta) {
							return beta;
						}
				}

				// Move ordering with improved heuristics
				uint16_t antMove = 0;
				if (!nodePtr.IsNull()) {
					float max_p = 0;
					for (uint8_t j = 0; j < nodePtr->edges.size(); j++) {
						if(nodePtr->edges[j].Entries() == 0) continue;

						const auto p = nodePtr->edges[j].getProbability<white>();
						const auto m = nodePtr->edges[j].Move();
						const auto e = nodePtr->edges[j].Entries();
						if (p > max_p) {
							max_p = p;
							antMove = m;
						}

						if (e > 500) {
							for (uint8_t i = 0; i < collector.size; i++) {
								if (collector.moves[i] == m) {
									collector.entries[i] = e;
									break;
								}
							}
						}
					}
				}

				for (uint8_t i = 0; i < collector.size; i++) {
					const auto mcode = collector.moves[i];
					const Gigantua::Board::Move<white> mv(mcode);
					
					int order = SimpleSort(pos, mv);
					
					if (mcode == bestMove) order += 1000000;
					else if (mcode == antMove) order += 2000000;
					else if (mcode == ctx.killerMove1[ctx.ply]) order += Killer1MoveCost;
					else if (mcode == ctx.killerMove2[ctx.ply]) order += Killer2MoveCost;
					else order += historyTable[mv.from()][mv.to()] / 100;
					
					collector.order[i] = order;
				}

				const uint8_t searchSize = collector.size;
				int oldAlpha = alpha;
				for (uint8_t m = 0; m < searchSize; m++) {
					if (!searchStarted) break;

					collector.SortMoves(m);

					const Gigantua::Board::Move<white> move(collector.moves[collector.index[m]]);
					const auto mcode = collector.moves[collector.index[m]];
					const auto order = collector.order[collector.index[m]];
					const auto entries = collector.entries[collector.index[m]];

					const auto next = move.play(pos);

					ctx.ply++;
					if (ctx.ply < MaxSearchDepth) {
						ctx.repetition[ctx.ply] = next.Hash;
					}

					const bool reduce = m > 0 && entries < 50000 && !inCheck && order < 100;

					int score = std::numeric_limits<int>::max();
					if (reduce) {
						// more conservative LMR formula
						int reduction = pvNode || entries > 10000 ? 0 : int(log2f(depth) * 0.5f + log2f(m) * 0.3f + 0.5f);
						if (reduction && order > 50) reduction--;
						if (reduction && improving) reduction--;
						if (reduction && entries > 500) reduction--;

						// try a null-window search with reduction
						while ((score = -MiniMaxAB<!white>(ctx, next, depth - 1 - reduction, -alpha - 1, -alpha, true)) > alpha
								&& reduction > 0) reduction = 0;
					}

					if (score > alpha)
						score = -MiniMaxAB<!white>(ctx, next, depth - 1, -beta, -alpha);

					ctx.ply--;

					if (score > alpha) {
						bestMove = mcode;
						alpha = score;
						ctx.pvTable.table[ctx.ply].Compose(mcode, ctx.pvTable.table[ctx.ply + 1]);
					
						if (alpha >= beta) {
							// Update killer moves
							if (!inCheck && order < 100) {
								ctx.killerMove2[ctx.ply] = ctx.killerMove1[ctx.ply];
								ctx.killerMove1[ctx.ply] = mcode;
								historyTable[move.from()][move.to()] += depth * depth;
							}
							break;
						}
					}

					if(alpha > MatVal - MaxSearchDepth) {
						break;
					}
				}

				const auto flag = alpha >= beta ? TTable::Flag::Beta : (alpha == oldAlpha ? TTable::Flag::Alpha : TTable::Flag::Value);

				tTable.Put(pos, alpha, bestMove, depth, flag);

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
				ctx.repetition[0] = current.Hash;

				searchStarted = true;
				int score = MiniMaxAB<white>(ctx, current, depth, -1000000, 1000000);
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
						currentBestMove = mv[0].move;
						return false;
					}
				}

				currentBestLine.size = 0;
				searchStarted = true;

				searchThreads.clear();
				for (uint16_t i = 0; i < threadsNum; i++) {
					SearchThread st;
					st.ctx.Clear();
					searchThreads.push_back(std::move(st));
				}

				for (size_t i = 0; i < threadsNum; i++) {
					searchThreads[i].threadPtr.reset(new std::thread([this, current, milliseconds, i, onWin, threadsNum]() {
						uint8_t depth = i * 2;
						int64_t search_time_ms = milliseconds;
						
						while (searchStarted && depth < MaxSearchDepth) {
							depth += i == 0 ? 1 : 2;
							Gigantua::Board pos = current;
							searchThreads[i].ctx.repetition[0] = pos.Hash;
							const auto startTime = std::chrono::high_resolution_clock::now();

							int alpha = -MatVal;
							int beta  = MatVal - 1;

							int score = MiniMaxAB<white>(searchThreads[i].ctx, pos, depth, alpha, beta);

							const auto stopTime = std::chrono::high_resolution_clock::now();
							const auto dur_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime);

							if (searchStarted && i == 0) {
								if (searchThreads[i].ctx.pvTable.GetBest().size > 0) {
									currentBestScore = score;
									currentBestLine = searchThreads[i].ctx.pvTable.GetBest();
									currentBestMove = currentBestLine.line[0];
									std::cout << "d" << int(depth) << "s" << score << "ms" << dur_ms.count() << "(" << Gigantua::Board::moveStr(currentBestMove) << ")" << std::endl;
								}

								if (IsMateScore(score)) {
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

			void Stop() {
				searchStarted = false;
				for (auto& t : searchThreads) {
					t.Wait();
				}
			}

			uint16_t GetBestMoveTT(const Gigantua::Board& brd) const { return tTable.GetBestMove(brd); }
			PvLine GetBestLine() const { return currentBestLine; }
			uint16_t BestMove() const { return currentBestMove; }
			int BestScore() const { return currentBestScore; }
		};

	}//namespace AlphaBeta
}//namespace Search