# Kết thúc trận

Mô tả luồng: [DESIGN.md](../../DESIGN.md)

```mermaid
sequenceDiagram
    autonumber
    participant A as A0
    participant S as Server
    participant P as Thread pool
    participant B as B0 - tàu cuối của đội B

    A->>S: {cmd:ATTACK, target:B0, missiles:2}
    S->>S: B0 HP về 0, đội B không còn tàu sống
    S-->>A: res 1000
    S->>A: ATTACK {destroyed:true, state}
    S->>B: ATTACK {destroyed:true, state}
    S->>A: SHIP_DESTROYED {player:B0, reason:ATTACK}
    S->>B: SHIP_DESTROYED {player:B0, reason:ATTACK}
    S->>A: MATCH_END {winner_team_id:5, loser_team_id:8, state}
    S->>B: MATCH_END {winner_team_id:5, loser_team_id:8, state}
    S->>S: hủy Match và đồng hồ, 2 đội về IDLE
    S->>P: job LOG(MATCH_END 5 thắng 8)
```
