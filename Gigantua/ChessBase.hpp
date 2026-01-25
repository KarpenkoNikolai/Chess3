#pragma once

#include <iostream>
#include <sstream>
#include <string_view>
#include <assert.h>
#include <array>
#include "MoveMap.hpp"

namespace Gigantua {

	//This is the most important definition for performance!
	namespace Lookup = ChessLookup::LookupPext;

	static constexpr const char* SQSTR[64] = {
		"h1", "g1", "f1", "e1", "d1", "c1", "b1", "a1",
		"h2", "g2", "f2", "e2", "d2", "c2", "b2", "a2",
		"h3", "g3", "f3", "e3", "d3", "c3", "b3", "a3",
		"h4", "g4", "f4", "e4", "d4", "c4", "b4", "a4",
		"h5", "g5", "f5", "e5", "d5", "c5", "b5", "a5",
		"h6", "g6", "f6", "e6", "d6", "c6", "b6", "a6",
		"h7", "g7", "f7", "e7", "d7", "c7", "b7", "a7",
		"h8", "g8", "f8", "e8", "d8", "c8", "b8", "a8"
	};

	enum class FenField {
		white,
		hasEP,
		WCastleL,
		WCastleR,
		BCastleL,
		BCastleR
	};

	struct FEN {
		static constexpr uint64_t FenEnpassant(std::string_view FEN) {
			uint64_t i = 0;

			while (FEN[i++] != ' ')
			{

			}
			char wb = FEN[i++];
			i++;

			//Castling
			while (FEN[i++] != ' ')
			{

			}

			//En Passant
			char EorMinus = FEN[i++];
			if (EorMinus != '-') {
				if (wb == 'w') {
					//Todo where to store Enpassant
					int EPpos = 32 + ('h' - EorMinus);
					return 1ull << EPpos;
				}
				if (wb == 'b') {
					//Todo where to store Enpassant
					int EPpos = 24 + ('h' - EorMinus);
					return 1ull << EPpos;
				}
			}
			return 0;
		}


		template<FenField field>
		static constexpr bool FenInfo(std::string_view FEN) {
			uint64_t i = 0;
			char c{};

			while ((c = FEN[i++]) != ' ')
			{

			}
			char wb = FEN[i++];

			//White
			if constexpr (field == FenField::white) {
				if (wb == 'w') return true;
				else return false;
			}
			i++;

			//Castling
			while ((c = FEN[i++]) != ' ')
			{
				if constexpr (field == FenField::WCastleR) {
					if (c == 'K') return true;
				}
				if constexpr (field == FenField::WCastleL) {
					if (c == 'Q') return true;
				}
				if constexpr (field == FenField::BCastleR) {
					if (c == 'k') return true;
				}
				if constexpr (field == FenField::BCastleL) {
					if (c == 'q') return true;
				}
			}
			if constexpr (field == FenField::WCastleR || field == FenField::WCastleL || field == FenField::BCastleR || field == FenField::BCastleL)
			{
				return false;
			}

			//En Passant
			char EorMinus = FEN[i++];
			if (EorMinus != '-') {
				if (wb == 'w') {
					//Todo where to store Enpassant
					//int EPpos = 32 + ('h' - EorMinus);
					if constexpr (field == FenField::hasEP) return true;
				}
				if (wb == 'b') {
					//Todo where to store Enpassant
					//int EPpos = 24 + ('h' - EorMinus);
					if constexpr (field == FenField::hasEP) return true;
				}
			}
			if constexpr (field == FenField::hasEP) return false;
		}

		/// Transform FEN character 'n' or 'Q' into bitmap where the bits correspond to the field
		static constexpr uint64_t FenToBmp(std::string_view FEN, char p)
		{
			uint64_t i = 0;
			char c{};
			int Field = 63;

			uint64_t result = 0;
			while ((c = FEN[i++]) != ' ')
			{
				uint64_t P = 1ull << Field;
				switch (c) {
				case '/': Field += 1; break;
				case '1': break;
				case '2': Field -= 1; break;
				case '3': Field -= 2; break;
				case '4': Field -= 3; break;
				case '5': Field -= 4; break;
				case '6': Field -= 5; break;
				case '7': Field -= 6; break;
				case '8': Field -= 7; break;
				default:
					if (c == p) result |= P; //constexpr parsing happens here
				}
				Field--;
			}
			return result;
		}
	};

	//Constexpr class as template parameter
	class BoardStatus {
	private:
		static constexpr uint64_t WNotOccupiedL = 0b01110000ull;
		static constexpr uint64_t WNotAttackedL = 0b00111000ull;

		static constexpr uint64_t WNotOccupiedR = 0b00000110ull;
		static constexpr uint64_t WNotAttackedR = 0b00001110ull;

		static constexpr uint64_t BNotOccupiedL = 0b01110000ull << 56ull;
		static constexpr uint64_t BNotAttackedL = 0b00111000ull << 56ull;

		static constexpr uint64_t BNotOccupiedR = 0b00000110ull << 56ull;
		static constexpr uint64_t BNotAttackedR = 0b00001110ull << 56ull;

