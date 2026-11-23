# Lecture 01 — C programming for network applications

Source: `Lec01.ReviewCProgramming.pdf`

## Values, control flow, and functions

C distinguishes integer and floating-point types and supports arrays of a fixed element type. For example, `int a[10]` allocates ten contiguous integers indexed from 0 through 9. Type widths depend on the implementation, so use `sizeof` to measure storage and fixed-width integer types such as `uint32_t` when a protocol specifies a width. A C string is a character array terminated by a zero byte. Space for the terminator is part of its storage requirement.

Conditions treat zero as false and nonzero as true. Use `if`/`else` for decisions based on expressions and `switch` to dispatch on discrete values. Execution can fall through to the next case unless a statement such as `break` transfers control. A `for` loop groups initialization, condition, and update. A `while` loop tests before entering its body, while `do`/`while` executes the body at least once. A `break` exits the innermost loop or switch that encloses it.

A function declaration gives the compiler the return type and parameter types. Arguments are passed by value. The entry point `main(int argc, char *argv[])` receives a count and an array of argument strings. When present, `argv[0]` is the invocation name, so check `argc` before reading a user-supplied argument such as `argv[1]`.

## Pointers, arrays, and ownership

`&x` gives the address of an object, and `*p` accesses the object at an address. A function can modify its caller's object by receiving a copy of its address. To replace the caller's pointer itself, return the new pointer or pass a pointer to that pointer. A pointer's type determines what it can dereference and the size of each step in pointer arithmetic.

An array usually converts to a pointer to its first element in expressions and when passed to a function. The callee needs the element count or another stopping rule. `sizeof(array)` measures the complete array where the array type is still available. `sizeof(pointer)` measures only the pointer.

- `const int *p` allows reassignment of `p` and prevents writes through `p`.
- `int *const p` fixes the address in `p` and allows writes to the pointed-to integer.
- `const int *const p` applies both constraints.
- `int *f(void)` declares a function returning an integer pointer. `int (*f)(void)` declares a pointer to a function returning an integer.
- `void *` carries an object address without a concrete element type. Convert it to a suitable typed pointer before dereferencing.

`malloc` allocates uninitialized storage. `calloc` allocates storage for an array and zeroes its bytes. `realloc` can move a block, so keep the original address until the resize succeeds. Check allocation failures, release owned allocations with `free`, and keep pointers valid for the full duration of their use. In particular, a function's automatic local object ceases to exist when it returns.

## Structures and linked lists

A structure groups related fields. Use `value.member` for a structure object and `ptr->member` when accessing it through a pointer. A singly linked list stores a link to the next node in each node. Its head identifies the start, and a null link marks the end. Insertion and deletion update links, including the head when the first node changes. Save any link you still need before freeing a node. Socket APIs use structures to pass addresses, option values, and other grouped data.

## File and stream I/O

The standard I/O library in `<stdio.h>` gives buffered `FILE *` streams. `fopen` selects a mode such as read, write, or append. `fgetc` returns an `int` so it can represent each byte and `EOF`. `fgets` reads a bounded line or line fragment, and `fputs` writes a string. A file-copy loop can read bounded chunks with `fgets` and forward each one to `stdout`, then check for read or write errors and close the input. Use `fread` and `fwrite` for binary blocks. `printf` and `fprintf` format output to standard output and a selected stream respectively. `scanf` and `fscanf` parse formatted input and require checking both conversion counts and field widths. Use bounded `fgets` for line input. The historical `gets` function had no way to limit the input size.

POSIX I/O uses integer file descriptors. `open` (declared in `<fcntl.h>`) acquires a descriptor, and `read`, `write`, and `close` (declared in `<unistd.h>`) transfer bytes and release it. The transfer calls report counts, which can be shorter than requested. A positive-length read can return zero at end of file, and a failure returns `-1` with details in `errno`. Socket descriptors follow much of the same I/O model.
