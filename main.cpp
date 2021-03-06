#include <assert.h>

#include <boost\algorithm\string.hpp>
#include <boost\filesystem.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class Statistic {
   public:
    Statistic(const std::string& name) : name_(name) {}
    virtual void Compute(const std::string& inp) = 0;
    virtual std::string str() const = 0;
    virtual ~Statistic(){};
    const std::string& name() { return name_; }
    std::mutex& update_mutex() { return update_mutex_; }

   private:
    std::string name_;
    std::mutex update_mutex_;
};

class NumericalStat : public Statistic {
   public:
    NumericalStat(const std::string& name) : Statistic(name) {}

    void Compute(const std::string& inp) override {
        double cur_val = std::stod(inp);

        std::lock_guard<std::mutex> guard(update_mutex());

        this->update_mean(cur_val);
        this->update_min_max(cur_val);
    }
    std::string str() const override {
        std::ostringstream ostream;
        ostream << "mean: " << cur_mean_ << "\n";
        ostream << "min: " << min_ << "\n";
        ostream << "max: " << max_ << "\n";
        return ostream.str();
    }

    static std::shared_ptr<Statistic> instance(const std::string& name, const std::vector<std::string>&) {
        return std::make_shared<NumericalStat>(name);
    }

   private:
    void update_mean(double cur_val) {
        if (n_ == 0) {
            cur_mean_ = cur_val;
        } else {
            cur_mean_ = static_cast<double>(n_) / static_cast<double>(n_ + 1) * cur_mean_ + cur_val / (n_ + 1);
        }
        n_++;
    }
    void update_min_max(double cur_val) {
        if (cur_val > max_) max_ = cur_val;
        if (cur_val < min_) min_ = cur_val;
    }

   private:
    double cur_mean_ = 0.0;
    uint64_t n_ = 0;
    double min_ = DBL_MAX;
    double max_ = DBL_MIN;
};

// CounterStat: is used to count the freqency of the categrical fild
// Example:
//		CounterStat(std::vector<std::string>{}); is used to count the freqency of the caterical field
//		CounterStat({" ", ""}); if the inp is a english sentence, this is used to count the freqency of the character.
class CounterStat : public Statistic {
   public:
    typedef std::unordered_map<std::string, uint64_t> Counter;
    typedef std::pair<std::string, uint64_t> StrUint64Pair;

    CounterStat(const std::string& name, const std::vector<std::string>& delims) : delims_(delims), Statistic(name) {
        counters_ = std::vector<Counter>{delims_.size() + 1, Counter{}};
        assert(delims_.size() + 1 == counters_.size());
    }
    void Compute(const std::string& inp) override {
        std::lock_guard<std::mutex> guard(update_mutex());
        this->RecusivelyCompute(inp, 0);
    }

    std::string str() const override {
        std::vector<StrUint64Pair> tmp_vec;
        std::ostringstream ostream;
        for (const auto& counter : counters_) {
            tmp_vec.clear();
            std::copy(counter.begin(), counter.end(), std::back_inserter<std::vector<StrUint64Pair>>(tmp_vec));
            std::sort(tmp_vec.begin(), tmp_vec.end(),
                      [](const StrUint64Pair& left, const StrUint64Pair& right) -> bool {
                          return left.second > right.second;
                      });
            for (const auto& item : tmp_vec) {
                ostream << item.first << "\t" << item.second << "\n";
            }
        }
        return ostream.str();
    }

    static std::shared_ptr<Statistic> instance(const std::string& name, const std::vector<std::string>& delims) {
        return std::make_shared<CounterStat>(name, delims);
    }

   private:
    void RecusivelyCompute(const std::string& inp, int level) {
        Counter& cur_level_counter = counters_[level];
        if (cur_level_counter.find(inp) == cur_level_counter.end()) {
            cur_level_counter[inp] = 0;
        }
        cur_level_counter[inp]++;
        if (level < delims_.size()) {
            const std::string& cur_delim = delims_[level];
            std::vector<std::string> splited_res;
            boost::split(splited_res, inp, boost::is_any_of(cur_delim), boost::token_compress_on);
            for (const auto& item : splited_res) {
                RecusivelyCompute(item, level + 1);
            }
        }
    }

   private:
    std::vector<std::string> delims_;
    std::vector<Counter> counters_;
};

class StatManager {
   public:
    static StatManager& instance() {
        static StatManager stat_manager;
        return stat_manager;
    }

