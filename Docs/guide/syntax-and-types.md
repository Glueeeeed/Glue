# Syntax, Types & Mutability

Glue is a statically and strongly typed language with an emphasis on explicit variable mutability and clear syntax.


## Comments

Glue supports both single-line comments and distinctive block comments:

- **Single-line comment**: Begins with `//` and continues to the end of the line.
- **Glue block comment**: Begins with `/*g` and ends with `g*/`.

```c
// This is a standard single-line comment

/*g
  This is a Glue block comment.
  It can span multiple lines.
g*/
```


## Data Types

Glue provides fundamental primitive types mapped directly to native LLVM representations:

| Type | LLVM Equivalent | Description | Example Literal |
| :--- | :--- | :--- | :--- |
| `byte` | `i8` | 8-bit signed integer | `127`, `-128` |
| `small` | `i16` | 16-bit signed integer | `32767`, `-32768` |
| `int` | `i32` | 32-bit signed integer | `42`, `-10` |
| `long` | `i64` | 64-bit signed integer | `9223372036854775807` |
| `float` | `float` | 32-bit single-precision floating point | `3.14f`, `0.5f` |
| `double` | `double` | 64-bit double-precision floating point | `3.1415926535` |
| `bool` / `boolean` | `i1` | Boolean logical values | `true`, `false` |
| `string` | `ptr` | Null-terminated string literal | `"Hello, Glue!"` |

::: tip Numeric Literals
Floating-point numbers with a trailing `f` (e.g. `10.5f`) are parsed as `float`. Floating-point numbers without suffix (e.g. `10.5`) default to `double`.
:::


## Variable Declarations & Mutability Modifiers

Glue introduces three tiers of variable mutability:

### 1. Standard-Mutable Variables
Declared with `<type> <identifier> = <expression>;`. Can be reassigned arbitrarily many times.

```c
int counter = 0;
counter = 1;
counter = 10;
```

### 2. Constants (`const`)
Declared with `const <type> <identifier> = <expression>;`. Immutable after declaration. Any reassignment attempt causes a compile-time semantic error.

```c
const double PI = 3.14159;
// PI = 3.14; // Compile Error: cannot assign to variable 'PI' because it is a constant
```

### 3. Sticky Variables (`sticky`)
Declared with `sticky <type> <identifier> = <expression>;`. A `sticky` variable allows **exactly one** reassignment during its lifetime. Any subsequent reassignment triggers a compile-time semantic error.

```c
sticky string appStatus = "starting";
appStatus = "ready"; // OK: first reassignment allowed

// appStatus = "stopped"; // Compile Error: variable 'appStatus' is 'sticky' and has already been reassigned once
```


::: warning Sticky Limitations

**Sticky is not allowed in while statement**

Sticky variables are not allowed in `while` statement. Declaration and asignment sticky variables in a while statement result in an error.

:::


## Type Promotion

Glue automatically promotes operand types during binary arithmetic and comparison operations. When two operands of different numeric types participate in an operation, the result type is the wider (higher-priority) type. This prevents data loss and matches the behavior of languages like C and Java.

### Type Priority Table

Types are ordered from lowest to highest priority. During an operation, the type with the higher priority becomes the result type:

| Priority | Type  | LLVM Type | Bits  |
| -------- | ----- | --------- | ----- |
| 0        | `byte`   | `i8`   | 8     |
| 1        | `small`  | `i16`  | 16    |
| 2        | `int`    | `i32`  | 32    |
| 3        | `long`   | `i64`  | 64    |
| 4        | `float`  | `float`| 32    |
| 5        | `double` | `double`| 64   |

### Promotion Rules

- **Integer + Integer**: The narrower integer is sign-extended to the wider integer type.
  ```c
  byte b = 5;
  int x = 10;
  int sum = b + x;       // byte is promoted to int; result is int
  ```

- **Integer + Floating-Point**: The integer is converted to floating-point.
  ```c
  int a = 10;
  double factor = 2.5;
  double result = a * factor;  // int is promoted to double; result is double
  ```

- **Float + Double**: The `float` is extended to `double`.
  ```c
  float f = 1.5f;
  double d = 2.0;
  double sum = f + d;    // float is promoted to double; result is double
  ```

- **Comparison operators** (`==`, `!=`, `<`, `<=`, `>`, `>=`) always return `bool`, regardless of operand types. The operands themselves are still promoted to a common type before comparison.

## Declaration Limitations

::: warning Important Limitations
1. **Declaration with Initialization**:
   Variables must be initialized at declaration time (`int x = 5;`). Uninitialized declarations (`int x;`) are not supported.
2. **Compound Assignment Operators**:
   Operators like `+=`, `-=`, `*=`, `/=`, as well as `++` and `--`, are not yet implemented. Use explicit reassignment: `count = count + 1;`.
:::
