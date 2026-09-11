# Syntax, Types & Mutability

Glue is a statically and strongly typed language with an emphasis on explicit variable mutability and clear syntax.

---

## Comments

Glue supports both single-line comments and distinctive block comments:

- **Single-line comment**: Begins with `//` and continues to the end of the line.
- **Glue block comment**: Begins with `/*g` and ends with `g*/`.

```glue
// This is a standard single-line comment

/*g
  This is a Glue block comment.
  It can span multiple lines.
g*/
```

---

## Data Types

Glue provides fundamental primitive types mapped directly to native LLVM representations:

| Type | LLVM Equivalent | Description | Example Literal |
| :--- | :--- | :--- | :--- |
| `int` | `i32` | 32-bit signed integer | `42`, `-10` |
| `float` | `float` | 32-bit single-precision floating point | `3.14f`, `0.5f` |
| `double` | `double` | 64-bit double-precision floating point | `3.1415926535` |
| `bool` / `boolean` | `i1` | Boolean logical values | `true`, `false` |
| `string` | `ptr` | Null-terminated string literal | `"Hello, Glue!"` |

::: tip Numeric Literals
Floating-point numbers with a trailing `f` or `F` (e.g. `10.5f`) are parsed as `float`. Floating-point numbers without suffix (e.g. `10.5`) default to `double`.
:::

---

## Variable Declarations & Mutability Modifiers

Glue introduces three tiers of variable mutability:

### 1. Standard Mutable Variables
Declared with `<type> <identifier> = <expression>;`. Can be reassigned arbitrarily many times.

```glue
int counter = 0;
counter = 1;
counter = 10;
```

### 2. Constants (`const`)
Declared with `const <type> <identifier> = <expression>;`. Immutable after declaration. Any reassignment attempt causes a compile-time semantic error.

```glue
const double PI = 3.14159;
// PI = 3.14; // Compile Error: cannot assign to variable 'PI' because it is a constant
```

### 3. Sticky Variables (`sticky`)
Declared with `sticky <type> <identifier> = <expression>;`. A `sticky` variable allows **exactly one** reassignment during its lifetime. Any subsequent reassignment triggers a compile-time semantic error.

```glue
sticky string appStatus = "starting";
appStatus = "ready"; // OK: first reassignment allowed

// appStatus = "stopped"; // Compile Error: variable 'appStatus' is 'sticky' and has already been reassigned once
```

---

## Current Scope & Declaration Limitations

::: warning Important Scope Limitations
1. **Flat Symbol Table (No Lexical Scoping / Shadowing)**:
   The semantic analyzer currently tracks variables across a single flat symbol table per compilation unit. Variables declared inside blocks or separate functions must have unique identifier names throughout the file; variable shadowing across inner `{ ... }` blocks is not supported.
2. **Declaration with Initialization**:
   Variables must be initialized at declaration time (`int x = 5;`). Uninitialized declarations (`int x;`) are not supported.
3. **Compound Assignment Operators**:
   Operators like `+=`, `-=`, `*=`, `/=`, as well as `++` and `--`, are not yet implemented. Use explicit reassignment: `count = count + 1;`.
:::
