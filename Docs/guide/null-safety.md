# Null safety System

Glue introduces a **Null Safety** system centered around the `nulo` literal (the equivalent of `null`/`nil`). Its primary goal is to completely eliminate `NullPointerException`
style errors and undefined behavior at runtime.

The system draws inspiration from languages such as Kotlin, Swift, TypeScript, and C#.


## The `nulo` Literal

`nulo` is a keyword and literal that represents the absence of a value. The name is borrowed from **Spanish**, where *nulo* means "null". It can be assigned to any **nullable** type.

```c
int? x = nulo;
string? name = nulo;
```


## Nullable Types

A type becomes a nullable type by adding the `?` modifier to its base type, creating the type **QPSN** (from the Spanish `Que puede ser nulo`).

| Nullable Type | Base Type | Description |
| :--- | :--- | :--- |
| `byte?` | `byte` | 8-bit integer that may hold `nulo` |
| `small?` | `small` | 16-bit integer that may hold `nulo` |
| `int?` | `int` | 32-bit integer that may hold `nulo` |
| `long?` | `long` | 64-bit integer that may hold `nulo` |
| `huge?` | `huge` | 128-bit integer that may hold `nulo` |
| `float?` | `float` | 32-bit float that may hold `nulo` |
| `double?` | `double` | 64-bit double that may hold `nulo` |
| `string?` | `string` | String that may hold `nulo` |

::: warning Boolean Nullability
The `bool` type **cannot** be nullable. There is no `bool?` type in Glue a boolean value is always either `true` or `false`, never `nulo`.
:::



### Declaration Rules

- **Non-nullable types** must always be initialized at declaration. An uninitialized declaration (`int x;`) is a compile-time error.
- **Nullable types** without an explicit initializer are implicitly assigned `nulo`.

```c
int? x;        // Implicitly: int? x = nulo;
int y = 10;    // OK: initialized
// int z;      // Compile Error: non-nullable variable must be initialized
```


## Core Safety Rules

### Rule 1: No Assigning `nulo` to Non-Nullable Types

You cannot assign `nulo` or a nullable expression to a non-nullable variable.

```c
int a = 5;
a = nulo;      // Compile Error: cannot assign 'nulo' to non-nullable type 'int'

int? b = nulo;
a = b;         // Compile Error: cannot assign nullable 'int?' to non-nullable 'int'
```

### Rule 2: No Arithmetic Operations on Nullable Types

Arithmetic operators (`+`, `-`, `*`, `/`, `%`) cannot be applied to nullable types. The operand must be non-nullable.

```c
int? x = 10;
int? y = 20;

int result = x + y;  // Compile Error: cannot perform arithmetic on nullable types
```

To perform arithmetic, first narrow the type (see **Type Narrowing** below).


## Type Narrowing

Type Narrowing is a **flow-sensitive** mechanism. When the compiler encounters a condition that excludes `nulo`, it temporarily treats the variable as its non-nullable base type within the corresponding code block.

### Basic Narrowing with `!= nulo`

Inside the `then` branch of `if (x != nulo)`, the variable `x` is narrowed from `T?` to `T`:

```c
int? x = 10;

if (x != nulo) {
    int y = x;    // OK: x is narrowed to 'int' inside this block
    int sum = x + 5;  // OK: arithmetic is allowed
}

// Outside the block, x is still 'int?'
int z = x;        // Compile Error: x is 'int?' here
```

### Narrowing with `== nulo`

When checking `x == nulo`, the narrowing applies to the `else` branch:

```c
int? x;

if (x == nulo) {
    // x is still 'int?' here
} else {
    int y = x;    // OK: x is narrowed to 'int' in the else branch
}
```

### Implicit Truthiness

A nullable variable used directly as a condition narrows in the `then` branch, while negation (`!x`) narrows in the `else` branch:

```c
int? x = 10;

if (x) {
    int y = x;    // OK: narrowed to 'int'
}

if (!x) {
    // x is still nullable here
} else {
    int y = x;    // OK: narrowed to 'int'
}
```


## Edge Cases

### Reassignment Inside a Narrowed Block

If a narrowed variable is reassigned a nullable value inside the block, the narrowing is revoked:

```c
int? x = 10;

if (x != nulo) {
    int a = x;    // OK: x is 'int'
    x = nulo;     // x becomes nullable again
    int b = x;    // Compile Error: x is 'int?' now
}
```

### Early Exit / Guard Clauses

When a guard clause checks for `nulo` and exits early via `return`, `break`, or `throw`, the code after the `if` block inherits the narrowed state:

```c
func int calculate(int? x) {
    if (x == nulo) {
        return 0;
    }
    // x is narrowed to 'int' for the rest of the function
    return x + 5;
}
```

### `while` Loops

At the start of each iteration, the loop condition narrows the variable. Reassignment inside the body invalidates the narrowing for subsequent lines:

```c
int? x = 10;

while (x != nulo) {
    int a = x;    // OK: narrowed at the start of iteration
    x = getNext(); // May return nulo 
    int b = x;    // Compile Error: x is 'int?' again
}

// After the loop, x is always nulo (the loop terminated because x == nulo)
```

## `Sino` Operator

The `sino` operator (from the Spanish **sino** `“otherwise”`) is used to specify a default value when the expression on the left-hand side evaluates to `nulo`. This is known as the **null-coalescing operator**.

As an alternative to the keyword `sino`, you can use the symbol `??`. Both forms are equivalent:

```c
int? x = nulo;

int a = x sino 42;    // Result: 42
int b = x ?? 42;      // Result: 42 (equivalent notation)
```

### Behavior

The operator takes two operands:
- **Left operand**: a nullable expression (`T?`).
- **Right operand**: a default value of type `T` (non-nullable).

If the left operand is **not** `nulo`, the operator returns its value. Otherwise, it returns the right operand.

```c
string? name = "Anna";
string greeting = name sino "Guest";   // Result: "Anna"

string? missing = nulo;
string fallback = missing ?? "Guest";  // Result: "Guest"
```

### Return Type

The result of the `sino` / `??` operator is always **non-nullable** (`T`), because in both cases (the value from the left or the default value from the right) the result is a value that is not `nulo`.

```c
int? x = 10;

int y = x sino 0;   // OK: result type is 'int' (non-nullable)
```

### Combining with Type Narrowing

The `sino` operator is an alternative to manual type narrowing. Instead of checking for `nulo` in an `if` block, you can directly provide a default value:

```c
int? x = 10;

// Without the sino operator
if (x != nulo) {
    print(x + 5);
} else {
    print(5);
}

// With the sino operator shorter notation
print((x sino 0) + 5);
```

### Nesting

The `sino` / `??` operators can be chained to create cascading default values:

```c
int? a = nulo;
int? b = nulo;
int? c = 100;

int result = a sino b sino c sino 0;  // Result: 100 (first non-nulo value)
```

---

::: tip Design Philosophy
The Nulo system ensures that null-related errors are caught at **compile time**, not runtime. By making nullability explicit in the type system and narrowing types through flow analysis, Glue eliminates an entire class of bugs without requiring manual null checks throughout the codebase.
::::
