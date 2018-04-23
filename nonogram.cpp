#include <iostream>
#include <vector>

enum state { EMPTY, SOLID, CROSSED };
enum direction { ROW, COLUMN };
struct LineName {
  direction dir;
  int index;
};
struct LineStats {
  int wiggleRoom;
};

class Solver;

class Slice {
  Solver &solver_;
  const std::vector<state> &g_;
  int offset0_;
  int step_;
  int length_;

 public:
  Slice(Solver &solver, int offset0, int step, int length);
  Slice(Solver &solver, LineName name);

  state get(int i) const { return g_[offset0_ + step_ * i]; };
  void set(int i, state s) const;

  int length() const { return length_; };

  // returns the first position >= start where a hole (no X) of size
  // length is found. If no such hole is found, return -1.
  int findHoleStartingAt(int start, int length) const;

  // returns the length of strip (consecutive same state cells)
  // starting at position i.
  int stripLength(int i) const;

  int indexOfNextSolid(int start, int bound) const;

  // setSegment between i and j (exclusive) to state val.
  int setSegment(int i, int j, state val) const;

  Slice reverse() const;
};

class Line {
 private:
  Solver &solver_;
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

    State(const Line &l);
  };
  State getState() const;
  void setState(State &&s);
};

class Solver {
 public:
  const int width_;
  const int height_;
  std::vector<state> g_;

  Solver(std::vector<std::vector<int>> rows,
         std::vector<std::vector<int>> cols);

  void set(int x, int y, state s);
};

// Slice implementation

Slice::Slice(Solver &solver, int offset0, int step, int length)
    : solver_(solver),
      g_(solver.g_),
      offset0_(offset0),
      step_(step),
      length_(length){};

Slice::Slice(Solver &solver, LineName name) : solver_(solver), g_(solver.g_) {
  if (name.dir == ROW) {
    offset0_ = solver.width_ * name.index;
    step_ = 1;
    length_ = solver.width_;
  } else {
    offset0_ = name.index;
    step_ = solver.width_;
    length_ = solver.height_;
  }
}

void Slice::set(int i, state s) const {
  int o = offset0_ + step_ * i;
  int x = o % solver_.width_;
  int y = (o - x) / solver_.width_;
  solver_.set(x, y, s);
}

int Slice::findHoleStartingAt(int start, int length) const {
  int found = 0;
  for (int i = start; i < length_; i++) {
    if (get(i) == CROSSED) {
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
  state val = get(i);
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
    if (get(i) == SOLID) {
      return i;
    }
  }
  return -1;
}

// setSegment between i and j (exclusive) to state val. Return number
// of cells changed.
int Slice::setSegment(int i, int j, state val) const {
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
    : solver_(solver),
      name(name),
      len_(len),
      lb_(len.size()),
      ub_(len.size()),
      done_(len.size()),
      slice_(solver, name) {
  int sum = 0;
  for (auto l : len) {
    sum += l;
  }
  stats.wiggleRoom = slice_.length() - sum;
}

void Line::updateStats() {
  int w = 0;

  for (int i = 0; i < lb_.size(); i++) {
    int wi = ub(i) - lb(i) + 1 - len_[i];
    if (w < wi) w = wi;
  }
  stats.wiggleRoom = w;
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
           slice.get(hole + len[i]) == SOLID) {
      skippedSolid = skippedSolid || slice.get(hole) == SOLID;
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
      slice_.setSegment(prevU + 1, l, CROSSED);
    }

    if (done(i)) {
      continue;
    }

    if (u - len(i) + 1 <= l + len(i) - 1) {
      slice_.setSegment(u - len(i) + 1, l + len(i), SOLID);
    }

    if (u - l + 1 == len(i)) {
      done_[i] = true;
    }
  }
  if (ub(numSegments() - 1) + 1 < slice_.length()) {
    slice_.setSegment(ub(numSegments() - 1) + 1, slice_.length(), CROSSED);
  }
  return true;
}

// returns a segment index ranges (left inclusive, right exclusive)
// that lb(i) <= start and ub(i) >= end.
std::pair<int, int> Line::collidingSegments(int start, int end) {
  int first, second;
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

    if (slice_.get(i) == EMPTY) {
      if (slice_.get(i - 1) != CROSSED || slice_.get(i + stripLen) != CROSSED) {
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

      slice_.setSegment(i, i + stripLen, CROSSED);
    } else if (slice_.get(i) == SOLID) {
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
        state s = slice_.get(j);
        if (s == SOLID) {
          break;
        }
        if (s == EMPTY) {
          continue;
        }
        // we have strip "SSS  X" and can prepend some S
        if (slice_.setSegment(j - minLen, i, SOLID) > 0) {
          stripLen += i - (j - minLen);
          i = j - minLen;
        }
        break;
      }
      for (int j = i - 1; j >= i + stripLen - minLen && j >= 0; j--) {
        state s = slice_.get(j);
        if (s == SOLID) {
          break;
        }
        if (s == EMPTY) {
          continue;
        }
        // we have strip "X  SSS" and can append some S
        if (slice_.setSegment(i + stripLen, j + minLen + 1, SOLID) > 0) {
          stripLen += j + minLen + 1 - (i + stripLen);
        }
        break;
      }
      if (maxLen == stripLen) {
        slice_.setSegment(i - 1, i, CROSSED);
        slice_.setSegment(i + stripLen, i + stripLen + 1, CROSSED);
      }
    }
  }
  return true;
}

bool Line::infer() {
  // special case for no segments.
  if (numSegments() == 0) {
    slice_.setSegment(0, slice_.length(), CROSSED);
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

Solver::Solver(std::vector<std::vector<int>> rows,
               std::vector<std::vector<int>> cols)
    : width_(cols.size()), height_(rows.size()) {}

void Solver::set(int x, int y, state s){

};
