# MiniShell – Custom Unix-like Shell in C

This repository contains a custom-built shell program implemented in C, developed as part of the Operating Systems course at UC3M (2024–2025). The shell supports basic command execution, history management, and internal commands like `mycalc` and `myhistory`.

## 🧩 Components

### 🐚 minishell
A simplified shell that:
- Parses and executes command sequences with support for:
  - Pipes (`|`)
  - Input (`<`), output (`>`), and error redirection (`!>`)
  - Background execution (`&`)
- Forks child processes for command execution
- Handles inter-process communication via pipes
- Maintains a history of recent commands

### ➕ mycalc
An internal command with arithmetic capabilities:
- Syntax: `mycalc <operand1> <add|mul|div> <operand2>`
- Handles:
  - Addition and persistent accumulation via environment variable `Acc`
  - Multiplication and division (with division by zero handling)
  - Error messages to standard output or error depending on context

### 📜 myhistory
An internal command to view or re-execute past commands:
- `myhistory` – lists command history
- `myhistory <n>` – re-executes the nth command from history
- Handles invalid input and out-of-range access gracefully

## 🧪 Testing

The program has been tested extensively with:
- Single and multiple piped commands
- Various redirection setups
- Background and foreground processes
- Custom internal commands with correct/incorrect usage

Example:
```bash
MSH>> ls | grep o | sort
MSH>> mycalc 5 add 8
MSH>> myhistory 0
```

## ⚙️ How to Compile & Run

1. Compile:
```bash
make
```

2. Run:
```bash
./msh
```
