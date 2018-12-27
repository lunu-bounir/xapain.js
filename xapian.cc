#include <cstdlib>
#include <sstream>
#include <xapian.h>
#include <emscripten.h>






#include <iostream>




using namespace std;

Xapian::WritableDatabase db("database", Xapian::DB_CREATE_OR_OPEN);
Xapian::MSet matches;

struct Result {
  double rank;
  double weight;
  string data;
};
Result* results;

// maximum length of a returned string
auto buffer = (char*) malloc(500);
inline const char* cstr(const string& message) {
  const int len = EM_ASM_INT({
    return Math.max($0, 500);
  }, 500);

  buffer[len] = '\0';
  memcpy(buffer, message.data(), len);
  return buffer;
}

extern "C" void add(
  const char* key, // uniQue id (to retrieve the actual content from JS side)
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

  Xapian::Stem stemmer(lang);
  indexer.set_stemmer(stemmer);
  indexer.set_stemming_strategy(indexer.STEM_SOME_FULL_POS);

  Xapian::Document doc;
  indexer.set_document(doc);
  doc.set_data(key);

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
    while (std::getline(ss, keyword,',')) {
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

  db.add_document(doc);
}

extern "C" int percent(int index) {
  const double weight = matches[index].get_weight();
  return matches.convert_to_percent(weight);
}

extern "C" const char* snippet(const char* lang, const char* text, size_t length, const char* omit) {
  Xapian::Stem stemmer(lang);
  const string str = matches.snippet(text, length, stemmer, matches.SNIPPET_BACKGROUND_MODEL, "<b>", "</b>", omit);
  return cstr(str);
}

extern "C" const char* key(int index) {
  const string str = matches[index].get_document().get_data();
  return cstr(str);
}

extern "C" const char* query(const char* lang, const char* querystring, int offset, int pagesize) {
  // Start an enquire session.
  Xapian::Enquire enquire(db);

  // Parse the query string to produce a Xapian::Query object.
  Xapian::QueryParser qp;
  Xapian::Stem stemmer(lang);
  qp.set_stemmer(stemmer);
  qp.set_database(db);
  qp.set_stemming_strategy(Xapian::QueryParser::STEM_SOME);

  // Start of prefix configuration.
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
  Xapian::Query query = qp.parse_query(querystring);

  // Find results for the query.
  enquire.set_query(query);
  matches = enquire.get_mset(offset, pagesize);

  const string str = to_string(matches.size()) + '/' + to_string(matches.get_matches_estimated());
  return cstr(str);
}

extern "C" const char* languages() {
  Xapian::Stem stemmer("none");
  const string str = stemmer.get_available_languages();
  return cstr(str);
}
