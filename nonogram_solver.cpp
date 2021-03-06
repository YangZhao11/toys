#include "nonogram_solver.h"
#include <iostream>

// Slice implementation

Slice::Slice(Solver &solver, int offset0, int step, int length)
    : solver_(solver),
      g_(solver.g_),
      offset0_(offset0),
      step_(step),
      length_(length){};

Slice::Slice(Solver &solver, LineName name) : solver_(solver), g_(solver.g_) {
  if (name.dir == Direction::ROW) {
    offset0_ = solver.width_ * name.index;
    step_ = 1;
    length_ = solver.width_;
  } else {
    offset0_ = name.index;
    step_ = solver.width_;
    length_ = solver.height_;
  }
}

void Slice::set(int i, CellState s) const {
  int o = offset0_ + step_ * i;
  int x = o % solver_.width_;
  int y = (o - x) / solver_.width_;
  solver_.set(x, y, s);
}

int Slice::findHoleStartingAt(int start, int length) const {
  int found = 0;
  for (int i = start; i < length_; i++) {
    if (get(i) == CellState::CROSSED) {
      found = 0;
    } else {
      found += 1;
      if (found >= length) {
        return i - found + 1;
      }
    }
  }
  return -1;
};

int Slice::stripLength(int i) const {
  CellState val = get(i);
  int n = 0;
  for (; i < length_; i++) {
    if (get(i) == val) {
      n++;
    } else {
      return n;
    }
  }
  return n;
}

int Slice::indexOfNextSolid(int start, int bound) const {
  for (int i = start; i < bound; i++) {
    if (get(i) == CellState::SOLID) {
      return i;
    }
  }
  return -1;
}

// setSegment between i and j (exclusive) to state val. Return number
// of cells changed.
int Slice::setSegment(int i, int j, CellState val) const {
  int changed = 0;
  for (int n = i; n < j; n++) {
    if (get(n) != val) {
      set(n, val);
      changed++;
    }
  }
  return changed;
}

Slice Slice::reverse() const {
  return Slice(solver_, offset0_ + step_ * (length_ - 1), -step_, length_);
}

// Line implementation
Line::Line(Solver &solver, LineName name, std::vector<int> &&len)
    : len_(len),
      lb_(len.size()),
      ub_(len.size()),
      done_(len.size()),
      slice_(solver, name),
      name(name) {
  int sum = 0;
  for (auto l : len) {
    sum += l;
  }
  stats.wiggleRoom = slice_.length() - sum;
  stats.numSegments = len.size();
  stats.doneSegments = 0;
  stats.numChanges = 0;
}

void Line::updateStats() {
  int w = 0;

  for (int i = 0; i < lb_.size(); i++) {
    int wi = ub(i) - lb(i) + 1 - len_[i];
    if (w < wi) w = wi;
  }
  stats.wiggleRoom = w;

  int total_done = 0;
  for (auto d : done_) {
    if (d) {
      total_done++;
    }
  }
  stats.doneSegments = total_done;
  stats.numChanges = 0;
}

// Fit all segments to the leftmost position in slice, satisfying the
// constraints. Also obey known lower bound. Returns false when no fit
// can be found.
bool Line::fitLeftMost(Slice slice, const std::vector<int> &len,
                       std::vector<int> &lb) {
  int cursor = 0;  // cursor tracks a position in slice
  int i = 0;       // i is an index of sLen / lb

  while (cursor < slice.length()) {
    int lBound = i >= len.size() ? slice.length() : lb[i];
    if (lBound > cursor) {
      int nextSolid = slice.indexOfNextSolid(cursor, lBound);
      if (nextSolid == -1) {
        cursor = lBound;
        continue;
      }

      // we need to pull a segment here to cover nextSolid.
      int stripLen = slice.stripLength(nextSolid);
      do {
        i--;
      } while (i >= 0 && len[i] < stripLen);
      if (i < 0) {
        return false;
      }

      // move cursor back to where the pulled segment was.
      // Then pull the segment to the place where it'll
      // cover nextSolid. Skip to next round because we may
      // have exposed a SOLID strip when pulling segment i.
      cursor = lb[i];
      lb[i] = nextSolid + stripLen - len[i];
      continue;
    }
    // see if we can find a hole at cursor that's big enough.
    int hole = slice.findHoleStartingAt(cursor, len[i]);
    if (hole == -1) {
      return false;
    }

    // Move the segment forward if the tail is next to a solid
    // cell. Also remember if skipped over solids - need to
    // trace back and find an earlier segment to cover it in
    // that case.
    bool skippedSolid = false;
    while (hole + len[i] < slice.length() &&
           slice.get(hole + len[i]) == CellState::SOLID) {
      skippedSolid = skippedSolid || slice.get(hole) == CellState::SOLID;
      hole++;
    }
    lb[i] = hole;
    if (!skippedSolid) {
      // set next allowable position and work on next segment
      cursor = hole + len[i] + 1;
      i++;
    }
  }  // cursor loop
  // check remaining segment constraints
  if (i < len.size()) {
    return false;
  }
  return true;
}

