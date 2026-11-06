// A source's tree written by a target's page (TranslateTree, polyir/translate/Page.hpp): the tree read through its schema and
// written once, then its imports, support files and hoisted text placed where the program's rule puts them.
#include "polyir/translate/BuiltTree.hpp"
#include "Writer.hpp"

namespace polyir::translate {

  using std::stable_sort;

  namespace {

    // ── what the program's rule places once its items are written ──

    // The imports the rules used, in the order the page lists them: a page writes them at the top (the fact `imports`).
    Vector<Node> importNodes(const Writer& writer, const Vector<String>& used) {
      Vector<Node> lines;

      for (const String& name : writer.rules->importOrder) {
        if (find(used.begin(), used.end(), name) != used.end()) {
          lines.push_back(Verbatim(writer.rules->imports.at(name)));
        }
      }

      return lines;
    }

    // The support files the rules used, each as its text (the program's part `support`).
    Vector<Node> supportNodes(Writer& writer, const Vector<String>& used) {
      Vector<Node> files;

      for (const String& name : used) {
        const auto path = writer.rules->support.find(name);
        const Map<String, String>& texts = writer.index->page.files;  // the page's and its base's
        const auto file = path == writer.rules->support.end() ? texts.end() : texts.find(path->second);

        if (path != writer.rules->support.end() && file == texts.end()) {
          WriterProblem(writer, writer.rules->file + ": the support file " + path->second + " is not there");
        }

        if (file != texts.end()) {
          files.push_back(Verbatim(file->second));
        }
      }

      return files;
    }

    // What the rules' `hoist:` lines wrote, where declarations go (the fact `hoisted`).
    Vector<Node> hoistedNodes(const Vector<String>& hoisted) {
      Vector<Node> nodes;

      for (const String& text : hoisted) {
        nodes.push_back(Verbatim(text));
      }

      return nodes;
    }

    void addFactNodes(Node& program, const String& name, Vector<Node> nodes) {
      if (!nodes.empty()) {
        AddNodes(program, name, PartKind::Fact, move(nodes));
      }
    }

    // ── written once, with marks ──

    // The program is written once, with a mark where its support files, imports and hoisted text go. The marks are then
    // replaced by what the writing used.
    String markOf(const String& part) {
      return "\x02" + part + "\x02";
    }

    // The program's part `support`, when its source's program has one.
    NodePart* supportOf(Node& program) {
      for (NodePart& part : program.parts) {
        if (part.name == "support") {
          return &part;
        }
      }

      return nullptr;
    }

    void addMarks(Node& program) {
      if (NodePart* support = supportOf(program)) {
        support->nodes.push_back(Verbatim(markOf("support")));
      }

      AddNode(program, "imports", PartKind::Fact, Verbatim(markOf("imports")));
      AddNode(program, "hoisted", PartKind::Fact, Verbatim(markOf("hoisted")));
    }

    void removeMarks(Node& program) {
      const auto isMark = [](const NodePart& part) { return part.name == "imports" || part.name == "hoisted"; };

      program.parts.erase(remove_if(program.parts.begin(), program.parts.end(), isMark), program.parts.end());

      if (NodePart* support = supportOf(program)) {
        support->nodes.clear();
      }
    }

    // What each mark stands for, written as its part would be.
    Map<String, Lines> markedLines(Writer& writer) {
      Map<String, Lines> lines;
      lines[markOf("support")] = WritePart(writer, NodePart{"support", PartKind::Block, "", supportNodes(writer, writer.supportUsed)});
      lines[markOf("imports")] = WritePart(writer, NodePart{"imports", PartKind::Fact, "", importNodes(writer, writer.supportUsed)});
      lines[markOf("hoisted")] = WritePart(writer, NodePart{"hoisted", PartKind::Fact, "", hoistedNodes(writer.hoisted)});

      return lines;
    }

    // Each mark alone on its line replaced by its lines, at the line's indentation; false when a mark is anywhere else.
    bool replaceMarks(Lines& lines, const Map<String, Lines>& marked) {
      Lines out;

      for (const String& line : lines) {
        const size_t indent = min(line.size(), line.find_first_not_of(' '));
        const auto found = marked.find(line.substr(indent));

        if (found == marked.end() && line.find('\x02') != String::npos) {
          return false;
        }

        for (const String& made : found == marked.end() ? Lines{line} : found->second) {
          out.push_back(made.empty() || found == marked.end() ? made : line.substr(0, indent) + made);
        }
      }

      lines = move(out);

      return true;
    }

