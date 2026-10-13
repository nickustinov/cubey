#include "ui/font.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

namespace cubey::font {

namespace {

// The marks of the alphabets Cubey speaks, each drawn once for a letter 3, 4 or 5 dots wide, over its middle; a
// cedilla and an ogonek under it.
enum class Mark : uint8_t { Acute, Grave, Circumflex, Diaeresis, Ring, Tilde, Dot, Breve, Cedilla, Ogonek };

struct MarkRows {
    Mark mark;
    size_t wide, count;
    std::array<std::string_view, 3> rows;
};
constexpr MarkRows kMarks[] = {
    {Mark::Acute, 5, 2, {"...#.", "..#.."}},       {Mark::Grave, 5, 2, {".#...", "..#.."}},
    {Mark::Circumflex, 5, 2, {"..#..", ".#.#."}},  {Mark::Diaeresis, 5, 1, {".#.#."}},
    {Mark::Ring, 5, 3, {"..#..", ".#.#.", "..#.."}}, {Mark::Tilde, 5, 2, {".##.#", "#..#."}},
    {Mark::Dot, 5, 1, {"..#.."}},                  {Mark::Breve, 5, 2, {"#...#", ".###."}},
    {Mark::Cedilla, 5, 2, {"..#..", ".##.."}},     {Mark::Ogonek, 5, 2, {"...#.", "...##"}},
    {Mark::Acute, 4, 2, {"..#.", ".#.."}},         {Mark::Cedilla, 4, 2, {".#..", "##.."}},
    {Mark::Acute, 3, 2, {"..#", ".#."}},           {Mark::Grave, 3, 2, {"#..", ".#."}},
    {Mark::Circumflex, 3, 2, {".#.", "#.#"}},      {Mark::Diaeresis, 3, 1, {"#.#"}},
    {Mark::Ogonek, 3, 2, {".#.", ".##"}},
};
constexpr std::string_view kDots = "........"; // a row between a mark and its letter, as wide as it

const MarkRows &rows_of(Mark mark, size_t wide) {
    for (const MarkRows &m : kMarks)
        if (m.mark == mark && m.wide == wide) return m;
    std::abort(); // a letter of the font's own with no mark drawn for its width: its tests say which
}

// A glyph's rows ('/' between), kept in the store as views of its text.
Rows split_into(std::vector<std::string_view> &store, std::string_view glyph) {
    const size_t first = store.size();
    for (size_t at = 0;;) {
        const size_t next = glyph.find('/', at);
        store.push_back(glyph.substr(at, next == std::string_view::npos ? std::string_view::npos : next - at));
        if (next == std::string_view::npos) break;
        at = next + 1;
    }
    return {store.data() + first, store.size() - first};
}

size_t rows_in(const char *glyph) { return 1 + static_cast<size_t>(std::count(glyph, glyph + std::strlen(glyph), '/')); }

// A letter with a mark, as Latvian's are drawn: over a small letter, on the rows just above it (a one-row mark a row
// higher, a row between); over a capital, on the line's top rows, a row between (a ring, three rows, touching it); a
// cedilla or an ogonek under either, below its line, as Ļ's comma. Its rows, views of the mark's and its letter's.
Letter marked(std::vector<std::string_view> &store, const Letter &base, Mark mark, bool capital) {
    const size_t wide = base.rows[0].size(), first = store.size();
    const MarkRows &m = rows_of(mark, wide);
    const bool under = mark == Mark::Cedilla || mark == Mark::Ogonek;
    const bool between = !under && (capital ? m.count < 3 : m.count == 1);
    if (!under) store.insert(store.end(), m.rows.begin(), m.rows.begin() + static_cast<long>(m.count));
    if (between) store.push_back(kDots.substr(0, wide));
    store.insert(store.end(), base.rows.begin(), base.rows.end());
    if (under) store.insert(store.end(), m.rows.begin(), m.rows.begin() + static_cast<long>(m.count));
    const int over = under ? 0 : static_cast<int>(m.count) + between;
    return {base.top - over, {store.data() + first, store.size() - first}};
}

// The font's letters: every one's rows in one store, views of its glyph's text (a marked letter's, of its mark's and
// its letter's), and the letters, sorted by name, in another: built once, each in one allocation, reserved for all
// it holds so none moves once a letter points at it. On the board an allocation for each letter took the internal
// RAM its Wi-Fi and TLS need (allocations of 1 KB and more go to PSRAM).
struct Store {
    std::vector<std::string_view> rows;
    std::vector<std::pair<std::string_view, Letter>> letters;

