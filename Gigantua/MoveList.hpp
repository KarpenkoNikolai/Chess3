#pragma once

#include "ChessBase.hpp"
#include "MoveGen.hpp"

namespace Gigantua{

	namespace MoveList {

		template<class TImpl, bool white>
		class MoveCollectorBase
		{
		public:
			void Collect(const Board::Move<white>& move) const
			{
				static_cast<const TImpl*>(this)->CollectImpl(move);
			}

			void KingMove(uint8_t from, uint8_t to) const
			{
				Collect(Board::Move<white>(from, to, MoveType::KingMove));
			}

			void KingCastleLeft() const
			{
				constexpr uint8_t kingpos = white ? 3 : 59;
				constexpr uint8_t kingto = kingpos + 2;
				Collect(Board::Move<white>(kingpos, kingto, MoveType::KingCastleLeft));
			}

			void KingCastleRight() const
			{
				constexpr uint8_t kingpos = white ? 3 : 59;
				constexpr uint8_t kingto = kingpos - 2;
				Collect(Board::Move<white>(kingpos, kingto, MoveType::KingCastleRight));
			}

			void PawnMove(uint8_t from, uint8_t to) const
			{
				Collect(Board::Move<white>(from, to, MoveType::PawnMove));
			}

			void PawnAtk(uint8_t from, uint8_t to) const
			{
				Collect(Board::Move<white>(from, to, MoveType::PawnAtk));
			}

			void PawnEnpassantTake(uint8_t from, uint8_t to) const
			{
				Collect(Board::Move<white>(from, to, MoveType::PawnEnpassantTake));
			}

			void PawnPush(uint8_t from, uint8_t to) const
			{
				Collect(Board::Move<white>(from, to, MoveType::PawnPush));
			}

			void PawnPromote(uint8_t from, uint8_t to) const
			{
				Collect(Board::Move<white>(from, to, MoveType::QueenMovePromote));
				Collect(Board::Move<white>(from, to, MoveType::KnightMovePromote));
				Collect(Board::Move<white>(from, to, MoveType::BishopMovePromote));
				Collect(Board::Move<white>(from, to, MoveType::RookMovePromote));
			}

			void KnightMove(uint8_t from, uint8_t to) const
			{
				Collect(Board::Move<white>(from, to, MoveType::KnightMove));
			}

			void BishopMove(uint8_t from, uint8_t to) const
			{
				Collect(Board::Move<white>(from, to, MoveType::BishopMove));
			}

			void RookMove(uint8_t from, uint8_t to) const
			{
				Collect(Board::Move<white>(from, to, MoveType::RookMove));
			}

			void QueenMove(uint8_t from, uint8_t to) const
			{
				Collect(Board::Move<white>(from, to, MoveType::QueenMove));
			}
		};

		template<bool white>
		class MoveCollector : public Gigantua::MoveList::MoveCollectorBase<MoveCollector<white>, white>
		{
		public:
			mutable std::vector<Board::Move<white>> moves;
			MoveCollector() {
				moves.reserve(128);
			}

			void CollectImpl(const Board::Move<white>& move) const
			{
				moves.push_back(move);
			}
		};

		template<bool white>
		class MoveSizeCollector : public Gigantua::MoveList::MoveCollectorBase<MoveSizeCollector<white>, white>
		{
		public:
			mutable size_t moves = 0;

			void CollectImpl(const Board::Move<white>& /*move*/) const
			{
				moves++;
			}
		};

		//PinHVD1D2 |= Path from Enemy to excluding King + enemy. Has Seemap from king as input
		//Must have enemy slider AND own piece or - VERY special: can clear enemy enpassant pawn
		template<bool white>
		_ForceInline void RegisterPinD12(uint8_t king, uint8_t enemy, const Board& brd, uint64_t& epTarget, uint64_t& bishopPin)
		{
			const uint64_t Pinmask = ChessLookup::PinBetween[king * 64 + enemy];

			//Deep possible chess problem:
			//Enemy Enpassant Pawn gives check - Could be taken by EP Pawn - But is pinned and therefore not a valid EPTarget
			// https://lichess.org/editor?fen=6q1%2F8%2F8%2F3pP3%2F8%2F1K6%2F8%2F8+w+-+-+0+1
			//Can delete EP Status
			if (epTarget) {
				if (Pinmask & epTarget) epTarget = 0;
			}

			if (Pinmask & OwnColor<white>(brd)) {
				bishopPin |= Pinmask;
			}
		}