    // A page that puts one of those holes anywhere but alone on its line has the program written again, with them in place.
    Lines writtenAgain(Writer& writer, Node& program, const Vector<String>& found) {
      Writer again = MakeWriter(*writer.index, *writer.target, *writer.schema);
      again.problems = found;
      removeMarks(program);

      if (NodePart* support = supportOf(program)) {
        support->nodes = supportNodes(again, writer.supportUsed);
      }

      addFactNodes(program, "imports", importNodes(again, writer.supportUsed));
      addFactNodes(program, "hoisted", hoistedNodes(writer.hoisted));
      Lines lines = WriteStatement(again, program);
      writer = move(again);

      return lines;
    }

    bool isOpening(const String& line) {
      return !line.empty() && (line.back() == '{' || line.back() == ':' || line.back() == '(');
    }

    bool isClosing(const String& line) {
      const size_t first = line.find_first_not_of(' ');

      return first != String::npos && String("})]").find(line[first]) != String::npos;
    }

    // The first line after `i` with more than spaces, or "" for none.
    String nextFilled(const Lines& lines, size_t i) {
      for (size_t next = i + 1; next < lines.size(); next++) {
        if (lines[next].find_first_not_of(' ') != String::npos) {
          return lines[next];
        }
      }

      return "";
    }

    // One blank line at most between lines, none just inside a block, none at the ends.
    String tidied(const Lines& lines) {
      Lines kept;

      for (size_t i = 0; i < lines.size(); i++) {
        const String line = lines[i].substr(0, lines[i].find_last_not_of(' ') + 1);
        const bool blank = line.empty();
        const bool loose = blank && (kept.empty() || kept.back().empty() || isOpening(kept.back()) || isClosing(nextFilled(lines, i)));

        if (!loose) {
          kept.push_back(line);
        }
      }

      while (!kept.empty() && kept.back().empty()) {
        kept.pop_back();
      }

      return kept.empty() ? "" : JoinLines(kept) + "\n";
    }

    // The line of the page a problem names (`Js.translate:12: ...`), 0 for none.
    long lineOf(const String& problem) {
      const size_t colon = problem.find(".translate:");

      return colon == String::npos ? 0 : strtol(problem.c_str() + colon + 11, nullptr, 10);
    }

    // The problems in the order of the page's lines, as a list of what to write.
    Vector<String> inPageOrder(Vector<String> problems) {
      stable_sort(problems.begin(), problems.end(), [](const String& a, const String& b) { return lineOf(a) < lineOf(b); });

      return problems;
    }

    // A program written by a page with no problems of its own.
    Translation writtenFrom(Node program, const PageIndex& index, const Target& target, const Schema& schema, const Vector<String>& found) {
      Writer writer = MakeWriter(index, target, schema);
      writer.problems = found;
      addMarks(program);
      Lines lines = WriteStatement(writer, program);

      if (!replaceMarks(lines, markedLines(writer))) {
        lines = writtenAgain(writer, program, found);
      }

      if (writer.lowerStatement) {
        WriterProblem(writer, index.page.rules.file + ": no rule for `choose`, and its statement has no lower form");
      }

      if (!writer.problems.empty()) {
        return Translation{"", inPageOrder(writer.problems)};
      }

      return writer.target->spans ? SpansReadOut(writer, tidied(lines)) : Translation{tidied(lines), {}};
    }

  }  // namespace

  // A page for another source is refused before anything is written, and so is a page with problems of its own.
  Translation TranslateBuilt(Node root, const Schema& schema, const Target& target, const Vector<String>& found) {
    const SharedPtr<const PageIndex> index = IndexOf(target, schema);
    const TranslateRules& page = index->page.rules;
    const Vector<String> pageProblems = page.source == schema.name ? index->problems : Vector<String>{
      target.file + ": the page translates `" + page.source + "`, and the tree is a `" + schema.name + "` tree"};

    if (!pageProblems.empty()) {
      return Translation{"", pageProblems, "", schema.name, page.target};
    }

    Vector<String> problems = found;
    const Vector<String> fitted = FitToSchema(root, schema);
    problems.insert(problems.end(), fitted.begin(), fitted.end());

    const bool library = PartNamed(root, "library") != nullptr;
    Translation out = writtenFrom(move(root), *index, target, schema, problems);
    out.extension = FileExtension(page.style, target.name, library);
    out.source = schema.name;
    out.target = page.target;

    return out;
  }

  Translation TranslateTree(const SourceTree& tree, const Target& target, const Vector<String>& found) {
    ReadTree read = ReadSourceTree(tree);
    Vector<String> problems = found;
    problems.insert(problems.end(), read.problems.begin(), read.problems.end());

    return TranslateBuilt(move(read.root), *tree.schema, target, problems);
  }

}  // namespace polyir::translate