    const Letter *built(std::string_view name) const { // while building: as they came
        for (const auto &[n, l] : letters)
            if (n == name) return &l;
        return nullptr;
    }
    const Letter *find(std::string_view name) const { // once built and sorted
        const auto at = std::lower_bound(letters.begin(), letters.end(), name, [](const auto &e, std::string_view n) { return e.first < n; });
        return at != letters.end() && at->first == name ? &at->second : nullptr;
    }
    void sort() { std::sort(letters.begin(), letters.end(), [](const auto &a, const auto &b) { return a.first < b.first; }); }
};

// Each letter with a mark: small and capital, the letters it is drawn from, and its mark (į drawn: its i has no room).
struct Marked {
    const char *small, *capital, *small_of, *capital_of;
    Mark mark;
};
const Marked kMarked[] = {
    {"à", "À", "a", "A", Mark::Grave}, {"á", "Á", "a", "A", Mark::Acute}, {"â", "Â", "a", "A", Mark::Circumflex},
    {"ã", "Ã", "a", "A", Mark::Tilde}, {"ä", "Ä", "a", "A", Mark::Diaeresis}, {"å", "Å", "a", "A", Mark::Ring},
    {"ą", "Ą", "a", "A", Mark::Ogonek}, {"ç", "Ç", "c", "C", Mark::Cedilla}, {"ć", "Ć", "c", "C", Mark::Acute},
    {"è", "È", "e", "E", Mark::Grave}, {"é", "É", "e", "E", Mark::Acute}, {"ê", "Ê", "e", "E", Mark::Circumflex},
    {"ë", "Ë", "e", "E", Mark::Diaeresis}, {"ę", "Ę", "e", "E", Mark::Ogonek}, {"ė", "Ė", "e", "E", Mark::Dot},
    {"ì", "Ì", "ı", "I", Mark::Grave}, {"í", "Í", "ı", "I", Mark::Acute}, {"î", "Î", "ı", "I", Mark::Circumflex},
    {"ï", "Ï", "ı", "I", Mark::Diaeresis}, {nullptr, "Į", nullptr, "I", Mark::Ogonek},
    {"ñ", "Ñ", "n", "N", Mark::Tilde}, {"ń", "Ń", "n", "N", Mark::Acute}, {"ò", "Ò", "o", "O", Mark::Grave},
    {"ó", "Ó", "o", "O", Mark::Acute}, {"ô", "Ô", "o", "O", Mark::Circumflex}, {"õ", "Õ", "o", "O", Mark::Tilde},
    {"ö", "Ö", "o", "O", Mark::Diaeresis}, {"ù", "Ù", "u", "U", Mark::Grave}, {"ú", "Ú", "u", "U", Mark::Acute},
    {"û", "Û", "u", "U", Mark::Circumflex}, {"ü", "Ü", "u", "U", Mark::Diaeresis}, {"ų", "Ų", "u", "U", Mark::Ogonek},
    {"ÿ", "Ÿ", "y", "Y", Mark::Diaeresis}, {"ś", "Ś", "s", "S", Mark::Acute}, {"ź", "Ź", "z", "Z", Mark::Acute},
    {"ż", "Ż", "z", "Z", Mark::Dot},
    {"ё", "Ё", "e", "E", Mark::Diaeresis}, {"й", "Й", "и", "И", Mark::Breve}, {"ї", "Ї", "ı", "I", Mark::Diaeresis},
};

// Cyrillic letters shaped as Latin ones, drawn as those.
const std::pair<const char *, const char *> kSameAs[] = {
    {"а", "a"}, {"е", "e"}, {"о", "o"}, {"р", "p"}, {"с", "c"}, {"у", "y"}, {"х", "x"}, {"і", "i"},
    {"А", "A"}, {"В", "B"}, {"Е", "E"}, {"К", "K"}, {"М", "M"}, {"Н", "H"}, {"О", "O"}, {"Р", "P"}, {"С", "C"},
    {"Т", "T"}, {"Х", "X"}, {"І", "I"},
};

// Each small letter's capital, but the English ones' and those with a mark (kMarked).
const std::pair<const char *, const char *> kCapitals[] = {
    {"ā", "Ā"}, {"č", "Č"}, {"ē", "Ē"}, {"ģ", "Ģ"}, {"ī", "Ī"}, {"ķ", "Ķ"}, {"ļ", "Ļ"}, {"ņ", "Ņ"}, {"š", "Š"},
    {"ū", "Ū"}, {"ž", "Ž"}, {"æ", "Æ"}, {"œ", "Œ"}, {"ø", "Ø"}, {"ł", "Ł"}, {"ß", "ẞ"}, {"į", "Į"}, {"ı", "I"},
    {"а", "А"}, {"б", "Б"}, {"в", "В"}, {"г", "Г"}, {"ґ", "Ґ"}, {"д", "Д"}, {"е", "Е"}, {"є", "Є"}, {"ж", "Ж"},
    {"з", "З"}, {"и", "И"}, {"і", "І"}, {"к", "К"}, {"л", "Л"}, {"м", "М"}, {"н", "Н"}, {"о", "О"}, {"п", "П"},
    {"р", "Р"}, {"с", "С"}, {"т", "Т"}, {"у", "У"}, {"ф", "Ф"}, {"х", "Х"}, {"ц", "Ц"}, {"ч", "Ч"}, {"ш", "Ш"},
    {"щ", "Щ"}, {"ъ", "Ъ"}, {"ы", "Ы"}, {"ь", "Ь"}, {"э", "Э"}, {"ю", "Ю"}, {"я", "Я"},
};

} // namespace

const Glyph &digit(char d) {
    static const Glyph digits[] = {
        {5, {".###.", "#...#", "#...#", "#...#", "#...#", ".###."}},
        {3, {".#.", "##.", ".#.", ".#.", ".#.", "###"}}, // the font's
        {5, {".###.", "#...#", "...#.", "..#..", ".#...", "#####"}},
        {5, {"####.", "....#", ".###.", "....#", "....#", "####."}}, // the font's
        {5, {"...#.", "..##.", ".#.#.", "#####", "...#.", "...#."}},
        {5, {"#####", "#....", "####.", "....#", "....#", "####."}},
        {5, {".###.", "#....", "####.", "#...#", "#...#", ".###."}},
        {5, {"#####", "....#", "...#.", "..#..", "..#..", "..#.."}},
        {5, {".###.", "#...#", ".###.", "#...#", "#...#", ".###."}},
        {5, {".###.", "#...#", "#...#", ".####", "....#", ".###."}},
    };
    return digits[d >= '0' && d <= '9' ? d - '0' : 0];
}

int width(const std::string &digits) {
    int w = 0;
    for (char d : digits) w += digit(d).width + 1;
    return w ? w - 1 : 0;
}

const Letter *letter(const std::string &utf8) {
    static const Store store = [] {
        using Glyph = std::pair<const char *, std::pair<int, const char *>>; // its name, its first row, its rows ('/' between)
        // As the font writes them (dotfont.ts), on its own 9-row line.
        static const Glyph small[] = {
            {"a", {2, ".###./....#/.####/#...#/.####"}},
            {"b", {1, "#..../####./#...#/#...#/#...#/####."}},
            {"c", {2, ".###/#.../#.../#.../.###"}},
            {"d", {1, "....#/.####/#...#/#...#/#...#/.####"}},
            {"e", {2, ".###./#...#/#####/#..../.###."}},
            {"f", {1, "..##/.#../####/.#../.#../.#.."}},
            {"g", {2, ".####/#...#/#...#/#...#/.####/....#/.###."}},
            {"h", {1, "#..../####./#...#/#...#/#...#/#...#"}},
            {"i", {0, "#/./#/#/#/#/#"}},
            {"j", {0, "..#/.../..#/..#/..#/..#/..#/..#/##."}},
            {"k", {1, "#.../#..#/#.#./##../#.#./#..#"}},
            {"l", {1, "#/#/#/#/#/#"}},
            {"m", {2, "##.#./#.#.#/#.#.#/#.#.#/#.#.#"}},
            {"n", {2, "####./#...#/#...#/#...#/#...#"}},
            {"o", {2, ".###./#...#/#...#/#...#/.###."}},
            {"p", {2, "####./#...#/#...#/#...#/####./#..../#...."}},
            {"q", {2, ".####/#...#/#...#/#...#/.####/....#/....#"}}, // drawn: p turned round
            {"r", {2, "#.##/##../#.../#.../#..."}},
            {"s", {2, ".####/#..../.###./....#/####."}},
            {"t", {1, ".#../####/.#../.#../.#../..##"}},
            {"u", {2, "#...#/#...#/#...#/#...#/.####"}},
            {"v", {2, "#...#/#...#/#...#/.#.#./..#.."}},
            {"w", {2, "#...#/#.#.#/#.#.#/#.#.#/.#.#."}},
            {"x", {2, "#...#/.#.#./..#../.#.#./#...#"}},
            {"y", {2, "#...#/#...#/#...#/#...#/.####/....#/.###."}},
            {"z", {2, "#####/...#./..#../.#.../#####"}},
            // Latvian's, drawn
            {"ā", {0, ".###./...../.###./....#/.####/#...#/.####"}},
            {"č", {0, "#.#./.#../.###/#.../#.../#.../.###"}},
            {"ē", {0, ".###./...../.###./#...#/#####/#..../.###."}},
            {"ģ", {0, "...#./..#../.####/#...#/#...#/#...#/.####/....#/.###."}},
            {"ī", {0, "###/.../.#./.#./.#./.#./.#."}},
            {"ķ", {1, "#.../#..#/#.#./##../#.#./#..#/.#../#..."}},
            {"ļ", {1, "#./#./#./#./#./#./.#/#."}},
            {"ņ", {2, "####./#...#/#...#/#...#/#...#/..#../.#..."}},
            {"š", {0, ".#.#./..#../.####/#..../.###./....#/####."}},
            {"ū", {0, ".###./...../#...#/#...#/#...#/#...#/.####"}},
            {"ž", {0, ".#.#./..#../#####/...#./..#../.#.../#####"}},
            // The few that are no letter and a mark, a dotless i to mark, and Cyrillic's own, drawn
            {"ı", {2, ".#./.#./.#./.#./.#."}},
            {"į", {0, "#./../#./#./#./#./#./.#"}},
            {"æ", {2, ".##.##./...#..#/.######/#..#.../.##.###"}},
            {"œ", {2, ".##.##./#..#..#/#..####/#..#.../.##.###"}},
            {"ø", {2, ".###./#..##/#.#.#/##..#/.###."}},
            {"ł", {1, ".#./.#./.##/##./.#./.#."}},
            {"ß", {1, ".##../#..#./#.#../#..#./#...#/#.##."}},
            {"б", {1, ".####/#..../####./#...#/#...#/.###."}},
            {"в", {2, "####./#...#/####./#...#/####."}},
            {"г", {2, "####/#.../#.../#.../#..."}},
            {"ґ", {1, "...#/####/#.../#.../#.../#..."}},
            {"д", {2, ".###./.#.#./.#.#./.#.#./#####/#...#"}},
            {"є", {2, ".###/#.../###./#.../.###"}},
            {"ж", {2, "#.#.#/#.#.#/.###./#.#.#/#.#.#"}},
            {"з", {2, "####./....#/.###./....#/####."}},
            {"и", {2, "#...#/#..##/#.#.#/##..#/#...#"}},
            {"к", {2, "#..#/#.#./##../#.#./#..#"}},
            {"л", {2, ".####/.#..#/.#..#/.#..#/#...#"}},
            {"м", {2, "#...#/##.##/#.#.#/#...#/#...#"}},
            {"н", {2, "#...#/#...#/#####/#...#/#...#"}},
            {"п", {2, "#####/#...#/#...#/#...#/#...#"}},
            {"т", {2, "#####/..#../..#../..#../..#.."}},
            {"ф", {1, "..#../.###./#.#.#/#.#.#/#.#.#/.###./..#.."}},
            {"ц", {2, "#..#./#..#./#..#./#..#./#####/....#"}},
            {"ч", {2, "#...#/#...#/.####/....#/....#"}},
            {"ш", {2, "#.#.#/#.#.#/#.#.#/#.#.#/#####"}},
            {"щ", {2, "#.#.#/#.#.#/#.#.#/#.#.#/#####/....#"}},
            {"ъ", {2, "##.../.#.../.###./.#..#/.###."}},
            {"ы", {2, "#...#/#...#/###.#/#.#.#/###.#"}},
            {"ь", {2, "#.../#.../###./#..#/###."}},
            {"э", {2, "###./...#/.###/...#/###."}},
            {"ю", {2, "#..#./#.#.#/###.#/#.#.#/#..#."}},
            {"я", {2, ".####/#...#/.####/.#..#/#...#"}},
        };
        // Drawn, on Cubey's 11-row line.
        static const Glyph capitals[] = {
            {"A", {3, ".###./#...#/#...#/#####/#...#/#...#"}},
            {"B", {3, "####./#...#/####./#...#/#...#/####."}},
            {"C", {3, ".####/#..../#..../#..../#..../.####"}},
            {"D", {3, "####./#...#/#...#/#...#/#...#/####."}},
            {"E", {3, "#####/#..../####./#..../#..../#####"}},
            {"F", {3, "#####/#..../####./#..../#..../#...."}},
            {"G", {3, ".####/#..../#..../#..##/#...#/.####"}},
            {"H", {3, "#...#/#...#/#####/#...#/#...#/#...#"}},
            {"I", {3, "###/.#./.#./.#./.#./###"}},
            {"J", {3, "....#/....#/....#/....#/#...#/.###."}},
            {"K", {3, "#...#/#..#./###../#.#../#..#./#...#"}},
            {"L", {3, "#..../#..../#..../#..../#..../#####"}},
            {"M", {3, "#...#/##.##/#.#.#/#...#/#...#/#...#"}},
            {"N", {3, "#...#/##..#/#.#.#/#..##/#...#/#...#"}},
            {"O", {3, ".###./#...#/#...#/#...#/#...#/.###."}},
            {"P", {3, "####./#...#/#...#/####./#..../#...."}},
            {"Q", {3, ".###./#...#/#...#/#.#.#/#..#./.##.#"}},
            {"R", {3, "####./#...#/#...#/####./#..#./#...#"}},
            {"S", {3, ".####/#..../.###./....#/....#/####."}},
            {"T", {3, "#####/..#../..#../..#../..#../..#.."}},
            {"U", {3, "#...#/#...#/#...#/#...#/#...#/.###."}},
            {"V", {3, "#...#/#...#/#...#/#...#/.#.#./..#.."}},
            {"W", {3, "#...#/#...#/#...#/#.#.#/##.##/#...#"}},
            {"X", {3, "#...#/.#.#./..#../..#../.#.#./#...#"}},
            {"Y", {3, "#...#/.#.#./..#../..#../..#../..#.."}},
            {"Z", {3, "#####/...#./..#../.#.../#..../#####"}},
            {"Ā", {1, ".###./...../.###./#...#/#...#/#####/#...#/#...#"}},
            {"Č", {0, ".#.#./..#../...../.####/#..../#..../#..../#..../.####"}},
            {"Ē", {1, ".###./...../#####/#..../####./#..../#..../#####"}},
            {"Ģ", {3, ".####/#..../#..../#..##/#...#/.####/..#../.#..."}},
            {"Ī", {1, "###/.../###/.#./.#./.#./.#./###"}},
            {"Ķ", {3, "#...#/#..#./###../#.#../#..#./#...#/..#../.#..."}},
            {"Ļ", {3, "#..../#..../#..../#..../#..../#####/..#../.#..."}},
            {"Ņ", {3, "#...#/##..#/#.#.#/#..##/#...#/#...#/..#../.#..."}},
            {"Š", {0, ".#.#./..#../...../.####/#..../.###./....#/....#/####."}},
            {"Ū", {1, ".###./...../#...#/#...#/#...#/#...#/#...#/.###."}},
            {"Ž", {0, ".#.#./..#../...../#####/...#./..#../.#.../#..../#####"}},
            // The few that are no letter and a mark, and Cyrillic's own, drawn
            {"Æ", {3, ".#####/#..#../#..###/####../#..#../#..###"}},
            {"Œ", {3, ".#####/#..#../#..###/#..#../#..#../.#####"}},
            {"Ø", {3, ".###./#..##/#.#.#/#.#.#/##..#/.###."}},
            {"Ł", {3, "#..../#..../#.#../##.../#..../#####"}},
            {"ẞ", {3, "####./#...#/#..#./#...#/#...#/#.##."}},
            {"Б", {3, "#####/#..../####./#...#/#...#/####."}},
            {"Г", {3, "#####/#..../#..../#..../#..../#...."}},
            {"Ґ", {2, "....#/#####/#..../#..../#..../#..../#...."}},
            {"Д", {3, ".###./.#.#./.#.#./.#.#./.#.#./#####/#...#"}},
            {"Є", {3, ".####/#..../###../#..../#..../.####"}},
            {"Ж", {3, "#.#.#/#.#.#/.###./.###./#.#.#/#.#.#"}},
            {"З", {3, "####./....#/.###./....#/....#/####."}},
            {"И", {3, "#...#/#..##/#.#.#/#.#.#/##..#/#...#"}},
            {"Л", {3, ".####/.#..#/.#..#/.#..#/.#..#/#...#"}},
            {"П", {3, "#####/#...#/#...#/#...#/#...#/#...#"}},
            {"У", {3, "#...#/#...#/#...#/.####/....#/.###."}},
            {"Ф", {3, "..#../.###./#.#.#/#.#.#/.###./..#.."}},
            {"Ц", {3, "#..#./#..#./#..#./#..#./#..#./#####/....#"}},
            {"Ч", {3, "#...#/#...#/#...#/.####/....#/....#"}},
            {"Ш", {3, "#.#.#/#.#.#/#.#.#/#.#.#/#.#.#/#####"}},
            {"Щ", {3, "#.#.#/#.#.#/#.#.#/#.#.#/#.#.#/#####/....#"}},
            {"Ъ", {3, "##.../.#.../.###./.#..#/.#..#/.###."}},
            {"Ы", {3, "#...#/#...#/###.#/#.#.#/#.#.#/###.#"}},
            {"Ь", {3, "#..../#..../####./#...#/#...#/####."}},
            {"Э", {3, "####./....#/.####/....#/....#/####."}},
            {"Ю", {3, "#..#./#.#.#/###.#/#.#.#/#.#.#/#..#."}},
            {"Я", {3, ".####/#...#/#...#/.####/.#..#/#...#"}},
        };
        // A few signs, drawn in its style about the capitals' middle (a sum, a question), and a space.
        static const Glyph signs[] = {
            {"+", {4, "..#../..#../#####/..#../..#.."}},
            {"-", {6, "####"}},
            {"=", {5, "####/..../####"}},
            {"?", {3, ".###./#...#/...#./..#../...../..#.."}},
            {"!", {3, "#/#/#/#/./#"}},
            {".", {8, "#"}},
            {",", {8, ".#/#."}},
            {":", {5, "#/./#"}},
            {" ", {3, "..."}},
            // The rest a Wi-Fi password may have (the keyboard's), and its keys' icons
            {"@", {3, ".###./#...#/#.###/#.#.#/#.##./.###."}},
            {"#", {3, ".#.#./#####/.#.#./.#.#./#####/.#.#."}},
            {"$", {2, "..#../.####/#.#../.###./..#.#/####./..#.."}},
            {"%", {3, "##.../##..#/..#../.#.../#..##/...##"}},
            {"&", {3, ".##../#..#./.##../#.#.#/#..#./.##.#"}},
            {"*", {4, "..#../#.#.#/.###./#.#.#/..#.."}},
            {"(", {2, ".#/#./#./#./#./#./.#"}},
            {")", {2, "#./.#/.#/.#/.#/.#/#."}},
            {"_", {9, "#####"}},
            {";", {5, ".#/../.#/#."}},
            {"/", {3, "....#/...#./..#../..#../.#.../#...."}},
            {"\\", {3, "#..../.#.../..#../..#../...#./....#"}},
            {"'", {3, "#/#"}},
            {"\"", {3, "#.#/#.#"}},
            {"<", {4, "...#/..#./.#../..#./...#"}},
            {">", {4, "#.../.#../..#./.#../#..."}},
            {"[", {2, "##/#./#./#./#./#./##"}},
            {"]", {2, "##/.#/.#/.#/.#/.#/##"}},
            {"{", {2, ".##/.#./.#./#../.#./.#./.##"}},
            {"}", {2, "##./.#./.#./..#/.#./.#./##."}},
            {"~", {5, ".#.../#.#.#/...#."}},
            {"^", {3, "..#../.#.#./#...#"}},
            {"|", {2, "#/#/#/#/#/#/#"}},
            {"`", {2, "#./.#"}},
            {"⇧", {3, "..#../.###./#####/.###./.###./.###."}},
            {"⌫", {4, "..#../.##../#####/.##../..#.."}},
            {"✓", {4, "....#/...#./#.#../.#..."}},
            {"✗", {3, "#...#/.#.#./..#../..#../.#.#./#...#"}},
            {"…", {8, "#.#.#"}},
            {"◀", {3, "..#/.##/###/###/.##/..#"}},
            {"▶", {3, "#../##./###/###/##./#.."}},
            {"␣", {7, "#...#/#####"}},
        };
        Store s;
        size_t rows = kDigitRows * 10, letters = 10 + std::size(kSameAs);
        const auto count = [&](const auto &table) {
            for (const Glyph &glyph : table) rows += rows_in(glyph.second.second), letters++;
        };
        count(small), count(capitals), count(signs);
        for (const Marked &m : kMarked) rows += (m.small ? 2 : 1) * kLineRows, letters += m.small ? 2 : 1; // at most a line each
        s.rows.reserve(rows);
        s.letters.reserve(letters);
        const auto add = [&s](const Glyph &glyph, int down) {
            s.letters.push_back({glyph.first, {glyph.second.first + down, split_into(s.rows, glyph.second.second)}});
        };
        for (const Glyph &glyph : small) add(glyph, 2); // the font's line, two rows down on Cubey's
        for (const Glyph &glyph : capitals) add(glyph, 0);
        for (const Glyph &glyph : signs) add(glyph, 0);
        constexpr std::string_view digits = "0123456789";
        for (char d = '0'; d <= '9'; d++) { // its digits, on the capitals' rows
            const size_t first = s.rows.size();
            s.rows.insert(s.rows.end(), digit(d).rows, digit(d).rows + kDigitRows);
            s.letters.push_back({digits.substr(static_cast<size_t>(d - '0'), 1), {kCapitalTop, {s.rows.data() + first, kDigitRows}}});
        }
        for (const auto &[same, as] : kSameAs) s.letters.push_back({same, *s.built(as)});
        for (const Marked &m : kMarked) {
            if (m.small) s.letters.push_back({m.small, marked(s.rows, *s.built(m.small_of), m.mark, false)});
            s.letters.push_back({m.capital, marked(s.rows, *s.built(m.capital_of), m.mark, true)});
        }
        s.sort();
        return s;
    }();
    return store.find(utf8);
}

const Letter *tiny(const std::string &utf8) {
    static const Store store = [] {
        static const std::pair<const char *, const char *> drawn[] = {
            {"A", ".#./#.#/###/#.#"}, {"B", "##./##./#.#/##."}, {"C", ".##/#../#../.##"}, {"D", "##./#.#/#.#/##."},
            {"E", "###/##./#../###"}, {"F", "###/##./#../#.."}, {"G", ".##/#../#.#/.##"}, {"H", "#.#/###/#.#/#.#"},
            {"I", "###/.#./.#./###"}, {"J", "..#/..#/#.#/.#."}, {"K", "#.#/##./#.#/#.#"}, {"L", "#../#../#../###"},
            {"M", "#...#/##.##/#.#.#/#...#"}, {"N", "#..#/##.#/#.##/#..#"}, {"O", ".#./#.#/#.#/.#."},
            {"P", "##./#.#/##./#.."}, {"Q", ".#./#.#/##./.##"}, {"R", "##./#.#/##./#.#"}, {"S", ".##/#../..#/##."},
            {"T", "###/.#./.#./.#."}, {"U", "#.#/#.#/#.#/###"}, {"V", "#.#/#.#/#.#/.#."}, {"W", "#...#/#.#.#/##.##/#...#"},
            {"X", "#.#/.#./.#./#.#"}, {"Y", "#.#/#.#/.#./.#."}, {"Z", "###/.#./#../###"},
        };
        Store s;
        size_t rows = 0;
        for (const auto &glyph : drawn) rows += rows_in(glyph.second);
        s.rows.reserve(rows);
        s.letters.reserve(std::size(drawn));
        for (const auto &[name, glyph] : drawn) s.letters.push_back({name, {0, split_into(s.rows, glyph)}});
        s.sort();
        return s;
    }();
    return store.find(upper(utf8));
}

std::vector<std::string> split(const std::string &utf8) {
    std::vector<std::string> out;
    for (size_t i = 0; i < utf8.size();) {
        size_t n = 1;
        while (i + n < utf8.size() && (static_cast<unsigned char>(utf8[i + n]) & 0xC0) == 0x80) n++;
        out.push_back(utf8.substr(i, n));
        i += n;
    }
    return out;
}

std::string upper(const std::string &utf8) {
    if (utf8.size() == 1 && utf8[0] >= 'a' && utf8[0] <= 'z') return std::string(1, static_cast<char>(utf8[0] - 'a' + 'A'));
    for (const auto &[small, capital] : kCapitals)
        if (utf8 == small) return capital;
    for (const Marked &m : kMarked)
        if (m.small && utf8 == m.small) return m.capital;
    return utf8;
}

} // namespace cubey::font
