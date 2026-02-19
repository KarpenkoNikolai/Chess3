#pragma once

#include <array>
#include <vector>
#include <atomic>

#include <../Gigantua/ChessBase.hpp>

namespace Search {


	struct GameTree {
		static constexpr uint8_t BucketSize = 32;
		const size_t HashTableSize;

		struct Edge {
		private:
			static constexpr float Smoothing = 0.01f;
			uint16_t move = 0;
			uint32_t entries = 0;
			float sugar = 0;
			float toxin = 0;

		public:
			Edge() {}

			void Reset(uint16_t m) {
				move = m;
				entries = 0;
				sugar = 0;
				toxin = 0;
			}

			uint16_t Move() const { return move; }
			uint32_t Entries() const { return entries; }
			void ResetEntries(uint32_t e) { entries = e; }

			template<bool white>
			void AddSugar(float s) {
				if constexpr (white) sugar += s * 0.001f;
				else toxin += s * 0.001f;
			}

			void AddEntries(float cost) {
				entries++;
			}
			void MergeEntries(uint32_t e) { entries += e; }

			template <bool white>
			float getProbability() const
			{
				if (entries == 0) return 40000.0f;

				const auto s = (sugar + Smoothing);
				const auto t = (toxin + Smoothing);

				if constexpr (white) {
					const auto d = (s) / (entries);
					return d;
				}
				else {
					const auto d = (t) / (entries);
					return d;
				}
			}
		};

		struct EdgeList {
		private:
			std::vector<Edge> m_data;
			size_t m_dataSize = 0;

		public:
			EdgeList() {
				m_data.resize(64);
			}

			void Set(const uint16_t* moves, const uint8_t* index, uint8_t size) {
				if (size > 64) {
					size = 64;
				}
				m_dataSize = size;
				for (uint8_t i = 0; i < size; i++) m_data[i].Reset(moves[index[i]]);
			}

			size_t size() const { return m_dataSize; }
			const Edge* data() const { return m_data.data(); }

			Edge& operator[] (size_t i) { return m_data[i]; }
			const Edge& operator[] (size_t i) const { return m_data[i]; }
		};

		struct NodePtr;
		struct ConstNodePtr;

		struct Node {
			uint64_t time = 0;
			Gigantua::Board board;
			EdgeList edges;

		private:
			friend struct NodePtr;
			friend struct ConstNodePtr;
			friend struct GameTree;
			mutable std::atomic<uint8_t> m_locked = 0;
		};

		struct NodePtr
		{
			NodePtr(Node* ptr = nullptr) : m_ptr(ptr) {
			}

			~NodePtr() {
				Unlock();
			}

			void Unlock() {
				if (m_ptr) m_ptr->m_locked &= 0b11111110;
			}

			bool IsNull() const { return m_ptr == nullptr; }

			Node* operator->() const {
				return m_ptr;
			}

		private:
			Node* m_ptr;
		};

		struct ConstNodePtr
		{
			ConstNodePtr(const Node* ptr = nullptr) : m_ptr(ptr) {
			}

			~ConstNodePtr() {
				Unlock();
			}

			void Unlock() {
				if (m_ptr) m_ptr->m_locked &= 0b11111110;
			}

			bool IsNull() const { return m_ptr == nullptr; }

			const Node* operator->() const {
				return m_ptr;
			}

		private:
			const Node* m_ptr;
		};

		typedef std::array<Node, BucketSize> Bucket;
		typedef std::vector<Bucket> HashTable;

		HashTable hashTable;
		mutable uint64_t time = 1;
		const size_t Size;

		GameTree(size_t size) : HashTableSize(size / BucketSize), hashTable(HashTableSize), Size(size) {
		}

		NodePtr Get(const Gigantua::Board& brd) {
			Bucket& bucket = hashTable[brd.Hash % HashTableSize];

			for (uint8_t i = 0; i < BucketSize; i++) {
				if (bucket[i].m_locked & 0b00000010)
					continue;

				bucket[i].m_locked |= 0b00000001;
				if (bucket[i].board == brd) {
					bucket[i].time = time;
					time++;
					return NodePtr(&bucket[i]);
				}
				bucket[i].m_locked &= 0b11111110;
			}

			return NodePtr();
		}

		ConstNodePtr Get(const Gigantua::Board& brd) const {
			const Bucket& bucket = hashTable[brd.Hash % HashTableSize];

			for (uint8_t i = 0; i < BucketSize; i++) {
				if (bucket[i].m_locked & 0b00000010)
					continue;

				bucket[i].m_locked |= 0b00000001;
				if (bucket[i].board == brd) {
					return ConstNodePtr(&bucket[i]);
				}
				bucket[i].m_locked &= 0b11111110;
			}

			return ConstNodePtr();
		}

		NodePtr Put(const Gigantua::Board& brd, const uint16_t* moves, const uint8_t* index, uint8_t size) {
			Bucket& bucket = hashTable[brd.Hash % HashTableSize];

			uint64_t minTime = std::numeric_limits<uint64_t>::max();
			uint8_t minIndex = 0;

			for (uint8_t i = 0; i < BucketSize; i++) {
				if (bucket[i].time == 0ull) {
					minIndex = i;
					break;
				}

				if (bucket[i].time < minTime) {
					minTime = bucket[i].time;
					minIndex = i;
				}
			}

			uint8_t expected = 0;
			if (!bucket[minIndex].m_locked.compare_exchange_strong(expected, 0b00000001,
				std::memory_order_acquire,
				std::memory_order_relaxed)) {
				return NodePtr(); // failed to acquire lock
			}

			bucket[minIndex].m_locked |= 0b00000010;
			bucket[minIndex].board = brd;
			bucket[minIndex].time = time;
			time++;
			bucket[minIndex].edges.Set(moves, index, size);
			bucket[minIndex].m_locked &= 0b11111101;

			bucket[minIndex].m_locked |= 0b00000001;
			return NodePtr(&bucket[minIndex]);
		}
	};

}

