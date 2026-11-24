# Lỗi luật chơi trong trận

Mô tả luồng: [DESIGN.md](../../DESIGN.md)

```mermaid
sequenceDiagram
    autonumber
    participant B as B0 - chưa tới lượt
    participant A as A0 - đang tới lượt
    participant S as Server

    B->>S: {cmd:ATTACK, target:A0, missiles:1}
    S-->>B: {res, code:7002} chưa tới lượt
    B->>S: {cmd:RELOAD, index:0}
    S-->>B: {res, code:1000}
    Note over B,S: RELOAD là ngoại lệ, làm được ngoài lượt

    A->>S: {cmd:ATTACK, target:B0, indexes:[0,1]}
    S-->>A: {res, code:7007} trộn pháo tự động và laze
    A->>S: {cmd:ATTACK, target:A1, missiles:1}
    S-->>A: {res, code:7006} bắn đồng đội
    A->>S: {cmd:BUY, item:MISSILE, qty:3}
    S-->>A: {res, code:7004} không đủ coin
    Note over A,S: Lệnh lỗi không tốn lượt, không trừ gì.<br/>Đồng hồ lượt vẫn chạy.

    A->>S: {cmd:CHEST_OPEN}
    S-->>A: res 1000
    A->>S: {cmd:BUY, item:AMMO_BOX, qty:1}
    S-->>A: {res, code:7012} đang mở rương
```
