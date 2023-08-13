#include "server/server.h"

#include <unistd.h>

int main() {
    // 守护进程
    daemon(1, 0);

    Server server(
        9006, 3, 60000, true,
        3306, "weilai", "", "mydb",
        12, 12, true, 1
    );
    server.start();
}