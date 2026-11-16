# Lỗi định dạng và lỗi chưa đăng nhập

Mô tả luồng: [DESIGN.md](../../DESIGN.md)

```mermaid
sequenceDiagram
    autonumber
    participant C as Client
    participant S as Server

    C->>S: {id:3, cmd:LOGIN, username (JSON thiếu ngoặc)
    S-->>C: {res, id:null, code:2001}
    C->>S: {id:4, cmd:FLY}
    S-->>C: {res, id:4, code:2002}
    C->>S: {id:5, cmd:TEAM_CREATE}
    S-->>C: {res, id:5, code:3001, msg:Not logged in}
    Note over S: Kiểm tra đăng nhập đứng trước kiểm tra tham số,<br/>nên trả 3001 chứ không phải 2003
    C->>S: (9000 byte không có xuống dòng)
    S-->>C: {res, id:null, code:2004}
    S->>S: đóng kết nối
```
