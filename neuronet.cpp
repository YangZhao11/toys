#include "neuronet.hpp"
#include <numeric>

Layer::Layer(int dim_in, std::vector<double> &&c)
    : c_(c), dim_in_(dim_in), dim_out_(c.size() / (1 + dim_in)){};

std::vector<double> Layer::evaluate(const std::vector<double> &in) const {
  std::vector<double> out;
  out.reserve(dim_out_);
  auto iter = c_.cbegin();
  for (int o = 0; o < dim_out_; o++) {
    double v = -(*iter);
    iter++;
    v += std::inner_product(in.cbegin(), in.cend(), iter, 0.0);
    iter += in.size();
    out.push_back(v > 0 ? v : 0);
  }
  return out;
};

Net::Net(std::vector<Layer> &&layers) : layers_(layers){};

Net::Net(const std::vector<std::vector<double>> &coefs, int dim_in) {
  layers_.reserve(coefs.size());
  for (auto &c : coefs) {
    std::vector<double> c_copy = c;
    layers_.emplace_back(dim_in, std::move(c_copy));
    dim_in = layers_.back().dim_out();
  }
};

std::vector<double> Net::evaluate(const std::vector<double> &in) const {
  std::vector<double> out(in);
  for (auto &l : layers_) {
    out = l.evaluate(out);
  }
  return out;
};
