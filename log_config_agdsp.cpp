/*
 *  log_config_agdsp.cpp - The AGDSP configurations.
 *
 *  Copyright (C) 2015-2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-11-13 Zhang Ziyi
 *  Initial version.
 */

#include "log_config.h"
#include "parse_utils.h"

int LogConfig::parse_agdsp_log_dest(const uint8_t* buf,
                                    LogConfig::AgDspLogDestination& dest) {
  const uint8_t* t;
  size_t tlen;

  t = get_token(buf, tlen);
  if (!t) {
    return -1;
  }

  int ret = 0;

  if (3 == tlen && !memcmp(t, "off", 3)) {
    dest = AGDSP_LOG_DEST_OFF;
  } else if (4 == tlen && !memcmp(t, "uart", 4)) {
    dest = AGDSP_LOG_DEST_UART;
  } else if (2 == tlen && !memcmp(t, "ap", 2)) {
    dest = AGDSP_LOG_DEST_AP;
  } else {
    ret = -1;
  }

  return ret;
}

void LogConfig::set_agdsp_log_dest(AgDspLogDestination dest) {
  if (dest != m_agdsp_log_dest) {
    m_agdsp_log_dest = dest;
    m_dirty = true;
  }
}

void LogConfig::enable_agdsp_pcm_dump(bool enable /*= true*/) {
  if (enable != m_agdsp_pcm_dump) {
    m_agdsp_pcm_dump = enable;
    m_dirty = true;
  }
}