		static constexpr uint64_t WRookL_Change = 0b11111000ull;
		static constexpr uint64_t BRookL_Change = 0b11111000ull << 56ull;
		static constexpr uint64_t WRookR_Change = 0b00001111ull;
		static constexpr uint64_t BRookR_Change = 0b00001111ull << 56ull;

		uint64_t status = 0;
	public:
		static constexpr uint64_t WRookL = 0b10000000ull;
		static constexpr uint64_t BRookL = 0b10000000ull << 56ull;
		static constexpr uint64_t WRookR = 0b00000001ull;
		static constexpr uint64_t BRookR = 0b00000001ull << 56ull;

		_ForceInline bool WhiteMove() const { return status & 0b00001; }

		_ForceInline bool WCastleL() const { return status & 0b00010; }
		_ForceInline bool WCastleR() const { return status & 0b00100; }

		_ForceInline bool BCastleL() const { return status & 0b01000; }
		_ForceInline bool BCastleR() const { return status & 0b10000; }

		_ForceInline uint64_t EnPassantTarget() const { return status & 0x000000ffff000000; };

		_ForceInline uint64_t Value() const { return status; };

		BoardStatus() {}

		BoardStatus(bool white, bool wcast_left, bool wcast_right, bool bcast_left, bool bcast_right, uint64_t enPassantTarget)
		{
			if (white)       status |= 0b00001;
			if (wcast_left)  status |= 0b00010;
			if (wcast_right) status |= 0b00100;
			if (bcast_left)  status |= 0b01000;
			if (bcast_right) status |= 0b10000;

			status |= enPassantTarget;
		}

		BoardStatus(uint64_t value) : status(value)
		{
		}

		BoardStatus(std::string_view fen) : BoardStatus(
			FEN::FenInfo<FenField::white>(fen),
			FEN::FenInfo<FenField::WCastleL>(fen),
			FEN::FenInfo<FenField::WCastleR>(fen),
			FEN::FenInfo<FenField::BCastleL>(fen),
			FEN::FenInfo<FenField::BCastleR>(fen),
			FEN::FenEnpassant(fen))
		{}

		friend bool operator==(const BoardStatus& lhs, const BoardStatus& rhs) { return lhs.status == rhs.status; }
		friend bool operator!=(const BoardStatus& lhs, const BoardStatus& rhs) { return lhs.status != rhs.status; }

		bool CanCastle() const {
			if (WhiteMove()) return WCastleL() || WCastleR();
			else return BCastleL() || BCastleR();
		}

		bool CanCastleLeft() const {
			if (WhiteMove()) return WCastleL();
			else return BCastleL();
		}

		bool CanCastleRight() const {
			if (WhiteMove()) return WCastleR();
			else return BCastleR();
		}

		uint64_t Castle_RookswitchR() const {
			if (WhiteMove()) return 0b00000101ull;
			else return 0b00000101ull << 56;
		}
		uint64_t Castle_RookswitchL() const {
			if (WhiteMove()) return 0b10010000ull;
			else return 0b10010000ull << 56;
		}

		_Inline bool CanCastleLeft(uint64_t attacked, uint64_t occupied, uint64_t rook) const {
			if (WhiteMove() && WCastleL())
			{
				if (occupied & WNotOccupiedL) return false;
				if (attacked & WNotAttackedL) return false;
				if (rook & WRookL) return true;
				return false;
			}
			else if (BCastleL()) {
				if (occupied & BNotOccupiedL) return false;
				if (attacked & BNotAttackedL) return false;
				if (rook & BRookL) return true;
				return false;
			}
			return false;
		}

		_Inline bool CanCastleRight(uint64_t attacked, uint64_t occupied, uint64_t rook) const {
			if (WhiteMove() && WCastleR())
			{
				if (occupied & WNotOccupiedR) return false;
				if (attacked & WNotAttackedR) return false;
				if (rook & WRookR) return true;
				return false;
			}
			else if (BCastleR()) {
				if (occupied & BNotOccupiedR) return false;
				if (attacked & BNotAttackedR) return false;
				if (rook & BRookR) return true;
				return false;
			}
			return false;
		}

		bool IsLeftRook(uint64_t rook) const {
			if (WhiteMove()) return WRookL == rook;
			else return BRookL == rook;
		}
		bool IsRightRook(uint64_t rook) const {
			if (WhiteMove()) return WRookR == rook;
			else return BRookR == rook;
		}


		BoardStatus PawnPush(uint64_t epTarget) const {
			return BoardStatus(((status & (0b11111)) ^ 0b1) | epTarget);
		}

		//Moving the king
		BoardStatus KingMove() const {
			if (WhiteMove()) {
				return BoardStatus((status & (0b11001)) ^ 0b1);
			}
			else {
				return BoardStatus((status & (0b00111)) ^ 0b1);
			}
		}

		//Moving a castling rook
		BoardStatus RookMove_Left() const {
			if (WhiteMove()) {
				return BoardStatus((status & (0b11101)) ^ 0b1);
			}
			else {
				return BoardStatus((status & (0b10111)) ^ 0b1);
			}
		}

