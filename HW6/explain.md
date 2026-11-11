# HW6 – Giải thích mã nguồn chi tiết

Ứng dụng đăng bài (message board) qua TCP, server dùng **multi-thread** (POSIX threads).
Tài liệu này đi qua **từng file** trong bài nộp `DangHoangSon_20235994_HW06.zip`: file đó làm gì, có những hàm nào, và tại sao lại viết như vậy.

---

## Mục lục

1. [Tổng quan kiến trúc](#1-tổng-quan-kiến-trúc)
2. [Giao thức và định dạng thông điệp](#2-giao-thức-và-định-dạng-thông-điệp)
3. [Mô hình đa luồng và đồng bộ](#3-mô-hình-đa-luồng-và-đồng-bộ)
4. [Makefile](#4-makefile)
5. [TCP_Server – từng file](#5-tcp_server--từng-file)
   - [main.c](#51-tcp_servermainc)
   - [server.h / server.c](#52-tcp_serverserverh--serverc)
   - [session.h / session.c](#53-tcp_serversessionh--sessionc)
   - [registry.h / registry.c](#54-tcp_serverregistryh--registryc)
   - [account.h / account.c](#55-tcp_serveraccounth--accountc)
   - [protocol.h / protocol.c](#56-tcp_serverprotocolh--protocolc)
   - [stream.h / stream.c](#57-tcp_serverstreamh--streamc-dùng-chung-với-client)
6. [TCP_Client – từng file](#6-tcp_client--từng-file)
   - [main.c](#61-tcp_clientmainc)
   - [client.h / client.c](#62-tcp_clientclienth--clientc)
   - [util.h / util.c](#63-tcp_clientutilh--utilc)
   - [stream.h / stream.c](#64-tcp_clientstreamh--streamc)
7. [Đi theo một request từ đầu đến cuối](#7-đi-theo-một-request-từ-đầu-đến-cuối)
8. [Kiểm thử đã thực hiện](#8-kiểm-thử-đã-thực-hiện)
9. [Câu hỏi thầy có thể hỏi](#9-câu-hỏi-thầy-có-thể-hỏi)

---

## 1. Tổng quan kiến trúc

```
                       ./server 5500
┌──────────────────────────────────────────────────────────────┐
│ Main thread (server.c: runServerLoop)                        │
│   accept() ──► malloc ClientTask ──► pthread_create(detached)│
│      ▲                                     │                 │
│      └──────────── lặp lại ◄───────────────┘                 │
│                                                              │
│ Client thread #1  (server.c: clientThread)                   │
│   handleClient()  ─ session.c                                │
│     recvLine() ─ stream.c   (tách thông điệp theo "\r\n")    │
│     parseRequest() ─ protocol.c   (USER / POST / BYE / ?)    │
│     findAccount() ─ account.c     (đọc account.txt)          │
│     claimAccount()/releaseAccount() ─ registry.c ◄──┐        │
│     sendMessage() ─ stream.c                        │ mutex  │
│ Client thread #2 ... #N  (giống hệt)  ──────────────┘        │
└──────────────────────────────────────────────────────────────┘

                       ./client 127.0.0.1 5500
┌──────────────────────────────────────────────────────────────┐
│ main.c: menu 1-4 ─► handleLogin/handlePost/handleLogout      │
│ client.c: sendRequest() ─► gửi "LỆNH tham_số\r\n", nhận mã   │
│           describeResponse() ─► mã → câu thông báo           │
│ util.c:   readLine(), trimSpaces()  (đọc bàn phím)           │
│ stream.c: giống server                                       │
└──────────────────────────────────────────────────────────────┘
```

Nguyên tắc chia file: **mỗi file một trách nhiệm**.

| File | Trách nhiệm | Biết về socket? | Biết về thread? |
|---|---|---|---|
| `server.c` | mở cổng, accept, tạo thread | có | **có** |
| `session.c` | trạng thái 1 phiên + xử lý 3 lệnh | qua `stream.c` | không (chỉ gọi registry) |
| `registry.c` | danh sách tài khoản đang đăng nhập | không | **có** (mutex) |
| `account.c` | đọc `account.txt` | không | không |
| `protocol.c` | nhận dạng loại request | không | không |
| `stream.c` | gửi/nhận theo dòng trên TCP | có | không |

Vì vậy phần đa luồng chỉ tập trung ở **2 file**: `server.c` (tạo thread) và `registry.c` (dữ liệu dùng chung). Các file còn lại không dùng biến toàn cục nào nên tự động an toàn khi nhiều thread cùng gọi.

---

## 2. Giao thức và định dạng thông điệp

Đề cho sinh viên tự chọn định dạng; bài này dùng **chuỗi ký tự, mỗi thông điệp kết thúc bằng `"\r\n"`** – đúng với chương trình `tcp_test.c` của thầy.

**Client → Server**

| Request | Ý nghĩa |
|---|---|
| `USER username\r\n` | đăng nhập |
| `POST article\r\n` | đăng bài (article có thể chứa dấu cách) |
| `BYE\r\n` | đăng xuất |

**Server → Client**: chỉ gửi đúng **mã 3 chữ số** + `"\r\n"` (ví dụ `110\r\n`). Client tự đổi mã thành câu thông báo.
Lý do không gửi kèm chữ: `tcp_test.c` kiểm tra bằng `strstr(rBuff, "110")`; nếu gửi thêm chữ có thể vô tình chứa một mã khác.

| Mã | Khi nào |
|---|---|
| 100 | ngay khi kết nối thành công |
| 110 | đăng nhập thành công |
| 120 | đăng bài thành công |
| 130 | đăng xuất thành công |
| 211 | tài khoản bị khóa (status 0) |
| 212 | tài khoản không tồn tại (kể cả username rỗng) |
| 213 | phiên này **đã** đăng nhập rồi |
| **214** | tài khoản đang được đăng nhập ở **phiên khác** (mới trong HW6) |
| 221 | POST/BYE khi chưa đăng nhập |
| 300 | không nhận dạng được loại request (hoặc dòng quá dài) |

**Thứ tự kiểm tra khi nhận `USER x`** (trong `session.c: handleLogin`):

```
phiên này đã đăng nhập?        → 213
x không có trong account.txt?  → 212
x có status 0?                 → 211
x đang được phiên khác giữ?    → 214
ngược lại                      → 110 (và đánh dấu x đang đăng nhập)
```

Quy tắc nhận dạng request (`protocol.c`): từ khóa **phân biệt hoa thường**, cách tham số đúng **1 dấu cách**.
`USER` hoặc `USER ` (không tên) vẫn được coi là lệnh đăng nhập → trả `212` (test của thầy yêu cầu `USER \r\n` **không** được trả 300). `BYE` phải đứng một mình; `BYE abc` → 300.

---

## 3. Mô hình đa luồng và đồng bộ

### 3.1. Một thread cho một client ("thread-per-connection")

- **Main thread** chỉ làm một việc: `accept()` kết nối mới rồi giao cho một thread mới.
- **Mỗi client có một thread riêng**, chạy `handleClient()` cho đến khi client ngắt kết nối. Một client chậm (ví dụ đang gõ bài) chỉ làm thread của chính nó chờ ở `recv()`; các client khác không bị ảnh hưởng.
- Thread được tạo ở trạng thái **detached** (`PTHREAD_CREATE_DETACHED`): khi thread kết thúc, hệ điều hành tự thu hồi tài nguyên, không cần ai `pthread_join`. Đây là phiên bản thread của việc "dọn zombie" bằng `SIGCHLD` ở HW5.
- Stack mỗi thread đặt **256 KiB** thay vì mặc định 8 MiB, vì một phiên chỉ dùng khoảng 12–16 KiB stack. Nhờ vậy server chịu được hàng nghìn client mà không tốn không gian địa chỉ vô ích (đã thử 1000 client cùng lúc → 1001 thread).

### 3.2. Dữ liệu dùng chung và mutex

Khác biệt quan trọng nhất giữa process (HW5) và thread (HW6): **các thread dùng chung bộ nhớ**.
HW6 yêu cầu "một tài khoản chỉ được đăng nhập trên một client", nên server phải có **một danh sách chung** các tài khoản đang đăng nhập mà mọi thread đều đọc/ghi → phải bảo vệ bằng **mutex**.

Toàn bộ chương trình chỉ có **đúng một** dữ liệu dùng chung có thể thay đổi: danh sách `loggedIn` trong `registry.c`, bảo vệ bởi `registryLock`. Mọi thứ khác hoặc là:
- **riêng của từng thread** (biến `Session` nằm trên stack của thread đó), hoặc
- **chỉ đọc** (đường dẫn `"account.txt"`), hoặc
- **được chuyển quyền sở hữu** (struct `ClientTask` do main thread `malloc`, client thread `free`).

### 3.3. Tránh race condition khi đăng nhập

Tình huống nguy hiểm: 2 client gửi `USER admin` cùng lúc. Nếu viết

```c
if (!dangDangNhap("admin"))   // thread A và B cùng thấy "chưa"
    themVao("admin");         // cả hai cùng thêm → 2 người cùng giữ admin
```

thì cả hai đều thành công. Bài này gộp **kiểm tra + thêm vào trong cùng một lần giữ khóa** (`claimAccount`), nên chỉ đúng một thread thắng.
Đã kiểm chứng: 300 kết nối gửi `USER admin` cùng một lúc → đúng **1 lần `110`, 299 lần `214`**.

### 3.4. Luôn trả lại tài khoản khi phiên kết thúc

Đề cho phép kết thúc phiên bằng `BYE` **hoặc đóng chương trình client**. Nếu chỉ giải phóng tài khoản khi nhận `BYE`, client đóng cửa sổ sẽ làm tài khoản bị "kẹt" mãi ở trạng thái đăng nhập.
`handleClient()` luôn gọi `logout()` **sau khi** vòng lặp phục vụ kết thúc, bất kể vì lý do gì (BYE rồi đóng, client tắt, lỗi mạng). `tcp_test.c` kiểm tra đúng điều này: kết nối chính đăng nhập `admin` rồi `close()` mà không BYE; ngay sau đó worker2 phải đăng nhập `admin` thành công.

### 3.5. Về mục "[6] Thread scheduling: 1 điểm"

Đề không giải thích mục này. Bài làm đáp ứng theo cách hiểu phổ biến:
- main thread không bao giờ bị chặn bởi một client nào (chỉ `accept`);
- mỗi client được phục vụ song song trên thread riêng, các thread không chờ nhau ngoại trừ đoạn khóa rất ngắn trong `registry.c` (chỉ duyệt danh sách, **không** đọc file hay gửi mạng khi đang giữ khóa);
- thread tự giải phóng (detached), không rò rỉ thread hay bộ nhớ;
- lỗi tạo thread (hết tài nguyên) chỉ làm từ chối **một** kết nối, server vẫn chạy.

---

## 4. Makefile

```make
CC      := gcc
CFLAGS  := -pthread -std=c11 -Wall -Wextra -O2 -MMD -MP -D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64
```

| Cờ | Tác dụng |
|---|---|
| `-pthread` | bật hỗ trợ POSIX threads khi **biên dịch** (định nghĩa `_REENTRANT`) và khi **liên kết** (link `libpthread`). Trên Ubuntu 20.04 (glibc 2.31) thiếu cờ này sẽ lỗi link `pthread_create`. |
| `-std=c11` | chuẩn C11 nghiêm ngặt (không có phần mở rộng GNU). |
| `-D_POSIX_C_SOURCE=200809L` | vì `-std=c11` ẩn các hàm POSIX, cần macro này để có khai báo `getline`, `strtok_r`, `nanosleep`, `sigaction`, `inet_ntop`… |
| `-Wall -Wextra` | bật cảnh báo; bài build **không có cảnh báo nào** trên GCC 9.4. |
| `-O2` | tối ưu. |
| `-MMD -MP` | tự sinh file phụ thuộc `.d` để khi sửa một `.h` thì các `.c` liên quan được build lại. |
| `-D_FILE_OFFSET_BITS=64` | giữ lại từ HW4 (file lớn), vô hại với bài này. |

Các quy tắc:
- `SERVER_SOURCES := $(wildcard TCP_Server/*.c)` – tự lấy **mọi** file `.c`, thêm file mới (như `registry.c`) không cần sửa Makefile.
- Object được đặt riêng trong `build/server/` và `build/client/`. Hai thư mục có file trùng tên (`main.c`, `stream.c`) nên **bắt buộc** phải tách, nếu không `main.o` của client sẽ đè `main.o` của server.
- `| $(BUILD_DIR)/server` là *order-only prerequisite*: tạo thư mục trước khi biên dịch nhưng không làm build lại khi thời gian sửa thư mục thay đổi.
- `make` → ra `./server` và `./client` (đúng tên đề yêu cầu); `make clean` → xóa `build/`, `server`, `client`.

---

## 5. TCP_Server – từng file

### 5.1. `TCP_Server/main.c`

Điểm vào của server: `./server Port_Number`.

**`parsePort(text, &port)`** – đổi chuỗi sang số bằng `strtol` và kiểm tra chặt:
- `end == text` → chuỗi rỗng hoặc không bắt đầu bằng số;
- `*end != '\0'` → có ký tự thừa (`"55a"`);
- ngoài khoảng 1–65535 → sai.
Dùng `strtol` thay vì `atoi` vì `atoi("abc")` trả 0 mà không báo lỗi.

**`main`** thực hiện theo thứ tự:
1. Kiểm tra đúng 1 tham số, cổng hợp lệ.
2. `checkAccountFile("account.txt")` – nếu file không đọc được thì **dừng ngay khi khởi động** với thông báo rõ ràng, thay vì chạy lên rồi mọi lần đăng nhập đều thất bại. File được tìm ở **thư mục hiện tại** khi chạy `./server`.
3. `signal(SIGPIPE, SIG_IGN)` – khi ghi vào socket mà client đã đóng, Linux gửi tín hiệu `SIGPIPE`, mặc định **giết cả process** (tức là giết tất cả thread). Bỏ qua tín hiệu này thì `send()` chỉ trả lỗi `EPIPE`. (Ngoài ra `stream.c` còn dùng cờ `MSG_NOSIGNAL` – hai lớp bảo vệ.)
4. `setvbuf(stdout, NULL, _IOLBF, 0)` – stdout đệm theo dòng, để log hiện ngay kể cả khi chuyển hướng ra file. `printf` của glibc tự có khóa nội bộ nên mỗi lệnh `printf` từ nhiều thread không bị trộn lẫn giữa chừng.
5. `createServerSocket(port)` rồi `runServerLoop(...)` – vòng lặp vô hạn.

### 5.2. `TCP_Server/server.h` / `server.c`

Phần **mạng + đa luồng** của server.

#### Hằng số và kiểu dữ liệu

```c
#define CLIENT_ADDRESS_LEN (INET_ADDRSTRLEN + 8)   // "255.255.255.255:65535" + '\0'
#define CLIENT_THREAD_STACK_SIZE (256 * 1024)

typedef struct {
    int connFd;                               // socket của client
    char clientAddress[CLIENT_ADDRESS_LEN];   // "IP:port", dùng cho log
    const char *accountPath;                  // "account.txt"
} ClientTask;
```

`ClientTask` là "gói hàng" main thread giao cho client thread. **Tại sao phải `malloc` mà không truyền `&connFd`?** Nếu truyền địa chỉ của biến cục bộ `connFd` trong vòng lặp, thì trước khi thread mới kịp đọc, main thread có thể đã `accept()` client tiếp theo và **ghi đè** `connFd` → hai thread phục vụ cùng một socket, một socket bị bỏ rơi. Mỗi kết nối có `ClientTask` riêng trên heap thì không thể xảy ra chuyện đó. Quy ước sở hữu: main thread cấp phát → client thread `free` khi xong; nếu tạo thread thất bại thì main thread tự `free`.

#### `createServerSocket(port)`
1. `socket(AF_INET, SOCK_STREAM, 0)` – socket TCP IPv4.
2. `setsockopt(SO_REUSEADDR)` – cho phép chạy lại server ngay trên cùng cổng sau khi tắt (không phải chờ trạng thái `TIME_WAIT` hết hạn).
3. `bind` vào `INADDR_ANY:port` (mọi card mạng). `htons`/`htonl` đổi sang thứ tự byte mạng.
4. `listen(listenFd, SOMAXCONN)` – hàng đợi kết nối tối đa của hệ thống, để khi nhiều client kết nối dồn dập (test 64 và 160 kết nối) không bị từ chối.
Lỗi ở bước nào thì `perror`, đóng socket, trả `-1`.

#### `formatClientAddress` (static)
`inet_ntop` đổi địa chỉ nhị phân sang chuỗi, ghép thành `"IP:port"`. `inet_ntop` an toàn với thread (khác với `inet_ntoa` dùng buffer tĩnh).

#### `isResourceError(error)` (static)
Trả 1 nếu lỗi là do **hết tài nguyên**: `EMFILE`/`ENFILE` (hết file descriptor), `ENOBUFS`/`ENOMEM` (hết bộ nhớ), `EAGAIN` (`pthread_create` báo hết giới hạn thread). Khi đó vòng lặp ngủ 1 giây để tránh quay vòng 100% CPU trong lúc chờ tài nguyên được trả lại.

#### `clientThread(arg)` (static) – thân của mỗi client thread
```c
setsockopt(task->connFd, IPPROTO_TCP, TCP_NODELAY, ...);
printf("Client %s connected\n", ...);
handleClient(task->connFd, task->clientAddress, task->accountPath);
close(task->connFd);
printf("Client %s disconnected\n", ...);
free(task);
return NULL;
```
- `TCP_NODELAY` tắt thuật toán Nagle: mỗi phản hồi ngắn (5 byte) được gửi đi **ngay**, không bị giữ lại để gộp với phản hồi sau. Cần cho "Stream test 1" (xem 5.3).
- Thread tự đóng socket và tự giải phóng `task` → không cần main thread dọn.

#### `startClientThread(task)` (static)
```c
pthread_attr_init(&attr);
pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
pthread_attr_setstacksize(&attr, CLIENT_THREAD_STACK_SIZE);
pthread_create(&thread, &attr, clientThread, task);
pthread_attr_destroy(&attr);
```
Lưu ý: các hàm `pthread_*` **trả mã lỗi trực tiếp** (không đặt `errno`), nên code lấy giá trị trả về để in bằng `strerror(error)`.
Gọi `pthread_attr_destroy` ngay sau `pthread_create` là hợp lệ – thread đã được tạo xong với thuộc tính đó.

#### `runServerLoop(listenFd, accountPath)`
Vòng lặp vô hạn của main thread:
1. `accept()`. Nếu lỗi: `EINTR` (bị tín hiệu ngắt) và `ECONNABORTED` (client hủy trước khi accept xong) là bình thường → thử lại im lặng; lỗi khác → `perror` (và ngủ 1s nếu hết tài nguyên). **Không bao giờ thoát vòng lặp** → đáp ứng tiêu chí "server phục vụ nhiều client liên tục".
2. `malloc` một `ClientTask`; hết bộ nhớ → đóng kết nối này, tiếp tục.
3. Điền `connFd`, `accountPath`, địa chỉ client.
4. `startClientThread`. Thất bại → in lỗi, đóng socket (client thấy kết nối bị đóng), `free(task)`, tiếp tục.
Sau khi giao cho thread, main thread **không chạm vào** `task` hay `connFd` nữa.

### 5.3. `TCP_Server/session.h` / `session.c`

Xử lý **một phiên** làm việc của một client. Mọi hàm ở đây chạy trong client thread.

#### Cấu trúc `Session`
```c
typedef struct {
    Connection conn;                 // socket + bộ đệm nhận (stream.h)
    const char *clientAddress;       // để in log
    const char *accountPath;         // "account.txt"
    bool loggedIn;                   // trạng thái đăng nhập CỦA PHIÊN NÀY
    char username[MAX_LINE_LEN + 1]; // tài khoản đang đăng nhập
    LoginEntry login;                // mắt xích trong registry khi loggedIn == true
} Session;
```
`Session` là biến cục bộ trong `handleClient`, nằm trên **stack của client thread** → mỗi thread có bản riêng, không cần khóa. Đây đúng là gợi ý của đề: *"trạng thái đăng nhập phải được quản lý ở server và được kiểm tra mỗi khi nhận request"*.

Phần duy nhất được chia sẻ là `login` (được móc vào danh sách chung của `registry.c`).

#### `handleLogin(session, username)` – xử lý `USER`
1. `loggedIn` đã true → `213`.
2. `findAccount()` đọc `account.txt`:
   - `ACCOUNT_BANNED` → `211`;
   - `ACCOUNT_NOT_FOUND` → `212`;
   - `ACCOUNT_FILE_ERROR` (file bị xóa/khóa quyền lúc đang chạy) → in lỗi ra stderr và trả `212` (không xác minh được tài khoản thì không cho đăng nhập); server vẫn chạy.
3. Tài khoản hợp lệ: **copy username vào `session->username` trước**, rồi mới `claimAccount(&session->login, session->username)`. Thứ tự này quan trọng: registry lưu **con trỏ** tới `session->username`, nên chuỗi đó phải là bộ nhớ thuộc session (không phải con trỏ vào bộ đệm dòng `line` – sẽ bị ghi đè ở request sau).
4. `claimAccount` thất bại → xóa username, trả `214`. Thành công → `loggedIn = true`, trả `110`.

Đọc file **không** nằm trong vùng khóa: việc đọc 10.240 dòng tốn thời gian, nếu giữ khóa trong lúc đó mọi thread khác muốn đăng nhập phải chờ.

#### `handlePost(session)` – xử lý `POST`
Chưa đăng nhập → `221`, ngược lại → `120`. Đề không yêu cầu lưu bài; nội dung bài hiện trong log server (dòng `[IP:port] POST nội dung -> 120`).

#### `logout(session)` (static) và `handleLogout(session)` – xử lý `BYE`
- `logout` là hàm dọn dẹp dùng chung: nếu đang đăng nhập thì `releaseAccount`, đặt `loggedIn = false`, xóa username. Gọi khi chưa đăng nhập thì không làm gì → an toàn khi gọi nhiều lần.
- `handleLogout`: chưa đăng nhập → `221`; ngược lại `logout()` rồi `130`.

#### `processRequest(session, line)` (static)
Gọi `parseRequest` (protocol.c) để biết loại lệnh rồi chuyển cho handler tương ứng; loại không xác định → `300`.

#### `serveRequests(session)` (static) – vòng lặp chính của phiên
1. Gửi `100` (chào mừng). Lỗi gửi → kết thúc phiên.
2. Lặp:
   - `recvLine()` lấy **một thông điệp hoàn chỉnh** (đến `\n`).
   - `STREAM_TOO_LONG` (dòng > 4096 byte) → trả `300`, phiên **tiếp tục** (phần thừa của dòng đã được `recvLine` bỏ đi).
   - Lỗi khác / client đóng → `return`.
   - Dòng chứa byte `'\0'` ở giữa (`length != strlen(line)`) → `300`: nếu không kiểm tra, phần sau `'\0'` bị "giấu" đi và `USER tung\0bt` sẽ bị hiểu thành `USER tung`.
   - In log `[IP:port] request -> mã`, gửi mã trả lời.
   - **Giãn cách khi client gửi dồn**: nếu sau khi xử lý một dòng mà bộ đệm vẫn còn dữ liệu chưa xử lý (`conn.start != conn.end`), tức client đã gửi nhiều request trong một gói, thì ngủ 20 ms trước khi xử lý request tiếp theo.

   *Tại sao có đoạn giãn cách?* "Stream test 1" của `tcp_test.c` gửi `USER admin\r\nPOST Hello world\r\nPOST Test stream\r\n` trong **một** lần `send`, rồi gọi `recv` **ba lần**, mỗi lần chờ tối đa 100 ms, và mong mỗi lần nhận đúng một mã. TCP là luồng byte, không giữ ranh giới thông điệp: nếu server trả 3 mã liền nhau, chúng thường đến client trong **một** lần `recv` (`110\r\n120\r\n120\r\n`) và hai lần `recv` sau bị timeout → test in "fail" dù server trả lời đúng. `TCP_NODELAY` + giãn 20 ms làm mỗi mã đến thành một gói riêng. Client thật (chương trình `client`) luôn chờ phản hồi rồi mới gửi tiếp, nên bộ đệm luôn rỗng sau mỗi dòng và **không bao giờ** phải chờ 20 ms.

#### `handleClient(connFd, clientAddress, accountPath)` – hàm public
Khởi tạo `Session`, gọi `serveRequests`, rồi **luôn** gọi `logout()`. Việc tách vòng lặp ra `serveRequests` (có nhiều chỗ `return`) giúp đảm bảo chỉ có **một** đường thoát duy nhất đi qua bước dọn dẹp – tài khoản không bao giờ bị kẹt. Đồng thời, vì `logout` chạy trước khi `handleClient` trả về (tức trước khi `Session` trên stack biến mất), registry không bao giờ giữ con trỏ tới vùng nhớ đã hết hạn.

### 5.4. `TCP_Server/registry.h` / `registry.c`

**File mới của HW6**: danh sách các tài khoản đang đăng nhập, dùng chung cho mọi thread.

#### Kiểu dữ liệu
```c
typedef struct LoginEntry {
    const char *username;      // trỏ tới session->username
    struct LoginEntry *next;
} LoginEntry;
```
Đây là **danh sách liên kết "nhúng" (intrusive list)**: mỗi `Session` đã chứa sẵn một `LoginEntry`, registry chỉ móc các mắt xích đó lại với nhau. Ưu điểm:
- **không cần `malloc`** → `claimAccount` không bao giờ thất bại vì hết bộ nhớ, không có rò rỉ;
- mỗi phiên có tối đa 1 mắt xích → giải phóng chính xác mắt xích của mình.

#### Biến toàn cục (dữ liệu dùng chung duy nhất của chương trình)
```c
static LoginEntry *loggedIn = NULL;
static pthread_mutex_t registryLock = PTHREAD_MUTEX_INITIALIZER;
```
`static` → chỉ nhìn thấy trong file này; muốn đụng tới danh sách bắt buộc phải qua 2 hàm bên dưới, và cả hai đều khóa mutex. `PTHREAD_MUTEX_INITIALIZER` khởi tạo mutex tĩnh, không cần gọi `pthread_mutex_init` trong `main`.

#### `claimAccount(entry, username)`
```
lock
  duyệt danh sách: có ai trùng username?  → claimed = false
  nếu không trùng: gắn entry vào đầu danh sách
unlock
return claimed
```
Kiểm tra và thêm **trong cùng một lần khóa** → nguyên tử (atomic), chống race như đã phân tích ở mục 3.3. So sánh bằng `strcmp` (phân biệt hoa thường, giống `account.c`).

#### `releaseAccount(entry)`
Duyệt bằng **con trỏ tới con trỏ** (`LoginEntry **link`): `link` lần lượt trỏ vào `loggedIn`, rồi vào `next` của từng phần tử. Khi `*link == entry` thì `*link = entry->next` – gỡ phần tử ra mà không cần xử lý riêng trường hợp phần tử đầu danh sách. Không tìm thấy thì không làm gì.

Độ phức tạp O(n) với n = số người đang đăng nhập – với quy mô bài tập (vài nghìn) là không đáng kể; vùng khóa chỉ có thao tác bộ nhớ, không có I/O.

### 5.5. `TCP_Server/account.h` / `account.c`

Đọc file tài khoản `account.txt` (mỗi dòng `username status`).

**Đặc điểm file thầy cho** (đã kiểm tra): 10.240 dòng, xuống dòng kiểu **Windows CRLF** (`\r\n`), có một username dài **327 ký tự**, `ductq` có status 0.

#### `checkAccountFile(path)`
Thử `fopen` để đọc; dùng lúc khởi động server.

#### `parseAccountLine(line, &username)` (static)
Tách dòng bằng `strtok_r` với tập phân cách `" \t\r\n"`:
- token 1 = username, token 2 = status, **không được có token 3**;
- status phải đúng `"0"` hoặc `"1"`;
- sai định dạng → trả `-1` (dòng bị bỏ qua, không làm hỏng việc đọc).
Đưa `\r` vào tập phân cách để xử lý file CRLF – nếu không, status sẽ là `"1\r"` và không khớp `"1"`.
Dùng **`strtok_r`** chứ không dùng `strtok`: `strtok` lưu vị trí trong biến tĩnh ẩn, nhiều thread cùng gọi sẽ phá nhau; `strtok_r` lưu vị trí vào biến `save` cục bộ → **thread-safe**.

#### `findAccount(path, username)`
1. Username rỗng → `ACCOUNT_NOT_FOUND` ngay (để `USER \r\n` → 212).
2. `fopen` (mỗi lần gọi mở file riêng → mỗi thread có `FILE*` riêng, không chia sẻ).
3. Đọc từng dòng bằng **`getline`**: tự cấp phát và nới rộng bộ đệm theo độ dài dòng → không giới hạn độ dài (dòng 327 ký tự vẫn đúng; nếu dùng `fgets` với buffer 256 byte thì dòng đó bị cắt làm hai và sai).
4. Gặp dòng khớp đầu tiên → `ACCOUNT_ACTIVE` hoặc `ACCOUNT_BANNED`.
5. Đọc hết không thấy: nếu `ferror(file)` (lỗi đọc giữa chừng) → `ACCOUNT_FILE_ERROR`, ngược lại `ACCOUNT_NOT_FOUND`.
6. `free(line)`, `fclose(file)`.

File được **đọc lại mỗi lần đăng nhập** → nếu sửa `account.txt` (khóa/mở một tài khoản) khi server đang chạy, lần đăng nhập sau sẽ áp dụng ngay, không cần khởi động lại.

### 5.6. `TCP_Server/protocol.h` / `protocol.c`

Định nghĩa giao thức ở một chỗ duy nhất.

**`protocol.h`**:
- `MAX_LINE_LEN` = `MAX_MESSAGE_LEN` = 4096 byte – độ dài tối đa một request.
- Tên lệnh `"USER"`, `"POST"`, `"BYE"`.
- Tất cả mã phản hồi dạng chuỗi: `REPLY_WELCOME "100"`, …, `REPLY_LOGGED_IN_ELSEWHERE "214"`, `REPLY_UNKNOWN_REQUEST "300"`. Đặt tên có nghĩa giúp `session.c` dễ đọc (`return REPLY_NOT_LOGGED_IN;` thay vì `return "221";`).
- `enum { REQUEST_LOGIN, REQUEST_POST, REQUEST_LOGOUT, REQUEST_UNKNOWN }`.

**`protocol.c`**:
- `matchCommand(line, keyword, &argument)` (static): dòng khớp nếu là `keyword` đứng một mình, hoặc `keyword` + **một** dấu cách + tham số. `argument` trỏ vào phần sau dấu cách (hoặc chuỗi rỗng). Ví dụ: `"USERtungbt"` không khớp (thiếu dấu cách) → 300; `"user tungbt"` không khớp (chữ thường) → 300.
- `parseRequest(line, &argument)`: thử USER, rồi POST, rồi `BYE` (so sánh **nguyên dòng** – BYE không có tham số), còn lại → `REQUEST_UNKNOWN`.
Hàm không sửa `line` và không dùng biến tĩnh → thread-safe.

### 5.7. `TCP_Server/stream.h` / `stream.c` (dùng chung với client)

Giải quyết bài toán **"xử lý truyền dòng" (stream transmission, 2 điểm)**: TCP là luồng byte, một lần `recv()` có thể nhận **một phần** thông điệp, **đúng một**, hoặc **nhiều** thông điệp dính nhau. Ranh giới thông điệp được xác định bằng ký tự `"\n"` (có `"\r"` đứng trước thì bỏ).

#### Cấu trúc `Connection`
```c
typedef struct {
    int fd;
    char buffer[STREAM_BUFFER_SIZE];   // 4096 byte đã nhận nhưng chưa dùng
    size_t start;                      // vị trí byte chưa đọc đầu tiên
    size_t end;                        // vị trí sau byte cuối cùng đã nhận
} Connection;
```
Byte còn thừa sau một dòng (thuộc thông điệp tiếp theo) **được giữ lại** trong `buffer[start..end)` cho lần gọi sau – không bị mất. Mỗi phiên có `Connection` riêng (nằm trong `Session`) nên không cần khóa.

#### `recvLine(conn, line, size)`
```
lặp:
  nếu bộ đệm rỗng (start == end): recv() thêm dữ liệu vào buffer
      (client đóng → STREAM_CLOSED, lỗi → STREAM_ERROR)
  lấy 1 byte c = buffer[start++]
  c == '\n'  → hết dòng, thoát
  dòng đã đầy → đánh dấu tooLong, BỎ byte này nhưng vẫn đọc tiếp đến '\n'
  ngược lại   → line[length++] = c
tooLong → trả STREAM_TOO_LONG
bỏ '\r' cuối dòng (nếu có), thêm '\0', trả độ dài
```
Ba tình huống của TCP đều đúng:
- **Thiếu** (`"POST I am tungbt"` chưa có `\r\n`): bộ đệm cạn, hàm gọi `recv` lần nữa và chờ – server **không** trả lời nửa thông điệp (Stream test 2).
- **Đủ một**: trả về dòng đó.
- **Nhiều dòng dính nhau**: trả dòng đầu, phần còn lại nằm trong bộ đệm, lần gọi sau lấy tiếp mà không cần `recv` (Stream test 1).

Dòng quá dài vẫn được đọc **hết đến `\n`** rồi mới báo lỗi, để lần gọi sau bắt đầu đúng ở thông điệp kế tiếp (không bị lệch luồng). Chấp nhận cả `"\n"` trơn (không có `\r`).

#### `receiveSome(fd, data, size)` (static)
Gọi `recv` một lần; `recv` trả 0 → client đóng kết nối; bị ngắt bởi tín hiệu (`EINTR`) → gọi lại.

#### `sendAll(fd, data, length)`
`send()` có thể gửi **ít hơn** số byte yêu cầu (gửi một phần) – hàm lặp đến khi gửi hết. Cờ `MSG_NOSIGNAL`: gửi vào socket đã đóng trả lỗi `EPIPE` thay vì sinh `SIGPIPE`.

#### `sendMessage(fd, message)`
Ghép `message + "\r\n"` vào một buffer rồi `sendAll` **một lần** (để mã và `\r\n` đi cùng một gói). Từ chối message dài hơn 4096 byte.

---

## 6. TCP_Client – từng file

Client **không** dùng thread (đề chỉ yêu cầu server đa luồng); nó là chương trình tuần tự: hiện menu → đọc lựa chọn → gửi request → chờ và hiển thị phản hồi.

### 6.1. `TCP_Client/main.c`

Giao diện người dùng (đáp ứng tiêu chí "client có giao diện và thông báo phù hợp", nếu thiếu bị −1 điểm).

#### `ClientState`
```c
typedef struct {
    Connection conn;
    bool loggedIn;
    char username[INPUT_BUFFER_SIZE];
} ClientState;
```
Client chỉ lưu trạng thái **để hiển thị** ("Logged in as: tungbt"). Server mới là nơi quyết định: client **không chặn trước** các thao tác (ví dụ vẫn gửi `POST` khi chưa đăng nhập) để người dùng thấy đúng thông báo lỗi `221` từ server, khớp kịch bản test trong đề.

#### Màn hình
```
>> [100] Connected to the message board

========== MESSAGE BOARD ==========
Logged in as: tungbt          (hoặc "Not logged in")
-----------------------------------
  1. Log in
  2. Post a message
  3. Log out
  4. Exit
===================================
Your choice: 1
Username: admin
>> [213] You are already logged in, log out first
```

#### Các hàm
- `parsePort` – giống server.
- `printResponse(code)` – in `>> [mã] câu giải thích` (dùng `describeResponse`).
- `printMenu(state)` – vẽ menu và trạng thái đăng nhập; `fflush(stdout)` để dòng "Your choice:" (không có `\n`) hiện ngay.
- `prompt(label, input, size)` – in nhãn và đọc một dòng.
- `exchange(state, command, argument, response, size)` – gửi request qua `sendRequest`, in phản hồi; mất kết nối → in `>> Lost connection to the server` và trả −1.
- `handleLogin`:
  - đọc username, `trimSpaces` bỏ khoảng trắng hai đầu;
  - kiểm tra **phía client**: rỗng → "Username must not be empty"; quá dài → báo giới hạn (không gửi);
  - gửi `USER username`; nhận `110` → cập nhật `state.username` để hiển thị.
- `handlePost`: đọc nội dung bài (giữ nguyên khoảng trắng bên trong), chặn bài rỗng/chỉ có dấu cách và bài quá dài; gửi `POST nội dung`.
- `handleLogout`: gửi `BYE`; `130` → xóa trạng thái hiển thị.
- `readChoice()`: chỉ chấp nhận đúng một ký tự `1`–`4` (sau khi bỏ khoảng trắng); `abc`, `12`, dòng trống → "Invalid choice", không thoát. Hết input (Ctrl+D) → trả −1.
- Mỗi handler trả **1 = chạy tiếp**, **0 = kết thúc chương trình** (mất kết nối hoặc hết input).

#### `main`
1. Kiểm tra tham số `IP_Addr Port_Number`, `signal(SIGPIPE, SIG_IGN)` (server tắt giữa chừng không giết client).
2. `connectToServer`, nhận mã chào mừng; không phải `100` → thoát.
3. Vòng lặp menu. Chọn 4 hoặc Ctrl+D → in `Goodbye!`, đóng socket. Đóng socket chính là "đóng chương trình client" – server sẽ tự giải phóng tài khoản (mục 3.4), nên không bắt buộc gửi BYE trước khi thoát.

Mọi lỗi (server tắt, mạng đứt) đều được bắt và thông báo, client **thoát bình thường** với mã 0 chứ không crash (tránh −1 điểm "runtime error causing the client to terminate").

### 6.2. `TCP_Client/client.h` / `client.c`

Phần mạng của client.

**`client.h`**:
- Tên lệnh, các mã thành công cần so sánh (`100`, `110`, `120`, `130`).
- `MAX_USERNAME_LEN = MAX_MESSAGE_LEN - sizeof("USER")`: `sizeof("USER")` = 5 (4 chữ + `'\0'`), đúng bằng độ dài `"USER "` (4 chữ + dấu cách) → username dài nhất vừa khít một request 4096 byte. Tương tự `MAX_ARTICLE_LEN`.

**`client.c`**:
- `RESPONSE_MESSAGES[]` – bảng tra mã → câu thông báo tiếng Anh, có thêm `214 → "This account is already logged in on another client"`.
- `connectToServer(ip, port)`: `inet_pton` kiểm tra IP (sai → báo "invalid IP address"), `socket`, `connect`.
- `sendRequest(conn, command, argument, response, size)`: ghép `"COMMAND"` hoặc `"COMMAND argument"` bằng `snprintf` (kiểm tra tràn), `sendMessage` rồi `recvLine` để nhận **đúng một** dòng phản hồi.
- `describeResponse(code)`: tra bảng; mã lạ → "Unknown response from the server".

### 6.3. `TCP_Client/util.h` / `util.c`

Đọc bàn phím an toàn.

- `readLine(buffer, size)`: `fgets` một dòng, bỏ `\n` (và `\r`). Nếu dòng dài hơn buffer, **đọc bỏ phần còn lại đến hết dòng** và trả `READ_TOO_LONG` – nếu không, phần thừa sẽ bị đọc nhầm thành lựa chọn menu ở lần sau. Hết input → `READ_EOF`.
- `trimSpaces(text)`: bỏ dấu cách/tab ở hai đầu (sửa tại chỗ, trả con trỏ tới ký tự đầu tiên không trống).

### 6.4. `TCP_Client/stream.h` / `stream.c`

**Giống hệt** bản của server (mục 5.7). Client cũng cần tách thông điệp theo `\r\n` vì phản hồi của server có thể đến dính nhau hoặc bị chia nhỏ. Hai thư mục phải độc lập (đề yêu cầu tách mã nguồn client/server) nên file được để ở cả hai nơi.

---

## 7. Đi theo một request từ đầu đến cuối

Người dùng ở cửa sổ 2 chọn `1`, gõ `tungbt`, trong khi cửa sổ 1 đang đăng nhập `tungbt`:

1. **client/main.c** `handleLogin` đọc `"tungbt"`, gọi `exchange` → `sendRequest(conn, "USER", "tungbt")`.
2. **client/client.c** ghép `"USER tungbt"`, **stream.c** `sendMessage` gửi `USER tungbt\r\n`.
3. **server**: thread của cửa sổ 2 đang chờ trong `recvLine` → nhận được byte, gặp `\n`, trả `"USER tungbt"`.
4. **session.c** `serveRequests` → `processRequest` → **protocol.c** `parseRequest` → `REQUEST_LOGIN`, argument = `"tungbt"`.
5. `handleLogin`: phiên chưa đăng nhập → **account.c** `findAccount` đọc file → `ACCOUNT_ACTIVE`.
6. Copy `"tungbt"` vào `session.username`; **registry.c** `claimAccount` khóa mutex, thấy `"tungbt"` đã có (mắt xích của cửa sổ 1) → mở khóa, trả `false`.
7. `handleLogin` trả `"214"`; server in log `[127.0.0.1:41574] USER tungbt -> 214` và `sendMessage("214")`.
8. **client** `recvLine` nhận `"214"` → `printResponse` → `>> [214] This account is already logged in on another client`. Trạng thái vẫn "Not logged in".
9. Sau đó cửa sổ 1 chọn `4. Exit` (không gửi BYE): thread của cửa sổ 1 nhận `STREAM_CLOSED` → `serveRequests` return → `handleClient` gọi `logout` → `releaseAccount` gỡ `"tungbt"`. Cửa sổ 2 thử lại → `110`.

---

## 8. Kiểm thử đã thực hiện

Tất cả chạy trên **Ubuntu 20.04.5, GCC 9.4.0, glibc 2.31** (đúng môi trường chấm).

| Kiểm thử | Kết quả |
|---|---|
| `make` từ thư mục giải nén sạch của file zip | thành công, **0 cảnh báo** |
| `tcp_test 5600 10` của thầy, chạy lặp 30 lần | **0 dòng "fail"**, 0 "Server closes connection" |
| Phần tuần tự của test | `221 221 211 110 213 120 130 221 110 130 212 300 110 120 120 120` – đúng như test mong đợi |
| Concurrency test 2 (64 kết nối cùng `USER admin`, giữ kết nối) | 1 × `110`, 63 × `214`; 64/64 phiên có phản hồi |
| Worker3 (10 thread × 16 kết nối) | 160/160 phiên phản hồi, 1 × `110`, 159 × `214`, không timeout |
| 2 client thật cùng một tài khoản | cửa sổ 2 nhận `214`, POST → `221`; cửa sổ 1 thoát không BYE → cửa sổ 2 đăng nhập được `110` |
| Race: 300 kết nối gửi `USER admin` cùng lúc | đúng 1 × `110`, 299 × `214`; đóng hết → `admin` đăng nhập lại được |
| 1000 kết nối đồng thời, 1000 tài khoản khác nhau | 1000/1000 `110` + `120`; server có 1001 thread; đóng hết → còn **1 thread** |
| Dòng 10.000 byte; `user` chữ thường; `BYE abc`; byte NUL giữa dòng | `300`, phiên vẫn tiếp tục |
| Username 327 ký tự (dòng đầu `account.txt`) | `110` |
| Client ngắt giữa chừng một request | server không ảnh hưởng |
| `account.txt` mất quyền đọc khi đang chạy | trả `212`, server vẫn chạy; khôi phục quyền → `110` |
| Kill server khi client đang mở | client báo `Lost connection to the server`, thoát mã 0 |

**Cách chạy**

```bash
make
./server 5500                # account.txt phải nằm ở thư mục hiện tại
./client 127.0.0.1 5500      # mở nhiều terminal để thử nhiều client
gcc -pthread tcp_test.c -o test && ./test 5500 10
```

---

## 9. Câu hỏi thầy có thể hỏi

**Thread khác process (HW5) ở chỗ nào trong bài này?**
Ở HW5 mỗi client là một process, bộ nhớ tách biệt, nên "một tài khoản chỉ một client" sẽ cần cơ chế liên tiến trình (shared memory, file khóa…). Ở HW6 các thread chung bộ nhớ, nên chỉ cần một danh sách toàn cục + mutex. Đổi lại phải cẩn thận: không dùng biến tĩnh/toàn cục không được bảo vệ, không dùng hàm không thread-safe (`strtok`, `inet_ntoa`), không truyền con trỏ tới biến sẽ bị ghi đè (`&connFd`).

**Tại sao dùng detached thread mà không `pthread_join`?**
Main thread chạy vòng `accept` vô hạn, không có chỗ nào thích hợp để join. Thread không detach mà không join sẽ thành "zombie thread" giữ stack mãi. Detached thì tài nguyên được thu hồi ngay khi thread kết thúc.

**Nếu không có mutex thì sao?**
Hai thread cùng đăng nhập một tài khoản có thể cùng thấy "chưa ai giữ" và cùng thêm vào → vi phạm luật 214; hoặc một thread đang gỡ phần tử trong khi thread khác đang duyệt → đọc con trỏ hỏng, server có thể crash.

**Tại sao đọc `account.txt` ngoài vùng khóa?**
Đọc file chậm (10.240 dòng). Giữ khóa trong lúc đó làm mọi lần đăng nhập/đăng xuất của các client khác phải xếp hàng. Kết quả đọc file chỉ dùng trong thread hiện tại nên không cần khóa; chỉ bước "kiểm tra + đánh dấu đang đăng nhập" cần khóa.

**Client đóng cửa sổ thì tài khoản có bị kẹt không?**
Không. `recv` trả 0 → `serveRequests` kết thúc → `handleClient` luôn gọi `logout()` → `releaseAccount`.

**Xử lý "stream" như thế nào?**
`stream.c` giữ bộ đệm riêng cho mỗi kết nối và cắt thông điệp theo `\n`; thông điệp thiếu thì chờ thêm, thông điệp dư thì giữ lại cho lần sau (mục 5.7).

**Sleep 20 ms có làm server chậm không?**
Chỉ xảy ra khi một client gửi **nhiều request trong cùng một gói** mà không chờ phản hồi (chỉ chương trình test làm vậy) và chỉ làm chậm **chính client đó** (mỗi client một thread). Client thật không bao giờ gặp.

**Giới hạn của thiết kế?**
- Mỗi client một thread: rất nhiều client (hàng chục nghìn) sẽ tốn tài nguyên; khi đó nên dùng thread pool hoặc `epoll`. Với yêu cầu bài tập (đã thử 1000 client) là đủ.
- Bài đăng không được lưu lại (đề không yêu cầu), chỉ in ra log server.
- Request tối đa 4096 byte; dài hơn bị trả `300`.
