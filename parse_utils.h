/*
 *  parse_utils.h - The parsing utility functions declarations.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-2-16 Zhang Ziyi
 *  Initial version.
 */
#ifndef _PARSE_UTILS_H_
#define _PARSE_UTILS_H_

#include <cstdint>
#include <cstddef>

const uint8_t* get_token(const uint8_t* buf, size_t& tlen);
const uint8_t* get_token(const uint8_t* data, size_t len, size_t& tlen);

const uint8_t* find_str(const uint8_t* data, size_t len, const uint8_t* pat,
                        size_t pat_len);

/*  parse_number - parse a number.
 *  @data: the data that contains the number. parse_number() expects
 *         each byte in data is in the range of '0' to '9'.
 *  @len: the length of data in byte.
 *  @num: the number to return.
 *
 *  Return Value:
 *    0: success. The number value is returned in num.
 *    -1: invalid number.
 */
int parse_number(const uint8_t* data, size_t len, unsigned& num);

/*  parse_number - parse a number from the data.
 *  @data: the data that contains the number. parse_number() expects
 *         the first char is a digit.
 *  @len: the length of data in byte.
 *  @num: the number to return.
 *  @parsed: the number of bytes that are parsed.
 *
 *  Return Value:
 *    0: success. The number value is returned in num.
 *    -1: invalid number.
 */
int parse_number(const uint8_t* data, size_t len, unsigned& num,
                 size_t& parsed);

#endif  // !_PARSE_UTILS_H_
