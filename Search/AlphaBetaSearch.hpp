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
			static constexpr int Killer2MoveCost = 3000;

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
				std::array<uint16_t, MaxSearchDepth> killerMove1 = {};
				std::array<uint16_t, MaxSearchDepth> killerMove2 = {};
				std::array<uint64_t, MaxSearchDepth> repetition = {};

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
			}

			int Evaluate(const Gigantua::Board& brd) const {
				return m_costFunc(brd);
			}

			bool IsMateScore(int score) const {
				return std::abs(score) > (MatVal - MaxSearchDepth);
			}

			int ScoreFromTT(int score, int ply) const {
				if (score >= (MatVal - MaxSearchDepth)) {
					return score - ply;
				}
				if (score <= -(MatVal - MaxSearchDepth)) {
					return score + ply;
				}
				return score;
			}

			int ScoreToTT(int score, int ply) const {
				if (score >= (MatVal - MaxSearchDepth)) {
					return score + ply;
				}
				if (score <= -(MatVal - MaxSearchDepth)) {
					return score - ply;
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

			static int getEndgamePhase(const Gigantua::Board& brd) {
				const int queens = Bitcount(brd.WQueen | brd.BQueen);
				const int rooks = Bitcount(brd.WRook | brd.BRook);
				const int minors = Bitcount(brd.WKnight | brd.BKnight | brd.WBishop | brd.BBishop);

				int phase = 256 - queens * 40 - rooks * 20 - minors * 10;
				return std::clamp(phase, 0, 256);
			}

			template<bool white>
			static bool hasNonPawnMaterial(const Gigantua::Board& brd) {
				if constexpr (white) {
					return (brd.WKnight | brd.WBishop | brd.WRook | brd.WQueen) != 0ull;
				}
				else {
					return (brd.BKnight | brd.BBishop | brd.BRook | brd.BQueen) != 0ull;
				}
			}

			template<bool white>
			int QuiescenceSearch(SearchCtx& ctx, const Gigantua::Board& pos, int alpha, int beta, int qply) {

				if (ctx.ply >= MaxSearchDepth) return 0;
				if (isDraw(pos)) return 0;

				for (int i = int(ctx.ply) - 2; i >= 0; i-=2) {
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
					
					if (stand_pat + 2900 < alpha) {
						return alpha;
					}
				}

				MoveCollector<white> collector;
				Gigantua::MoveList::EnumerateMoves<MoveCollector<white>, white>(collector, pos);

				if (!inCheck) {
					for (uint8_t i = 0; i < collector.size; i++) {
						const Gigantua::Board::Move<white> mv(collector.moves[i]);
						collector.order[i] = SimpleSort(pos, mv, qply >= 4);
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

					if (order < 50)
						break;

					if (!inCheck && order < 9000) {
						const int staticGain = order;
						if ((stand_pat + staticGain + 600) <= alpha) {
							continue;
						}
					}

					if (order > 9000 || order < 100) {
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
						if (alpha >= beta)
							break;
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
				int8_t depth, int alpha, int beta, int myOrder = 0)
			{
				if (ctx.ply >= MaxSearchDepth) return 0;
				if (depth < 0) depth = 0;

				const bool inCheck = Gigantua::MoveList::InCheck<white>(pos);

				if (depth < 1 && !inCheck) {
					return QuiescenceSearch<white>(ctx, pos, alpha, beta, 0);
				}

				if (alpha < -MatVal) alpha = -MatVal;
				if (beta > MatVal - 1) beta = MatVal - 1;
				if (alpha >= beta) return alpha;

				const bool rootNode = (ctx.ply == 0);

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

				const bool pvNode = (beta - alpha) > 1;
				uint16_t bestMove = 0;

				// TT probe with improved cutoff
				if (!pvNode && ctx.ply) {
					int cost = tTable.Get(pos, alpha, beta, depth, bestMove);
					if (cost != TTable::NAN_VAL) {
						return ScoreFromTT(cost, ctx.ply);
					}
				}

				ctx.pvTable.table[ctx.ply].Clear();

				MoveCollector<white> collector;
				Gigantua::MoveList::EnumerateMoves<MoveCollector<white>, white>(collector, pos);

				if (collector.size == 0) {
					if (inCheck) {
						return -MatVal + ctx.ply;
					}
					return 0;
				}

				bool futilityPruning = false;

				if (myOrder < 100 && !pvNode && !inCheck && depth < 8 && alpha > -20000 && !rootNode) {
					int staticEval = Evaluate(pos);

					// Reverse Futility Pruning
					int rfpMargin = 320 * depth;
					if ((staticEval - rfpMargin) >= beta) {
						return (staticEval + beta) / 2;
					}

					// Extended Futility Pruning
					int futilityMargin = 220 * depth;
					if ((staticEval + futilityMargin) < alpha) {
						futilityPruning = true;
					}
				}

				// Move ordering with improved heuristics
				uint16_t antMove = 0;
				if (!nodePtr.IsNull()) {
					float max_e = 0;
					for (uint8_t j = 0; j < nodePtr->edges.size(); j++) {
						if(nodePtr->edges[j].Entries() == 0) continue;

						const auto e = nodePtr->edges[j].getProbability<white>();
						const auto m = nodePtr->edges[j].Move();
						if (e > max_e) {
							max_e = e;
							antMove = m;
						}
					}
				}

				for (uint8_t i = 0; i < collector.size; i++) {
					const auto mcode = collector.moves[i];
					const Gigantua::Board::Move<white> mv(mcode);
					
					int order = SimpleSort(pos, mv);
					
					if (mcode == bestMove) order = 1000000;
					else if (mcode == antMove) order += 2000000;
					else if (mcode == ctx.killerMove1[ctx.ply]) order += Killer1MoveCost;
					else if (mcode == ctx.killerMove2[ctx.ply]) order += Killer2MoveCost;
					
					collector.order[i] = order;
				}

				TTable::Flag flag = TTable::Flag::Alpha;
				uint8_t searchSize = collector.size;
				int bestScore = -1000000;

				for (uint8_t m = 0; m < searchSize; m++) {
					if (!searchStarted) break;

					collector.SortMoves(m);

					const Gigantua::Board::Move<white> move(collector.moves[collector.index[m]]);
					const auto mcode = collector.moves[collector.index[m]];
					const auto order = collector.order[collector.index[m]];

					if (futilityPruning && m > 4) {
						continue;
					}

					const auto next = move.play(pos);

					ctx.ply++;
					if (ctx.ply < MaxSearchDepth) {
						ctx.repetition[ctx.ply] = next.Hash;
					}

					int score = std::numeric_limits<int>::max();

					// Late Move Reduction (LMR)
					if (m > 0 && depth > 1 && !inCheck && alpha > -20000) {
						int reduction = int(0.8f + log2(m)*0.3f + log2(depth)*0.5f);
						if (reduction && pvNode) reduction--;
						if (reduction && order > 100) reduction--;

						score = -MiniMaxAB<!white>(ctx, next, depth - 1 - reduction, -alpha - 1, -alpha, order);

						if (score > alpha) {
							if (reduction > 0) {
								score = -MiniMaxAB<!white>(ctx, next, depth - 1, -alpha - 1, -alpha, order);
							}

							if (score > alpha) {
								score = std::numeric_limits<int>::max();
							}
						}
					}

					if (score > alpha) {//full window search
						score = -MiniMaxAB<!white>(ctx, next, depth - 1, -beta, -alpha, order);
					}

					ctx.ply--;

					if (score > bestScore) {
						bestScore = score;
					}

					if (score > alpha) {
						flag = TTable::Flag::Value;
						bestMove = mcode;
						alpha = score;
						ctx.pvTable.table[ctx.ply].Compose(mcode, ctx.pvTable.table[ctx.ply + 1]);

						if (alpha >= beta) {
							if (order < 100) {
								// Update killer moves
								ctx.killerMove2[ctx.ply] = ctx.killerMove1[ctx.ply];
								ctx.killerMove1[ctx.ply] = mcode;
							}
							flag = TTable::Flag::Beta;
							break;
						}
					}
				}

				tTable.Put(pos, ScoreToTT(alpha, ctx.ply), bestMove, depth, flag);
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
						uint8_t depth = 1;
						int64_t search_time_ms = milliseconds;
						
						while (searchStarted && depth < MaxSearchDepth) {
							depth++;
							Gigantua::Board pos = current;
							searchThreads[i].ctx.repetition[0] = pos.Hash;
							const auto startTime = std::chrono::high_resolution_clock::now();

							int window = isEndgame(pos) ? 50 : 80;
							int score = 0;
							int attempts = 0;
							int prevScore = currentBestScore;
							bool useAspiration = false && (depth > 4 && prevScore != 0 && !IsMateScore(prevScore));

							do {
								int alpha = useAspiration && attempts == 0 ? prevScore - window : -1000000;
								int beta  = useAspiration && attempts == 0 ? prevScore + window : 1000000;

								score = MiniMaxAB<white>(searchThreads[i].ctx, pos, depth, alpha, beta);

								// if fail-low or fail-high, widen and retry
								if (score <= alpha) {
									window = std::min(window * 2, 1000);
									attempts++;
									useAspiration = true;
								}
								else if (score >= beta) {
									window = std::min(window * 2, 1000);
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
								if (searchThreads[i].ctx.pvTable.GetBest().size > 0) {
									currentBestScore = score;
									currentBestLine = searchThreads[i].ctx.pvTable.GetBest();
									std::cout << "d" << int(depth) << "s" << score << "ms" << dur_ms.count() << "(" << Gigantua::Board::moveStr(currentBestLine.line[0]) << ")" << std::endl;
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
			uint16_t BestMove() const { return currentBestLine.size > 0 ? currentBestLine.line[0] : 0; }
			int BestScore() const { return currentBestScore; }
		};

	}//namespace AlphaBeta
}//namespace Search