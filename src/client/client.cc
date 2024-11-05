#include <arpa/inet.h>
#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_ADRESS "127.0.0.1"
#define SERVER_PORT 4000

void permutations(int *arr, int len, int n, unsigned long &res) {
  if (n == 1) {
    res++;
    return;
  } else {
    for (int i = 0; i < n - 1; i++) {
      permutations(arr, len, n - 1, res);
      if (n % 2 == 0) {
        int buff = arr[i];
        arr[i] = arr[n - 1];
        arr[n - 1] = buff;
      } else {
        int buff = arr[0];
        arr[0] = arr[n - 1];
        arr[n - 1] = buff;
      }
    }
    permutations(arr, len, n - 1, res);
  }
}

int main() {
  int client_socket;
  struct sockaddr_in server_address;

  client_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (client_socket == -1) {
    std::cerr << "Ошибка создания сокета" << std::endl;
    return 1;
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = inet_addr(SERVER_ADRESS);
  server_address.sin_port = htons(SERVER_PORT);

  if (connect(client_socket, (struct sockaddr *)&server_address,
              sizeof(server_address)) == -1) {
    std::cerr << "Ошибка подключения к серверу" << std::endl;
    return 1;
  }

  std::cout << "Подключение к серверу установлено" << std::endl;

  while (true) {
    int *arr, len;
    recv(client_socket, &len, sizeof(int), 0);
    len = ntohl(len);
    arr = new int[len];
    recv(client_socket, arr, sizeof(int) * len, 0);
    std::cout << "Полученные числа:" << std::endl;
    for (int i = 0; i < len; i++) {
      arr[i] = ntohl(arr[i]);
      std::cout << arr[i] << ' ';
    }
    std::cout << std::endl;
    getchar();
    unsigned long res = 0;
    permutations(arr, len, len, res);
    res = htonl(res);

    send(client_socket, &res, sizeof(int), 0);
  }

  close(client_socket);

  return 0;
}