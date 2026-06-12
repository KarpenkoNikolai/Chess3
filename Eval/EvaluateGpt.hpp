#pragma once

#include <../Gigantua/ChessBase.hpp>

#include <algorithm>
#include <array>

namespace GPT
{
	// Piece values in centipawns
	namespace PieceValue {
		constexpr int Pawn = 100;
		constexpr int Knight = 320;
		constexpr int Bishop = 330;
		constexpr int Rook = 500;
		constexpr int Queen = 900;
		constexpr int King = 20000;
	}

	// Piece-Square Tables (PST) for positional evaluation
	// Values are from white's perspective, will be mirrored for black
	namespace PST {
		// Pawn PST - encourage center control and advancement
		constexpr std::array<int, 64> Pawn = {
			  0,   0,   0,   0,   0,   0,   0,   0,
			 50,  50,  50,  50,  50,  50,  50,  50,
			 10,  10,  20,  30,  30,  20,  10,  10,
			  5,   5,  10,  25,  25,  10,   5,   5,
			  0,   0,   0,  20,  20,   0,   0,   0,
			  5,  -5, -10,   0,   0, -10,  -5,   5,
			  5,  10,  10, -20, -20,  10,  10,   5,
			  0,   0,   0,   0,   0,   0,   0,   0
		};

		// Knight PST - avoid edges, prefer center
		constexpr std::array<int, 64> Knight = {
			-50, -40, -30, -30, -30, -30, -40, -50,
			-40, -20,   0,   0,   0,   0, -20, -40,
			-30,   0,  10,  15,  15,  10,   0, -30,
			-30,   5,  15,  20,  20,  15,   5, -30,
			-30,   0,  15,  20,  20,  15,   0, -30,
			-30,   5,  10,  15,  15,  10,   5, -30,
			-40, -20,   0,   5,   5,   0, -20, -40,
			-50, -40, -30, -30, -30, -30, -40, -50
		};

		// Bishop PST - long diagonals, avoid edges
		constexpr std::array<int, 64> Bishop = {
			-20, -10, -10, -10, -10, -10, -10, -20,
			-10,   0,   0,   0,   0,   0,   0, -10,
			-10,   0,   5,  10,  10,   5,   0, -10,
			-10,   5,   5,  10,  10,   5,   5, -10,
			-10,   0,  10,  10,  10,  10,   0, -10,
			-10,  10,  10,  10,  10,  10,  10, -10,
			-10,   5,   0,   0,   0,   0,   5, -10,
			-20, -10, -10, -10, -10, -10, -10, -20
		};

		// Rook PST - prefer 7th rank and open files
		constexpr std::array<int, 64> Rook = {
			  0,   0,   0,   0,   0,   0,   0,   0,
			  5,  10,  10,  10,  10,  10,  10,   5,
			 -5,   0,   0,   0,   0,   0,   0,  -5,
			 -5,   0,   0,   0,   0,   0,   0,  -5,
			 -5,   0,   0,   0,   0,   0,   0,  -5,
			 -5,   0,   0,   0,   0,   0,   0,  -5,
			 -5,   0,   0,   0,   0,   0,   0,  -5,
			  0,   0,   0,   5,   5,   0,   0,   0
		};

		// Queen PST - slight center preference
		constexpr std::array<int, 64> Queen = {
			-20, -10, -10,  -5,  -5, -10, -10, -20,
			-10,   0,   0,   0,   0,   0,   0, -10,
			-10,   0,   5,   5,   5,   5,   0, -10,
			 -5,   0,   5,   5,   5,   5,   0,  -5,
			  0,   0,   5,   5,   5,   5,   0,  -5,
			-10,   5,   5,   5,   5,   5,   0, -10,
			-10,   0,   5,   0,   0,   0,   0, -10,
			-20, -10, -10,  -5,  -5, -10, -10, -20
		};

		// King PST - Middlegame (stay safe, prefer castled position)
		constexpr std::array<int, 64> KingMiddle = {
			-30, -40, -40, -50, -50, -40, -40, -30,
			-30, -40, -40, -50, -50, -40, -40, -30,
			-30, -40, -40, -50, -50, -40, -40, -30,
			-30, -40, -40, -50, -50, -40, -40, -30,
			-20, -30, -30, -40, -40, -30, -30, -20,
			-10, -20, -20, -20, -20, -20, -20, -10,
			 20,  20,   0,   0,   0,   0,  20,  20,
			 20,  30,  10,   0,   0,  10,  30,  20
		};

