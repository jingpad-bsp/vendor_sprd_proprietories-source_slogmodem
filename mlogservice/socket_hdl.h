/*
 *  socket_hdl.h - Socket send and receive definition.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#ifndef _SOCKET_HDL_H
#define _SOCKET_HDL_H

enum ResponseErrorCode {
  REC_SUCCESS,
  REC_UNKNOWN_REQ,
  REC_INVAL_PARAM,
  REC_FAILURE,
  REC_IN_TRANSACTION,
  REC_CP_ASSERTED,
  REC_MAXERROR
};
int close_at_channel(void);
int open_at_channel(void);
int send_response(int conn, enum ResponseErrorCode err);
int sersendCmd(const uint8_t* cmd, char* res, uint8_t* socketName);
int send_at(const uint8_t* cmd, int len, int* fd);
int send_at_and_get_result(const uint8_t*cmd, uint8_t* res, int len);
int send_at_parse_result(const uint8_t* cmd, uint8_t* res, int len);

#endif // !SOCKET_HDL_H_
