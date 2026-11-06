// Spans (polyir/translate/Page.hpp, core/docs/translate.md): a written statement's mark naming where it came from, read out of
// the finished text into the translation's mappings, with the page's `line` lines put before statements; and the mappings as a
// Source Map (version 3). Nothing here runs unless the translation asks for spans.
#include "Writer.hpp"

namespace polyir::translate {

  namespace {

    const char kSpanMark = '\x03';

    // ── the marks ──

    String markOf(size_t index) {
      return String(1, kSpanMark) + to_string(index) + String(1, kSpanMark);
    }

    // A written line with its marks taken out: its text, and each mark's span (its place in the writer's list) and column.
    struct MarkedLine {
      String text;
      Vector<Pair<size_t, int>> marks;
    };

    MarkedLine marksOf(const String& line) {
      MarkedLine out;

      for (size_t at = 0; at < line.size(); at++) {
        const size_t end = line[at] == kSpanMark ? line.find(kSpanMark, at + 1) : String::npos;

        if (end == String::npos) {
          out.text += line[at];
        } else {
          out.marks.push_back({stoul(line.substr(at + 1, end - at - 1)), static_cast<int>(out.text.size())});
          at = end;
        }
      }

      return out;
    }

    // ── the page's `line` lines ──

    String replacedAll(String text, const String& hole, const String& value) {
      for (size_t found = text.find(hole); found != String::npos; found = text.find(hole, found + value.size())) {
        text.replace(found, hole.size(), value);
      }

      return text;
    }

    // `#line {line} "{file}"` with the span's place in it. `{code?}` is the code too, its line left out when there is none.
    String filled(const String& text, const SourceSpan& at) {
      const String place = replacedAll(replacedAll(text, "{file}", at.file), "{line}", to_string(at.line));

      return replacedAll(replacedAll(replacedAll(place, "{column}", to_string(at.column)), "{code?}", at.code), "{code}", at.code);
    }

    // The span with its line's code: its source's, else the target's `codeOf` when it has one.
    SourceSpan withCode(const Writer& writer, const SourceSpan& at) {
      SourceSpan span = at;
      const bool ask = span.code.empty() && writer.target->codeOf;
      span.code = ask ? writer.target->codeOf(at.file, at.line) : span.code;

      return span;
    }

    // The page's `line` lines for a span, each at the indentation of the statement they go before, with no spaces after.
    Lines spanLines(const String& line, const SourceSpan& at, const String& indent) {
      Lines lines;
      IStringStream text(line);

      for (String made; getline(text, made);) {
        const bool noCode = made.find("{code?}") != String::npos && at.code.empty();
        const String written = indent + filled(made, at);
        noCode ? void() : lines.push_back(written.substr(0, written.find_last_not_of(' ') + 1));
      }

      return lines;
    }

    // The marks read out of the finished text: a mapping for each, at its line and column.
    struct ReadOut {
      Lines lines;
      Vector<SpanMapping> mappings;
      SourceSpan last;  // the span the last `line` lines were for
    };

    bool sameLine(const SourceSpan& a, const SourceSpan& b) {
      return a.line == b.line && a.file == b.file;
    }

    // One written line: the `line` lines before it when its first mark is for another source line, then the line itself.
    void readLine(ReadOut& out, const Writer& writer, const String& line) {
      const MarkedLine marked = marksOf(line);
      const auto spanLine = writer.rules->spans.find("line");
      const SourceSpan* first = marked.marks.empty() ? nullptr : &writer.spans[marked.marks[0].first];

      if (first != nullptr && writer.target->spanLines && spanLine != writer.rules->spans.end() && !sameLine(*first, out.last)) {
        const String indent = marked.text.substr(0, marked.text.find_first_not_of(' '));
        const Lines before = spanLines(spanLine->second, withCode(writer, *first), indent);
        out.lines.insert(out.lines.end(), before.begin(), before.end());
        out.last = *first;
      }

      out.lines.push_back(marked.text);

      for (const auto& [index, column] : marked.marks) {
        out.mappings.push_back(SpanMapping{static_cast<int>(out.lines.size()), column, writer.spans[index]});
      }
    }

    // ── the source map ──

