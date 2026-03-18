#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>  // 必须引入：支持 std::string ↔ Python str
#include <string>

namespace nb = nanobind;

// 普通函数
int add(int a, int b) {
    return a + b;
}

// 待封装的类
class Greeter {
private:
    std::string m_name;
public:
    Greeter(const std::string& name) : m_name(name) {}
    std::string greet() const { return "Hello, " + m_name + "!"; }
    static std::string default_greet() { return "Hello, World!"; }
    std::string get_name() const { return m_name; }
    void set_name(const std::string& name) { m_name = name; }
};

// 核心绑定逻辑
NB_MODULE(Aether, m) {
    // 绑定加法函数
    m.def("add", &add, nb::arg("a"), nb::arg("b"), "A simple addition function");

    // 绑定 Greeter 类（修复字符串类型转换）
    nb::class_<Greeter>(m, "Greeter")
        // 显式绑定构造函数，确保字符串类型转换
        .def(nb::init<const std::string&>(), nb::arg("name"), "Create a Greeter instance with a name")
        // 绑定成员函数
        .def("greet", &Greeter::greet, "Return a greeting message")
        // 绑定静态函数
        .def_static("default_greet", &Greeter::default_greet, "Default greeting")
        // 绑定属性
        .def_prop_rw("name", &Greeter::get_name, &Greeter::set_name, "Name of the greeter");
}