		BoardStatus RookMove_Right() const {
			if (WhiteMove()) {
				return BoardStatus((status & (0b11011)) ^ 0b1);
			}
			else {
				return BoardStatus((status & (0b01111)) ^ 0b1);
			}
		}


		BoardStatus SilentMove() const {
			return BoardStatus((status & (0b11111)) ^ 0b1);
		}
	};

	enum class BoardPiece {
		Pawn = 0, Knight, Bishop, Rook, Queen, King, None
	};

	enum class MoveType
	{
		KingMove = 0,
		KingCastleLeft,
		KingCastleRight,
		PawnMove,
		PawnAtk,
		PawnEnpassantTake,
		PawnPush,
		KnightMove,
		BishopMove,
		RookMove,
		QueenMove,
		KnightMovePromote,
		BishopMovePromote,
		RookMovePromote,
		QueenMovePromote
	};


	struct Board {
		uint64_t BPawn = 0;
		uint64_t BKnight = 0;
		uint64_t BBishop = 0;
		uint64_t BRook = 0;
		uint64_t BQueen = 0;
		uint64_t BKing = 0;

		uint64_t WPawn = 0;
		uint64_t WKnight = 0;
		uint64_t WBishop = 0;
		uint64_t WRook = 0;
		uint64_t WQueen = 0;
		uint64_t WKing = 0;

		BoardStatus status = 0;

		uint64_t Hash = 0;

		static uint64_t cell(int8_t x, int8_t y) {
			if (x < 0 || y < 0 || x > 7 || y > 7) return 0ull;

			return 1ull << uint8_t(x + 8 * y);
		}

		_ForceInline uint64_t Black() const { return BPawn | BKnight | BBishop | BRook | BQueen | BKing; }
		_ForceInline uint64_t White() const { return WPawn | WKnight | WBishop | WRook | WQueen | WKing; }
		_ForceInline uint64_t Occ() const { return Black() | White(); }

		static constexpr uint64_t DarkSquares = 0x55aa55aa55aa55aaull;

		Board() {}

		Board(
			uint64_t bp, uint64_t bn, uint64_t bb, uint64_t br, uint64_t bq, uint64_t bk,
			uint64_t wp, uint64_t wn, uint64_t wb, uint64_t wr, uint64_t wq, uint64_t wk, BoardStatus st) :
			BPawn(bp), BKnight(bn), BBishop(bb), BRook(br), BQueen(bq), BKing(bk),
			WPawn(wp), WKnight(wn), WBishop(wb), WRook(wr), WQueen(wq), WKing(wk),
			status(st)
		{
			Hash = BPawn * 0x0ea77a98b5a86659ull;
			Hash ^= BKnight * 0xb501bce657c8a33aull;
			Hash ^= BBishop * 0xfe204f09d21696b4ull;
			Hash ^= BRook * 0x1c1d761faacf8548ull;
			Hash ^= BQueen * 0xd166dc9059f73ca6ull;
			Hash ^= BKing * 0x3234a26e1484b3d0ull;

			Hash ^= WPawn * 0xc7628097fc96e72cull;
			Hash ^= WKnight * 0xdcc28057e306a015ull;
			Hash ^= WBishop * 0xb11fcb5fabdfbeb2ull;
			Hash ^= WRook * 0xd047c0d37f7c414dull;
			Hash ^= WQueen * 0x04e8ab76cca34dc4ull;
			Hash ^= WKing * 0x304052e4bbbff0eaull;
			Hash ^= status.Value() * 0x1b96ed4a75ba0db8ull;

			Hash = (Hash ^ (Hash >> 30)) * 0xbf58476d1ce4e5b9ull;
			Hash = (Hash ^ (Hash >> 27)) * 0x94d049bb133111ebull;
			Hash = Hash ^ (Hash >> 31);
		}

		Board(std::string_view FEN) :
			Board(FEN::FenToBmp(FEN, 'p'), FEN::FenToBmp(FEN, 'n'), FEN::FenToBmp(FEN, 'b'), FEN::FenToBmp(FEN, 'r'), FEN::FenToBmp(FEN, 'q'), FEN::FenToBmp(FEN, 'k'),
				FEN::FenToBmp(FEN, 'P'), FEN::FenToBmp(FEN, 'N'), FEN::FenToBmp(FEN, 'B'), FEN::FenToBmp(FEN, 'R'), FEN::FenToBmp(FEN, 'Q'), FEN::FenToBmp(FEN, 'K'),
				FEN)
		{
		}

		bool IsNull() const
		{
			return Hash == 0ull;
		}

		Board Mirror() const
		{
			return Board(ReverseBits(WPawn), ReverseBits(WKnight), ReverseBits(WBishop), ReverseBits(WRook), ReverseBits(WQueen), ReverseBits(WKing),
				ReverseBits(BPawn), ReverseBits(BKnight), ReverseBits(BBishop), ReverseBits(BRook), ReverseBits(BQueen), ReverseBits(BKing),
				BoardStatus(!status.WhiteMove(), status.BCastleL(), status.BCastleR(), status.WCastleL(), status.WCastleR(), ReverseBits(status.EnPassantTarget())));
		}

