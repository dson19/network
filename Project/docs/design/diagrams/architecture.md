# Kiến trúc tổng thể

Mô tả luồng: [DESIGN.md](../DESIGN.md)

```mermaid
flowchart LR
    subgraph Client["Client - GTK3, 1 luồng"]
        UI["Giao diện GTK"]
        CNET["Net module<br/>g_io_add_watch"]
        CSTATE["Client state<br/>snapshot mới nhất"]
        UI -->|"lệnh người dùng"| CNET
        CNET -->|"response, event"| CSTATE
        CSTATE -->|"vẽ lại"| UI
    end

    subgraph Server["Server"]
        subgraph Main["Luồng chính - epoll"]
            LOOP["Event loop<br/>epoll_wait"]
            CONN["Connection<br/>buffer nhận / gửi"]
            DISP["Dispatcher<br/>parse JSON, kiểm tra quyền"]
            SESS["Session manager"]
            TEAM["Team manager"]
            LOBBY["Challenge và Lobby"]
            MATCH["Match engine"]
            TIMER["Timer<br/>đồng hồ lượt, rương, idle"]
        end
        subgraph Pool["Thread pool"]
            W["Worker 1..N"]
        end
        DB[("SQLite")]
        LOG[["File log"]]
    end

    CNET <-->|"TCP, mỗi dòng 1 JSON"| CONN
    LOOP --> CONN --> DISP
    DISP --> SESS & TEAM & LOBBY & MATCH
    TIMER -->|"timeout của epoll_wait"| LOOP
    DISP -->|"hàng đợi việc"| W
    W -->|"eventfd + hàng đợi kết quả"| LOOP
    W --> DB
    W --> LOG
```
