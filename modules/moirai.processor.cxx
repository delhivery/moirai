export module moirai.processor;

export import std;
export import moirai.search_document;
import moirai.solver;
import moirai.transportation;

export template <PathTraversalMode P>
auto parse_path(const Path&) -> std::vector<SearchPathLocation>;

export template <PathTraversalMode P>
void parse_path_into(const Path&, std::vector<SearchPathLocation>&);