		friend bool operator==(const Board& a, const Board& b)
		{
			if (a.Hash != b.Hash) return false;
			
			return a.BPawn == b.BPawn &&
				a.BKnight == b.BKnight &&
				a.BBishop == b.BBishop &&
				a.BRook == b.BRook &&
				a.BQueen == b.BQueen &&
				a.BKing == b.BKing &&
				a.WPawn == b.WPawn &&
				a.WKnight == b.WKnight &&
				a.WBishop == b.WBishop &&
				a.WRook == b.WRook &&
				a.WQueen == b.WQueen &&
				a.WKing == b.WKing &&
				a.status == b.status;
		}

		friend bool operator!=(const Board& lhs, const Board& rhs) { return !(lhs == rhs); }

		template<bool white>
		struct Move {
			uint16_t move;

			Move() : move(0) {}

			Move(uint16_t _move) : move(_move) {}

			Move(uint8_t from, uint8_t to, MoveType type) {
				move = from;
				move |= uint16_t(to) << 6;
				move |= uint16_t(type) << 12;
			}


			uint8_t from() const { return  move & 0b0000000000111111; }
			uint8_t to()   const { return  (move & 0b0000111111000000) >> 6; }
			MoveType type() const { return MoveType((move & 0b1111000000000000) >> 12); }

			BoardPiece who(const Board& brd) const
			{
				const uint64_t cell = (1ull << from()); 
				if constexpr (white) {
					if (brd.WPawn & cell) return BoardPiece::Pawn;
					if (brd.WKnight & cell) return BoardPiece::Knight;
					if (brd.WBishop & cell) return BoardPiece::Bishop;
					if (brd.WRook & cell) return BoardPiece::Rook;
					if (brd.WQueen & cell) return BoardPiece::Queen;
					if (brd.WKing & cell) return BoardPiece::King;
				}
				else {
					if (brd.BPawn & cell) return BoardPiece::Pawn;
					if (brd.BKnight & cell) return BoardPiece::Knight;
					if (brd.BBishop & cell) return BoardPiece::Bishop;
					if (brd.BRook & cell) return BoardPiece::Rook;
					if (brd.BQueen & cell) return BoardPiece::Queen;
					if (brd.BKing & cell) return BoardPiece::King;
				}

				return BoardPiece::None;
			}

			BoardPiece captured(const Board& brd) const
			{
				if (type() == MoveType::PawnEnpassantTake) return BoardPiece::Pawn;

				const uint64_t cell = (1ull << to()); 
				if constexpr (white) {
					if (brd.BPawn & cell) return BoardPiece::Pawn;
					if (brd.BKnight & cell) return BoardPiece::Knight;
					if (brd.BBishop & cell) return BoardPiece::Bishop;
					if (brd.BRook & cell) return BoardPiece::Rook;
					if (brd.BQueen & cell) return BoardPiece::Queen;
				}
				else {
					if (brd.WPawn & cell) return BoardPiece::Pawn;
					if (brd.WKnight & cell) return BoardPiece::Knight;
					if (brd.WBishop & cell) return BoardPiece::Bishop;
					if (brd.WRook & cell) return BoardPiece::Rook;
					if (brd.WQueen & cell) return BoardPiece::Queen;
				}

				return BoardPiece::None;
			}

			bool isQueenPromote() const
			{
				switch (type())
				{
				case MoveType::QueenMovePromote:
					return true;
				default:
					break;
				}

				return false;
			}

