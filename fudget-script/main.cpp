#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <iomanip>
#include <random>
#include <chrono>
#include <algorithm>
#include <regex>
#include <cstdint>
#include <cmath>
#include <cctype>

// populate_json.cpp
// Compile: g++ -std=c++17 populate_json.cpp -o populate_json
// Run:
// ./populate_json "input.json" "ACCOUNT_RETAIL.csv" "ACCOUNT_PREPAID.csv" "SAVING_HV.csv" "output.json" "json_key_for_retail" "json_key_for_prepaid" "json_key_for_saving"

using namespace std;

string key_retail;
string key_prepaid;
string key_saving;

static std::mt19937_64 rng((unsigned)std::chrono::system_clock::now().time_since_epoch().count());

string gen_uuid_v4() {
    uint64_t a = rng();
    uint64_t b = rng();
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << uint32_t(a >> 32)
        << "-"
        << std::setw(4) << uint16_t((a >> 16) & 0xffff)
        << "-"
        << std::setw(4) << uint16_t(((a) & 0x0fff) | 0x4000)
        << "-"
        << std::setw(4) << uint16_t(((b >> 48) & 0x3fff) | 0x8000)
        << "-"
        << std::setw(12) << (b & 0xffffffffffffULL);
    return oss.str();
}

static inline string& ltrim(string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [](unsigned char ch) { return !std::isspace(ch); }));
    return s;
}
static inline string& rtrim(string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
        [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    return s;
}
static inline string& trim(string& s) { return ltrim(rtrim(s)); }

vector<string> split_csv_line(const string& line) {
    vector<string> out;
    string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == ';') {
            out.push_back(cur);
            cur.clear();
        }
        else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

double parse_euro_number_to_double(string s) {
    trim(s);
    if (s.empty()) return NAN;
    s.erase(remove(s.begin(), s.end(), ' '), s.end());
    bool has_comma = (s.find(',') != string::npos);
    if (has_comma) {
        s.erase(remove(s.begin(), s.end(), '.'), s.end());
        std::replace(s.begin(), s.end(), ',', '.');
    }
    try {
        return stod(s);
    }
    catch (...) {
        return NAN;
    }
}

long long date_ddmmyyyy_to_epoch_ms(const string& s) {
    string t = s;
    trim(t);
    if (t.empty()) return 0;
    int d = 0, m = 0, y = 0;
    if (sscanf_s(t.c_str(), "%d.%d.%d", &d, &m, &y) != 3) {
        if (sscanf_s(t.c_str(), "%d/%d/%d", &d, &m, &y) != 3) {
            return 0;
        }
    }
    struct tm tm {};
    tm.tm_mday = d;
    tm.tm_mon = m - 1;
    tm.tm_year = y - 1900;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    time_t tt = mktime(&tm);
    if (tt == -1) return 0;
    long long ms = (long long)tt * 1000LL;
    return ms;
}

int find_max_order_in_json(const string& jsonText, const string& entriesKey) {
    size_t pos = jsonText.find("\"" + entriesKey + "\"");
    if (pos == string::npos) return 0;
    size_t brace = jsonText.find('{', pos);
    if (brace == string::npos) return 0;
    int depth = 0;
    size_t i = brace;
    size_t end = string::npos;
    for (; i < jsonText.size(); ++i) {
        if (jsonText[i] == '{') depth++;
        else if (jsonText[i] == '}') {
            depth--;
            if (depth == 0) { end = i; break; }
        }
    }
    if (end == string::npos) return 0;
    string inner = jsonText.substr(brace + 1, end - brace - 1);
    std::regex re(R"("order"\s*:\s*(\d+))");
    std::smatch m;
    int maxv = 0;
    string::const_iterator start(inner.cbegin()), finish(inner.cend());
    while (std::regex_search(start, finish, m, re)) {
        int val = stoi(m[1].str());
        if (val > maxv) maxv = val;
        start = m.suffix().first;
    }
    return maxv;
}

string insert_entries_into_json(const string& jsonText, const string& entriesKey, const string& entriesJsonToInsert) {
    string out = jsonText;
    size_t keypos = out.find("\"" + entriesKey + "\"");
    if (keypos == string::npos) {
        size_t rootClose = out.rfind('}');
        if (rootClose == string::npos) return out + "\n";
        string insertion = ",\n\t\"" + entriesKey + "\": {\n" + entriesJsonToInsert + "\n\t}";
        out.insert(rootClose, insertion);
        return out;
    }
    size_t braceOpen = out.find('{', keypos);
    if (braceOpen == string::npos) return out;
    int depth = 0;
    size_t i = braceOpen;
    size_t braceClose = string::npos;
    for (; i < out.size(); ++i) {
        if (out[i] == '{') depth++;
        else if (out[i] == '}') {
            depth--;
            if (depth == 0) { braceClose = i; break; }
        }
    }
    if (braceClose == string::npos) return out;
    string inner = out.substr(braceOpen + 1, braceClose - braceOpen - 1);
    string tmp = inner;
    trim(tmp);
    bool empty = tmp.empty();
    string insertion;
    if (empty) {
        insertion = "\n" + entriesJsonToInsert + "\n";
    }
    else {
        insertion = ",\n" + entriesJsonToInsert + "\n";
    }

    out.insert(braceClose, insertion);
    return out;
}

string build_entries_json_fragment(const vector<string>& jsonEntries) {
    stringstream ss;
    for (size_t i = 0; i < jsonEntries.size(); ++i) {
        ss << "\t\t" << jsonEntries[i];
        if (i + 1 < jsonEntries.size())
            ss << ",";
        ss << "\n";
    }
    return ss.str();
}

string build_single_entry_json(const string& uuid, double amount, long long date_ms, bool isIncome, const string& name, int order) {
    std::ostringstream ss;
    ss << "\"" << uuid << "\": {\n";
    ss << "\t\t\t\"amount\": ";
    ss.setf(std::ios::fixed); ss << setprecision(2);
    ss << amount << ",\n";
    ss << "\t\t\t\"date\": " << date_ms << ",\n";
    ss << "\t\t\t\"id\": \"" << uuid << "\",\n";
    ss << "\t\t\t\"isIncome\": " << (isIncome ? "true" : "false") << ",\n";
    string safeName = name;
    size_t p = 0;
    while ((p = safeName.find('"', p)) != string::npos) { safeName.replace(p, 1, "\\\""); p += 2; }
    ss << "\t\t\t\"name\": \"" << safeName << "\",\n";
    ss << "\t\t\t\"order\": " << order << ",\n";
    ss << "\t\t\t\"paid\": false\n";
    ss << "\t\t}";
    return ss.str();
}

struct ParsedRow {
    double amount;
    bool isIncome;
    long long date_ms;
    string name;
};

int process_csv_to_entries(const string& csvPath, const string& jsonText, const string& entriesKey, vector<string>& outEntries) {
    ifstream in(csvPath);
    if (!in) {
        cerr << "Warning: could not open CSV: " << csvPath << "\n";
        return 0;
    }
    string header;
    if (!std::getline(in, header)) {
        cerr << "Warning: CSV empty: " << csvPath << "\n";
        return 0;
    }
    auto hdrCols = split_csv_line(header);
    unordered_map<string, int> colIndex;
    for (size_t i = 0; i < hdrCols.size(); ++i) {
        string h = hdrCols[i];
        trim(h);
        colIndex[h] = (int)i;
    }
    auto find_col = [&](const string& want)->int {
        for (auto& kv : colIndex) {
            if (kv.first == want) return kv.second;
        }
        for (auto& kv : colIndex) {
            string k = kv.first;
            string lowk = k; string loww = want;
            transform(lowk.begin(), lowk.end(), lowk.begin(), ::tolower);
            transform(loww.begin(), loww.end(), loww.begin(), ::tolower);
            if (lowk.find(loww) != string::npos) return kv.second;
            if (loww.find(lowk) != string::npos) return kv.second;
        }
        return -1;
        };

    int idx_DATUM_KNJIZENJA = find_col("DATUM KNJIŽENJA");
    int idx_NAMEN = find_col("NAMEN");
    int idx_DOBRO = find_col("DOBRO");
    int idx_BREME = find_col("BREME");

    if (idx_DATUM_KNJIZENJA < 0 || idx_NAMEN < 0 || (idx_DOBRO < 0 && idx_BREME < 0)) {
        cerr << "Warning: CSV header missing expected columns in " << csvPath << "\n";
    }

    int max_order_existing = find_max_order_in_json(jsonText, entriesKey);

    vector<ParsedRow> parsed;
    string line;
    while (std::getline(in, line)) {
        if (line.size() == 0) continue;
        auto cols = split_csv_line(line);
        string s_date = (idx_DATUM_KNJIZENJA >= 0 && idx_DATUM_KNJIZENJA < (int)cols.size()) ? cols[idx_DATUM_KNJIZENJA] : "";
        string s_name = (idx_NAMEN >= 0 && idx_NAMEN < (int)cols.size()) ? cols[idx_NAMEN] : "";
        string s_dobro = (idx_DOBRO >= 0 && idx_DOBRO < (int)cols.size()) ? cols[idx_DOBRO] : "";
        string s_breme = (idx_BREME >= 0 && idx_BREME < (int)cols.size()) ? cols[idx_BREME] : "";

        trim(s_date); trim(s_name); trim(s_dobro); trim(s_breme);
        bool isIncome = false;
        double amount = 0.0;
        if (!s_dobro.empty()) {
            double v = parse_euro_number_to_double(s_dobro);
            if (!isnan(v)) { amount = v; isIncome = true; }
            else continue;
        }
        else if (!s_breme.empty()) {
            double v = parse_euro_number_to_double(s_breme);
            if (!isnan(v)) { amount = -v; isIncome = false; }
            else continue;
        }
        else {
            continue;
        }
        long long date_ms = date_ddmmyyyy_to_epoch_ms(s_date);
        ParsedRow pr;
        pr.amount = amount;
        pr.isIncome = isIncome;
        pr.date_ms = date_ms;
        pr.name = s_name;
        parsed.push_back(pr);
    }

    int k = (int)parsed.size();
    int added = 0;
    for (int i = 0; i < k; ++i) {
        int order = max_order_existing + (k - i);
        const ParsedRow& pr = parsed[i];
        string uuid = gen_uuid_v4();
        string single = build_single_entry_json(uuid, pr.amount, pr.date_ms, pr.isIncome, pr.name, order);
        outEntries.push_back(single);
        ++added;
    }

    return added;
}

int main(int argc, char* argv[]) {
    if (argc < 6) {
        cerr << "Usage: " << argv[0] << " input.json ACCOUNT_RETAIL.csv ACCOUNT_PREPAID.csv SAVING_HV.csv output.json json_key_for_retail json_key_for_prepaid json_key_for_saving\n";
        cerr << "Example:\n";
        cerr << argv[0] << " template.json ACCOUNT_RETAIL.csv ACCOUNT_PREPAID.csv /mnt/data/SAVING_HV~4011010003508749~3508749-promet.csv output.json entries_cea64cz1-2223-5be2-aa08-12d14f21z722 entries_cea64cz1-2223-5be2-aa08-12d14f21z723 entries_cea64cz1-2223-5be2-aa08-12d14f21z724\n";
        return 1;
    }

    string inputJsonPath = argv[1];
    string retailCsv = argv[2];
    string prepaidCsv = argv[3];
    string savingCsv = argv[4];
    string outputJsonPath = argv[5];
    key_retail = argv[6];
    key_prepaid = argv[7];
    key_saving = argv[8];


    ifstream jf(inputJsonPath);
    if (!jf) {
        cerr << "Error: cannot open input JSON: " << inputJsonPath << "\n";
        return 1;
    }
    stringstream buf;
    buf << jf.rdbuf();
    string jsonText = buf.str();

    vector<string> entriesRetail, entriesPrepaid, entriesSaving;
    int addedRetail = 0, addedPrepaid = 0, addedSaving = 0;
    string newJson = jsonText;

    if (retailCsv != "") {
        addedRetail = process_csv_to_entries(retailCsv, jsonText, key_retail, entriesRetail);
        string fragRetail = build_entries_json_fragment(entriesRetail);
        if (!entriesRetail.empty()) newJson = insert_entries_into_json(newJson, key_retail, fragRetail);
    }
    if (prepaidCsv != "") {
        addedPrepaid = process_csv_to_entries(prepaidCsv, jsonText, key_prepaid, entriesPrepaid);
        string fragPrepaid = build_entries_json_fragment(entriesPrepaid);
        if (!entriesPrepaid.empty()) newJson = insert_entries_into_json(newJson, key_prepaid, fragPrepaid);
    }
    if (savingCsv != "") {
        addedSaving = process_csv_to_entries(savingCsv, jsonText, key_saving, entriesSaving);
        string fragSaving = build_entries_json_fragment(entriesSaving);
        if (!entriesSaving.empty()) newJson = insert_entries_into_json(newJson, key_saving, fragSaving);
    }

    ofstream out(outputJsonPath);
    if (!out) {
        cerr << "Error: cannot open output file: " << outputJsonPath << "\n";
        return 1;
    }
    out << newJson;
    out.close();

    cout << "Done. Added entries - Retail: " << addedRetail << ", Prepaid: " << addedPrepaid << ", Saving: " << addedSaving << "\n";
    cout << "Output written to " << outputJsonPath << "\n";
    return 0;
}
