#include <fstream>
#include <iostream>
#include <vector>
#include "cxxopts.hpp"
#include "json.hpp"
#include "nonogram_solver.h"
#include "task_queue.h"

// PictureFile represents a nonogram puzzle.
struct PictureFile {
  std::vector<std::vector<int>> rows;
  std::vector<std::vector<int>> cols;
};

// read a nonogram puzzle from file. The content would be a json
// object with rows and cols field; each is an array of line segment
// constraints. See tv.json for an example.
PictureFile readPictureFile(std::string filename) {
  std::ifstream ifs(filename);
  std::string content(std::istreambuf_iterator<char>{ifs},
                      std::istreambuf_iterator<char>());

  auto j = nlohmann::json::parse(content);
  std::vector<std::string> rows = j["rows"];
  std::vector<std::string> cols = j["cols"];

  PictureFile p;
  for (std::string &r : rows) {
    std::istringstream iss(r);
    std::vector<int> parsed(std::istream_iterator<int>{iss},
                            std::istream_iterator<int>());
    p.rows.push_back(std::move(parsed));
  }
  for (std::string &r : cols) {
    std::istringstream iss(r);
    std::vector<int> parsed(std::istream_iterator<int>{iss},
                            std::istream_iterator<int>());
    p.cols.push_back(std::move(parsed));
  }
  return p;
};

// global config object
static Solver::Config config;

std::string RunSolver(std::string filename) {
  auto p = readPictureFile(filename);

  Solver s(config, std::move(p.rows), std::move(p.cols));
  bool solved = s.solve();
  std::ostringstream stringStream;

  stringStream << filename << (solved ? " solved " : " failed ") << s.width_
               << " " << s.height_ << " " << s.stats_.lineCount << " "
               << s.stats_.wrongGuesses << " " << s.stats_.maxDepth;

  return stringStream.str();
}

int main(int argc, char *argv[]) {
  cxxopts::Options options("nonogram", "nonogram solver");
  options.add_options()("config", "config json string",
                        cxxopts::value<std::string>())(
      "f,file", "json files to read",
      cxxopts::value<std::vector<std::string>>());
  options.parse_positional({"file"});
  auto opt = options.parse(argc, argv);

  if (!opt.count("f") || !opt.count("config")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  auto &files = opt["f"].as<std::vector<std::string>>();

  auto config_json = nlohmann::json::parse(opt["config"].as<std::string>());
  config.wiggleRoom = config_json["wiggleRoom"];
  config.numSegments = config_json["numSegments"];
  config.doneSegments = config_json["doneSegments"];
  config.numChanges = config_json["numChanges"];
  config.rowCoef = config_json["rowCoef"];
  config.colCoef = config_json["colCoef"];
  std::vector<double> edgeScore = config_json["edgeScore"];
  if (edgeScore.size() != edgeScoreLen) {
    return (1);
  }
  std::copy(edgeScore.begin(), edgeScore.end(), config.edgeScore);

  std::vector<std::vector<double>> coef = config_json["coef"];
  Net *net = new Net(coef, gridSize);
  if (net->dim_out() != 2) {
    std::cerr << "config: coef dimensionality error\n";
    return 1;
  }
  config.n = std::unique_ptr<Net>(net);
  config.maxLines = config_json["maxLines"];

  TaskQueue q(20);
  for (auto f : files) {
    q.Add([f]() -> std::string { return RunSolver(f); });
  }
  q.Close();

  std::optional<std::string> s;
  for (s = q.GetResult(); s;
       s = q.GetResult()) {
    std::cout << s.value() << std::endl;
  }
}
