# 🔐 CryptoShell

A C++17 terminal-based secure messaging application built to explore authentication, cryptographic workflows, session management, and secure software engineering principles.

CryptoShell provides a complete command-line experience where users can securely register, log in, encrypt and decrypt messages, and maintain a personal encrypted message history. The project was developed to strengthen practical knowledge of C++, secure software design, and cybersecurity fundamentals.

> **Note:** CryptoShell is an educational project designed for learning and portfolio purposes. It is **not intended for production-grade cryptographic security**.

---

## ✨ Features

- 👤 User Registration & Login
- 🔑 Password Hashing
- 🔒 Message Encryption & Decryption
- 📜 Per-User Encrypted Message History
- 📊 Dashboard with Session Statistics
- 👤 Profile & Password Management
- 🎨 Interactive ANSI-Based Terminal UI
- 💾 Persistent Local Storage

---

## 🔐 Encryption Pipeline

```
Plain Text
    ↓
Variable Caesar Shift
    ↓
XOR Encryption
    ↓
Hexadecimal Encoding
    ↓
Cipher Text
```

Decryption performs the reverse process to recover the original message.

---

## 🛠 Tech Stack

- C++17
- Standard Template Library (STL)
- File Handling
- Object-Oriented Programming Concepts
- ANSI Escape Sequences
- Cross-Platform Terminal Support

---

## 🏗 Project Architecture

```
User
   │
   ▼
Authentication
   │
   ▼
Session Manager
   │
 ┌─┴──────────────┐
 ▼               ▼
Encryption   History Manager
      │           │
      └─────┬─────┘
            ▼
     Local Storage
```

---

## 📁 Repository Structure

```
CryptoShell/
│
├── cryptoshell.cpp
├── cs_users.dat
├── cs_hist_<username>.dat
└── README.md
```

---



## 🧠 Skills Demonstrated

- Secure Authentication
- Password Hashing
- Session Management
- File I/O
- Modular C++ Programming
- CLI Application Development
- Defensive Programming
- Software Architecture

---

## ⚠ Security Notice

CryptoShell uses a **custom encryption pipeline** for educational purposes. It demonstrates cybersecurity concepts and secure software workflows but should **not be used to protect sensitive or confidential data**.

---

## 🔮 Future Improvements

- AES-256 Encryption
- Argon2 / bcrypt Password Hashing
- File & Folder Encryption
- SQLite Database
- Secure Key Management
- Unit Testing
- Cross-Platform GUI
- Improved Logging & Error Handling

---

