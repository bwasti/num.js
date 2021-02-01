#include <iostream>
#include <node.h>
#include <node_object_wrap.h>
#include <type_traits>
#include <utility>

namespace detail {

template <typename> void type_id() {}

using type_id_t = void (*)();

template <typename Callable> union storage {
  storage() {}
  std::decay_t<Callable> callable;
};

template <int, typename Callable, typename Ret, typename... Args>
auto fnptr_(Callable &&c, Ret (*)(Args...)) {
  static bool used = false;
  static storage<Callable> s;
  using type = decltype(s.callable);

  if (used)
    s.callable.~type();
  new (&s.callable) type(std::forward<Callable>(c));
  used = true;

  return [](Args... args) -> Ret {
    return Ret(s.callable(std::forward<Args>(args)...));
  };
}

template <typename Fn, int N, typename Callable> Fn *fnptr(Callable &&c) {
  return fnptr_<N>(std::forward<Callable>(c), (Fn *)nullptr);
}

template <typename T>
T new_cast_to(v8::Local<v8::Value> &&v, v8::Local<v8::Context> &context);

template <typename T>
v8::Local<v8::Value> new_cast_from(T v, v8::Isolate *isolate);

#define NUMERIC_TYPE(type)                                                     \
  template <>                                                                  \
  type new_cast_to<type>(v8::Local<v8::Value> && v,                            \
                         v8::Local<v8::Context> & context) {                   \
    type out = v->IsUndefined() ? 0 : v->NumberValue(context).FromMaybe(0);    \
    return out;                                                                \
  }                                                                            \
                                                                               \
  template <>                                                                  \
  v8::Local<v8::Value> new_cast_from<type>(type v, v8::Isolate * isolate) {    \
    return v8::Number::New(isolate, v);                                        \
  }

NUMERIC_TYPE(double);
NUMERIC_TYPE(float);
NUMERIC_TYPE(int);

//template <>
//v8::Local<v8::Value> new_cast_from<std::vector<float>>(std::vector<float> v, v8::Isolate * isolate) {
//  auto array = v8::Float32Array::New(v8::ArrayBuffer::New(isolate, v.size() * 4), 0, size);
//
//  return v8::Number::New(isolate, v);
//}

template <typename T, typename F, typename Out, typename Tup, size_t... index>
Out call_from_impl(T* obj, F f, const v8::FunctionCallbackInfo<v8::Value> &args,
                 v8::Local<v8::Context> &context,
                 std::index_sequence<index...>) {
  return (obj->*f)(new_cast_to<typename std::tuple_element<index, Tup>::type>(
      args[index], context)...);
}

template <typename T, typename F, typename Out, typename... Args>
Out call_from(T* obj, F f, const v8::FunctionCallbackInfo<v8::Value> &args,
            v8::Local<v8::Context> &context) {
  constexpr int N = sizeof...(Args);
  assert(N == args.Length());
  using Seq = std::make_index_sequence<N>;
  return call_from_impl<T, F, Out, std::tuple<Args...>>(obj, f, args, context, Seq{});
}

template <typename T, typename Tup, size_t... index>
T *new_from_impl(const v8::FunctionCallbackInfo<v8::Value> &args,
                 v8::Local<v8::Context> &context,
                 std::index_sequence<index...>) {
  return new T(new_cast_to<typename std::tuple_element<index, Tup>::type>(
      args[index], context)...);
}

template <typename T, typename... Args>
T *new_from(const v8::FunctionCallbackInfo<v8::Value> &args,
            v8::Local<v8::Context> &context) {
  constexpr int N = sizeof...(Args);
  assert(N == args.Length());
  using Seq = std::make_index_sequence<N>;
  return new_from_impl<T, std::tuple<Args...>>(args, context, Seq{});
}

} // namespace detail

