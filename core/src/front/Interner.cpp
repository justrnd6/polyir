// The interner (Ast.hpp): every name in a program is stored once and stood for by a small number, its Symbol, so comparing two names
// is comparing two integers. The table is a hash table with open addressing: a name's hash picks a slot, and a taken slot holding a
// different name sends the search on to the next slot. The table doubles when it is half full.
#include "front/Ast.hpp"

namespace polyir::front {

  namespace {

    constexpr uint32_t kEmptySlot = kNoIndex;
    constexpr size_t kFirstTableSize = 256;  // the slots of a new table: a power of two, so a hash is masked into it

    // FNV-1a, a standard simple string hash: start from its offset basis, then mix in each byte with xor and a multiply by its prime.
    constexpr uint32_t kFnvOffsetBasis = 2166136261u;
    constexpr uint32_t kFnvPrime = 16777619u;

    uint32_t hashOf(StringView text) {
      uint32_t hash = kFnvOffsetBasis;

      for (char c : text) {
        hash = (hash ^ static_cast<unsigned char>(c)) * kFnvPrime;
      }

      return hash;
    }

    // Symbol 0 is the empty string. A fresh (all-empty) interner gets its table and that symbol the first time a name is interned,
    // so `Interner names;` is as good as MakeInterner().
    void prepare(Interner& names) {
      if (!names.slots.empty()) {
        return;
      }

      names.slots.assign(kFirstTableSize, kEmptySlot);
      Intern(names, "");
    }

    // The slot holding `text`, or the empty slot where it belongs (open addressing, linear probing).
    uint32_t slotOf(const Interner& names, StringView text) {
      const uint32_t mask = static_cast<uint32_t>(names.slots.size() - 1);
      uint32_t slot = hashOf(text) & mask;

      while (names.slots[slot] != kEmptySlot && names.texts[names.slots[slot]] != text) {
        slot = (slot + 1) & mask;
      }

      return slot;
    }

    // Keeps a copy of `text` and puts its new symbol in `slot`.
    uint32_t storeText(Interner& names, uint32_t slot, StringView text) {
      names.storage.emplace_back(text);
      names.texts.push_back(names.storage.back());
      names.slots[slot] = static_cast<uint32_t>(names.texts.size() - 1);

      return names.slots[slot];
    }

    // A table twice the size, with every symbol put back in the slot its hash now picks.
    void grow(Interner& names) {
      names.slots.assign(names.slots.size() * 2, kEmptySlot);

      for (uint32_t id = 0; id < names.texts.size(); id++) {
        names.slots[slotOf(names, names.texts[id])] = id;
      }
    }

  }  // namespace

  Interner MakeInterner() {
    Interner names;
    prepare(names);

    return names;
  }

  Symbol Intern(Interner& names, StringView text) {
    prepare(names);
    const uint32_t slot = slotOf(names, text);

    if (names.slots[slot] != kEmptySlot) {
      return Symbol{names.slots[slot]};
    }

    const uint32_t id = storeText(names, slot, text);

    if (names.texts.size() * 2 > names.slots.size()) {
      grow(names);
    }

    return Symbol{id};
  }

  Interner& GlobalInterner() {
    static Interner instance = MakeInterner();

    return instance;
  }

}  // namespace polyir::front