		//PinHVD1D2 |= Path from Enemy to excluding King + enemy. Has Seemap from king as input
		template<bool white>
		_ForceInline void RegisterPinHV(uint8_t king, uint8_t enemy, const Board& brd, uint64_t& rookPin)
		{
			const uint64_t Pinmask = ChessLookup::PinBetween[king * 64 + enemy];

			if (Pinmask & OwnColor<white>(brd)) {
				rookPin |= Pinmask;
			}
		}


		template<bool IsWhite>
		_Compiletime uint64_t EPRank()
		{
			if constexpr (IsWhite) return 0xFFull << 32;
			else return 0xFFull << 24;
		}

		template<bool white>
		_ForceInline void RegisterPinEP(uint8_t kingsquare, uint64_t king, uint64_t enemyRQ, const Board& brd, uint64_t& epTarget)
		{
			const uint64_t pawns = Pawns<white>(brd);
			//Special Horizontal1 https://lichess.org/editor?fen=8%2F8%2F8%2F1K1pP1q1%2F8%2F8%2F8%2F8+w+-+-+0+1
			//Special Horizontal2 https://lichess.org/editor?fen=8%2F8%2F8%2F1K1pP1q1%2F8%2F8%2F8%2F8+w+-+-+0+1

			//King is on EP rank and enemy HV walker is on same rank

			//Remove enemy EP and own EP Candidate from OCC and check if Horizontal path to enemy Slider is open
			//Quick check: We have king - Enemy Slider - Own Pawn - and enemy EP on the same rank!
			if ((EPRank<white>() & king) && (EPRank<white>() & enemyRQ) && (EPRank<white>() & pawns))
			{
				uint64_t EPLpawn = pawns & ((epTarget & Pawns_NotRight()) >> 1); //Pawn that can EPTake to the left
				uint64_t EPRpawn = pawns & ((epTarget & Pawns_NotLeft()) << 1);  //Pawn that can EPTake to the right

				if (EPLpawn) {
					uint64_t AfterEPocc = brd.Occ() & ~(epTarget | EPLpawn);
					if ((Lookup::Rook(kingsquare, AfterEPocc) & EPRank<white>()) & enemyRQ) epTarget = 0;
				}
				if (EPRpawn) {
					uint64_t AfterEPocc = brd.Occ() & ~(epTarget | EPRpawn);
					if ((Lookup::Rook(kingsquare, AfterEPocc) & EPRank<white>()) & enemyRQ) epTarget = 0;
				}
			}
		}

		//Checkmask |= Path from Enemy to including King + square behind king + enemy //Todo: make this unconditional again!
		_ForceInline void CheckBySlider(uint8_t kingsq, uint8_t enemysq, uint64_t& Kingban, uint64_t& Checkmask) {
			if (Checkmask == 0xffffffffffffffffull)
			{
				Checkmask = ChessLookup::PinBetween[kingsq * 64 + enemysq]; //Checks are only stopped between king and enemy including taking the enemy
			}
			else Checkmask = 0;
			Kingban |= ChessLookup::CheckBetween[kingsq * 64 + enemysq]; //King cannot go to square opposite to slider
		}


