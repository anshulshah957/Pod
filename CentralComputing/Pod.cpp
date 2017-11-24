#include "Pod.h"

using namespace std;
Sensor * sensor;
Network * network;
Xmega * xmega;
bool running = true;


void write_loop(){
  bool active_connection = true;
  while(running && active_connection){
    usleep(100000);
    int written = network->write_data();
    print_info("Written is %d\n", written);
    active_connection = written != -1;
  }
  print_info("Writing failed, exiting write thread\n");
}

void read_loop(){
  bool active_connection = true;
  Network_Command buffer;
  while (running && active_connection){
    int bytes_read = network->read_command(&buffer);
    active_connection = bytes_read != -1;
  }
}
void network_loop(){
  while(running){ 
    int clientfd = network->accept_client(); 
    print_info("Client fd is: %d\n", clientfd);
    if(clientfd > 0){
      print_info("Starting network threads\n");
      thread read_thread(read_loop);
      thread write_thread(write_loop);

      read_thread.join();
      write_thread.join();
      print_info("Client exited, looking for next client\n");
    } else {
      print_info("Accept failed, abort\n");
      break;
    }
  }
}

void logic_loop(){
  while(running){
    xmega->read();
    sensor->update_buffers();
  }
}

void int_handler(int signo){
  network->close_server();
  running = false;
}

void pipe_handler(int signo){

}
int main(){

  // parsing, setup, etc
  signal(SIGINT, int_handler);
  signal(SIGPIPE, pipe_handler);
  xmega = new Xmega(); 
  sensor = new Sensor(xmega);
  network = new Network(sensor);
  const char* host = "127.0.0.1";
  const char* port = "8800";
  network->start_server(host, port);

  thread network_thread(network_loop);
  thread logic_thread(logic_loop);
  network_thread.join(); 
  logic_thread.join();
  return 0; 
}
