#include <cassert>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <cerrno>

#include "services.h"
#include "impl_msgq.hpp"


static bool service_exists(std::string path){
  for (const auto& it : services) {
    if (it.name == path) {
      return true;
    }
  }
  return false;
}

static size_t get_size(std::string endpoint){
  size_t sz = DEFAULT_SEGMENT_SIZE;

  if (endpoint == "roadCameraState" || endpoint == "driverCameraState" || endpoint == "wideRoadCameraState"){
    sz *= 10;
  }

  return sz;
}


MSGQContext::MSGQContext() {
}

MSGQContext::~MSGQContext() {
}

void MSGQMessage::init(size_t sz) {
  size = sz;
  data = new char[size];
}

void MSGQMessage::init(char * d, size_t sz) {
  size = sz;
  data = new char[size];
  memcpy(data, d, size);
}

void MSGQMessage::takeOwnership(char * d, size_t sz) {
  size = sz;
  data = d;
}

void MSGQMessage::close() {
  if (size > 0){
    delete[] data;
  }
  size = 0;
}

MSGQMessage::~MSGQMessage() {
  this->close();
}

int MSGQSubSocket::connect(Context *context, std::string endpoint, std::string address, bool conflate, bool check_endpoint){
  assert(context);
  assert(address == "127.0.0.1");

  if (check_endpoint && !service_exists(std::string(endpoint))){
    std::cout << "Warning, " << std::string(endpoint) << " is not in service list." << std::endl;
  }

  q = new msgq_queue_t;
  int r = msgq_new_queue(q, endpoint.c_str(), get_size(endpoint));
  if (r != 0){
    return r;
  }

  msgq_init_subscriber(q);

  if (conflate){
    q->read_conflate = true;
  }

  return 0;
}


Message * MSGQSubSocket::receive(){
  MSGQMessage *r = NULL;

  msgq_msg_t msg;
  int rc = msgq_msg_recv(&msg, q);
  if (rc > 0){
    r = new MSGQMessage;
    r->takeOwnership(msg.data, msg.size);
  }

  return (Message*)r;
}

MSGQSubSocket::~MSGQSubSocket(){
  if (q != NULL){
    msgq_close_queue(q);
    delete q;
  }
}

int MSGQPubSocket::connect(Context *context, std::string endpoint, bool check_endpoint){
  assert(context);

  if (check_endpoint && !service_exists(std::string(endpoint))){
    std::cout << "Warning, " << std::string(endpoint) << " is not in service list." << std::endl;
  }

  q = new msgq_queue_t;
  int r = msgq_new_queue(q, endpoint.c_str(), get_size(endpoint));
  if (r != 0){
    return r;
  }

  msgq_init_publisher(q);

  return 0;
}

int MSGQPubSocket::sendMessage(Message *message){
  msgq_msg_t msg;
  msg.data = message->getData();
  msg.size = message->getSize();

  return msgq_msg_send(&msg, q);
}

int MSGQPubSocket::send(char *data, size_t size){
  msgq_msg_t msg;
  msg.data = data;
  msg.size = size;

  return msgq_msg_send(&msg, q);
}

MSGQPubSocket::~MSGQPubSocket(){
  if (q != NULL){
    msgq_close_queue(q);
    delete q;
  }
}


void MSGQPoller::registerSocket(SubSocket * socket){
  assert(num_polls + 1 < MAX_POLLERS);
  polls[num_polls].q = (msgq_queue_t*)socket->getRawSocket();

  sockets.push_back(socket);
  num_polls++;
}

std::vector<SubSocket*> MSGQPoller::poll(int timeout){
  std::vector<SubSocket*> r;

  msgq_poll(polls, num_polls, timeout);
  for (size_t i = 0; i < num_polls; i++){
    if (polls[i].revents){
      r.push_back(sockets[i]);
    }
  }

  return r;
}
