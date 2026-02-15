#pragma once

#include "../Gigantua/ChessBase.hpp"

namespace Search {

	static constexpr uint8_t MaxMovesInPosition = 0xff;

	template<bool white>
	class MoveCollector : public Gigantua::MoveList::MoveCollectorBase<MoveCollector<white>, white>
	{
	public:
		mutable std::array<uint16_t, MaxMovesInPosition> moves;
		mutable std::array<int32_t, MaxMovesInPosition> order;
		mutable std::array<uint8_t, MaxMovesInPosition> index;
		mutable uint8_t size = 0;

		void Reset() {
			size = 0;
		}

		bool CollectImpl(const Gigantua::Board::Move<white>& move) const
		{
			index[size] = size;
			order[size] = 0;
			moves[size] = move.move;
			size++;
			return true;
		}

		void SortMoves(uint8_t pos) {
			uint8_t hieght = pos;

			for (uint8_t i = pos + 1; i < size; i++) {
				if (order[index[i]] > order[index[hieght]]) {
					hieght = i;
				}
			}

			uint8_t temp = index[hieght];
			index[hieght] = index[pos];
			index[pos] = temp;
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

				const uint8_t tmp = index[i];
				index[i] = index[j];
				index[j] = tmp;
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

	static constexpr std::array<int32_t, 7> capOrder = { 136, 782, 830, 1289, 2529, 0, 0 };

	template<bool white>
	static int32_t SimpleSort(const Gigantua::Board& pos, const Gigantua::Board::Move<white> move, bool onlyCap = false)
	{
		int32_t result = capOrder[int(move.captured(pos))];

		if (result) {
			if (move.who(pos) == Gigantua::BoardPiece::Pawn) result += 5;
			if (move.who(pos) == Gigantua::BoardPiece::Knight) result += 3;
			if (move.who(pos) == Gigantua::BoardPiece::Bishop) result += 2;
			if (move.who(pos) == Gigantua::BoardPiece::Rook) result += 1;
		}

		if (move.isQueenPromote()) result += 3000;

		if (onlyCap) return result;

		const auto next = move.play(pos);
		if (Gigantua::MoveList::InCheck<!white>(next)) result += 10000;

		if (result)
			return result;

		if (Gigantua::MoveList::QueenInCheck<!white>(next)) {
			result += 90;
			if (move.who(pos) == Gigantua::BoardPiece::Pawn) result += 5;
			if (move.who(pos) == Gigantua::BoardPiece::Knight) result += 3;
			if (move.who(pos) == Gigantua::BoardPiece::Bishop) result += 2;
			if (move.who(pos) == Gigantua::BoardPiece::Rook) result += 1;
		}
		else if (Gigantua::MoveList::RookInCheck<!white>(next)) {
			result += 80;
			if (move.who(pos) == Gigantua::BoardPiece::Pawn) result += 5;
			if (move.who(pos) == Gigantua::BoardPiece::Knight) result += 3;
			if (move.who(pos) == Gigantua::BoardPiece::Bishop) result += 2;
		}
		else if (Gigantua::MoveList::KnightInCheck<!white>(next)) {
			result += 70;
			if (move.who(pos) == Gigantua::BoardPiece::Pawn) result += 5;
			if (move.who(pos) == Gigantua::BoardPiece::Bishop) result += 2;
			if (move.who(pos) == Gigantua::BoardPiece::Rook) result += 1;
		}
		else if (Gigantua::MoveList::BishopInCheck<!white>(next)) {
			result += 60;
			if (move.who(pos) == Gigantua::BoardPiece::Pawn) result += 5;
			if (move.who(pos) == Gigantua::BoardPiece::Knight) result += 2;
			if (move.who(pos) == Gigantua::BoardPiece::Rook) result += 1;
		}

		if (!result) {
			result += 200 / (Gigantua::MoveList::MovesCount<!white>(next) + 1);
		}

		if (move.type() == Gigantua::MoveType::PawnMove) result += 5;

		return result;
	}

}//namespace Search