		// King PST - Endgame (activate king, move to center)
		constexpr std::array<int, 64> KingEnd = {
			-50, -40, -30, -20, -20, -30, -40, -50,
			-30, -20, -10,   0,   0, -10, -20, -30,
			-30, -10,  20,  30,  30,  20, -10, -30,
			-30, -10,  30,  40,  40,  30, -10, -30,
			-30, -10,  30,  40,  40,  30, -10, -30,
			-30, -10,  20,  30,  30,  20, -10, -30,
			-30, -30,   0,   0,   0,   0, -30, -30,
			-50, -30, -30, -30, -30, -30, -30, -50
		};
	}

	class Evaluator {
	private:
		// Convert square index to mirrored index for black pieces
		static constexpr int MirrorSquare(int sq) {
			return sq ^ 56; // Flip rank (XOR with 0b111000)
		}

		// Get PST value for a piece at a square
		template<Gigantua::BoardPiece piece, bool isWhite>
		static int GetPSTValue(int square, int phase) {
			int sq = isWhite ? square : MirrorSquare(square);
			
			if constexpr (piece == Gigantua::BoardPiece::Pawn) {
				return PST::Pawn[sq];
			}
			else if constexpr (piece == Gigantua::BoardPiece::Knight) {
				return PST::Knight[sq];
			}
			else if constexpr (piece == Gigantua::BoardPiece::Bishop) {
				return PST::Bishop[sq];
			}
			else if constexpr (piece == Gigantua::BoardPiece::Rook) {
				return PST::Rook[sq];
			}
			else if constexpr (piece == Gigantua::BoardPiece::Queen) {
				return PST::Queen[sq];
			}
			else if constexpr (piece == Gigantua::BoardPiece::King) {
				// Interpolate between middle and endgame
				int middleValue = PST::KingMiddle[sq];
				int endValue = PST::KingEnd[sq];
				return (middleValue * phase + endValue * (256 - phase)) / 256;
			}
			return 0;
		}

		// Evaluate mobility (number of pseudo-legal moves)
		static int EvaluateMobility(const Gigantua::Board& board) {
			// Simplified mobility - count of pieces that have room to move
			int mobility = 0;
			uint64_t occupied = board.Occ();
			
			// More sophisticated mobility would require move generation
			// This is a placeholder - can be enhanced
			return mobility;
		}

		// Evaluate pawn structure
		static int EvaluatePawns(const Gigantua::Board& board) {
			int score = 0;
			uint64_t wPawns = board.WPawn;
			uint64_t bPawns = board.BPawn;

			// Doubled pawns penalty
			for (int file = 0; file < 8; ++file) {
				uint64_t fileMask = 0x0101010101010101ull << file;
				int wPawnsOnFile = Bitcount(wPawns & fileMask);
				int bPawnsOnFile = Bitcount(bPawns & fileMask);
				
				if (wPawnsOnFile > 1) score -= 10 * (wPawnsOnFile - 1);
				if (bPawnsOnFile > 1) score += 10 * (bPawnsOnFile - 1);
			}

			// Isolated pawns penalty
			for (int file = 0; file < 8; ++file) {
				uint64_t fileMask = 0x0101010101010101ull << file;
				uint64_t adjMask = 0;
				if (file > 0) adjMask |= 0x0101010101010101ull << (file - 1);
				if (file < 7) adjMask |= 0x0101010101010101ull << (file + 1);

				if ((wPawns & fileMask) && !(wPawns & adjMask)) score -= 15;
				if ((bPawns & fileMask) && !(bPawns & adjMask)) score += 15;
			}

			// Passed pawns bonus
			uint64_t wp = wPawns;
			Bitloop(wp) {
				int sq = SquareOf(wp);
				int rank = sq / 8;
				int file = sq % 8;
				
				uint64_t frontSpan = 0xFFull << ((rank + 1) * 8);
				uint64_t fileMask = 0x0101010101010101ull << file;
				uint64_t adjFiles = 0;
				if (file > 0) adjFiles |= 0x0101010101010101ull << (file - 1);
				if (file < 7) adjFiles |= 0x0101010101010101ull << (file + 1);
				
				if (!(bPawns & frontSpan & (fileMask | adjFiles))) {
					score += 20 + (rank * 10); // Bonus increases with rank
				}
			}

			uint64_t bp = bPawns;
			Bitloop(bp) {
				int sq = SquareOf(bp);
				int rank = sq / 8;
				int file = sq % 8;
				
				uint64_t frontSpan = 0xFFull >> ((8 - rank) * 8);
				uint64_t fileMask = 0x0101010101010101ull << file;
				uint64_t adjFiles = 0;
				if (file > 0) adjFiles |= 0x0101010101010101ull << (file - 1);
				if (file < 7) adjFiles |= 0x0101010101010101ull << (file + 1);
				
				if (!(wPawns & frontSpan & (fileMask | adjFiles))) {
					score -= 20 + ((7 - rank) * 10);
				}
			}

			return score;
		}

