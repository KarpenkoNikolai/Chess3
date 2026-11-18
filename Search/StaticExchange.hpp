#pragma once

#include "../Gigantua/MoveGen.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

namespace Search {

	struct StaticExchangeEvaluation {
		static constexpr std::array<int, 6> PieceValues = { 136, 782, 830, 1289, 2529, 20000 };

		using Pieces = std::array<std::array<uint64_t, 6>, 2>;

		static uint64_t Occupancy(const Pieces& pieces) {
			uint64_t occupancy = 0;
			for (const auto& side : pieces) {
				for (const auto piece : side) occupancy |= piece;
			}
			return occupancy;
		}

		static uint64_t AttackersTo(const Pieces& pieces, int side, uint8_t square, uint64_t occupancy) {
			const uint64_t target = 1ull << square;
			const bool white = side == 0;
			uint64_t attackers = 0;

			if (white) {
				attackers |= (Gigantua::Pawn_AttackLeft<false>(target) | Gigantua::Pawn_AttackRight<false>(target)) & pieces[side][0];
			}
			else {
				attackers |= (Gigantua::Pawn_AttackLeft<true>(target) | Gigantua::Pawn_AttackRight<true>(target)) & pieces[side][0];
			}

			attackers |= Gigantua::Lookup::Knight(square) & pieces[side][1];
			attackers |= Gigantua::Lookup::Bishop(square, occupancy) & (pieces[side][2] | pieces[side][4]);
			attackers |= Gigantua::Lookup::Rook(square, occupancy) & (pieces[side][3] | pieces[side][4]);
			return attackers;
		}

		static int PromotionPiece(Gigantua::MoveType type) {
			switch (type) {
			case Gigantua::MoveType::KnightMovePromote: return 1;
			case Gigantua::MoveType::BishopMovePromote: return 2;
			case Gigantua::MoveType::RookMovePromote: return 3;
			case Gigantua::MoveType::QueenMovePromote: return 4;
			default: return -1;
			}
		}

		template<bool white>
		static int Evaluate(const Gigantua::Board& pos, const Gigantua::Board::Move<white>& move) {
			Pieces pieces{};
			pieces[0] = { pos.WPawn, pos.WKnight, pos.WBishop, pos.WRook, pos.WQueen, pos.WKing };
			pieces[1] = { pos.BPawn, pos.BKnight, pos.BBishop, pos.BRook, pos.BQueen, pos.BKing };

			const int movingSide = white ? 0 : 1;
			const int capturedSide = 1 - movingSide;
			const int movingPiece = static_cast<int>(move.who(pos));
			const int capturedPiece = static_cast<int>(move.captured(pos));
			const int promotionPiece = PromotionPiece(move.type());
			const int resultPiece = promotionPiece >= 0 ? promotionPiece : movingPiece;
			const uint8_t targetSquare = move.to();
			const uint64_t from = 1ull << move.from();
			const uint64_t target = 1ull << targetSquare;

			int gain[32]{};
			gain[0] = capturedPiece < 6 ? PieceValues[capturedPiece] : 0;
			if (promotionPiece >= 0) gain[0] += PieceValues[promotionPiece] - PieceValues[0];

			if (capturedPiece < 6) {
				int capturedSquare = targetSquare;
				if (move.type() == Gigantua::MoveType::PawnEnpassantTake) {
					capturedSquare += white ? -8 : 8;
				}
				pieces[capturedSide][capturedPiece] &= ~(1ull << capturedSquare);
			}

			pieces[movingSide][movingPiece] &= ~from;
			pieces[movingSide][resultPiece] |= target;

			int occupantSide = movingSide;
			int occupantPiece = resultPiece;
			int side = capturedSide;
			int depth = 0;
			uint64_t occupancy = Occupancy(pieces);

			while (depth < 30) {
				const uint64_t attackers = AttackersTo(pieces, side, targetSquare, occupancy);
				int attackerPiece = -1;
				uint64_t attackerSquare = 0;

				for (int piece = 0; piece < 5; ++piece) {
					const uint64_t candidates = attackers & pieces[side][piece];
					if (candidates) {
						attackerPiece = piece;
						attackerSquare = candidates & (~candidates + 1);
						break;
					}
				}

				if (attackerPiece < 0) break;

				++depth;
				gain[depth] = PieceValues[occupantPiece] - gain[depth - 1];
				pieces[occupantSide][occupantPiece] &= ~target;
				pieces[side][attackerPiece] &= ~attackerSquare;
				pieces[side][attackerPiece] |= target;
				occupantSide = side;
				occupantPiece = attackerPiece;
				side = 1 - side;
				occupancy = Occupancy(pieces);
			}

			while (depth > 0) {
				gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
				--depth;
			}

			return gain[0];
		}
	};

}
