#include <iostream>
#include <thread>
#include <fstream>
#include <string>
#include <vector>
#include <boost\filesystem.hpp>
#include <boost\algorithm\string.hpp>
#include <assert.h>
#include <unordered_map>

class StatBase {
private:
	std::unordered_map<std::string, int> counter_;
	float mean_;
	float variance_;
	float max_;
	float min_;
};

class ColumnsInfo {
public:
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
	}

	const std::vector<std::pair<std::string, std::string>> cols_info() const {
		return cols_info_;
	}

	const std::pair<std::string, std::string >& cols_info(int i) const {
		assert(i < cols_info_.size());
		return cols_info_[i];
	}

private:
	std::vector<std::pair<std::string, std::string>> cols_info_;
};

void stat_worker(const std::string& filepath, const std::string& delim,
		bool remove_first_line, bool remove_first_col) {

	std::ifstream inp_file(filepath);
	if (!inp_file) {
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

	}
}

/*
usage: main.exe remove_first_row[1] remove_first_col[1] delimer[t] float-age,categorical-name,string-title file_dir
*/
int main(int argc, char* argv[]) {
	std::string usage = "usage: main.exe re_first_row[1] re_first_col[1] delim[t]"
		"float-age,categorical-name,string-title file_dir";
	
	if (argc < 6) {
		throw usage;
	}
	bool remove_first_row = (argv[1] == "1");
	bool remove_first_col = (argv[2] == "1");
	std::string delim = argv[3];
	std::string dirname = argv[argc - 1];
	boost::filesystem::path datadir(dirname);
	boost::filesystem::directory_iterator dir_iter(datadir);
	for (; dir_iter != boost::filesystem::directory_iterator(); dir_iter++) {
		if (boost::filesystem::is_directory(*dir_iter)) {
			continue;
		}
		std::cout << dir_iter->path().string() << std::endl;
	}
};