			Board play(const Board& brd) const
			{
				switch (type())
				{
				case MoveType::KingMove:
					return Board::PieceMove<BoardPiece::King, white>(brd, brd.status.KingMove(), 1ull << from(), 1ull << to());
				case MoveType::KingCastleLeft:
					return Board::MoveCastle<white>(brd, brd.status.KingMove(), (1ull << from()) | (1ull << to()), brd.status.Castle_RookswitchL());
				case MoveType::KingCastleRight:
					return Board::MoveCastle<white>(brd, brd.status.KingMove(), (1ull << from()) | (1ull << to()), brd.status.Castle_RookswitchR());
				case MoveType::PawnMove:
					return Board::PieceMove<BoardPiece::Pawn, white, false>(brd, brd.status.SilentMove(), 1ull << from(), 1ull << to());
				case MoveType::PawnAtk:
					return Board::PieceMove<BoardPiece::Pawn, white, true>(brd, brd.status.SilentMove(), 1ull << from(), 1ull << to());
				case MoveType::PawnEnpassantTake:
					return Board::MoveEP<white>(brd, brd.status.SilentMove(), 1ull << from(), 1ull << to());
				case MoveType::PawnPush:
					return Board::PieceMove<BoardPiece::Pawn, white, false>(brd, brd.status.PawnPush(1ull << to()), 1ull << from(), 1ull << to());
				case MoveType::KnightMove:
					return Board::PieceMove<BoardPiece::Knight, white>(brd, brd.status.SilentMove(), 1ull << from(), 1ull << to());
				case MoveType::BishopMove:
					return Board::PieceMove<BoardPiece::Bishop, white>(brd, brd.status.SilentMove(), 1ull << Move<white>::from(), 1ull << Move<white>::to());
				case MoveType::RookMove:
				{
					BoardStatus newStatus = brd.status.SilentMove();

					if (brd.status.CanCastle()) {
						if (brd.status.IsLeftRook(1ull << from()))
						{
							newStatus = brd.status.RookMove_Left();
						}
						else if (brd.status.IsRightRook(1ull << from())) {
							newStatus = brd.status.RookMove_Right();
						}
					}

					return Board::PieceMove<BoardPiece::Rook, white>(brd, newStatus, 1ull << from(), 1ull << to());
				}
				case MoveType::QueenMove:
					return Board::PieceMove<BoardPiece::Queen, white>(brd, brd.status.SilentMove(), 1ull << from(), 1ull << to());
				case MoveType::KnightMovePromote:
					return Board::PieceMovePromote<BoardPiece::Knight, white>(brd, brd.status.SilentMove(), 1ull << from(), 1ull << to());
				case MoveType::BishopMovePromote:
					return Board::PieceMovePromote<BoardPiece::Bishop, white>(brd, brd.status.SilentMove(), 1ull << from(), 1ull << to());
				case MoveType::RookMovePromote:
					return Board::PieceMovePromote<BoardPiece::Rook, white>(brd, brd.status.SilentMove(), 1ull << from(), 1ull << to());
				case MoveType::QueenMovePromote:
					return Board::PieceMovePromote<BoardPiece::Queen, white>(brd, brd.status.SilentMove(), 1ull << from(), 1ull << to());
				default:
					break;
				}

				assert(false);
				return Board::Null();
			}
		};

		static std::string moveStr(uint8_t from, uint8_t to) {
			std::string result = SQSTR[from];
			result += SQSTR[to];
			return result;
		}

		static std::string moveStr(uint64_t from, uint64_t to) {
			return moveStr(SquareOf(from), SquareOf(to));
		}

		static std::string moveStr(uint16_t move) {
			const uint8_t from = (move & 0b0000000000111111);
			const uint8_t to = ((move & 0b0000111111000000) >> 6);
			const MoveType type = MoveType((move & 0b1111000000000000) >> 12);
			std::string result = moveStr(from, to);

			switch (type)
			{
			case MoveType::KnightMovePromote:
				result += "n";
				break;
			case MoveType::BishopMovePromote:
				result += "b";
				break;
			case MoveType::RookMovePromote:
				result += "r";
				break;
			case MoveType::QueenMovePromote:
				result += "q";
				break;
			default:
				break;
			}

			return result;
		}

		static bool moveFromStr(const std::string& strMove, uint8_t& from, uint8_t& to, int8_t& promoteType) {// TODO (dummy impl)

			if (strMove.size() < 4) return false;

			from = 0ull;
			to = 0ull;
			promoteType = -1;
			std::string fromStr;
			fromStr.resize(2);
			fromStr[0] = strMove[0];
			fromStr[1] = strMove[1];

			for (uint8_t i = 0; i < 64; i++) {
				if (fromStr == SQSTR[i]) {
					from = i;
					break;
				}
			}

			std::string toStr;
			toStr.resize(2);
			toStr[0] = strMove[2];
			toStr[1] = strMove[3];

			for (uint8_t i = 0; i < 64; i++) {
				if (toStr == SQSTR[i]) {
					to = i;
					break;
				}
			}

			if (strMove.size() == 5) {
				if (strMove.back() == 'N' || strMove.back() == 'n') {
					promoteType = int8_t(MoveType::KnightMovePromote);
				}
				else if (strMove.back() == 'B' || strMove.back() == 'b') {
					promoteType = int8_t(MoveType::BishopMovePromote);
				}
				else if (strMove.back() == 'R' || strMove.back() == 'r') {
					promoteType = int8_t(MoveType::RookMovePromote);
				}
				else if (strMove.back() == 'Q' || strMove.back() == 'q') {
					promoteType = int8_t(MoveType::QueenMovePromote);
				}
			}

			return from != 0ull && to != 0ull && from != to;
		}

