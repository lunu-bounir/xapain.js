#include <cstdlib>
#include <sstream>
#include <xapian.h>
#include <emscripten.h>

struct Result {
  const double rank;
  const double weight;
  const std::string data;
};

Xapian::WritableDatabase db;
Xapian::MSet matches;
Result* results;

// maximum length of a returned string
const auto buffer = (char*) malloc(500);
inline const char* cstr(const std::string& message) {
  const int len = EM_ASM_INT({
    return Math.max($0, 500);
  }, 500);

  buffer[len] = '\0';
  memcpy(buffer, message.data(), len);
  return buffer;
}

extern "C" void prepare(const char* path) {
  const Xapian::WritableDatabase _db(path, Xapian::DB_CREATE_OR_OPEN);
  db = _db;
}

// Commit any pending modifications made to the database.
extern "C" void commit() {
  db.commit();
}

extern "C" void add(
  const char* uniQue, // uniQue id (to retrieve the actual content from JS side)
  const char* lang,
  const char* hostname,
  const char* url,
  const char* date,
  const char* filename,
  const char* mime,
  const char* title,
  const char* keywords,
  const char* description,
  const char* body
) {
  Xapian::TermGenerator indexer;

  const Xapian::Stem stemmer(lang);
  indexer.set_stemmer(stemmer);
  indexer.set_stemming_strategy(indexer.STEM_SOME_FULL_POS);

  Xapian::Document doc;
  indexer.set_document(doc);
  doc.set_data(uniQue);

  // https://xapian.org/docs/omega/termprefixes.html
  // Index each field with a suitable prefix.
  indexer.index_text(hostname, 1, "H");
  indexer.index_text(title, 1, "S");
  indexer.index_text(mime, 1, "T");
  indexer.index_text(url, 1, "U");
  indexer.index_text(date, 1, "D");
  indexer.index_text(filename, 1, "F");
  indexer.index_text(lang, 1, "L");
  indexer.index_text(description, 1, "D");

  std::stringstream ss(keywords);
  std::string keyword;
  if (keywords != NULL) {
    while (std::getline(ss, keyword, ',')) {
      indexer.index_text(keyword, 1, "K");
    }
  }

  // Index fields without prefixes for general search.
  indexer.index_text(url);
  indexer.increase_termpos();
  indexer.index_text(title);
  indexer.increase_termpos();
  indexer.index_text(filename);
  indexer.increase_termpos();
  indexer.index_text(body);

  // We use the identifier to ensure each object ends up in the
  // database only once no matter how many times we run the indexer.
  const std::string term = 'Q' + std::string(uniQue);
  doc.add_boolean_term(term);
  db.replace_document(term, doc);
}

extern "C" void clean(const char* uniQue) {
  const std::string term = 'Q' + std::string(uniQue);
  db.delete_document(term);
}

extern "C" int percent(int index) {
  const double weight = matches[index].get_weight();
  return matches.convert_to_percent(weight);
}

extern "C" const char* snippet(const char* lang, const char* text, size_t length, const char* omit) {
  const Xapian::Stem stemmer(lang);
  const std::string str = matches.snippet(text, length, stemmer, matches.SNIPPET_EXHAUSTIVE, "<b>", "</b>", omit);
  return cstr(str);
}

extern "C" const char* key(int index) {
  const std::string str = matches[index].get_document().get_data();
  return cstr(str);
}

extern "C" const char* query(const char* lang, const char* querystring, int offset, int pagesize) {
  // Start an enquire session.
  Xapian::Enquire enquire(db);

  // Parse the query string to produce a Xapian::Query object.
  Xapian::QueryParser qp;
  const Xapian::Stem stemmer(lang);
  qp.set_stemmer(stemmer);
  qp.set_database(db);
  qp.set_stemming_strategy(Xapian::QueryParser::STEM_SOME);

  // prefix configuration.
  qp.add_prefix("hostname", "H");
  qp.add_prefix("title", "S");
  qp.add_prefix("mime", "T");
  qp.add_prefix("url", "U");
  qp.add_prefix("date", "D");
  qp.add_prefix("filename", "F");
  qp.add_prefix("language", "L");
  qp.add_prefix("keyword", "K");
  qp.add_prefix("description", "D");

  // parse the query
  const Xapian::Query query = qp.parse_query(querystring, Xapian::QueryParser::FLAG_PARTIAL);

  // Find results for the query.
  enquire.set_query(query);
  matches = enquire.get_mset(offset, pagesize);

  const std::string str = std::to_string(matches.size()) + '/' + std::to_string(matches.get_matches_estimated());
  return cstr(str);
}

extern "C" const char* languages() {
  const Xapian::Stem stemmer("none");
  const std::string str = stemmer.get_available_languages();
  return cstr(str);
}
