# 🧠 Custom Memory Allocators in C (Linux)

This project implements three basic memory allocators from scratch, designed to run on **Linux** only:

- ✅ **Bump Allocator**
- ✅ **Linked List Allocator**
- ✅ **Arena / mmap Allocator**

Each allocator provides a different approach to managing dynamic memory without using the standard `malloc`/`free`.

---

## 🚀 Overview

### 🔹 Bump Allocator
Fast and simple. It moves a pointer forward to allocate memory. No deallocation (`free`) is supported.

### 🔹 Linked List Allocator
Manages memory blocks with a linked list. Supports both allocation and deallocation.

### 🔹 Arena / mmap Allocator
Uses `mmap` to request large blocks of memory and allocates from them internally.

---

## ⚙️ Requirements

- Linux OS / Virtual Machine (Windows option)
- GCC or Clang
- `make` (optional)

---

## 🧪 Build
`gcc -o "filename" "filename"`
