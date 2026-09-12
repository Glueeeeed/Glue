# Control Flow & Conditions

Glue provides standard control flow constructs: branching with `if` / `else` statements and iteration with `while` loops.

---

## The `if` / `else` Statement

Conditional branching executes blocks of code based on a boolean condition:

```c
func int main() {
    int score = 85;

    if (score >= 50) {
        shout("Passed the exam!");
    } else {
        shout("Failed the exam.");
    }

    return 0;
}
```

---

## The `while` Loop

The `while` loop repeatedly executes a block as long as its condition evaluates to `true`:

```c
func int main() {
    int i = 0;

    while (i < 5) {
        shout("Current index: ", i);
        i = i + 1;
    }

    return 0;
}
```

---

## Operators & Precedence

Glue provides standard mathematical, relational, and boolean operators:

- **Arithmetic**: `+`, `-`, `*`, `/`
- **Comparison**: `==`, `!=`, `<`, `<=`, `>`, `>=`
- **Logical**: `and`, `or`, `!`, `&&`, `||`
- **Grouping**: Parentheses `(` `)`

### Operator Precedence
Evaluation follows standard algebraic and boolean rules:
1. Literals, identifiers, and parenthesized expressions `( ... )`
2. Multiplicative: `*`, `/`
3. Additive: `+`, `-`
4. Relational / Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
5. Logical AND: `and`, `&&`
6. Logical OR: `or`, `||`

::: tip Automatic Type Promotion
Glue automatically promotes mixed numerical operations:
- When an integer (`int`) interacts with a floating-point value (`float`/`double`), the integer is cast to float (`sitofp`).
- Operations between `float` and `double` promote the result to `double`.
:::

---

## Critical Condition Expression Limitations

::: danger Explicit Comparison Requirement in Conditions
In the current version of the Glue compiler, conditional statements (`if`, `while`) **require an explicit binary comparison or boolean expression**. 

Passing a bare identifier as the condition is **not supported**:

```c
bool isActive = true;

// INCORRECT / UNSUPPORTED:
// if (isActive) {
//     shout("Active");
// }

// CORRECT:
if (isActive == true) {
    shout("Active");
}
```

Similarly, in while loops:
```c
bool isRunning = true;
int count = 0;

while (isRunning == true) {
    shout("Running count: ", count);
    count = count + 1;
    if (count == 3) {
        isRunning = false;
    }
}
```
Always use explicit binary operators (`==`, `!=`, `<`, `<=`, `>`, `>=`) within condition expressions.
:::
