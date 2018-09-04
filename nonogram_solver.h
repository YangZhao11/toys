#include <memory>
#include <vector>
#include "neuronet.hpp"

enum class CellState { EMPTY, SOLID, CROSSED };

enum class Direction { EMPTY, ROW, COLUMN };
struct LineName {
  Direction dir = Direction::EMPTY;
  int index = 0;

  // comparison needed for use as key
  bool operator==(const LineName &o) const {
    return dir == o.dir && index == o.index;
  }

  static LineName Row(int i) {
    LineName n{.dir = Direction::ROW, .index = i};
    return n;
  }

  static LineName Column(int i) {
    LineName n{.dir = Direction::COLUMN, .index = i};
    return n;
  }
};

// implement hash for LineName to use it as map key
namespace std {
template <>
struct hash<LineName> {
  std::size_t operator()(const LineName &n) const {
    return std::hash<Direction>()(n.dir) ^ std::hash<int>()(n.index);
  }
};
}  // namespace std

struct LineStats {
  int wiggleRoom;    // max wiggle for a segment
  int numSegments;   // number of segment constraints
  int doneSegments;  // number of segments marked done
  int numChanges;    // number of changes since last examination
};

class Solver;

class Slice {
  Solver &solver_;
  const std::vector<CellState> &g_;
  int offset0_;
  int step_;
  int length_;

 public:
  Slice(Solver &solver, int offset0, int step, int length);
  Slice(Solver &solver, LineName name);

  CellState get(int i) const { return g_[offset0_ + step_ * i]; };
  void set(int i, CellState s) const;

  int length() const { return length_; };

  // returns the first position >= start where a hole (no X) of size
  // length is found. If no such hole is found, return -1.
  int findHoleStartingAt(int start, int length) const;

  // returns the length of strip (consecutive same state cells)
  // starting at position i.
  int stripLength(int i) const;

  int indexOfNextSolid(int start, int bound) const;

  // setSegment between i and j (exclusive) to state val.
  int setSegment(int i, int j, CellState val) const;

  Slice reverse() const;
};

class Line {
 private:
  const std::vector<int> len_;
  std::vector<int> lb_;
  std::vector<int> ub_;
  std::vector<bool> done_;
  const Slice slice_;

  int numSegments() { return len_.size(); };
  int len(int i) { return len_[i]; };
  int lb(int i) { return lb_[i]; };
  int ub(int i) { return slice_.length() - ub_[ub_.size() - 1 - i] - 1; };
  bool done(int i) { return done_[i]; };

  static bool fitLeftMost(Slice slice, const std::vector<int> &len,
                          std::vector<int> &lb);

  // returns a segment index ranges (left inclusive, right exclusive)
  // that lb(i) <= start and ub(i) >= end.
  std::pair<int, int> collidingSegments(int start, int end);

 public:
  Line(Solver &solver, LineName name, std::vector<int> &&len);
  void updateStats();
  bool inferSegments();
  bool inferStrips();
  bool infer();

  LineName name;
  LineStats stats;

  struct State {
    std::vector<int> lb;
    std::vector<int> ub;
    std::vector<bool> done;

    explicit State(const Line &l);
    State() = default;
    State(const State &) = default;
    State(State &&) = default;
    State &operator=(const State &) = default;
    State &operator=(State &&) = default;
  };
  State getState() const;
  void setState(State &&s);
};

constexpr int edgeScoreLen = 5;  // special treatment of edge
constexpr int gridHalfEdge = 2;  // neuronet grid size (5x5)
constexpr int gridSize = (2 * gridHalfEdge + 1) * (2 * gridHalfEdge + 1);

class Solver {
 public:
  struct Config {
    // for getDirty sorting
    double wiggleRoom;
    double numSegments;
    double doneSegments;
    double numChanges;

    double LineScore(const LineStats &s) const {
      return wiggleRoom * s.wiggleRoom + numSegments * s.numSegments +
             doneSegments * s.doneSegments + numChanges * s.numChanges;
    };

    // for make a guess at X,Y
    double rowCoef;
    double colCoef;
    double edgeScore[edgeScoreLen];
    std::unique_ptr<Net> n;

    std::pair<double, CellState> GuessScore(const Solver &s, int x,
                                            int y) const;
    int maxLines;  // number of lines to check before failing
  };
  const Config &config_;

  const int width_;
  const int height_;
  std::vector<CellState> g_;
  LineName lineName_;  // the line we are working on
  bool failed_ = false;

  struct Guess {
    int x;
    int y;
    CellState val;

    static Guess Empty() {
      Guess g{.x = -1, .y = -1, .val = CellState::EMPTY};
      return g;
    };
    bool isEmpty() { return x == -1 && y == -1 && val == CellState::EMPTY; };
  } guessed_ = Guess::Empty();

  struct State {
    std::vector<CellState> g;
    std::vector<Line::State> lines;
    Guess guessed;
  };

  struct Stats {
    int lineCount = 0;
    int wrongGuesses = 0;
    int maxDepth = 0;
  } stats_;

 private:
  std::vector<std::unique_ptr<Line>> lines_;
  std::vector<LineName> dirty_;
  std::vector<State> states_;

 public:
  Solver(const Config &config, std::vector<std::vector<int>> &&rows,
         std::vector<std::vector<int>> &&cols);

  CellState get(int x, int y) const { return g_[x + y * width_]; };
  void set(int x, int y, CellState s);
  Line &getLine(LineName name) const {
    int i = name.dir == Direction::ROW ? name.index : name.index + height_;
    return *(lines_[i].get());
  };

  LineName getDirty();
  void markDirty(LineName n);
  std::vector<double> GridAt(int x, int y) const;

  void pushState();
  void popState();
  bool infer();
  Guess guess();
  bool solve();

  void printGrid();
};