		template<BoardPiece piece, bool IsWhite>
		static Board PieceMovePromote(const Board& existing, BoardStatus newStatus, uint64_t from, uint64_t to)
		{
			const uint64_t rem = ~to;
			const uint64_t bp = existing.BPawn;
			const uint64_t bn = existing.BKnight;
			const uint64_t bb = existing.BBishop;
			const uint64_t br = existing.BRook;
			const uint64_t bq = existing.BQueen;
			const uint64_t bk = existing.BKing;

			const uint64_t wp = existing.WPawn;
			const uint64_t wn = existing.WKnight;
			const uint64_t wb = existing.WBishop;
			const uint64_t wr = existing.WRook;
			const uint64_t wq = existing.WQueen;
			const uint64_t wk = existing.WKing;

			if constexpr (IsWhite) {
				if constexpr (BoardPiece::Queen == piece)  return Board(bp & rem, bn & rem, bb & rem, br & rem, bq & rem, bk, wp ^ from, wn, wb, wr, wq ^ to, wk, newStatus);
				if constexpr (BoardPiece::Rook == piece)   return Board(bp & rem, bn & rem, bb & rem, br & rem, bq & rem, bk, wp ^ from, wn, wb, wr ^ to, wq, wk, newStatus);
				if constexpr (BoardPiece::Bishop == piece) return Board(bp & rem, bn & rem, bb & rem, br & rem, bq & rem, bk, wp ^ from, wn, wb ^ to, wr, wq, wk, newStatus);
				if constexpr (BoardPiece::Knight == piece) return Board(bp & rem, bn & rem, bb & rem, br & rem, bq & rem, bk, wp ^ from, wn ^ to, wb, wr, wq, wk, newStatus);
			}
			else {
				if constexpr (BoardPiece::Queen == piece)  return Board(bp ^ from, bn, bb, br, bq ^ to, bk, wp & rem, wn & rem, wb & rem, wr & rem, wq & rem, wk, newStatus);
				if constexpr (BoardPiece::Rook == piece)   return Board(bp ^ from, bn, bb, br ^ to, bq, bk, wp & rem, wn & rem, wb & rem, wr & rem, wq & rem, wk, newStatus);
				if constexpr (BoardPiece::Bishop == piece) return Board(bp ^ from, bn, bb ^ to, br, bq, bk, wp & rem, wn & rem, wb & rem, wr & rem, wq & rem, wk, newStatus);
				if constexpr (BoardPiece::Knight == piece) return Board(bp ^ from, bn ^ to, bb, br, bq, bk, wp & rem, wn & rem, wb & rem, wr & rem, wq & rem, wk, newStatus);
			}
		}

		//Todo: elegant not code duplication for Castling
		template<bool IsWhite>
		static Board MoveCastle(const Board& existing, BoardStatus newStatus, uint64_t kingswitch, uint64_t rookswitch)
		{
			const uint64_t bp = existing.BPawn;
			const uint64_t bn = existing.BKnight;
			const uint64_t bb = existing.BBishop;
			const uint64_t br = existing.BRook;
			const uint64_t bq = existing.BQueen;
			const uint64_t bk = existing.BKing;

			const uint64_t wp = existing.WPawn;
			const uint64_t wn = existing.WKnight;
			const uint64_t wb = existing.WBishop;
			const uint64_t wr = existing.WRook;
			const uint64_t wq = existing.WQueen;
			const uint64_t wk = existing.WKing;

			if constexpr (IsWhite) {
				return Board(bp, bn, bb, br, bq, bk, wp, wn, wb, wr ^ rookswitch, wq, wk ^ kingswitch, newStatus);
			}
			else {
				return Board(bp, bn, bb, br ^ rookswitch, bq, bk ^ kingswitch, wp, wn, wb, wr, wq, wk, newStatus);
			}
		}

		//Todo: elegant not code duplication for EP taking. Where to and rem are different squares
		template<bool IsWhite>
		static Board MoveEP(const Board& existing, BoardStatus newStatus, uint64_t from, uint64_t to)
		{
			const uint64_t enemy = IsWhite ? to >> 8 : to << 8;


			const uint64_t rem = ~enemy;
			const uint64_t bp = existing.BPawn;
			const uint64_t bn = existing.BKnight;
			const uint64_t bb = existing.BBishop;
			const uint64_t br = existing.BRook;
			const uint64_t bq = existing.BQueen;
			const uint64_t bk = existing.BKing;

			const uint64_t wp = existing.WPawn;
			const uint64_t wn = existing.WKnight;
			const uint64_t wb = existing.WBishop;
			const uint64_t wr = existing.WRook;
			const uint64_t wq = existing.WQueen;
			const uint64_t wk = existing.WKing;
			const uint64_t mov = from | to;


			if constexpr (IsWhite) {
				return Board(bp & rem, bn, bb, br, bq, bk, wp ^ mov, wn, wb, wr, wq, wk, newStatus);
			}
			else {
				return Board(bp ^ mov, bn, bb, br, bq, bk, wp & rem, wn, wb, wr, wq, wk, newStatus);
			}
		}

		template<BoardPiece piece, bool IsWhite>
		static Board PieceMove(const Board& existing, BoardStatus newStatus, uint64_t from, uint64_t to)
		{
			if (to & Enemy<IsWhite>(existing)) return PieceMove<piece, IsWhite, true>(existing, newStatus, from, to);
			else return PieceMove<piece, IsWhite, false>(existing, newStatus, from, to);
		}