		// Calculate game phase (0 = endgame, 256 = opening)
		static int CalculatePhase(const Gigantua::Board& board) {
			int phase = 0;
			phase += Bitcount(board.WKnight | board.BKnight) * 1;
			phase += Bitcount(board.WBishop | board.BBishop) * 1;
			phase += Bitcount(board.WRook | board.BRook) * 2;
			phase += Bitcount(board.WQueen | board.BQueen) * 4;
			
			// Total material: 4N + 4B + 4R + 2Q = 24
			// Normalize to 0-256 range
			phase = std::min(phase * 256 / 24, 256);
			return phase;
		}

		// Evaluate bishop pair bonus
		static int EvaluateBishopPair(const Gigantua::Board& board) {
			int score = 0;
			
			if (Bitcount(board.WBishop) >= 2) score += 30;
			if (Bitcount(board.BBishop) >= 2) score -= 30;
			
			return score;
		}

		// Evaluate king safety
		static int EvaluateKingSafety(const Gigantua::Board& board, int phase) {
			// Only relevant in middlegame
			if (phase < 128) return 0;
			
			int score = 0;
			
			// Pawn shield bonus
			uint64_t wKing = board.WKing;
			uint64_t bKing = board.BKing;
			
			if (wKing) {
				int kSq = SquareOf(wKing);
				int file = kSq % 8;
				uint64_t shield = 0;
				
				// Check pawns in front of king
				if (kSq < 56) {
					shield = (1ull << (kSq + 8));
					if (file > 0 && kSq < 56) shield |= (1ull << (kSq + 7));
					if (file < 7 && kSq < 56) shield |= (1ull << (kSq + 9));
				}
				
				score += Bitcount(board.WPawn & shield) * 10;
			}
			
			if (bKing) {
				int kSq = SquareOf(bKing);
				int file = kSq % 8;
				uint64_t shield = 0;
				
				if (kSq >= 8) {
					shield = (1ull << (kSq - 8));
					if (file > 0 && kSq >= 8) shield |= (1ull << (kSq - 9));
					if (file < 7 && kSq >= 8) shield |= (1ull << (kSq - 7));
				}
				
				score -= Bitcount(board.BPawn & shield) * 10;
			}
			
			return score * phase / 256; // Scale by game phase
		}

		// Evaluate rook placement
		static int EvaluateRooks(const Gigantua::Board& board) {
			int score = 0;
			uint64_t wPawns = board.WPawn;
			uint64_t bPawns = board.BPawn;
			
			// Rook on open file bonus
			uint64_t wr = board.WRook;
			Bitloop(wr) {
				int sq = SquareOf(wr);
				int file = sq % 8;
				uint64_t fileMask = 0x0101010101010101ull << file;
				
				if (!(wPawns & fileMask)) {
					score += (bPawns & fileMask) ? 15 : 25; // Semi-open or open
				}
			}

			uint64_t br = board.BRook;
			Bitloop(br) {
				int sq = SquareOf(br);
				int file = sq % 8;
				uint64_t fileMask = 0x0101010101010101ull << file;
				
				if (!(bPawns & fileMask)) {
					score -= (wPawns & fileMask) ? 15 : 25;
				}
			}
			
			return score;
		}

