/**
 * MessageSender.cpp
 *
 *  Created on: Jun 22, 2016
 *      Author: Mateusz Midor
 */

#include "MessageSender.h"
#include "MessageCommon.h"

using namespace unixsocketipc;

MessageSender::MessageSender() {
}

/**
 * @name    ~MessageSender
 * @brief   Destructor. Clean up underlying unix socket resources
 */
MessageSender::~MessageSender() {
   // close the socket
   if (server_socket_fd)
      ::close(server_socket_fd);
}

/**
 * @name    init
 * @brief   Setup the sender for sending messages on socket pointed by filename
 * @param   filename Socket filename
 * @return  True if successful, False otherwise
 * @note    This method must be called before "send"
 *          The listener must be already running on the same socket filename or init will fail
 */
bool MessageSender::init(const char *filename) {
   // remember socket filename
   socket_filename = filename;

   // get a socket filedescriptor
   server_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);

   // check socket for failure
   if (server_socket_fd == -1) {
      DEBUG_MSG("%s: socket(AF_UNIX, SOCK_STREAM, 0) failed\n", __FUNCTION__);
      return false;
   }

   DEBUG_MSG("%s: connecting to listener %s...\n", __FUNCTION__, socket_filename.c_str());
      sockaddr_un remote;
      remote.sun_family = AF_UNIX;
      strcpy(remote.sun_path, socket_filename.c_str());
      unsigned length = strlen(remote.sun_path) + sizeof(remote.sun_family);
      if (connect(server_socket_fd, (sockaddr*)&remote, length) == -1) {
         DEBUG_MSG("%s: connect failed\n", __FUNCTION__);
         ::close(server_socket_fd);
         server_socket_fd = 0; // not initialized
         return false;
      }
   DEBUG_MSG("%s: done.\n", __FUNCTION__);

   // success
   return true;
}

/**
 * @name    send
 * @brief   Send a message to the receiver
 * @param   id Message ID
 * @param   data Message payload
 * @param   size Message payload size in bytes
 * @return  True if send was successful, False otherwise
 */
bool MessageSender::send(uint32_t id, const char *data, uint32_t size) {

   std::lock_guard<std::mutex> guard(mtx);
   if (!server_socket_fd) {
      DEBUG_MSG("%s: not initialized\n", __FUNCTION__);
      return false;
   }

   // send the message
   DEBUG_MSG ("%s: sending message: id:%d, size:%d\n", __FUNCTION__, id, size);
   if (!send_message(id, data, size)) {
      DEBUG_MSG("%s: send_message failed\n", __FUNCTION__);
      if (errno == EPIPE)
          DEBUG_MSG("%s: errno: EPIPE (connection broken)\n", __FUNCTION__);
      return false;
   }

   return true;
}

/**
 * @name    send_message
 * @note    Implementation detail
 */
bool MessageSender::send_message(uint32_t id, const char *buf, uint32_t size) {
   if (!send_buffer(reinterpret_cast<char*>(&id), sizeof(id)))
      return false;

   if (!send_buffer(reinterpret_cast<char*>(&size), sizeof(size)))
      return false;

   if (!send_buffer(buf, size))
      return false;

   return true;
}

/**
 * @name    send_buffer
 * @note    Implementation detail
 */
bool MessageSender::send_buffer(const char *buf, uint32_t size) {
   auto bytes_left = size;
   int num_bytes_sent;
   while ((bytes_left > 0) && ((num_bytes_sent = ::send(server_socket_fd, buf, bytes_left, MSG_NOSIGNAL)) > 0)) {
      bytes_left -= num_bytes_sent;
      buf += num_bytes_sent;
      DEBUG_MSG("%s: sent %d bytes\n", __FUNCTION__, num_bytes_sent);
   }

   return (bytes_left == 0); // success if all bytes sent
}

/**
 * @name    send_stop_listener
 * @brief   Notify the listener to stop listening
 * @return  True if send was successful, False otherwise
 */
bool MessageSender::send_stop_listener() {
   return send(STOP_LISTENING_MSG_ID, nullptr, 0);
}
