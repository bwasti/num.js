#include "jsbind.h"

struct TestObj {
  TestObj(float f, int i) : f_(f), i_(i) {}
  float getSum() { return f_ + i_; }
  float getProd() { return f_ * i_; }
  void setFloat(float k) { f_ = k; }
  float sum(std::vector<float> v) {
    float out = 0;
    for (auto i : v) {
      out += i;
    }
    return out;
  }
  std::vector<float> array() { return std::vector<float>(100, f_); }
  float f_;
  int i_;
};

struct Tensor {
  Tensor(std::vector<int> size) : size_(size) {}
  float sum() { return 10; }
  std::vector<int> size_;
};

std::shared_ptr<Tensor> randn() {
  return std::make_shared<Tensor>(std::vector<int>{1, 2});
}

void Initialize(v8::Local<v8::Object> exports) {
  js::class_<TestObj, float, int>(exports, "TestObj")
      .def("getProd", &TestObj::getProd)
      .def("setFloat", &TestObj::setFloat)
      .def("sum", &TestObj::sum)
      .def("array", &TestObj::array)
      .def("getSum", &TestObj::getSum);
  js::class_<Tensor, std::vector<int>>(exports, "Tensor")
      .def("sum", &Tensor::sum);

  js::methods(exports)
      .def("randn", randn);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
