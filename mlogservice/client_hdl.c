/*
 *  client_hdl.c - Client data process.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#include <cutils/sockets.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.h"
#include "client_hdl.h"
#include "misc.h"
#include "proc_cmd.h"
#include "socket_hdl.h"

static pthread_mutex_t socket_accept_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t socket_accept_condition = PTHREAD_COND_INITIALIZER;
static int client_fd = -1;


void process_req(const uint8_t* req, size_t len,const int c_fd) {
  const uint8_t* req_start = req;
  size_t orig_len = len;
  size_t tok_len;
  const uint8_t* endp = req + len;

  const uint8_t* token = get_token(req, len, &tok_len);

  if (!token) {
    return;
  }
  req = token + tok_len;
  len = endp - req;
  if (3 == tok_len && !memcmp(token, "GET", 3)) {
    proc_get(req, len, c_fd);
  } else if (3 == tok_len && !memcmp(token, "SET", 3)) {
    proc_set(req, len, c_fd);
  } else {
    send_response(c_fd,REC_UNKNOWN_REQ);//can't analysis request
  }
}

void process_data(struct DataBuffer* m_buffer, const int c_fd) {
  const uint8_t* start = m_buffer->buffer  + m_buffer->data_start;
  const uint8_t* end = start + m_buffer->data_len;
  size_t rlen = m_buffer->data_len;

  while(start < end) {
    const uint8_t* p1 = search_end(start, rlen);
    if (!p1) {//Not complete request
      break;
    }
    process_req(start, p1 - start, c_fd);
    start = p1 + 1;
    rlen = end - start;
  }

  if (rlen && start != m_buffer->buffer) {
    memmove(m_buffer->buffer, start , rlen);
  }
  m_buffer->data_len = rlen;
  m_buffer->data_start = rlen;
}

void* ls_socket_rw_thread(void* arg) {
  struct pollfd pol;
  uint8_t socket_rbuf[LS_DATA_LENGTH];
  struct DataBuffer m_buffer = {socket_rbuf, LS_DATA_LENGTH, 0, 0};

  pol.fd = *(int*)arg;
  pol.events = POLLIN;
  pol.revents = 0;

  info_log("%s socket_fd = %d", __FUNCTION__, pol.fd);
  while (true) {
    int err = poll(&pol, 1, -1);
    if (err > 0) {
      if (pol.revents && POLLIN == POLLIN) {
        size_t free_size = m_buffer.buf_size - m_buffer.data_start;
        ssize_t n = read(pol.fd, m_buffer.buffer + m_buffer.data_start, free_size);
        if (n > 0) {
          m_buffer.data_len += n;
          process_data(&m_buffer, pol.fd);
        } else if (n < 0) {
          err_log("read client error n = %d, errno = %d", n,errno);
          break;
        } else {
          err_log("client close the connection");
          break;
        }
      }
    } else {  //poll error
      err_log("poll error");
      break;
    }
  }
  info_log("%s close socket", __FUNCTION__);
  close(pol.fd);
  pthread_mutex_lock(&socket_accept_mutex);
  *(int*)arg = -1;
  pthread_mutex_unlock(&socket_accept_mutex);
  pthread_cond_signal(&socket_accept_condition);
  return NULL;
}

int client_process(void) {
  int fd = -1;
  pthread_t t_ls_skt_rw;

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  pthread_mutex_init(&socket_accept_mutex, 0);
  pthread_cond_init(&socket_accept_condition, 0);


  fd = socket_local_server("modem_log_service",
                           ANDROID_SOCKET_NAMESPACE_ABSTRACT,
                           SOCK_STREAM);
  if (fd < 0) {
    err_log("open modem_log_service failed");
    return 2;
  }

  while (true) {
    pthread_mutex_lock(&socket_accept_mutex);
    while (client_fd >= 0) {
      pthread_cond_wait(&socket_accept_condition, &socket_accept_mutex);
    }
    pthread_mutex_unlock(&socket_accept_mutex);
    if ((client_fd = accept(fd, (struct sockaddr*)NULL, NULL)) >= 0) {
      info_log("client connected!connect_fd:%d", client_fd );
      if (0 != pthread_create(&t_ls_skt_rw, &attr, ls_socket_rw_thread, &client_fd)) {
          err_log("log_service socket rw_thread create error");
          client_fd = -1;
      }
    }
  }
  close(fd);
  return 0;
}
