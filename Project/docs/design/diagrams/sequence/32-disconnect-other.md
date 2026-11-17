# Mất kết nối ở sảnh chờ, khi mở rương, khi rảnh

Mô tả luồng: [DESIGN.md](../../DESIGN.md)

```mermaid
sequenceDiagram
    autonumber
    participant X as Client mất kết nối
    participant S as Server
    participant O as Những người liên quan

    alt X đang trong sảnh chờ (bất kể vai trò)
        X--xS: mất kết nối
        S->>S: X rời đội, 2 đội lệch số người
        S->>O: LOBBY_CLOSED {reason:DISCONNECTED}
        S->>O: TEAM_UPDATE cho đồng đội của X
        S->>S: 2 đội về IDLE
    else X đang tới lượt và đang mở rương
        X--xS: mất kết nối
        S->>S: rương mất, bỏ qua lượt
        S->>O: CHEST_RESULT {correct:false, coin:0} cho đội X
        S->>O: TURN_SKIPPED {player:X, reason:DISCONNECTED}
    else X không ở sảnh chờ, không trong trận
        X--xS: mất kết nối
        S->>S: hủy phiên, X tự rời đội, hủy lời mời và lời xin vào của X
        S->>O: TEAM_UPDATE cho đồng đội (chuyển đội trưởng nếu X là đội trưởng)
    end
```
