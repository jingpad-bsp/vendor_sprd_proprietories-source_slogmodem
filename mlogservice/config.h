/*
 *  config.h - conf parsing definition.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#ifndef _CONFIG_H_
#define _CONFIG_H_

#define MAX_NAME 15

struct conf_file{
  uint8_t name[MAX_NAME];
  int mode;
  const uint8_t* at_check;
};

extern struct conf_file conf[];
extern uint8_t work_conf_file[];

int find_match_item(const uint8_t* buf, size_t name_len, int* index);
int parse_dsp_dest(const uint8_t* buf, int*  dest);
int parse_bool_param(const uint8_t* buf, int* mode);
int parse_number(const uint8_t* buf, unsigned long* num);

int parse_line(const uint8_t* buf, int line_num);
int parse_config_file(const uint8_t* config_file_path);
int read_config();
void set_config_files();

#endif // !CONFIG_H_
