#include <vector>

class Layer {
  std::vector<double> c_;
  const int dim_in_, dim_out_;

 public:
  // Length of c should be (1+dim_in) * dim_out, where the first
  // coefficient is the threshold.
  Layer(int dim_in, std::vector<double> &&c);
  int dim_in() const { return dim_in_; };
  int dim_out() const { return dim_out_; };
  std::vector<double> evaluate(const std::vector<double> &in) const;
};

class Net {
  std::vector<Layer> layers_;

 public:
  Net(std::vector<Layer> &&layers);
  Net(const std::vector<std::vector<double>> &coefs, int dim_in);
  int dim_in() const { return layers_.front().dim_in(); };
  int dim_out() const { return layers_.back().dim_out(); };
  std::vector<double> evaluate(const std::vector<double> &in) const;
};
