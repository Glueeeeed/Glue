# Functions & I/O (shout)

Functions and input/output form the core of program logic and interaction in Glue.

---

## Function Declarations

Functions are declared using the `func` keyword. Glue supports explicit return types as well as default `void` return types:

```c
func string getAppName() {
    return "Glue Engine";
}

func void logStartup() {
    shout("Starting application...");
    return;
}

// Omitting the return type defaults to void:
func printDivider() {
    shout("==========================");
    return;
}
```

### The `main` Entry Point

Every executable Glue program **must** contain an entry point function named `main` or `Main` with a return type of `int`.

```c
func int main() {
    printDivider();
    shout("Running: ", getAppName());
    printDivider();
    return 0;
}
```

::: info Main Function Mechanics
At code generation time, the compiler automatically handles the `main` entry point:
- Injects a completion message `Program completed successfully. Press Enter to exit.`.
- Injects a `getchar()` call to prevent immediate terminal termination on execution.
- Returns `0` to the operating system.
:::

---

## Standard Output: `shout`

Glue provides a built-in variadic logging statement named `shout`.

`shout` accepts a comma-separated list of expressions, evaluates them sequentially:

```c
func int main() {
    int age = 4;
    shout("Dog ", "Pedro", " is ", age, " years old. Status: ", age > 2);
    return 0;
}
```


---

## Current Function & I/O Limitations

::: danger Function Arguments & Parameter Support
In the current version of the Glue compiler:
1. **No Function Parameters / Arguments**:
   Functions currently **cannot accept arguments or parameters**. Declaring parameters in signatures (e.g. `func int add(int a, int b)`) or passing arguments in calls (e.g. `add(2, 3)`) is not supported. All functions must declare an empty parameter list `()`:
   ```c
   // UNSUPPORTED:
   // func int add(int a, int b) { return a + b; }

   // SUPPORTED:
   func int computeResult() {
       int a = 10;
       int b = 20;
       return a + b;
   }
   ```
2. **Sequential Declaration / No Forward Declarations**:
   Functions should be declared before they are called, or defined at top-level.
3. **No Local Variable Shadowing Across Functions**:
   Because the semantic analyzer symbol table is currently flat per compilation unit, variable identifiers should remain unique across your file.
4. **Standard Input (I/O)**:
   There is currently no built-in `listen` or `scanf`-like input statement in the language grammar.
:::