		template<bool white>
		_ForceInline uint64_t Refresh(const Board& brd, uint64_t& kingban, uint64_t& checkmask, uint64_t& epTarget, uint64_t& rookPin, uint64_t& bishopPin)
		{
			constexpr bool enemy = !white;
			const uint64_t king = King<white>(brd);
			const uint8_t kingsq = SquareOf(king);
			const uint64_t brdOcc = brd.Occ();


			//Pinned pieces + checks by sliders
			{
				rookPin = 0;
				bishopPin = 0;

				if (ChessLookup::RookMask[kingsq] & EnemyRookQueen<white>(brd))
				{
					uint64_t atkHV = Lookup::Rook(kingsq, brdOcc) & EnemyRookQueen<white>(brd);
					Bitloop(atkHV) {
						CheckBySlider(kingsq, SquareOf(atkHV), kingban, checkmask);
					}

					uint64_t pinnersHV = Lookup::Rook_Xray(kingsq, brdOcc) & EnemyRookQueen<white>(brd);
					Bitloop(pinnersHV)
					{
						RegisterPinHV<white>(kingsq, SquareOf(pinnersHV), brd, rookPin);
					}
				}
				if (ChessLookup::BishopMask[kingsq] & EnemyBishopQueen<white>(brd)) {
					uint64_t atkD12 = Lookup::Bishop(kingsq, brdOcc) & EnemyBishopQueen<white>(brd);
					Bitloop(atkD12) {
						CheckBySlider(kingsq, SquareOf(atkD12), kingban, checkmask);
					}

					uint64_t pinnersD12 = Lookup::Bishop_Xray(kingsq, brdOcc) & EnemyBishopQueen<white>(brd);
					Bitloop(pinnersD12)
					{
						RegisterPinD12<white>(kingsq, SquareOf(pinnersD12), brd, epTarget, bishopPin);
					}
				}

				if (epTarget)
				{
					RegisterPinEP<white>(kingsq, king, EnemyRookQueen<white>(brd), brd, epTarget);
				}
			}

			uint64_t king_atk = Lookup::King(kingsq) & EnemyOrEmpty<white>(brd) & ~kingban;
			if (king_atk == 0) return 0;

			//Calculate Enemy Knight - keep this first
			{
				uint64_t knights = Knights<enemy>(brd);
				Bitloop(knights) {
					kingban |= Lookup::Knight(SquareOf(knights));
				}
			}

			//Calculate Check from enemy pawns
			kingban |= (Pawn_AttackLeft<enemy>(Pawns<enemy>(brd) & Pawns_NotLeft()) | Pawn_AttackRight<enemy>(Pawns<enemy>(brd) & Pawns_NotRight()));


			//Calculate Enemy Bishop
			{
				uint64_t bishops = BishopQueen<enemy>(brd);
				Bitloop(bishops) {
					kingban |= Lookup::Bishop(SquareOf(bishops), brdOcc);
				}
			}

			//Calculate Enemy Rook
			{
				uint64_t rooks = RookQueen<enemy>(brd);
				Bitloop(rooks) {
					kingban |= Lookup::Rook(SquareOf(rooks), brdOcc);
				}
			}

			return king_atk & ~kingban;
		}

		//Also applies the checkmask
		template<bool IsWhite>
		_ForceInline void Pawn_PruneLeft(uint64_t& pawn, const uint64_t pinD1D2)
		{
			const uint64_t pinned = pawn & Pawn_InvertLeft<IsWhite>(pinD1D2 & Pawns_NotRight()); //You can go left and are pinned
			const uint64_t unpinned = pawn & ~pinD1D2;

			pawn = (pinned | unpinned); //You can go left and you and your targetsquare is allowed
		}

		template<bool IsWhite>
		_ForceInline void Pawn_PruneRight(uint64_t& pawn, const uint64_t pinD1D2)
		{
			const uint64_t pinned = pawn & Pawn_InvertRight<IsWhite>(pinD1D2 & Pawns_NotLeft()); //You can go right and are pinned
			const uint64_t unpinned = pawn & ~pinD1D2;

			pawn = (pinned | unpinned); //You can go right and you and your targetsquare is allowed
		}


		//THIS CHECKMASK: https://lichess.org/analysis/8/2p5/3p4/KP3k1r/2R1Pp2/6P1/8/8_b_-_e3_0_1
		template<bool IsWhite>
		_ForceInline void Pawn_PruneLeftEP(uint64_t& pawn, const uint64_t pinD1D2)
		{
			const uint64_t pinned = pawn & Pawn_InvertLeft<IsWhite>(pinD1D2 & Pawns_NotRight()); //You can go left and are pinned
			const uint64_t unpinned = pawn & ~pinD1D2;

			pawn = (pinned | unpinned);
		}

