import io

p = ("/boot/home/rchromium-chromium87-fast/chromium/third_party/webrtc/"
     "api/function_view.h")
s = io.open(p, encoding="utf-8").read()

if "function_view_internal" in s:
    print("ALREADY_PATCHED")
    raise SystemExit(0)

old_ns = """namespace rtc {

template <typename T>
class FunctionView;  // Undefined.
"""
new_ns = """namespace rtc {

namespace function_view_internal {
// The function-pointer constructor below deduces F as a function reference as
// well as a function pointer. For a reference, GCC sees a null test on
// something whose address can never be null and reports -Waddress. Routing the
// test through a pointer parameter keeps the check meaningful for genuine
// function pointers without tripping the warning.
template <typename T>
inline bool IsNotNull(T* p) {
  return p != nullptr;
}
}  // namespace function_view_internal

template <typename T>
class FunctionView;  // Undefined.
"""
assert s.count(old_ns) == 1, "namespace anchor count=%d" % s.count(old_ns)
s = s.replace(old_ns, new_ns, 1)

old_ctor = """      : call_(f ? CallFunPtr<typename std::remove_pointer<F>::type> : nullptr) {
"""
new_ctor = """      : call_(function_view_internal::IsNotNull(f)
                  ? CallFunPtr<typename std::remove_pointer<F>::type>
                  : nullptr) {
"""
assert s.count(old_ctor) == 1, "ctor count=%d" % s.count(old_ctor)
s = s.replace(old_ctor, new_ctor, 1)

io.open(p, "w", encoding="utf-8").write(s)
print("EDIT_OK")