    // One value as Source Map v3 writes it: base-64 digits of five bits each, lowest first, the sign in the lowest bit.
    String vlq(int value) {
      const char* const digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
      unsigned rest = value < 0 ? (static_cast<unsigned>(-value) << 1) | 1 : static_cast<unsigned>(value) << 1;
      String out;

      do {
        const unsigned digit = rest & 31;
        rest >>= 5;
        out += digits[rest > 0 ? digit | 32 : digit];
      } while (rest > 0);

      return out;
    }

    // What each segment is told against: the last segment's column on this line, and its source, line and column overall.
    struct MapState {
      int column = 0;
      int source = 0;
      int line = 0;
      int sourceColumn = 0;
    };

    String segmentOf(MapState& state, const SpanMapping& mapping, int source) {
      const int line = mapping.at.line - 1;
      const int column = max(mapping.at.column - 1, 0);
      const String text = vlq(mapping.column - state.column) + vlq(source - state.source) + vlq(line - state.line) + vlq(column - state.sourceColumn);
      state = MapState{mapping.column, source, line, column};

      return text;
    }

    Vector<String> sourcesOf(const Vector<SpanMapping>& mappings) {
      Vector<String> sources;

      for (const SpanMapping& mapping : mappings) {
        find(sources.begin(), sources.end(), mapping.at.file) == sources.end() ? sources.push_back(mapping.at.file) : void();
      }

      return sources;
    }

    int indexOf(const Vector<String>& sources, const String& file) {
      return static_cast<int>(find(sources.begin(), sources.end(), file) - sources.begin());
    }

    // The segments of written line `line`, from mapping `next` on; `next` ends past them.
    String lineSegments(MapState& state, const Translation& translation, const Vector<String>& sources, int line, size_t& next) {
      String text;
      state.column = 0;

      for (; next < translation.mappings.size() && translation.mappings[next].line == line; next++) {
        const SpanMapping& mapping = translation.mappings[next];
        text += (text.empty() ? "" : ",") + segmentOf(state, mapping, indexOf(sources, mapping.at.file));
      }

      return text;
    }

    String jsonString(const String& text) {
      String out = "\"";

      for (const char c : text) {
        out += c == '"' || c == '\\' ? String("\\") + c : String(1, c);
      }

      return out + "\"";
    }

    String jsonList(const Vector<String>& items) {
      String out = "[";

      for (const String& item : items) {
        out += (out.size() == 1 ? "" : ",") + jsonString(item);
      }

      return out + "]";
    }

  }  // namespace

  Lines WithSpanMark(Writer& writer, const Node& node, Lines lines) {
    const auto isFilled = [](const String& line) { return line.find_first_not_of(' ') != String::npos; };
    const auto filledLine = find_if(lines.begin(), lines.end(), isFilled);

    if (!writer.target->spans || node.at.line == 0 || filledLine == lines.end()) {
      return lines;
    }

    filledLine->insert(filledLine->find_first_not_of(' '), markOf(writer.spans.size()));
    writer.spans.push_back(node.at);

    return lines;
  }

  Translation SpansReadOut(const Writer& writer, const String& text) {
    ReadOut out;
    IStringStream lines(text);

    for (String line; getline(lines, line);) {
      readLine(out, writer, line);
    }

    const auto map = writer.rules->spans.find("map");
    Translation done{out.lines.empty() ? "" : JoinLines(out.lines) + "\n", {}};
    done.mappings = move(out.mappings);
    done.mapLine = map == writer.rules->spans.end() ? "" : map->second;

    return done;
  }

  String SourceMapText(const Translation& translation, const String& file) {
    const Vector<String> sources = sourcesOf(translation.mappings);
    const int lines = static_cast<int>(count(translation.text.begin(), translation.text.end(), '\n'));
    MapState state;
    size_t next = 0;
    String mappings;

    for (int line = 1; line <= lines; line++) {
      mappings += (line == 1 ? "" : ";") + lineSegments(state, translation, sources, line, next);
    }

    return "{\"version\":3,\"file\":" + jsonString(file) + ",\"sources\":" + jsonList(sources) + ",\"names\":[],\"mappings\":\"" +
      mappings + "\"}\n";
  }

  String SourceMapLink(const Translation& translation, const String& mapName) {
    return translation.mapLine.empty() ? "" : replacedAll(translation.mapLine, "{map}", mapName);
  }

}  // namespace polyir::translate