		template<bool IsWhite>
		_ForceInline void Pawn_PruneRightEP(uint64_t& pawn, const uint64_t pinD1D2)
		{
			const uint64_t pinned = pawn & Pawn_InvertRight<IsWhite>(pinD1D2 & Pawns_NotLeft()); //You can go right and are pinned
			const uint64_t unpinned = pawn & ~pinD1D2;

			pawn = (pinned | unpinned);
		}


		template<bool IsWhite>
		_ForceInline void Pawn_PruneMove(uint64_t& pawn, const uint64_t pinHV)
		{
			const uint64_t pinned = pawn & Pawn_Backward<IsWhite>(pinHV); //You can forward and are pinned by rook/queen in forward direction
			const uint64_t unpinned = pawn & ~pinHV;

			pawn = (pinned | unpinned); //You can go forward and you and your targetsquare is allowed
		}

		//This is needed in the case where the forward square is not allowed by the push is ok
		//Example: https://lichess.org/editor?fen=8%2F8%2F8%2F8%2F3K3q%2F8%2F5P2%2F8+w+-+-+0+1
		template<bool IsWhite>
		_ForceInline void Pawn_PruneMove2(uint64_t& pawn, const uint64_t pinHV)
		{
			const uint64_t pinned = pawn & Pawn_Backward2<IsWhite>(pinHV); //You can forward and are pinned by rook/queen in forward direction
			const uint64_t unpinned = pawn & ~pinHV;

			pawn = (pinned | unpinned); //You can go forward and you and your targetsquare is allowed
		}