    std::shared_ptr<Statistic> GetStatistic(const std::string& stat_name, const std::string& name,
                                            const std::vector<std::string>& delims) {
        if (manager_.find(stat_name) == manager_.end()) {
            std::ostringstream oss;
            oss << stat_name << " not registered in StatManager" << std::endl;
            throw oss.str();
        }
        return manager_[stat_name](name, delims);
    }

   private:
    StatManager() {
        manager_.insert(std::make_pair("numerical", NumericalStat::instance));
        manager_.insert(std::make_pair("categorical", CounterStat::instance));
    }

   private:
    std::unordered_map<std::string,
                       std::function<std::shared_ptr<Statistic>(const std::string&, const std::vector<std::string>&)>>
        manager_;
};

class ColumnsInfo {
   public:
    typedef std::pair<std::string, std::string> StrStrPair;

    ColumnsInfo(const std::string& columnsinfo) {
        std::vector<std::string> fields_list;
        boost::split(fields_list, columnsinfo, boost::is_any_of(","));
        std::vector<std::string> tmp_pair;
        for (const auto& item : fields_list) {
            tmp_pair.clear();
            boost::split(tmp_pair, item, boost::is_any_of("-"));
            assert(tmp_pair.size() == 2);
            cols_info_.push_back(std::make_pair(tmp_pair[0], tmp_pair[1]));
        }
        set_cols_stats();
    }

    const std::vector<StrStrPair> cols_info() const { return cols_info_; }

    const StrStrPair& cols_info(int i) const {
        assert(i < cols_info_.size());
        return cols_info_[i];
    }

    std::vector<std::shared_ptr<Statistic>>& cols_stats() { return cols_stats_; }

    void DumpStatInfo() {
        // TODO
        std::cout << "dumping stat info" << std::endl;
        for (size_t i = 0; i < cols_stats_.size(); i++) {
            std::cout << cols_stats_[i]->str() << std::endl;
        }
    }

   private:
    void set_cols_stats() {
        for (const auto& info_pair : cols_info_) {
            cols_stats_.push_back(
                StatManager::instance().GetStatistic(info_pair.first, info_pair.second, std::vector<std::string>()));
        }
    }

   private:
    // [[stat_type, name]]
    std::vector<StrStrPair> cols_info_;
    std::vector<std::shared_ptr<Statistic>> cols_stats_;
};

void stat_worker(const std::string& filepath, const std::string& delim, bool remove_first_line, bool remove_first_col,
                 ColumnsInfo& column_info) {
    std::cout << "processing " << filepath << std::endl;
    std::vector<std::shared_ptr<Statistic>>& statistics = column_info.cols_stats();
    std::ifstream inp_file(filepath);
    if (!inp_file) {
        std::cerr << "open file " << filepath << " error!" << std::endl;
        return;
    }
    bool first_line = true;
    std::string line;
    std::vector<std::string> line_items;
    while (std::getline(inp_file, line)) {
        if (first_line && remove_first_line) {
            first_line = false;
            continue;
        }
        line_items.clear();
        boost::split(line_items, line, boost::is_any_of(delim));
        if (remove_first_col) {
            line_items.erase(line_items.begin());
        }
        assert(line_items.size() == statistics.size());

        for (int i = 0; i < line_items.size(); i++) {
            statistics[i]->Compute(line_items[i]);
        }
    }
    std::cout << "processing " << filepath << " DONE!" << std::endl;
}

enum CommandLineParam { REMOVE_FIRST_ROW = 1, REMOVE_FIRST_COL = 2, DELIM = 3, COL_INFO = 4, FILE_DIR = 5 };

/*
usage: main.exe remove_first_row[1] remove_first_col[1] delimer[t] float-age,categorical-name,string-title file_dir
*/
int main(int argc, char* argv[]) {
    std::string usage =
        "usage: main.exe re_first_row[1] re_first_col[1] delim[t]"
        "float-age,categorical-name,string-title file_dir";

    if (argc < 6) {
        throw usage;
    }
    bool remove_first_row = (argv[REMOVE_FIRST_ROW] == "1");
    bool remove_first_col = (argv[REMOVE_FIRST_COL] == "1");
    std::string delim = argv[DELIM];
    std::string dirname = argv[FILE_DIR];
    boost::filesystem::path datadir(dirname);
    boost::filesystem::directory_iterator dir_iter(datadir);

    ColumnsInfo col_info(argv[COL_INFO]);

    for (; dir_iter != boost::filesystem::directory_iterator(); dir_iter++) {
        if (boost::filesystem::is_directory(*dir_iter)) {
            continue;
        }
        stat_worker(dir_iter->path().string(), delim, remove_first_row, remove_first_col, col_info);
    }
    col_info.DumpStatInfo();
};