// The first-token sets of the grammar's rules and the "expected ..." lists the Nina parser prints for them (copied from the generated parser of
// lang/src/Polyir.intent: the lists are sorted by token name, with quotes on keywords and punctuation).
#pragma once

#include "front/Lexer.hpp"

namespace polyir::front {

  // The parser names token kinds on almost every line, so it uses this short name for them: `K::LBRACE` is `TokenKind::LBRACE`.
  using K = TokenKind;

  // What can start each rule: the parser tests the current token against these before it commits to a rule.
  constexpr KindSet kTopLevelStart = MakeKindSet({K::BRIDGE, K::CORO, K::EXTERN, K::FUNC, K::GLOBAL, K::IMPORT, K::INTERFACE, K::KERNEL, K::META, K::STRUCT});
  constexpr KindSet kProgramStart = With(kTopLevelStart, K::EOF_);

  constexpr KindSet kPostfixStart = MakeKindSet({K::ARRAY, K::BUFFER, K::CHAR_LIT, K::CLOSURE, K::EITHERERR, K::EITHEROK, K::FALSE_, K::FLOAT_LIT, K::IDENT,
                                             K::IF, K::INT_LIT, K::LPAREN, K::MAP, K::MATCH, K::MUTARRAY, K::MUTMAP, K::MUTSET, K::NONE, K::SET, K::SOME,
                                             K::SPARSE, K::STRING_LIT, K::TRUE_, K::VARIANT, K::VEC, K::LBRACKET});
  // An expression starts like a postfix expression, or with one of the prefix operators `!`, `-`, `~` and `val`.
  constexpr KindSet kExprStart = With(With(With(With(kPostfixStart, K::BANG), K::MINUS), K::TILDE), K::VAL);
  constexpr KindSet kStmtStart = MakeKindSet({K::ARRAY, K::BRANCH, K::BUFFER, K::CHAR_LIT, K::CLOSURE, K::EITHERERR, K::EITHEROK, K::FALSE_, K::FLOAT_LIT,
                                          K::FOR, K::GOTO, K::IDENT, K::IF, K::INT_LIT, K::LABEL, K::LET, K::LPAREN, K::MAP, K::MATCH, K::MUTARRAY, K::MUTMAP,
                                          K::MUTSET, K::NONE, K::RETURN, K::SET, K::SOME, K::SPARSE, K::STRING_LIT, K::SWITCH, K::THROW, K::TRUE_,
                                          K::TRY, K::VAL, K::VAR, K::VARIANT, K::VEC, K::YIELD, K::LBRACKET});
  constexpr KindSet kTypeStart = MakeKindSet({K::ARRAY, K::BUFFER, K::CLOSURE, K::CORO, K::EITHER, K::FUNC, K::GRID, K::IDENT, K::MAP, K::MUTARRAY, K::MUTMAP,
                                          K::MUTSET, K::OPTIONAL, K::PTR, K::SET, K::SPARSE, K::TENSOR, K::VARIANT, K::VEC});
  constexpr KindSet kParamMarks = MakeKindSet({K::INDEX, K::UNIFORM, K::WORKGROUP, K::WRITABLE});
  constexpr KindSet kParamStart = With(kParamMarks, K::IDENT);
  constexpr KindSet kFieldStart = MakeKindSet({K::IDENT, K::VAR});
  constexpr KindSet kFuncStart = MakeKindSet({K::FUNC, K::KERNEL});
  constexpr KindSet kStructMemberStart = MakeKindSet({K::FUNC, K::IDENT, K::KERNEL, K::VAR});
  constexpr KindSet kHostMemberStart = MakeKindSet({K::FUNC, K::STATIC});
  constexpr KindSet kIdentStart = MakeKindSet({K::IDENT});
  constexpr KindSet kSuffixStart = MakeKindSet({K::DOT, K::LBRACKET});
  constexpr KindSet kPatternStart = MakeKindSet({K::CHAR_LIT, K::FALSE_, K::FLOAT_LIT, K::IDENT, K::INT_LIT, K::LBRACKET, K::MINUS, K::NONE, K::SOME,
                                                 K::STRING_LIT, K::TRUE_, K::VARIANT});

  // The "expected ..." text of a failed first-token test, word for word as the Nina parser writes it.
  constexpr StringView kProgramStartMsg = "'bridge', 'coro', end of input, 'extern', 'func', 'global', 'import', 'interface', 'kernel', 'meta' or 'struct'";
  constexpr StringView kExprStartMsg =
      "'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', "
      "'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec'";
  constexpr StringView kPostfixStartMsg =
      "'array', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', 'mutarray', 'mutmap', "
      "'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, 'true', 'variant' or 'vec'";
  constexpr StringView kTypeStartMsg =
      "'array', 'buffer', 'closure', 'coro', 'either', 'func', 'grid', IDENT, 'map', 'mutarray', 'mutmap', 'mutset', 'optional', 'ptr', 'set', 'sparse', "
      "'tensor', 'variant' or 'vec'";
  constexpr StringView kParamStartMsg = "IDENT, 'index', 'uniform', 'workgroup' or 'writable'";
  constexpr StringView kFieldStartMsg = "IDENT or 'var'";
  constexpr StringView kGlobalInitMsg = "'false', FLOAT_LIT, INT_LIT or 'true'";
  constexpr StringView kPatternStartMsg = "CHAR_LIT, 'false', FLOAT_LIT, IDENT, INT_LIT, '[', '-', 'None', 'Some', STRING_LIT, 'true' or 'variant'";

}  // namespace polyir::front