		template<class TCollectImpl, bool white>
		_ForceInline void _enumerate(
			const Board& brd, uint64_t kingatk, const uint64_t kingban, const uint64_t checkmask, const uint64_t epTarget, const uint64_t rookPin, const uint64_t bishopPin, TCollectImpl& collector)
		{
			const bool noCheck = (checkmask == 0xffffffffffffffffull);

			//All outside variables need to be in local scope as the Callback will change everything on enumeration
			const uint64_t movableSquare = EnemyOrEmpty<white>(brd) & checkmask;
			const uint64_t brdOcc = brd.Occ();

			//Kingmoves
			{
				Bitloop(kingatk)
				{
					collector.KingMove(SquareOf(King<white>(brd)), SquareOf(kingatk));
				}

				//Castling
				if (brd.status.CanCastleLeft()) {
					if (noCheck && brd.status.CanCastleLeft(kingban, brdOcc, Rooks<white>(brd))) {
						collector.KingCastleLeft();
					}
				}
				if (brd.status.CanCastleRight()) {
					if (noCheck && brd.status.CanCastleRight(kingban, brdOcc, Rooks<white>(brd))) {
						collector.KingCastleRight();
					}
				}
			}

			{
				//Horizontal pinned pawns cannot do anything https://lichess.org/editor?fen=3r4%2F8%2F3P4%2F8%2F3K1P1r%2F8%2F8%2F8+w+-+-+0+1
				//Pawns may seem to be able to enpassant/promote but can still be pinned and inside a checkmask
				//Vertical pinned pawns cannot take, but move forward 
				//D12 pinned pawns can take, but never move forward

				const uint64_t pawnsLR = Pawns<white>(brd) & ~rookPin; //These pawns can walk L or R
				const uint64_t pawnsHV = Pawns<white>(brd) & ~bishopPin; //These pawns can walk Forward

				//These 4 are basic pawn moves
				uint64_t Lpawns = pawnsLR & Pawn_InvertLeft<white>(Enemy<white>(brd) & Pawns_NotRight() & checkmask);    //Pawns that can take left
				uint64_t Rpawns = pawnsLR & Pawn_InvertRight<white>(Enemy<white>(brd) & Pawns_NotLeft() & checkmask);     //Pawns that can take right
				uint64_t Fpawns = pawnsHV & Pawn_Backward<white>(Empty(brd));	                                          //Pawns that can go forward 
				uint64_t Ppawns = Fpawns & Pawns_FirstRank<white>() & Pawn_Backward2<white>(Empty(brd) & checkmask);      //Pawns that can push

				Fpawns &= Pawn_Backward<white>(checkmask); //checkmask moved here to use fpawn for faster Ppawn calc: Pawn on P2 can only push - not move - https://lichess.org/editor?fen=rnbpkbnr%2Fppp3pp%2F4pp2%2Fq7%2F8%2F3P4%2FPPP1PPPP%2FRNBQKBNR+w+KQkq+-+0+1

				//These 4 basic moves get pruned with pin information 
				Pawn_PruneLeft<white>(Lpawns, bishopPin);
				Pawn_PruneRight<white>(Rpawns, bishopPin);
				Pawn_PruneMove<white>(Fpawns, rookPin);
				Pawn_PruneMove2<white>(Ppawns, rookPin);

				//This is Enpassant
				if (epTarget) {
					//The eppawn must be an enemy since its only ever valid for a single move
					uint64_t EPLpawn = pawnsLR & Pawns_NotLeft() & ((epTarget & checkmask) >> 1); //Pawn that can EPTake to the left - overflow will not matter because 'Notleft'
					uint64_t EPRpawn = pawnsLR & Pawns_NotRight() & ((epTarget & checkmask) << 1);  //Pawn that can EPTake to the right - overflow will not matter because 'NotRight'

					//Special check for pinned EP Take - which is a very special move since even XRay does not see through the 2 pawns on a single rank
					// White will push you cannot EP take: https://lichess.org/editor?fen=8%2F7B%2F8%2F8%2F4p3%2F3k4%2F5P2%2F8+w+-+-+0+1
					// White will push you cannot EP take: https://lichess.org/editor?fen=8%2F8%2F8%2F8%2F1k2p2R%2F8%2F5P2%2F8+w+-+-+0+1

					if (EPLpawn | EPRpawn) //Todo: bench if slower or faster
					{
						Pawn_PruneLeftEP<white>(EPLpawn, bishopPin);
						Pawn_PruneRightEP<white>(EPRpawn, bishopPin);

						if (EPLpawn) collector.PawnEnpassantTake(SquareOf(EPLpawn), SquareOf(Pawn_AttackLeft<white>(EPLpawn)));
						if (EPRpawn) collector.PawnEnpassantTake(SquareOf(EPRpawn), SquareOf(Pawn_AttackRight<white>(EPRpawn)));
					}
				}

				//We have pawns that can move on last rank
				if ((Lpawns | Rpawns | Fpawns) & Pawns_LastRank<white>())
				{
					uint64_t Promote_Left = Lpawns & Pawns_LastRank<white>();
					uint64_t Promote_Right = Rpawns & Pawns_LastRank<white>();
					uint64_t Promote_Move = Fpawns & Pawns_LastRank<white>();

					uint64_t NoPromote_Left = Lpawns & ~Pawns_LastRank<white>();
					uint64_t NoPromote_Right = Rpawns & ~Pawns_LastRank<white>();
					uint64_t NoPromote_Move = Fpawns & ~Pawns_LastRank<white>();

					Bitloop(Promote_Left) { collector.PawnPromote(SquareOf(Promote_Left), SquareOf(Pawn_AttackLeft<white>(Promote_Left))); }
					Bitloop(Promote_Right) { collector.PawnPromote(SquareOf(Promote_Right), SquareOf(Pawn_AttackRight<white>(Promote_Right))); }
					Bitloop(Promote_Move) { collector.PawnPromote(SquareOf(Promote_Move), SquareOf(Pawn_Forward<white>(Promote_Move))); }
					Bitloop(NoPromote_Left) { collector.PawnAtk(SquareOf(NoPromote_Left), SquareOf(Pawn_AttackLeft<white>(NoPromote_Left))); }
					Bitloop(NoPromote_Right) { collector.PawnAtk(SquareOf(NoPromote_Right), SquareOf(Pawn_AttackRight<white>(NoPromote_Right))); }
					Bitloop(NoPromote_Move) { collector.PawnMove(SquareOf(NoPromote_Move), SquareOf(Pawn_Forward<white>(NoPromote_Move))); }
					Bitloop(Ppawns) { collector.PawnPush(SquareOf(Ppawns), SquareOf(Pawn_Forward2<white>(Ppawns))); }
				}
				else {
					Bitloop(Lpawns) { collector.PawnAtk(SquareOf(Lpawns), SquareOf(Pawn_AttackLeft<white>(Lpawns))); }
					Bitloop(Rpawns) { collector.PawnAtk(SquareOf(Rpawns), SquareOf(Pawn_AttackRight<white>(Rpawns))); }
					Bitloop(Fpawns) { collector.PawnMove(SquareOf(Fpawns), SquareOf(Pawn_Forward<white>(Fpawns))); }
					Bitloop(Ppawns) { collector.PawnPush(SquareOf(Ppawns), SquareOf(Pawn_Forward2<white>(Ppawns))); }
				}
			}

			//Knightmoves
			{
				uint64_t knights = Knights<white>(brd) & ~(rookPin | bishopPin); //A pinned knight cannot move
				Bitloop(knights) {
					const uint8_t sq = SquareOf(knights);
					uint64_t move = Lookup::Knight(sq) & movableSquare;

					Bitloop(move) { collector.KnightMove(sq, SquareOf(move)); }
				}
			}

			//Removing the pinmask outside is faster than having if inside loop
			//Handling rare pinned queens together bishop/rook is faster

			const uint64_t queens = Queens<white>(brd);
			//Bishopmoves
			{
				uint64_t bishops = Bishops<white>(brd) & ~rookPin; //Non pinned bishops OR diagonal pinned queens

				uint64_t bish_pinned = (bishops | queens) & bishopPin;
				uint64_t bish_nopin = bishops & ~bishopPin;
				Bitloop(bish_pinned) {
					const uint8_t sq = SquareOf(bish_pinned);
					uint64_t move = Lookup::Bishop(sq, brdOcc) & movableSquare & bishopPin; //A D12 pinned bishop can only move along D12 pinned axis

					if ((1ull << sq) & queens) { Bitloop(move) { collector.QueenMove(sq, SquareOf(move)); } }
					else { Bitloop(move) { collector.BishopMove(sq, SquareOf(move)); } }
				}
				Bitloop(bish_nopin) {
					const uint8_t sq = SquareOf(bish_nopin);
					uint64_t move = Lookup::Bishop(sq, brdOcc) & movableSquare;

					while (move) { const uint64_t to = PopBit(move); collector.BishopMove(sq, SquareOf(to)); }
				}
			}

			//Rookmoves
			{
				uint64_t rooks = Rooks<white>(brd) & ~bishopPin;

				uint64_t rook_pinned = (rooks | queens) & rookPin;
				uint64_t rook_nopin = rooks & ~rookPin;
				Bitloop(rook_pinned) {
					const uint8_t sq = SquareOf(rook_pinned);
					uint64_t move = Lookup::Rook(sq, brdOcc) & movableSquare & rookPin; //A HV pinned rook can only move along HV pinned axis

					if ((1ull << sq) & queens) { Bitloop(move) { collector.QueenMove(sq, SquareOf(move)); } }
					else { Bitloop(move) { collector.RookMove(sq, SquareOf(move)); } }
				}
				Bitloop(rook_nopin) {
					const uint8_t sq = SquareOf(rook_nopin);
					uint64_t move = Lookup::Rook(sq, brdOcc) & movableSquare; //A HV pinned rook can only move along HV pinned axis

					Bitloop(move) { collector.RookMove(sq, SquareOf(move)); }
				}
			}
			//Calculate Enemy Queen
			{
				uint64_t queens = Queens<white>(brd) & ~(rookPin | bishopPin);
				Bitloop(queens) {
					const uint8_t sq = SquareOf(queens);
					uint64_t move = Lookup::Queen(sq, brdOcc) & movableSquare;

					Bitloop(move) {collector.QueenMove(sq, SquareOf(move)); }
				}
			}
		}


