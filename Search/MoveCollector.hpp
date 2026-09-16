#pragma once

#include "../Gigantua/ChessBase.hpp"

namespace Search {

	static constexpr uint8_t MaxMovesInPosition = 0xff;
	static constexpr std::array<int32_t, 7> MaterialCost = { 136, 782, 830, 1289, 2529, 0, 0 };

	template<bool white>
	class MoveCollector : public Gigantua::MoveList::MoveCollectorBase<MoveCollector<white>, white>
	{
	public:
		mutable std::array<uint16_t, MaxMovesInPosition> moves;
		mutable std::array<int32_t, MaxMovesInPosition> order;
		mutable std::array<uint8_t, MaxMovesInPosition> index;
		mutable std::array<uint32_t, MaxMovesInPosition> entries;
		mutable uint8_t size = 0;

		void Reset() {
			size = 0;
		}

		bool CollectImpl(const Gigantua::Board::Move<white>& move) const
		{
			moves[size] = move.move;
			order[size] = 0;
			index[size] = size;
			entries[size] = 0;
			size++;
			return true;
		}

		void SortMoves(uint8_t pos) {
			uint8_t height = pos;

			for (uint8_t i = pos + 1; i < size; i++) {
				if (order[index[i]] > order[index[height]]) {
					height = i;
				}
			}

			if (pos != height)
				std::swap(index[pos], index[height]);
		}

		void SortMovesEntries(uint8_t pos) {
			uint8_t height = pos;
			for (uint8_t i = pos + 1; i < size; i++) {
				if (entries[index[i]] > entries[index[height]]) {
					height = i;
				}
			}

			if (pos != height)
				std::swap(index[pos], index[height]);
		}

		void SortMoves() {
			sortMoves(0, size - 1);
		}

	private:
		int partition(int low, int high) {
			int pivot = order[index[low]];
			int i = low - 1;
			int j = high + 1;

			while (true) {
				do { i++; } while (order[index[i]] > pivot);
				do { j--; } while (order[index[j]] < pivot);
				if (i >= j) return j;

				std::swap(index[i], index[j]);
			}
		}

		void sortMoves(int low, int high) {
			if (low < high) {
				int pi = partition(low, high);
				sortMoves(low, pi);
				sortMoves(pi + 1, high);
			}
		}
	};


	template<bool white>
	static bool IsPassedPawnPush(
		const Gigantua::Board& pos,
		const Gigantua::Board::Move<white>& move)
	{
		using MoveType = Gigantua::MoveType;

		if (move.who(pos) != Gigantua::BoardPiece::Pawn)
			return false;

		if (move.captured(pos) != Gigantua::BoardPiece::None)
			return false;

		if (move.type() != MoveType::PawnMove &&
			move.type() != MoveType::PawnPush)
			return false;

		const int targetSquare = move.to();
		const int targetRank = targetSquare / 8;
		const int targetFile = targetSquare % 8;

		const uint64_t enemyPawns = white ? pos.BPawn : pos.WPawn;

		const int direction = white ? 1 : -1;
		const int firstRank = targetRank + direction;
		const int lastRank = white ? 7 : 0;

		for (int rank = firstRank;
			white ? rank <= lastRank : rank >= lastRank;
			rank += direction)
		{
			for (int file = std::max(0, targetFile - 1);
				file <= std::min(7, targetFile + 1);
				++file)
			{
				const uint64_t square = 1ull << (rank * 8 + file);

				if (enemyPawns & square)
					return false;
			}
		}

		return true;
	}

	template<bool white>
	static int32_t SimpleSort(const Gigantua::Board& pos, const Gigantua::Board::Move<white> move, bool onlyCap = false)
	{
		int32_t result = MaterialCost[int(move.captured(pos))];

		if (result) {
			if (move.who(pos) == Gigantua::BoardPiece::Pawn) result += 5;
			if (move.who(pos) == Gigantua::BoardPiece::Knight) result += 3;
			if (move.who(pos) == Gigantua::BoardPiece::Bishop) result += 2;
			if (move.who(pos) == Gigantua::BoardPiece::Rook) result += 1;
		}

		if (move.isQueenPromote()) result += 3000;

		if (onlyCap) return result;

		const auto next = move.play(pos);
		if (Gigantua::MoveList::InCheck<!white>(next)) {
			result += 10000;
			if (Gigantua::MoveList::MovesCount<!white>(next) == 0) result += 2000000;
		}

		if (result)
			return result;

		const auto skipNext = pos.SkipMove();

		if (!Gigantua::MoveList::QueenInCheck<!white>(skipNext) && Gigantua::MoveList::QueenInCheck<!white>(next)) {
			result += 90;
			if (move.who(pos) == Gigantua::BoardPiece::Pawn) result += 5;
			if (move.who(pos) == Gigantua::BoardPiece::Knight) result += 3;
			if (move.who(pos) == Gigantua::BoardPiece::Bishop) result += 2;
			if (move.who(pos) == Gigantua::BoardPiece::Rook) result += 1;
			return result;
		}

		if (!Gigantua::MoveList::RookInCheck<!white>(skipNext) && Gigantua::MoveList::RookInCheck<!white>(next)) {
			result += 80;
			if (move.who(pos) == Gigantua::BoardPiece::Pawn) result += 5;
			if (move.who(pos) == Gigantua::BoardPiece::Knight) result += 3;
			if (move.who(pos) == Gigantua::BoardPiece::Bishop) result += 2;
			return result;
		}

		if (!Gigantua::MoveList::KnightInCheck<!white>(skipNext) && Gigantua::MoveList::KnightInCheck<!white>(next)) {
			result += 70;
			if (move.who(pos) == Gigantua::BoardPiece::Pawn) result += 5;
			if (move.who(pos) == Gigantua::BoardPiece::Bishop) result += 2;
			if (move.who(pos) == Gigantua::BoardPiece::Rook) result += 1;
			return result;
		}

		if (!Gigantua::MoveList::BishopInCheck<!white>(skipNext) && Gigantua::MoveList::BishopInCheck<!white>(next)) {
			result += 60;
			if (move.who(pos) == Gigantua::BoardPiece::Pawn) result += 5;
			if (move.who(pos) == Gigantua::BoardPiece::Knight) result += 2;
			if (move.who(pos) == Gigantua::BoardPiece::Rook) result += 1;
			return result;
		}

		// Passed-pawn push bonus.
		if (IsPassedPawnPush<white>(pos, move)) {
			const int rank = move.to() / 8;
			const int advanceRank = white ? rank : 7 - rank;

			// Der Bonus steigt, je näher der Bauer an der Umwandlung ist.
			result += 40 + advanceRank * 20;
		}

		return result;
	}

}//namespace Search