bool Line::inferSegments() {
  for (int i = 0; i < numSegments(); i++) {
    int l = lb(i);
    int u = ub(i);
    int prevU = i > 0 ? ub(i - 1) : -1;

    if (l + len(i) - 1 > u) {
      return false;
    }

    if (l > prevU + 1) {
      slice_.setSegment(prevU + 1, l, CellState::CROSSED);
    }

    if (done(i)) {
      continue;
    }

    if (u - len(i) + 1 <= l + len(i) - 1) {
      slice_.setSegment(u - len(i) + 1, l + len(i), CellState::SOLID);
    }

    if (u - l + 1 == len(i)) {
      done_[i] = true;
    }
  }
  if (ub(numSegments() - 1) + 1 < slice_.length()) {
    slice_.setSegment(ub(numSegments() - 1) + 1, slice_.length(),
                      CellState::CROSSED);
  }
  return true;
}

// returns a segment index ranges (left inclusive, right exclusive)
// that lb(i) <= start and ub(i) >= end.
std::pair<int, int> Line::collidingSegments(int start, int end) {
  int first = 0, second = 0;
  bool found = false;
  for (int i = 0; i < numSegments(); i++) {
    if (ub(i) < end) {
      continue;
    }
    if (lb(i) <= start) {
      if (!found) {
        found = true;
        first = i;
      }
      second = i + 1;
    } else if (found) {
      break;
    }
  }
  return std::make_pair(first, second);
}

// Make inference for strips (consecutive cells with same state).
// Cases include:
//
// 1. "X X" can be marked "XXX" if all possible segments >= 2
// 2. "?SSS?" can be marked "XSSSX" if all possible segments =3.
// 3. "X SS " can be marked "X SSS" if all potential segments >= 4.
bool Line::inferStrips() {
  int stripLen = 0;

  for (int i = 0; i < slice_.length(); i += stripLen) {
    stripLen = slice_.stripLength(i);
    // this logic is never needed for slices at the edges
    if (i == 0 || i + stripLen == slice_.length()) {
      continue;
    }

    if (slice_.get(i) == CellState::EMPTY) {
      if (slice_.get(i - 1) != CellState::CROSSED ||
          slice_.get(i + stripLen) != CellState::CROSSED) {
        continue;
      }
      // find holes that's smaller than all potential
      // segments, and fill them with X.
      auto seg = collidingSegments(i, i + stripLen - 1);
      if (seg.first == seg.second) {
        continue;
      }
      int minLen = len(seg.first);
      for (int j = seg.first; j < seg.second; j++) {
        if (minLen > len(j)) {
          minLen = len(j);
        }
      }

      if (minLen <= stripLen) {
        continue;
      }

      slice_.setSegment(i, i + stripLen, CellState::CROSSED);
    } else if (slice_.get(i) == CellState::SOLID) {
      auto seg = collidingSegments(i, i + stripLen - 1);
      if (seg.first == seg.second) {
        continue;
      }
      if (seg.second - seg.first == 1 && done(seg.first)) {
        continue;
      }
      int maxLen = len(seg.first);
      int minLen = len(seg.first);
      for (int j = seg.first; j < seg.second; j++) {
        if (len(j) < minLen) {
          minLen = len(j);
        }
        if (len(j) > maxLen) {
          maxLen = len(j);
        }
      }

      for (int j = i + stripLen; j < i + minLen && j < slice_.length(); j++) {
        CellState s = slice_.get(j);
        if (s == CellState::SOLID) {
          break;
        }
        if (s == CellState::EMPTY) {
          continue;
        }
        // we have strip "SSS  X" and can prepend some S
        if (slice_.setSegment(j - minLen, i, CellState::SOLID) > 0) {
          stripLen += i - (j - minLen);
          i = j - minLen;
        }
        break;
      }
      for (int j = i - 1; j >= i + stripLen - minLen && j >= 0; j--) {
        CellState s = slice_.get(j);
        if (s == CellState::SOLID) {
          break;
        }
        if (s == CellState::EMPTY) {
          continue;
        }
        // we have strip "X  SSS" and can append some S
        if (slice_.setSegment(i + stripLen, j + minLen + 1, CellState::SOLID) >
            0) {
          stripLen += j + minLen + 1 - (i + stripLen);
        }
        break;
      }
      if (maxLen == stripLen) {
        slice_.setSegment(i - 1, i, CellState::CROSSED);
        slice_.setSegment(i + stripLen, i + stripLen + 1, CellState::CROSSED);
      }
    }
  }
  return true;
}