		template<class TCollector, bool white>
		static void EnumerateMoves(TCollector& collector, const Board& brd)
		{
			constexpr bool enemy = !white;

			uint64_t checkmask = 0xffffffffffffffffull;

			//Calculate Check from enemy pawns
			{
				const uint64_t pl = Pawn_AttackLeft<enemy>(Pawns<enemy>(brd) & Pawns_NotLeft());
				const uint64_t pr = Pawn_AttackRight<enemy>(Pawns<enemy>(brd) & Pawns_NotRight());

				if (pl & King<white>(brd)) checkmask = Pawn_AttackRight<white>(King<white>(brd));
				else if (pr & King<white>(brd)) checkmask = Pawn_AttackLeft<white>(King<white>(brd));
			}

			//Calculate Check from enemy knights
			{
				const uint64_t knightcheck = Lookup::Knight(SquareOf(King<white>(brd))) & Knights<enemy>(brd);
				if (knightcheck) checkmask = knightcheck;
			}

			uint64_t kingban = Lookup::King(SquareOf(King<enemy>(brd)));
			uint64_t epTarget = brd.status.EnPassantTarget();
			uint64_t rookPin = 0ull;
			uint64_t bishopPin = 0ull;

			uint64_t kingatk = Refresh<white>(brd, kingban, checkmask, epTarget, rookPin, bishopPin);

			_enumerate<TCollector, white>(brd, kingatk, kingban, checkmask, epTarget, rookPin, bishopPin, collector);
		}

