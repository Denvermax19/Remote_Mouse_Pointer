# Remote Mouse Pointer 🖱️

It is a lightweight C++ desktop application designed for teachers, mentors, and presenters. When students share their screens during online classes or video calls (such as Google Meet, Zoom, or Microsoft Teams), this tool provides a visible, high-contrast visual pointer overlay to easily point out specific elements and guide students visually without taking remote control of their machine.

---

## ✨ Features

- **Visual Remote Pointer:** Display a clear, custom pointer on screen to direct attention during screen-sharing sessions.
- **Click Diagnostics:** Built-in click diagnostic utility (`click_diag.cpp`) to track and verify screen interactions.
- **Lightweight & Fast:** Built using native C++ and CMake for minimal resource usage.

---

## 🚀 Getting Started

### Prerequisites

To build and run this project locally, ensure you have:
- **CMake** (v3.10 or higher)
- A C++ Compiler (GCC, Clang, or MSVC)

### Build Instructions

1. **Clone the repository:**
   ```bash
   git clone [https://github.com/Denvermax19/Remote_Mouse_Pointer.git](https://github.com/Denvermax19/Remote_Mouse_Pointer.git)
   cd Remote_Mouse_Pointer

```

2. **Build the application:**
```bash
mkdir build
cd build
cmake ..
cmake --build .

```


3. **Run the executable:**
```bash
./Remote_Mouse_Pointer

```



---

## 💡 How to Use

1. Launch the executable during a live video call or screen-share session.
2. Use the pointer overlay to direct focus, highlight UI elements, and guide the student visually.
