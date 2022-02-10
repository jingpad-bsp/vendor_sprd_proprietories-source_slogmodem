/*
 *  parse_at.h - At commands return value analysis definition.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#ifndef _PARSE_AT_H_
#define _PARSE_AT_H_

int parse_at_res(const uint8_t* cmd, uint8_t* buf, int len);

#endif // !PARSE_AT_H_