		template<bool white>
		static bool InCheck(const Board& brd)
		{
			constexpr bool enemy = !white;
			uint64_t checkmask = 0xffffffffffffffffull;

			const uint64_t king = King<white>(brd);
			const uint8_t kingsq = SquareOf(king);

			//Calculate Check from enemy pawns
			{
				const uint64_t pl = Pawn_AttackLeft<enemy>(Pawns<enemy>(brd) & Pawns_NotLeft());
				const uint64_t pr = Pawn_AttackRight<enemy>(Pawns<enemy>(brd) & Pawns_NotRight());

				if (pl & king) checkmask = Pawn_AttackRight<white>(king);
				else if (pr & king) checkmask = Pawn_AttackLeft<white>(king);
			}

			if (checkmask != 0xffffffffffffffffull) return true;

			//Calculate Check from enemy knights
			{
				const uint64_t knightcheck = Lookup::Knight(SquareOf(king)) & Knights<enemy>(brd);
				if (knightcheck) checkmask = knightcheck;
			}

			if (checkmask != 0xffffffffffffffffull) return true;

			//checks by sliders
			{
				const uint64_t brdOcc = brd.Occ();
				uint64_t kingban = Lookup::King(SquareOf(King<enemy>(brd)));

				if (ChessLookup::RookMask[kingsq] & EnemyRookQueen<white>(brd))
				{
					uint64_t atkHV = Lookup::Rook(kingsq, brdOcc) & EnemyRookQueen<white>(brd);
					Bitloop(atkHV) {
						CheckBySlider(kingsq, SquareOf(atkHV), kingban, checkmask);
						if (checkmask != 0xffffffffffffffffull) return true;
					}
				}

				if (ChessLookup::BishopMask[kingsq] & EnemyBishopQueen<white>(brd)) {
					uint64_t atkD12 = Lookup::Bishop(kingsq, brdOcc) & EnemyBishopQueen<white>(brd);
					Bitloop(atkD12) {
						CheckBySlider(kingsq, SquareOf(atkD12), kingban, checkmask);
						if (checkmask != 0xffffffffffffffffull) return true;
					}
				}
			}

			return false;
		}

		template<bool white>
		static bool QueenInCheck(const Board& brd)
		{
			constexpr bool enemy = !white;
			uint64_t checkmask = 0xffffffffffffffffull;

			const uint64_t king = Queens<white>(brd);// quenn is king
			if (!king) return false;
			const uint8_t kingsq = SquareOf(king);

			//Calculate Check from enemy pawns
			{
				const uint64_t pl = Pawn_AttackLeft<enemy>(Pawns<enemy>(brd) & Pawns_NotLeft());
				const uint64_t pr = Pawn_AttackRight<enemy>(Pawns<enemy>(brd) & Pawns_NotRight());

				if (pl & king) checkmask = Pawn_AttackRight<white>(king);
				else if (pr & king) checkmask = Pawn_AttackLeft<white>(king);
			}

			if (checkmask != 0xffffffffffffffffull) return true;

			//Calculate Check from enemy knights
			{
				const uint64_t knightcheck = Lookup::Knight(SquareOf(king)) & Knights<enemy>(brd);
				if (knightcheck) checkmask = knightcheck;
			}

			if (checkmask != 0xffffffffffffffffull) return true;

			return false;
		}