bool Line::infer() {
  // special case for no segments.
  if (numSegments() == 0) {
    slice_.setSegment(0, slice_.length(), CellState::CROSSED);
    return true;
  }

  // update left and right bounts
  if (!fitLeftMost(slice_, len_, lb_)) {
    return false;
  }
  std::vector<int> len_reverse(len_);
  std::reverse(len_reverse.begin(), len_reverse.end());
  if (!fitLeftMost(slice_.reverse(), len_reverse, ub_)) {
    return false;
  }
  updateStats();
  if (!inferSegments()) {
    return false;
  }
  if (!inferStrips()) {
    return false;
  }
  return true;
}

Line::State::State(const Line &l) : lb(l.lb_), ub(l.ub_), done(l.done_) {}

Line::State Line::getState() const { return Line::State(*this); }

void Line::setState(Line::State &&s) {
  lb_ = std::move(s.lb);
  ub_ = std::move(s.ub);
  done_ = std::move(s.done);
}

// Solver implementation

Solver::Solver(const Solver::Config &config,
               std::vector<std::vector<int>> &&rows,
               std::vector<std::vector<int>> &&cols)
    : config_(config),
      width_(cols.size()),
      height_(rows.size()),
      g_(cols.size() * rows.size(), CellState::EMPTY) {
  for (int i = 0; i < height_; i++) {
    lines_.push_back(
        std::make_unique<Line>(*this, LineName::Row(i), std::move(rows[i])));
    dirty_.push_back(LineName::Row(i));
  }
  for (int i = 0; i < width_; i++) {
    lines_.push_back(
        std::make_unique<Line>(*this, LineName::Column(i), std::move(cols[i])));
    dirty_.push_back(LineName::Column(i));
  }
}

void Solver::set(int x, int y, CellState val) {
  if (val == get(x, y)) {
    return;
  }
  if (get(x, y) != CellState::EMPTY) {
    failed_ = true;
    return;
  }

  g_[x + y * width_] = val;
  if (lineName_.dir != Direction::ROW) {
    markDirty(LineName::Row(y));
  }
  if (lineName_.dir != Direction::COLUMN) {
    markDirty(LineName::Column(x));
  }
};

void Solver::markDirty(LineName n) {
  auto f = std::find(dirty_.begin(), dirty_.end(), n);
  if (f == dirty_.end()) {
    dirty_.push_back(n);
    getLine(n).stats.numChanges++;
  }
};

LineName Solver::getDirty() {
  std::sort(dirty_.begin(), dirty_.end(),
            [this](LineName a, LineName b) -> bool {
              auto sa = config_.LineScore(this->getLine(a).stats);
              auto sb = config_.LineScore(this->getLine(b).stats);
              return sa < sb;
            });
  auto n = dirty_.back();
  dirty_.pop_back();
  return n;
};

void Solver::pushState() {
  Solver::State s;
  s.g = g_;
  s.guessed = guessed_;
  for (int i = 0; i < lines_.size(); i++) {
    s.lines.push_back(lines_[i]->getState());
  }

  states_.push_back(s);
  if (stats_.maxDepth < states_.size()) {
    stats_.maxDepth = states_.size();
  }
}