	public:
		// Main evaluation function
		static int Evaluate(const Gigantua::Board& board) {
			int score = 0;
			int phase = CalculatePhase(board);

			// Material and piece-square tables
			uint64_t pieces = board.WPawn;
			Bitloop(pieces) {
				int sq = SquareOf(pieces);
				score += PieceValue::Pawn + GetPSTValue<Gigantua::BoardPiece::Pawn, true>(sq, phase);
			}
			pieces = board.BPawn;
			Bitloop(pieces) {
				int sq = SquareOf(pieces);
				score -= PieceValue::Pawn + GetPSTValue<Gigantua::BoardPiece::Pawn, false>(sq, phase);
			}

			pieces = board.WKnight;
			Bitloop(pieces) {
				int sq = SquareOf(pieces);
				score += PieceValue::Knight + GetPSTValue<Gigantua::BoardPiece::Knight, true>(sq, phase);
			}
			pieces = board.BKnight;
			Bitloop(pieces) {
				int sq = SquareOf(pieces);
				score -= PieceValue::Knight + GetPSTValue<Gigantua::BoardPiece::Knight, false>(sq, phase);
			}

			pieces = board.WBishop;
			Bitloop(pieces) {
				int sq = SquareOf(pieces);
				score += PieceValue::Bishop + GetPSTValue<Gigantua::BoardPiece::Bishop, true>(sq, phase);
			}
			pieces = board.BBishop;
			Bitloop(pieces) {
				int sq = SquareOf(pieces);
				score -= PieceValue::Bishop + GetPSTValue<Gigantua::BoardPiece::Bishop, false>(sq, phase);
			}

			pieces = board.WRook;
			Bitloop(pieces) {
				int sq = SquareOf(pieces);
				score += PieceValue::Rook + GetPSTValue<Gigantua::BoardPiece::Rook, true>(sq, phase);
			}
			pieces = board.BRook;
			Bitloop(pieces) {
				int sq = SquareOf(pieces);
				score -= PieceValue::Rook + GetPSTValue<Gigantua::BoardPiece::Rook, false>(sq, phase);
			}

			pieces = board.WQueen;
			Bitloop(pieces) {
				int sq = SquareOf(pieces);
				score += PieceValue::Queen + GetPSTValue<Gigantua::BoardPiece::Queen, true>(sq, phase);
			}
			pieces = board.BQueen;
			Bitloop(pieces) {
				int sq = SquareOf(pieces);
				score -= PieceValue::Queen + GetPSTValue<Gigantua::BoardPiece::Queen, false>(sq, phase);
			}

			// King evaluation
			if (board.WKing) {
				int sq = SquareOf(board.WKing);
				score += GetPSTValue<Gigantua::BoardPiece::King, true>(sq, phase);
			}
			if (board.BKing) {
				int sq = SquareOf(board.BKing);
				score -= GetPSTValue<Gigantua::BoardPiece::King, false>(sq, phase);
			}

			// Positional factors
			score += EvaluatePawns(board);
			score += EvaluateBishopPair(board);
			score += EvaluateKingSafety(board, phase);
			score += EvaluateRooks(board);

			// Tempo bonus for side to move
			score += board.status.WhiteMove() ? 10 : -10;

			// Return score from white's perspective
			return board.status.WhiteMove() ? score : -score;
		}

		// Quick material-only evaluation
		static int EvaluateMaterial(const Gigantua::Board& board) {
			int score = 0;
			
			score += Bitcount(board.WPawn) * PieceValue::Pawn;
			score += Bitcount(board.WKnight) * PieceValue::Knight;
			score += Bitcount(board.WBishop) * PieceValue::Bishop;
			score += Bitcount(board.WRook) * PieceValue::Rook;
			score += Bitcount(board.WQueen) * PieceValue::Queen;
			
			score -= Bitcount(board.BPawn) * PieceValue::Pawn;
			score -= Bitcount(board.BKnight) * PieceValue::Knight;
			score -= Bitcount(board.BBishop) * PieceValue::Bishop;
			score -= Bitcount(board.BRook) * PieceValue::Rook;
			score -= Bitcount(board.BQueen) * PieceValue::Queen;
			
			return board.status.WhiteMove() ? score : -score;
		}
	};
}