namespace js {

template <typename T> class ObjectHolder : public node::ObjectWrap {
public:
  ObjectHolder(T *obj) : obj_(obj) {}

  template <typename... Args>
  static void newImpl(const v8::FunctionCallbackInfo<v8::Value> &args) {
    v8::Isolate *isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    if (args.IsConstructCall()) {
      ObjectHolder<T> *obj =
          new ObjectHolder<T>(detail::new_from<T, Args...>(args, context));
      obj->Wrap(args.This());
      args.GetReturnValue().Set(args.This());
    } else {
      isolate->ThrowException(v8::Exception::TypeError(
          v8::String::NewFromUtf8(isolate, "Must be called with `new`")
              .ToLocalChecked()));
    }
  }

  template <int N, typename Out, typename... Args>
  static v8::FunctionCallback FunctionWrap(Out (T::*f)(Args...)) {
    auto f_wrapped =
        [f](const v8::FunctionCallbackInfo<v8::Value> &args) -> void {
      v8::Isolate *isolate = args.GetIsolate();
      auto context = isolate->GetCurrentContext();

      ObjectHolder<T> *obj = ObjectWrap::Unwrap<ObjectHolder<T>>(args.Holder());
      using F = Out(T::*)(Args...);
      T* obj_ = obj->obj_.get();
      
      const Out &t = detail::call_from<T, F, Out, Args...>(obj_, f, args, context);
      args.GetReturnValue().Set(detail::new_cast_from<Out>(t, isolate));
    };

    return detail::fnptr<void(const v8::FunctionCallbackInfo<v8::Value> &), N>(
        f_wrapped);
  }

private:
  std::shared_ptr<T> obj_;
};

template <typename T> class JSObject {
public:
  JSObject<T>(const JSObject &) = delete;
  JSObject<T>(JSObject &&) = default;
  JSObject<T>() = default;

  template <typename... Args>
  static std::shared_ptr<JSObject<T>> Create(v8::Local<v8::Object> exports,
                                             std::string name) {
    auto jso = std::make_shared<JSObject<T>>();
    jso->class_name = name;
    jso->exports = exports;
    jso->isolate = jso->exports->GetIsolate();
    v8::Local<v8::Context> context = jso->isolate->GetCurrentContext();

    jso->addon_data_tpl = v8::ObjectTemplate::New(jso->isolate);
    jso->addon_data_tpl->SetInternalFieldCount(1);

    jso->addon_data =
        jso->addon_data_tpl->NewInstance(context).ToLocalChecked();
    jso->tpl = v8::FunctionTemplate::New(
        jso->isolate, ObjectHolder<T>::template newImpl<Args...>,
        jso->addon_data);

    jso->tpl->SetClassName(
        v8::String::NewFromUtf8(jso->isolate, jso->class_name.c_str())
            .ToLocalChecked());
    jso->tpl->InstanceTemplate()->SetInternalFieldCount(1);
    return jso;
  }

  template <typename F, int N> void def(std::string name, F f) {
    NODE_SET_PROTOTYPE_METHOD(tpl, name.c_str(),
                              ObjectHolder<T>::template FunctionWrap<N>(f));
  }

  ~JSObject<T>() {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Function> constructor =
        tpl->GetFunction(context).ToLocalChecked();
    addon_data->SetInternalField(0, constructor);
    exports
        ->Set(context,
              v8::String::NewFromUtf8(isolate, class_name.c_str())
                  .ToLocalChecked(),
              constructor)
        .FromJust();
  }

private:
  std::string class_name;
  v8::Local<v8::Object> exports;
  v8::Local<v8::FunctionTemplate> tpl;
  v8::Isolate *isolate;
  v8::Local<v8::ObjectTemplate> addon_data_tpl;
  v8::Local<v8::Object> addon_data;
};

template <typename T, int C> struct CountedType {
  CountedType(const CountedType &) = delete;
  CountedType(CountedType &&o) : underlying_(std::move(o.underlying_)) {}
  CountedType(std::shared_ptr<T> u) : underlying_(std::move(u)){};
  CountedType(CountedType<T, C - 1> &&j)
      : underlying_(std::move(j.underlying_)) {}

  std::shared_ptr<T> underlying_;

  template <typename F> CountedType<T, C + 1> def(std::string name, F f) {
    underlying_->template def<F, C>(name, f);
    return CountedType<T, C + 1>(std::move(*this));
  }
};

template <typename T, typename... Args>
CountedType<JSObject<T>, 0> class_(v8::Local<v8::Object> exports,
                                   std::string name) {
  auto js = JSObject<T>::template Create<Args...>(exports, name);
  return CountedType<JSObject<T>, 0>(js);
}

} // namespace js