void Solver::popState() {
  Solver::State &s = states_.back();
  g_ = std::move(s.g);
  guessed_ = s.guessed;
  for (int i = 0; i < lines_.size(); i++) {
    lines_[i]->setState(std::move(s.lines[i]));
  }
  dirty_.clear();
  states_.pop_back();
}

// make inference on lines until all lines are checked.
bool Solver::infer() {
  while (dirty_.size() > 0) {
    lineName_ = getDirty();
    Line &line = getLine(lineName_);
    if (!line.infer()) {
      return false;
    };
    stats_.lineCount++;
    lineName_.dir = Direction::EMPTY;
    if (failed_ || stats_.lineCount >= config_.maxLines) {
      return false;
    }
  }
  return true;
}

std::pair<double, CellState> Solver::Config::GuessScore(const Solver &s, int x,
                                                        int y) const {
  double score = LineScore(s.getLine(LineName::Row(y)).stats) * rowCoef +
                 LineScore(s.getLine(LineName::Column(x)).stats) * colCoef;
  int minX = std::min(x, s.width_ - 1 - x);
  int minY = std::min(y, s.height_ - 1 - y);
  if (minX < edgeScoreLen) {
    score += edgeScore[minX];
  }
  if (minY < edgeScoreLen) {
    score += edgeScore[minY];
  }

  std::vector<double> pattern = s.GridAt(x, y);
  std::vector<double> pattern_score = n->evaluate(pattern);
  CellState val = CellState::SOLID;
  if (pattern_score[0] > pattern_score[1]) {
    score += pattern_score[0];
    val = CellState::CROSSED;
  } else {
    score += pattern_score[1];
  }

  //  return std::make_pair(score, val);
  return std::make_pair(score, val);
}

// picks an unwritten cell and make a guess. Returs object like
// {x,y,val}. Returns empty guess if everything has been filled.
Solver::Guess Solver::guess() {
  auto r = Solver::Guess::Empty();

  double maxScore = -std::numeric_limits<double>::infinity();

  for (int x = 0; x < width_; x++) {
    for (int y = 0; y < height_; y++) {
      if (get(x, y) != CellState::EMPTY) {
        continue;
      }

      CellState val;
      double score;
      std::tie(score, val) = config_.GuessScore(*this, x, y);

      if (score > maxScore) {
        r.x = x;
        r.y = y;
        r.val = val;
        maxScore = score;
      }
    }
  }
  return r;
}

// Returns a vector representing the grid around point x,y.
std::vector<double> Solver::GridAt(int x, int y) const {
  std::vector<double> g;
  g.reserve(gridSize);
  for (int i = x - gridHalfEdge; i <= x + gridHalfEdge; i++) {
    for (int j = y - gridHalfEdge; j <= y + gridHalfEdge; j++) {
      if (i < 0 || i >= width_ || j < 0 || j >= height_) {
        g.push_back(-1);
        continue;
      }
      switch (get(i, j)) {
        case CellState::SOLID:
          g.push_back(1);
          break;
        case CellState::EMPTY:
          g.push_back(0);
          break;
        case CellState::CROSSED:
          g.push_back(-1);
          break;
      }
    }
  }
  return g;
};

bool Solver::solve() {
  while (true) {
    if (!infer() || failed_) {
      if (states_.size() == 0) {
        return false;
      }
      failed_ = false;
      popState();
      set(guessed_.x, guessed_.y,
          guessed_.val == CellState::SOLID ? CellState::CROSSED
                                           : CellState::SOLID);
      stats_.wrongGuesses++;
      guessed_ = Solver::Guess::Empty();
    } else {
      auto g = guess();
      if (g.isEmpty()) {
        return true;
      }
      guessed_ = g;
      pushState();
      set(g.x, g.y, g.val);
    }
  }
}

void Solver::printGrid() {
  for (int y = 0; y < height_; y++) {
    for (int x = 0; x < width_; x++) {
      switch (get(x, y)) {
        case CellState::EMPTY:
          std::cout << ' ';
          break;
        case CellState::SOLID:
          std::cout << '#';
          break;
        case CellState::CROSSED:
          std::cout << '.';
          break;
      }
    }
    std::cout << '\n';
  }
}
