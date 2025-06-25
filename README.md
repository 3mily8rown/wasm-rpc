# Wasm-RPC

### Function

A WebAssembly (WASM) client sends a message through a native C++ backend to be received by a WASM server. Communication is coordinated through a lightweight RPC-style mechanism.

---

### Prerequisites

- **CMake**
- **Ninja**
- **Python 3**
- **`protoc` (Protocol Buffers compiler)**

---

### To Configure / Build / Run

Examples assume you’re using the included `CMakePresets.json` file:

- `cmake --preset=default`  
  _Use this if you've changed the toolchain, `CMakeLists.txt`, or presets._

- `cmake --build --preset=clean-build`  
  _Full clean rebuild of both WASM and native code._

- `cmake --build --preset=full`  
  _Incremental build when you've changed native or WASM sources._

- `cmake --build --preset=dev`  
  _For native-only development._

- `cmake --build --preset=run`  
  _Builds native and runs the `wasm_host` binary._

---

###  Note

Initial code was brought across from the experimental repository:  
[https://github.com/3mily8rown/fyp](https://github.com/3mily8rown/fyp)