		template<bool white>
		static bool RookInCheck(const Board& brd)
		{
			constexpr bool enemy = !white;
			uint64_t checkmask = 0xffffffffffffffffull;

			uint64_t king = Rooks<white>(brd);// rook is king
			if (!king) return false;

			Bitloop(king) {
				const uint8_t kingsq = SquareOf(king);

				//Calculate Check from enemy pawns
				{
					const uint64_t pl = Pawn_AttackLeft<enemy>(Pawns<enemy>(brd) & Pawns_NotLeft());
					const uint64_t pr = Pawn_AttackRight<enemy>(Pawns<enemy>(brd) & Pawns_NotRight());

					if (pl & king) checkmask = Pawn_AttackRight<white>(king);
					else if (pr & king) checkmask = Pawn_AttackLeft<white>(king);
				}

				if (checkmask != 0xffffffffffffffffull) return true;

				//Calculate Check from enemy knights
				{
					const uint64_t knightcheck = Lookup::Knight(SquareOf(king)) & Knights<enemy>(brd);
					if (knightcheck) checkmask = knightcheck;
				}

				if (checkmask != 0xffffffffffffffffull) return true;
			}

			return false;
		}

		template<bool white>
		static bool BishopInCheck(const Board& brd)
		{
			constexpr bool enemy = !white;
			uint64_t checkmask = 0xffffffffffffffffull;

			uint64_t king = Bishops<white>(brd);// rook is king
			if (!king) return false;

			Bitloop(king) {
				const uint8_t kingsq = SquareOf(king);

				//Calculate Check from enemy pawns
				{
					const uint64_t pl = Pawn_AttackLeft<enemy>(Pawns<enemy>(brd) & Pawns_NotLeft());
					const uint64_t pr = Pawn_AttackRight<enemy>(Pawns<enemy>(brd) & Pawns_NotRight());

					if (pl & king) checkmask = Pawn_AttackRight<white>(king);
					else if (pr & king) checkmask = Pawn_AttackLeft<white>(king);
				}

				if (checkmask != 0xffffffffffffffffull) return true;

				//Calculate Check from enemy knights
				{
					const uint64_t knightcheck = Lookup::Knight(SquareOf(king)) & Knights<enemy>(brd);
					if (knightcheck) checkmask = knightcheck;
				}

				if (checkmask != 0xffffffffffffffffull) return true;
			}

			return false;
		}

		template<bool white>
		static bool KnightInCheck(const Board& brd)
		{
			constexpr bool enemy = !white;
			uint64_t checkmask = 0xffffffffffffffffull;

			uint64_t king = Knights<white>(brd);// rook is king
			if (!king) return false;
			
			Bitloop(king) {
				const uint8_t kingsq = SquareOf(king);

				//Calculate Check from enemy pawns
				{
					const uint64_t pl = Pawn_AttackLeft<enemy>(Pawns<enemy>(brd) & Pawns_NotLeft());
					const uint64_t pr = Pawn_AttackRight<enemy>(Pawns<enemy>(brd) & Pawns_NotRight());

					if (pl & king) checkmask = Pawn_AttackRight<white>(king);
					else if (pr & king) checkmask = Pawn_AttackLeft<white>(king);
				}

				if (checkmask != 0xffffffffffffffffull) return true;
			}

			return false;
		}

		template<bool white>
		static size_t MovesCount(const Board& brd)
		{
			MoveSizeCollector<white> collector;
			EnumerateMoves<MoveSizeCollector<white>, white>(collector, brd);
			return collector.moves;
		}


		template<bool white>
		static std::vector<Board::Move<white>> MoveList(const Board& brd)
		{
			MoveCollector<white> collector;
			EnumerateMoves<MoveCollector<white>, white>(collector, brd);
			return collector.moves;
		}

	}//namespace MoveList
}//namespace Gigantua


