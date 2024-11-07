#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#define SERVER_ADRESS "127.0.0.1"
#define SERVER_PORT 4000

struct data {
  int *arr;
  int len;
};

data gen_data() {
  data res;
  res.len = 5 + rand() % 5;
  res.arr = new int[res.len];
  for (int i = 0; i < res.len; i++) {
    res.arr[i] = rand() % 1000;
  }
  return res;
}

struct client {
  int socket;
  data d;

  void send_data(int *arr, int len, bool &client_disconnected) {
    d.arr = arr;
    d.len = len;
    int n = htonl(d.len);
    for (int i = 0; i < len; i++) {
      arr[i] = htonl(arr[i]);
    }
    if (send(socket, &n, sizeof(int), 0) == -1 ||
        send(socket, d.arr, sizeof(int) * d.len, 0) == -1) {
      std::cerr << "Ошибка отправки данных клиенту" << std::endl;
      client_disconnected = true;
    }
  }
  uint get_data(bool &client_disconnected) {
    int result;
    int bytes_received = recv(socket, &result, sizeof(result), 0);

    if (bytes_received > 0) {
      result = ntohl(result);
      if (result == -1) {
        std::cout << "Клиент сообщил о завершении соединения." << std::endl;
        client_disconnected = true;
      } else {
        std::cout << "Клиент отправил результат: " << result << std::endl;
        int disconnect_signal;
        int additional_bytes =
            recv(socket, &disconnect_signal, sizeof(int), MSG_DONTWAIT);

        if (additional_bytes > 0 && ntohl(disconnect_signal) == -1) {
          std::cout << "Клиент отправил сигнал об отключении после отправки "
                       "результата."
                    << std::endl;
          client_disconnected = true;
        }
      }
    } else if (bytes_received == 0 ||
               (bytes_received == -1 && errno != EWOULDBLOCK)) {
      if (bytes_received == 0) {
        std::cout << "Клиент разорвал соединение" << std::endl;
      } else {
        std::cerr << "Ошибка приема результата от клиента" << std::endl;
      }
      client_disconnected = true;
    }
    return result;
  };
};

class server {
private:
  int server_socket;
  struct sockaddr_in server_address;
  int wakeup_socket[2];

public:
  std::mutex clients_mutex;
  std::vector<client> clients;
  std::atomic<bool> running;

  int init(const char *adress, int port) {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
      std::cerr << "Ошибка создания сокета" << std::endl;
      return 01;
    }
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(adress);
    if (bind(server_socket, (struct sockaddr *)&server_address,
             sizeof(server_address)) == -1) {
      std::cerr << "Ошибка привязки сокета" << std::endl;
      close(server_socket);
      return 1;
    }

    if (listen(server_socket, 5) == -1) {
      std::cerr << "Ошибка прослушивания подключений" << std::endl;
      close(server_socket);
      return 1;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, wakeup_socket) == -1) {
      std::cerr << "Ошибка создания wakeup сокетов" << std::endl;
      close(server_socket);
      return 1;
    }

    std::cout << "Сервер запущен на порту " << SERVER_PORT << std::endl;
    running = true;
    return 0;
  }

  void work() {

    int client_socket;
    struct sockaddr_in client_address;
    socklen_t client_address_size;

    fd_set readfds;
    fd_set writefds;
    int max_fd;

    while (running) {
      FD_ZERO(&readfds);
      FD_ZERO(&writefds);
      FD_SET(server_socket, &readfds);
      FD_SET(wakeup_socket[1], &readfds);
      max_fd = std::max(server_socket, wakeup_socket[1]);

      for (int i = 0; i < clients.size(); i++) {
        FD_SET(clients[i].socket, &readfds);
        FD_SET(clients[i].socket, &writefds);
        if (clients[i].socket > max_fd) {
          max_fd = clients[i].socket;
        }
      }

      int activity = select(max_fd + 1, &readfds, &writefds, NULL, NULL);
      if (activity == -1) {
        std::cerr << "Ошибка select" << std::endl;
        close(server_socket);
        return;
      }

      if (FD_ISSET(wakeup_socket[1], &readfds)) {
        break;
      }

      if (FD_ISSET(server_socket, &readfds)) {
        client_address_size = sizeof(client_address);
        client_socket =
            accept(server_socket, (struct sockaddr *)&client_address,
                   &client_address_size);
        if (client_socket != -1) {
          clients.push_back({client_socket, {0, 0}});
          std::cout << "Подключен новый клиент" << std::endl;
        }
      }

      for (int i = 0; i < clients.size();) {
        bool client_disconnected = false;

        if (!client_disconnected && FD_ISSET(clients[i].socket, &readfds)) {
          clients[i].get_data(client_disconnected);
        }

        if (!client_disconnected && FD_ISSET(clients[i].socket, &writefds)) {
          struct data d = gen_data();
          clients[i].send_data(d.arr, d.len, client_disconnected);
          delete d.arr;
        }

        if (client_disconnected) {
          close(clients[i].socket);
          clients.erase(clients.begin() + i);
        } else {
          i++;
        }
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  void stop() {
    running = false;
    write(wakeup_socket[0], "x", 1);
  }

  void clean_up() {
    for (int i = 0; i < clients.size(); i++) {
      close(clients[i].socket);
    }
    close(server_socket);
  }
};

int main() {

  server serv;
  if (serv.init(SERVER_ADRESS, SERVER_PORT) != 0) {
    return 0;
  }
  std::thread server_thread(&server::work, &serv);

  std::string input;
  while (true) {
    std::cout << "Введите команду (stop): ";
    std::getline(std::cin, input);
    if (input == "stop") {
      serv.stop();
      std::cout << "Выключение сервера..." << std::endl;
      break;
    } else {
      std::cout << "Неизвестная команда." << std::endl;
    }
  }

  server_thread.join();
  serv.clean_up();
  std::cout << "Сервер выключен, завершение программы..." << std::endl;
  return 0;
}