		template<BoardPiece piece, bool IsWhite, bool IsTaking>
		static Board PieceMove(const Board& existing, BoardStatus newStatus, uint64_t from, uint64_t to)
		{
			const uint64_t bp = existing.BPawn;
			const uint64_t bn = existing.BKnight;
			const uint64_t bb = existing.BBishop;
			const uint64_t br = existing.BRook;
			const uint64_t bq = existing.BQueen;
			const uint64_t bk = existing.BKing;

			const uint64_t wp = existing.WPawn;
			const uint64_t wn = existing.WKnight;
			const uint64_t wb = existing.WBishop;
			const uint64_t wr = existing.WRook;
			const uint64_t wq = existing.WQueen;
			const uint64_t wk = existing.WKing;

			const uint64_t mov = from | to;

			if constexpr (IsTaking)
			{
				const uint64_t rem = ~to;
				if constexpr (IsWhite) {
					assert((bk & mov) == 0 && "Taking Black King is not legal!");
					assert((to & existing.White()) == 0 && "Cannot move to square of same white color!");
					if constexpr (BoardPiece::Pawn == piece)    return Board(bp & rem, bn & rem, bb & rem, br & rem, bq & rem, bk, wp ^ mov, wn, wb, wr, wq, wk, newStatus);
					if constexpr (BoardPiece::Knight == piece)  return Board(bp & rem, bn & rem, bb & rem, br & rem, bq & rem, bk, wp, wn ^ mov, wb, wr, wq, wk, newStatus);
					if constexpr (BoardPiece::Bishop == piece)  return Board(bp & rem, bn & rem, bb & rem, br & rem, bq & rem, bk, wp, wn, wb ^ mov, wr, wq, wk, newStatus);
					if constexpr (BoardPiece::Rook == piece)    return Board(bp & rem, bn & rem, bb & rem, br & rem, bq & rem, bk, wp, wn, wb, wr ^ mov, wq, wk, newStatus);
					if constexpr (BoardPiece::Queen == piece)   return Board(bp & rem, bn & rem, bb & rem, br & rem, bq & rem, bk, wp, wn, wb, wr, wq ^ mov, wk, newStatus);
					if constexpr (BoardPiece::King == piece)    return Board(bp & rem, bn & rem, bb & rem, br & rem, bq & rem, bk, wp, wn, wb, wr, wq, wk ^ mov, newStatus);
				}
				else {
					assert((wk & mov) == 0 && "Taking White King is not legal!");
					assert((to & existing.Black()) == 0 && "Cannot move to square of same black color!");
					if constexpr (BoardPiece::Pawn == piece)    return Board(bp ^ mov, bn, bb, br, bq, bk, wp & rem, wn & rem, wb & rem, wr & rem, wq & rem, wk, newStatus);
					if constexpr (BoardPiece::Knight == piece)  return Board(bp, bn ^ mov, bb, br, bq, bk, wp & rem, wn & rem, wb & rem, wr & rem, wq & rem, wk, newStatus);
					if constexpr (BoardPiece::Bishop == piece)  return Board(bp, bn, bb ^ mov, br, bq, bk, wp & rem, wn & rem, wb & rem, wr & rem, wq & rem, wk, newStatus);
					if constexpr (BoardPiece::Rook == piece)    return Board(bp, bn, bb, br ^ mov, bq, bk, wp & rem, wn & rem, wb & rem, wr & rem, wq & rem, wk, newStatus);
					if constexpr (BoardPiece::Queen == piece)   return Board(bp, bn, bb, br, bq ^ mov, bk, wp & rem, wn & rem, wb & rem, wr & rem, wq & rem, wk, newStatus);
					if constexpr (BoardPiece::King == piece)    return Board(bp, bn, bb, br, bq, bk ^ mov, wp & rem, wn & rem, wb & rem, wr & rem, wq & rem, wk, newStatus);
				}
			}
			else {
				if constexpr (IsWhite) {
					assert((bk & mov) == 0 && "Taking Black King is not legal!");
					assert((to & existing.White()) == 0 && "Cannot move to square of same white color!");
					if constexpr (BoardPiece::Pawn == piece)    return Board(bp, bn, bb, br, bq, bk, wp ^ mov, wn, wb, wr, wq, wk, newStatus);
					if constexpr (BoardPiece::Knight == piece)  return Board(bp, bn, bb, br, bq, bk, wp, wn ^ mov, wb, wr, wq, wk, newStatus);
					if constexpr (BoardPiece::Bishop == piece)  return Board(bp, bn, bb, br, bq, bk, wp, wn, wb ^ mov, wr, wq, wk, newStatus);
					if constexpr (BoardPiece::Rook == piece)    return Board(bp, bn, bb, br, bq, bk, wp, wn, wb, wr ^ mov, wq, wk, newStatus);
					if constexpr (BoardPiece::Queen == piece)   return Board(bp, bn, bb, br, bq, bk, wp, wn, wb, wr, wq ^ mov, wk, newStatus);
					if constexpr (BoardPiece::King == piece)    return Board(bp, bn, bb, br, bq, bk, wp, wn, wb, wr, wq, wk ^ mov, newStatus);
				}
				else {
					assert((wk & mov) == 0 && "Taking White King is not legal!");
					assert((to & existing.Black()) == 0 && "Cannot move to square of same black color!");
					if constexpr (BoardPiece::Pawn == piece)    return Board(bp ^ mov, bn, bb, br, bq, bk, wp, wn, wb, wr, wq, wk, newStatus);
					if constexpr (BoardPiece::Knight == piece)  return Board(bp, bn ^ mov, bb, br, bq, bk, wp, wn, wb, wr, wq, wk, newStatus);
					if constexpr (BoardPiece::Bishop == piece)  return Board(bp, bn, bb ^ mov, br, bq, bk, wp, wn, wb, wr, wq, wk, newStatus);
					if constexpr (BoardPiece::Rook == piece)    return Board(bp, bn, bb, br ^ mov, bq, bk, wp, wn, wb, wr, wq, wk, newStatus);
					if constexpr (BoardPiece::Queen == piece)   return Board(bp, bn, bb, br, bq ^ mov, bk, wp, wn, wb, wr, wq, wk, newStatus);
					if constexpr (BoardPiece::King == piece)    return Board(bp, bn, bb, br, bq, bk ^ mov, wp, wn, wb, wr, wq, wk, newStatus);
				}
			}
		}

