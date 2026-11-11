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

		// LMR Lookup Table: Pre-computed Late Move Reduction values
		// Helper function to initialize LMR table at compile time
		static consteval std::array<std::array<int, 64>, 64> GenerateLMRTable() {
			std::array<std::array<int, 64>, 64> table = {};
			
			for (int d = 0; d < 64; d++) {
				for (int m = 0; m < 64; m++) {
					// Using logarithm approximation for compile-time evaluation
					int logDepth = 0, logMove = 0;
					int d_tmp = d, m_tmp = m;
						
					// Bit-length based logarithm approximation
					while (d_tmp > 1) { logDepth++; d_tmp >>= 1; }
					while (m_tmp > 1) { logMove++; m_tmp >>= 1; }
						
					table[d][m] = (logMove * logDepth * 3 + 4) / 10;
				}
			}
			return table;
		}

		// LMRTable[depth][moveNumber] = reduction in half-plies
		static constexpr std::array<std::array<int, 64>, 64> LMRTable = GenerateLMRTable();

		struct Line {
			uint8_t size = 0;
			std::array<uint16_t, MaxSearchDepth> line;
		};

		class SearchEngine
		{
		private:
			static constexpr int MatVal = 500000;
			static constexpr int InMateVal = MatVal - MaxSearchDepth;
			static constexpr int KillerMoveCost = 150;
			static constexpr int AspirationWindow = 200;

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
				int nmpPly = 0;
				PvTable pvTable;
				std::array<std::array<uint16_t, MaxSearchDepth>, 4> killerMove = {};
				std::array<uint64_t, MaxSearchDepth> repetition = {};
				std::array<MoveCollector<true>, MaxSearchDepth> moveCollectorsWhite = {};
				std::array<MoveCollector<false>, MaxSearchDepth> moveCollectorsBlack = {};

				template<bool white>
				MoveCollector<white>& GetMoveCollector() {
					if constexpr (white) return moveCollectorsWhite[ply];
					else return moveCollectorsBlack[ply];
				}

				void Clear() {
					ply = 0;
					nmpPly = 0;
					pvTable.Clear();
					for (uint8_t i = 0; i < killerMove.size(); i++) {
						killerMove[i].fill(0);
					}
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

			void ClearSearch()
			{
				tTable.Clear();
			}

			int Evaluate(const Gigantua::Board& brd) const {
				return m_costFunc(brd);
			}

			template<bool white>
			void OrderMoves(SearchCtx& ctx, const Gigantua::Board& pos,
				MoveCollector<white>& collector, const GameTree::ConstNodePtr& nodePtr,
				uint16_t ttMove
			) const {
				uint16_t antMove = 0;
				if (!nodePtr.IsNull()) {
					float max_p = 0;
					for (uint8_t j = 0; j < nodePtr->edges.size(); j++) {
						if (nodePtr->edges[j].Entries() == 0) continue;

						const auto p = nodePtr->edges[j].getProbability<white>();
						const auto m = nodePtr->edges[j].Move();
						const auto e = nodePtr->edges[j].Entries();
						if (p > max_p) {
							max_p = p;
							antMove = m;
						}

						for (uint8_t i = 0; i < collector.size; i++) {
							if (collector.moves[i] == m) {
								collector.entries[i] = e;
								break;
							}
						}
					}
				}

				for (uint8_t i = 0; i < collector.size; i++) {
					const auto mcode = collector.moves[i];
					const Gigantua::Board::Move<white> mv(mcode);
					int order = SimpleSort(pos, mv);

					if (mcode == ttMove) order += 1000000;
					if (mcode == antMove) order += 1111111;

					for (size_t k = 0; k < ctx.killerMove.size(); k++) {
						if (mcode == ctx.killerMove[k][ctx.ply]) order += KillerMoveCost - k;
					}

					collector.order[i] = order;
				}
			}

			int ScoreFromTT(int score, int ply) const {
				if (score >= InMateVal) {
					return score - ply;
				}
				if (score <= -InMateVal) {
					return score + ply;
				}
				return score;
			}

			int ScoreToTT(int score, int ply) const {
				if (score >= InMateVal) {
					return score + ply;
				}
				if (score <= -InMateVal) {
					return score - ply;
				}
				return score;
			}

			static float EvaluateMaterial(const Gigantua::Board& brd)
			{
				float eval = 0;

				eval += (136) * (Bitcount(brd.WPawn) - Bitcount(brd.BPawn));
				eval += (782) * (Bitcount(brd.WKnight) - Bitcount(brd.BKnight));
				eval += (830) * (Bitcount(brd.WBishop) - Bitcount(brd.BBishop));
				eval += (1289) * (Bitcount(brd.WRook) - Bitcount(brd.BRook));
				eval += (2529) * (Bitcount(brd.WQueen) - Bitcount(brd.BQueen));

				return brd.status.WhiteMove() ? eval : -eval;
			}


			bool isDraw(const Gigantua::Board& brd, const SearchCtx& ctx) {
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


				for (size_t i = 0; i < history.size(); i++)
					if (brd.Hash == history[i]) return true;

				for (int i = int(ctx.ply) - 2; i >= 0; i -= 2) {
					if (brd.Hash == ctx.repetition[i])
						return true;
				}

				return false;
			}

			template<bool white>
			int QuiescenceSearch(SearchCtx& ctx, const Gigantua::Board& pos, int alpha, int beta)
			{	
				if (ctx.ply >= MaxSearchDepth) return 0;
				if (isDraw(pos, ctx)) return 0;

				const int originalAlpha = alpha;

				int stand_pat = 0;
				const bool inCheck = Gigantua::MoveList::InCheck<white>(pos);

				if (!inCheck) {
					stand_pat = Evaluate(pos);
					if (stand_pat >= beta) {
						return beta;
					}
					if (stand_pat > alpha) alpha = stand_pat;
				}

				MoveCollector<white>& collector = ctx.GetMoveCollector<white>();
				collector.Reset();
				Gigantua::MoveList::EnumerateMoves<MoveCollector<white>, white>(collector, pos);

				// Move ordering in Quiescence
				if (!inCheck) {
					for (uint8_t i = 0; i < collector.size; i++) {
						const Gigantua::Board::Move<white> mv(collector.moves[i]);
						collector.order[i] = SimpleSort(pos, mv, true);
					}
				}
				else {
					for (uint8_t i = 0; i < collector.size; i++) {
						const Gigantua::Board::Move<white> mv(collector.moves[i]);
						collector.order[i] = 10000 + SimpleSort(pos, mv);
					}
				}

				uint16_t bestMove = 0;
				for (uint8_t i = 0; i < collector.size; i++) {

					collector.SortMoves(i);
					const int order = collector.order[collector.index[i]];

					if (order == 0)
						break;

					const Gigantua::Board::Move<white> move(collector.moves[collector.index[i]]);
					const auto next = move.play(pos);

					ctx.ply++;
					if (ctx.ply < MaxSearchDepth) {
						ctx.repetition[ctx.ply] = next.Hash;
					}
					int score = -QuiescenceSearch<!white>(ctx, next, -beta, -alpha);
					ctx.ply--;

					if (score > alpha) {
						alpha = score;
						bestMove = move.move;

						if (alpha >= beta) {
							break;
						}
					}

					if (!searchStarted) break;
				}

				if (collector.size == 0) {
					return inCheck ? -MatVal + ctx.ply : 0;
				}

				if (!searchStarted) return 0;

				return alpha;
			}

			template<bool white> int MiniMaxAB(
				SearchCtx& ctx,
				const Gigantua::Board& pos,
				int depth, int alpha, int beta)
			{
				if (ctx.ply >= MaxSearchDepth)
					return 0;

				if (depth < 0) depth = 0;

				const bool inCheck = Gigantua::MoveList::InCheck<white>(pos);

				if (depth < 1 && !inCheck) {
					return QuiescenceSearch<white>(ctx, pos, alpha, beta);
				}

				const bool rootNode = (ctx.ply == 0);

				// Mate distance pruning
				if (!rootNode) {
					alpha = std::max(alpha, -MatVal + ctx.ply);
					beta = std::min(beta, MatVal - ctx.ply - 1);
					if (alpha >= beta) {
						return alpha;
					}
				}

				// Draw detection
				if (!rootNode) {
					if (isDraw(pos, ctx)) return 0;
				}

				Search::GameTree::ConstNodePtr nodePtr = nullptr;
				if (antTreePtr) {
					nodePtr = antTreePtr->Get(pos);
					nodePtr.Unlock();
				}

				uint16_t bestMove = 0;
				uint16_t ttMove = 0;
				int originalAlpha = alpha;

				// Transposition Table probe
				int ttCost = TTable::NAN_VAL;
				uint8_t ttDepth = 0;
				TTable::Flag ttFlag = TTable::Flag::Alpha;
				ttCost = tTable.Get(pos, ttMove, ttDepth, ttFlag);
				if (ttCost != TTable::NAN_VAL) {
					const int ttScore = ScoreFromTT(ttCost, ctx.ply);
					if (ttDepth >= depth && !inCheck) {
						switch (ttFlag) {
						case TTable::Flag::Alpha:
							if (ttScore <= alpha) return ttScore;
							break;
						case TTable::Flag::Beta:
							if (ttScore >= beta) return ttScore;
							break;
						case TTable::Flag::Value:
							return ttScore;
						}
					}
				}

				ctx.pvTable.table[ctx.ply].Clear();

				MoveCollector<white>& collector = ctx.GetMoveCollector<white>();
				collector.Reset();
				Gigantua::MoveList::EnumerateMoves<MoveCollector<white>, white>(collector, pos);

				if (collector.size == 0) {
					return inCheck ? -MatVal + ctx.ply : 0;
				}

				bool futilityPruning = false;
				if (!inCheck) {
					const int staticEval = Evaluate(pos);
				
					if (depth < 7 && staticEval + 200 * depth < alpha && alpha < InMateVal) {
						futilityPruning = true;
					}

					const uint64_t nonPawnMaterial = white
						? (pos.WKnight | pos.WBishop | pos.WRook | pos.WQueen)
						: (pos.BKnight | pos.BBishop | pos.BRook | pos.BQueen);

					if (depth >= 3 && ctx.nmpPly == 0 && nonPawnMaterial != 0
						&& staticEval >= beta + 300 && beta > -2000) {
						const int reduction = 2 + depth / 4;
						const auto nullPos = pos.SkipMove();

						ctx.ply++;
						ctx.nmpPly++;
						const int score = -MiniMaxAB<!white>(ctx, nullPos, depth - reduction, -beta, -beta + 1);
						ctx.nmpPly--;
						ctx.ply--;

						if (!searchStarted) return 0;
						if (score >= beta && abs(score) < InMateVal) return beta;
					}
				}

				if (!inCheck && ttMove == 0 && depth > 5) {
					depth--;
				}

				// Move ordering
				OrderMoves<white>(ctx, pos, collector, nodePtr, ttMove);

				const uint8_t searchSize = collector.size;

				int bestScore = -std::numeric_limits<int>::max();
				int quietMoveCount = 0;
				uint16_t bestMoveFound = 0;
		
				for (uint8_t m = 0; m < searchSize; m++) {					
					collector.SortMoves(m);
					const auto order = collector.order[collector.index[m]];
					if (order == 0) collector.SortMovesEntries(m);

					const Gigantua::Board::Move<white> move(collector.moves[collector.index[m]]);
					const auto mcode = collector.moves[collector.index[m]];

					const bool isQuiet = order == 0;

					if (isQuiet && !inCheck) quietMoveCount++;

					if (quietMoveCount > (2 + depth/2) && futilityPruning) {
						break;
					}

					const auto next = move.play(pos);

					ctx.ply++;
					if (ctx.ply < MaxSearchDepth) {
						ctx.repetition[ctx.ply] = next.Hash;
					}

					int newDepth = depth - 1;
					int reduction = 0;

					if (depth >= 4 && quietMoveCount > 0 && !inCheck) {
						const size_t depthIndex = std::min<size_t>(depth, 63);
						const size_t moveIndex = std::min<size_t>(quietMoveCount + 1, 63);
						reduction = LMRTable[depthIndex][moveIndex];
					}
					
					int score;
					if (quietMoveCount == 0) {
						score = -MiniMaxAB<!white>(ctx, next, newDepth, -beta, -alpha);
					}
					else {
						score = -MiniMaxAB<!white>(ctx, next, newDepth - reduction, -alpha - 1, -alpha);
						if (score > alpha && reduction > 0) {
							score = -MiniMaxAB<!white>(ctx, next, newDepth, -alpha - 1, -alpha);
						}
						if (score > alpha && score < beta) {
							score = -MiniMaxAB<!white>(ctx, next, newDepth, -beta, -alpha);
						}
					}
					
					ctx.ply--;
					
					if (score > bestScore) {
						bestScore = score;
						bestMoveFound = mcode;
					}

					if (score > alpha) {
						alpha = score;
						ctx.pvTable.table[ctx.ply].Compose(mcode, ctx.pvTable.table[ctx.ply + 1]);
					}

					if (alpha >= beta) {
						// Beta cutoff - update heuristics
						if (order < 100) {
							// Killer moves
							if (ctx.killerMove[0][ctx.ply] != mcode) {
								for (size_t k = ctx.killerMove.size() - 1; k > 0; k--) {
									ctx.killerMove[k][ctx.ply] = ctx.killerMove[k - 1][ctx.ply];
								}
								ctx.killerMove[0][ctx.ply] = mcode;
							}
						}

						break;
					}

					if (!searchStarted) break;
				}

				// Store in transposition table
				if (bestMoveFound) {
					const auto flag = bestScore >= beta ? TTable::Flag::Beta : 
					                 (bestScore > originalAlpha ? TTable::Flag::Value : TTable::Flag::Alpha);
					tTable.Put(pos, ScoreToTT(bestScore, ctx.ply), bestMoveFound, depth, flag);
				}

				return bestScore;
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
				int score = 0;
				for (int d = 1; d <= depth && searchStarted; d++) {
					score = MiniMaxAB<white>(ctx, current, d, -MatVal, MatVal);
					if (searchStarted && ctx.pvTable.GetBest().size > 0) {
						bestMove = ctx.pvTable.GetBest().line[0];
					}

					if (abs(score) >= InMateVal) {
						break;
					}
				}
				searchStarted = false;
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
						currentBestMove = 0;
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
				currentBestMove = 0;
				searchStarted = true;

				searchThreads.clear();
				for (uint16_t i = 0; i < threadsNum; i++) {
					SearchThread st;
					st.ctx.Clear();
					searchThreads.push_back(std::move(st));
				}

				for (size_t i = 0; i < threadsNum; i++) {
					searchThreads[i].threadPtr.reset(new std::thread([this, current, milliseconds, i, onWin, threadsNum]() {
						uint8_t depth = i == 0 ? 0 : 1;
						int64_t search_time_ms = milliseconds;
						int previousScore = 0;
						bool hasPreviousScore = false;

						while (searchStarted && depth < MaxSearchDepth) {
							depth += 1;
							Gigantua::Board pos = current;
							searchThreads[i].ctx.repetition[0] = pos.Hash;

							const auto startTime = std::chrono::high_resolution_clock::now();

							int score = 0;
							int delta = AspirationWindow;
							int alpha = hasPreviousScore
								? std::max(-MatVal, previousScore - delta)
								: -MatVal;
							int beta = hasPreviousScore
								? std::min(MatVal, previousScore + delta)
								: MatVal;

							while (searchStarted) {
								score = MiniMaxAB<white>(searchThreads[i].ctx, pos, depth, alpha, beta);
								if (!searchStarted) break;

								if (score <= alpha && alpha > -MatVal) {	
									alpha = std::max(-MatVal, alpha - delta);
									delta *= 2;
									continue;
								}

								if (score >= beta && beta < MatVal) {
									beta = std::min(MatVal, beta + delta);
									delta *= 2;
									continue;
								}

								break;
							}
							if (searchStarted) {
								previousScore = score;
								hasPreviousScore = true;
							}

							const auto stopTime = std::chrono::high_resolution_clock::now();
							const auto dur_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime);

							if (searchStarted && i == 0) {
								if (searchThreads[i].ctx.pvTable.GetBest().size > 0) {
									currentBestScore = score;
									currentBestLine = searchThreads[i].ctx.pvTable.GetBest();
									currentBestMove = currentBestLine.line[0];
									std::cout << "d" << int(depth) << "s" << score << "ms" << dur_ms.count() << "(" << Gigantua::Board::moveStr(currentBestMove) << ")" << std::endl;
								}

								if (abs(score) >= InMateVal) {
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
				uint16_t lastBestMove = currentBestMove;
				searchStarted = false;
				for (auto& t : searchThreads) {
					t.Wait();
				}
				currentBestMove = lastBestMove;
			}

			uint16_t GetBestMoveTT(const Gigantua::Board& brd) const { return tTable.GetBestMove(brd); }
			PvLine GetBestLine() const { return currentBestLine; }
			uint16_t BestMove() const { return currentBestMove; }
			int BestScore() const { return currentBestScore; }

		};

	}//namespace AlphaBeta
}//namespace Search