		Board SkipMove() const {
			return Board(BPawn, BKnight, BBishop, BRook, BQueen, BKing, WPawn, WKnight, WBishop, WRook, WQueen, WKing, status.SilentMove());
		}

		static std::string StartPositionFen() {
			return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
		}

		static Board StartPosition() {
			static Board sp("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
			return sp;
		}

		static Board Null() {
			static Board nullBoard;
			return nullBoard;
		}

		bool Empty(uint8_t col, uint8_t row) const {
			return ~Occ() & (1ull << (7 - col + 8 * row));
		}

		char PieceToChar(uint8_t col, uint8_t row) const {
			const uint64_t sq = (1ull << (7 - col + 8 * row));

			if (BPawn & sq) return 'p';
			if (BKnight & sq) return 'n';
			if (BBishop & sq) return 'b';
			if (BRook & sq) return 'r';
			if (BQueen & sq) return 'q';
			if (BKing & sq) return 'k';

			if (WPawn & sq) return 'P';
			if (WKnight & sq) return 'N';
			if (WBishop & sq) return 'B';
			if (WRook & sq) return 'R';
			if (WQueen & sq) return 'Q';
			if (WKing & sq) return 'K';

			return ' ';
		}

		std::string Fen() const {
			int emptyCnt;
			std::stringstream ss;

			for (int row = 7; row >= 0; --row)
			{
				for (int col = 0; col < 8; ++col)
				{
					for (emptyCnt = 0; col < 8 && Empty(col, row); ++col)
						++emptyCnt;

					if (emptyCnt)
						ss << emptyCnt;

					if (col < 8)
						ss << PieceToChar(col, row);
				}

				if (row > 0)
					ss << '/';
			}

			ss << (status.WhiteMove() ? " w " : " b ");

			bool castle = false;
			if (status.WCastleR() && (WRook & BoardStatus::WRookR)) {
				ss << 'K';
				castle = true;
			}

			if (status.WCastleL() && (WRook & BoardStatus::WRookL)) {
				ss << 'Q';
				castle = true;
			}

			if (status.BCastleR() && (BRook & BoardStatus::BRookR)) {
				ss << 'k';
				castle = true;
			}

			if (status.BCastleL() && (BRook & BoardStatus::BRookL)) {
				ss << 'q';
				castle = true;
			}

			if (!castle)
				ss << '-';

			if (status.EnPassantTarget()) {
				uint8_t sq = SquareOf(status.EnPassantTarget());
				if (status.WhiteMove()) sq += 8;
				else sq -= 8;

				ss << " " << SQSTR[sq] << " ";
			}
			else {
				ss << " - ";
			}

			return ss.str();
		}

		std::string Diagram(bool blackAtTop = true, bool includeFen = true, bool includeZobristKey = true) const {
			std::stringstream result;

			for (int y = 0; y < 8; y++) {
				int rankIndex = blackAtTop ? 7 - y : y;
				result << "+---+---+---+---+---+---+---+---+\n";

				for (int x = 0; x < 8; x++) {
					int fileIndex = blackAtTop ? x : 7 - x;

					result << "| " <<  PieceToChar(fileIndex, rankIndex) << " ";

					if (x == 7) {
						result << "| " << rankIndex + 1 << "\n";
					}
				}
			}

			result << "+---+---+---+---+---+---+---+---+\n";
			std::string fileNames = "  a   b   c   d   e   f   g   h  ";
			std::string fileNamesRev = "  h   g   f   e   d   c   b   a  ";
			result << (blackAtTop ? fileNames : fileNamesRev) << "\n\n";

			if (includeFen) {
				result << "Fen  : " << Fen() << "\n";
			}
			if (includeZobristKey) {
				result << "Hash : " << Hash << "\n";
			}

			return result.str();
		}
	};

	struct BoardHash {
		std::size_t operator()(const Board& b) const {
			return b.Hash;
		}
	};
}
